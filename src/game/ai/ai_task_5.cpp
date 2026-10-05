#include "ai/ai_records.h"
#include "ai/ai_side_controller.h"
#include "ai/ai_task_types.h"
#include "ai/ai_task_util.h"
#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "economy/defense_facility.h"
#include "economy/manufacturing_facility.h"
#include "economy/production_facility_types.h"
#include "galaxy/system.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "orders/orders_210_216.h"

using namespace aitask;

namespace {

constexpr int kScrapOrder = 0x200;
constexpr int kSpecialForceOrder = 0x242;
constexpr int kManufactureOrder210 = 0x210;
constexpr int kManufactureOrder212 = 0x212;
constexpr int kDestinationOrder = 0x214;

constexpr int kStageProduce = 0x3f4;
constexpr int kStageTargets = 0x3ec;

constexpr uint32_t kSystemClaimed = 0x400000;
constexpr uint32_t kSystemDropped = 0x10000000;

ObjectKeyValueNode* FirstNodeOfKind(ObjectKeyValueSet* team, ChildIterator* it) {
    it->Begin();
    return AddCurrentToTeam(team, it);
}

}  // namespace

AiTask5::AiTask5(int view) : AiStagedTask(5, view), m_key58(), m_system(), m_manager(), m_classKey(), m_selector(), m_targets(), m_dropped() {
    m_classKey.m_value = 0;
    m_stage = 0;
    m_step = 0;
    m_kind = 0;
    m_x48 = 0;
    m_x4c = 0;
    m_x50 = 0;
    m_x54 = 0;
    m_x24 = 4;
    m_targets.Clear();
    m_dropped.Clear();
}

AiTask5::~AiTask5() { REB_KEEP_VPTR_STORE(); }

void AiTask5::Read(istream* in) {
    AiTaskBase::Read(in);
    ReadValue32(in, &m_stage);
    ReadValue32(in, &m_step);
    ReadValue16(in, &m_kind);
    ReadValue32(in, &m_x48);
    ReadValue32(in, &m_x4c);
    ReadValue32(in, &m_x50);
    ReadValue32(in, &m_x54);
    ReadValue32(in, &m_x44);
    m_selector.Read(in);
    m_key58.Read(in);
    m_system.Read(in);
    m_manager.Read(in);
    m_classKey.Read(in);
    m_targets.Load(in);
    m_dropped.Load(in);
}

void AiTask5::Write(ostream* out) {
    AiTaskBase::Write(out);
    WriteValue32(out, &m_stage);
    WriteValue32(out, &m_step);
    WriteValue16(out, &m_kind);
    WriteValue32(out, &m_x48);
    WriteValue32(out, &m_x4c);
    WriteValue32(out, &m_x50);
    WriteValue32(out, &m_x54);
    WriteValue32(out, &m_x44);
    m_selector.Write(out);
    m_key58.Write(out);
    m_system.Write(out);
    m_manager.Write(out);
    m_classKey.Write(out);
    m_targets.Save(out);
    m_dropped.Save(out);
}

OrderBase* AiTask5::Step() {
    if (m_flags.Get() != 1) return nullptr;
    if (m_stage != kStageTargets) {
        if (m_stage != kStageProduce) {
            m_stage = kStageProduce;
            return nullptr;
        }
        return StepProduce();
    }
    return StepTargets();
}

OrderBase* AiTask5::StepProduce() {
    OrderBase* order = nullptr;
    switch (m_step) {
    case 1:
        m_step = NextKind() != 0 ? 5 : 6;
        return nullptr;
    case 2:
        order = BuildSpecialForceOrder();
        break;
    default:
        m_step = 9;
        return nullptr;
    case 5:
        m_step = ComputeSpecialForceCount();
        if (m_step != 0) return nullptr;
        m_stage = kStageTargets;
        m_finished = 1;
        return nullptr;
    case 6:
        if (IsOverQuota() != 0) {
            m_step = 2;
            return nullptr;
        }
        break;
    case 9:
        m_step = ChooseManager() != 0 ? 1 : 6;
        return nullptr;
    case 0xb:
        order = BuildManufactureOrder212();
        break;
    }
    m_step = 0;
    m_stage = kStageTargets;
    m_finished = 1;
    return order;
}

OrderBase* AiTask5::StepTargets() {
    OrderBase* order = nullptr;
    switch (m_step) {
    case 3:
        m_step = SelectTarget() != 0 ? 9 : 7;
        return nullptr;
    case 4:
        order = BuildScrapOrder();
        if (order) {
            m_finished = 1;
            ++m_priority;
        }
        m_step = 5;
        return order;
    case 5:
        m_step = ComputeFacilityCount();
        if (m_step != 0) return nullptr;
        m_stage = kStageProduce;
        m_finished = 1;
        return nullptr;
    default:
        m_step = 3;
        return nullptr;
    case 7:
        if (IsOverQuotaForFacilities() == 0) {
            m_step = 0;
            m_stage = kStageProduce;
            return nullptr;
        }
        m_step = 8;
        return nullptr;
    case 8:
        order = BuildCapitalOrder();
        break;
    case 9:
        m_owner->m_bitsA &= ~4u;
        if (m_owner->m_bitB == 4 && PickSupplySystem() != 0) {
            m_owner->AdvanceBitCursor();
            m_step = 4;
            return nullptr;
        }
        m_step = 0xc;
        return nullptr;
    case 10: {
        OrderBase* destination = BuildDestinationOrder(&m_system);
        if (destination) {
            m_step = 0xb;
            m_finished = 1;
            ++m_priority;
            return destination;
        }
        break;
    }
    case 0xb:
        order = BuildManufactureOrder210();
        break;
    case 0xc:
        AddTargetSystem();
        RefreshTargets();
        break;
    }
    m_step = 0;
    m_stage = kStageProduce;
    m_finished = 1;
    return order;
}

int AiTask5::ChooseManager() {
    int result = 0;
    bool found = false;
    m_owner->m_key18.Reset();
    ObjectKeyNode* node = m_targets.First();
    while (node && !found) {
        m_manager = KeyOfNode(node);
        AiSystemEval* eval = IsSystemKey(m_manager) ? m_owner->m_systems.FindEval(m_manager) : nullptr;
        const bool stale = !eval || (eval->m_flags30.m_value & 1) == 0 || (eval->m_flags28.m_value & kSystemClaimed) == 0;
        if (stale) {
            if (eval) {
                eval->m_flags28.m_value &= ~kSystemClaimed;
                eval->MarkChanged();
            }
            ObjectKeyNode* next = NextNode(node);
            ObjectKey key = KeyOfNode(node);
            m_targets.RemoveKey(&key);
            node = next;
            continue;
        }
        if (0 < eval->Stat(0x5c) && (eval->m_flags28.m_value & 0x400) != 0 && (eval->m_flags28.m_value & 3) == 0) {
            m_owner->m_key18 = m_manager;
            found = true;
        }
        node = NextNode(node);
    }
    if (!found) {
        m_manager = m_owner->m_key14;
        if (IsSystemKey(m_manager)) {
            AiSystemEval* eval = m_owner->m_systems.FindEval(m_manager);
            if (eval && (eval->m_flags30.m_value & 1) != 0 && (eval->m_flags28.m_value & kSystemClaimed) != 0 &&
                0 < eval->Stat(0x5c) && (eval->m_flags28.m_value & 0x400) != 0 && (eval->m_flags28.m_value & 3) == 0)
                found = true;
        }
        if (!found) {
            m_manager.Reset();
            return 0;
        }
    }
    System* system = static_cast<System*>(GetObjectInFamilies_90_98(m_view, &m_manager));
    if (system) {
        GameObjNotifier* manager = system->FindManufacturingManagerBySlot123(2);
        if (manager) {
            ObjectKey key;
            GetObjectKey(manager, &key);
            m_manager = key;
            result = 1;
        }
    }
    return result;
}

int AiTask5::NextKind() {
    bool wrapped = false;
    int found = 0;
    if (m_kind == 0) m_kind = 0x5a;
    const uint16_t start = m_kind;
    while (!wrapped) {
        const uint16_t current = m_kind;
        const int wanted = m_owner->Stat(0x2fc) < 8 ? 8 : m_owner->Stat(0x2fc);
        m_x48 = m_owner->m_stat[current] < wanted ? 1 : 0;
        switch (current) {
        case 0x5a:
            m_x50 = 0x1000000;
            m_x54 = 1;
            m_kind = 0x5b;
            break;
        case 0x5b:
            m_x50 = 0x2000000;
            m_x54 = 1;
            m_kind = 0x5c;
            break;
        case 0x5c:
            m_x50 = 0x4000000;
            m_x54 = 1;
            m_kind = 0x5d;
            break;
        case 0x5d:
            m_x50 = 0x8000000;
            m_x54 = 1;
            m_kind = 0x5a;
        }
        found = 0 < m_x48 ? 1 : 0;
        if (start == static_cast<uint16_t>(m_kind)) wrapped = true;
        if (found) break;
    }
    return found;
}

int AiTask5::IsOverQuota() { return Need() < 0; }

int AiTask5::ComputeSpecialForceCount() {
    int result = 0;
    const int need = Need();
    if (need < 1) {
        if (need < 0) result = 2;
        return result;
    }
    const int count = m_owner->ChooseClass(&m_classKey, 0x29, m_x50, m_x54, 1);
    if ((m_classKey.m_value & 0xff000000) != 0 && count != 0) {
        const int total = count * m_x48;
        if (total - need != 0 && need <= total) m_x48 = need / count;
    }
    if (0 < m_x48) {
        m_owner->m_key18.Reset();
        return 0xb;
    }
    return result;
}

OrderBase* AiTask5::BuildSpecialForceOrder() {
    ObjectKeyValueSet team;
    OrderBase* order = nullptr;
    ObjectKey picked;
    ObjectKey scratch;
    m_selector.Add(m_owner->RankCharacters(0x802000, 0, 0xa0, 0, 9, 0, 1000, 1));
    picked = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    if (!ai::KeyInFamilies(picked, 0x3c, 0x40)) {
        m_selector.Add(m_owner->RankCharacters(0x2000, 0, 0xa0, 0, 9, 0, 1000, 1));
        picked = *m_selector.PickBest(&scratch);
        m_selector.Reset();
    }
    if (ai::KeyInFamilies(picked, 0x3c, 0x40)) {
        ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(picked, 0);
        if (node) team.Add(node);
    }
    if (team.First()) order = NewTeamOrder(kSpecialForceOrder, m_view, &team);
    return order;
}

int AiTask5::SelectTarget() {
    int result = 0;
    int best = 1000;
    ObjectKey scratch;
    m_system.Reset();
    m_x44 = 0;
    for (ObjectKeyNode* node = m_targets.First(); node; node = NextNode(node)) {
        AiSystemEval* eval = m_owner->m_systems.FindEval(KeyOfNode(node));
        if (eval && (eval->m_flags30.m_value & 1) != 0 && (eval->m_flags28.m_value & 3) == 0 && 0 < eval->Stat(0x114) &&
            eval->Stat(0x5c) < best) {
            m_x44 = eval->Stat(0x114);
            best = eval->Stat(0x5c);
            m_system = KeyOfNode(eval);
            result = 1;
        }
    }
    if (result == 0) {
        m_selector.Add(m_owner->RankSectors(0x40, 0, 0, 0, 0, 0, 0x13, 1));
        if (m_targets.Count() != 0) m_selector.Add(m_owner->RankSectors(0x40, 0, 0, 0, 0, 0, 4, 2));
        m_system = *m_selector.PickBest(&scratch);
        m_selector.Reset();
        m_selector.Add(m_owner->RankSystemsOfSector(&m_system, 0, 0, 1, 0x400003, 0, 0x40000000, 5, 1));
        if (m_targets.Count() != 0)
            m_selector.Add(m_owner->RankSystemsOfSector(&m_system, 0, 0, 1, 0x400003, 0, 0x40000000, 0x33, 1));
        m_system = *m_selector.PickBest(&scratch);
        m_selector.Reset();
        if (IsSystemKey(m_system)) {
            result = 1;
            AiSystemEval* eval = m_owner->m_systems.FindEval(m_system);
            if (eval) {
                ObjectKeyNode* node = NewOrNull<ObjectKeyNode>(m_system);
                if (node) {
                    m_targets.Add(node);
                    eval->m_flags28.m_value |= kSystemClaimed;
                    eval->MarkChanged();
                }
            }
        }
    }
    return result;
}

int AiTask5::PickSupplySystem() {
    int result = 0;
    ObjectKey scratch;
    m_x4c = 0;
    m_selector.Add(m_owner->RankSectors(0x1000, 0, 0, 0, 0, 0, 0x14, 1));
    m_manager = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    m_selector.Add(m_owner->RankSystemsOfSector(&m_manager, 0x800800, 0, 1, 3, 0, 0, 6, 1));
    m_manager = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    if (IsSystemKey(m_manager)) {
        AiSystemEval* eval = m_owner->m_systems.FindEval(m_manager);
        System* system = static_cast<System*>(GetObjectInFamilies_90_98(m_view, &m_manager));
        if (system && eval) {
            GameObjNotifier* manager = system->FindManufacturingManagerBySlot123(1);
            if (manager) {
                m_x4c = eval->Stat(0x60);
                if (1 < m_x4c) --m_x4c;
                ObjectKey key;
                GetObjectKey(manager, &key);
                m_manager = key;
                result = 1;
            }
        }
    }
    return result;
}

int AiTask5::IsOverQuotaForFacilities() { return NeedForTargets() < 0; }

OrderBase* AiTask5::BuildScrapOrder() {
    ObjectKeyValueSet team;
    OrderBase* order = nullptr;
    ObjectKeyValueNode* last = nullptr;
    if (!IsSystemKey(m_system)) return order;
    AiSystemEval* eval = m_owner->m_systems.FindEval(m_system);
    if (!eval) return order;
    GameObjNotifier* system = GetObjectInFamilies_90_98(m_view, &m_system);
    if (system) {
        if ((eval->m_flags28.m_value & 0x200000) == 0) last = AddAllOfKind<ManufacturingFacility28Iterator>(&team, system, last);
        if ((eval->m_flags28.m_value & 0x3800000) == 0) last = AddAllOfKind<ManufacturingFacility2AIterator>(&team, system, last);
        last = AddAllOfKind<DefenseFacility25Iterator>(&team, system, last);
        if (eval->Stat(0x84) == 0 && last == nullptr) {
            AiSideController* owner = m_owner;
            if (owner->Stat(0x1d0) < owner->Stat(0x1d4) || eval->Stat(0x50) == 0) {
                ProductionFacilityType2DIterator it(system, 1);
                last = FirstNodeOfKind(&team, &it);
            }
            if (last == nullptr && (owner->Stat(0x1d4) <= owner->Stat(0x1d0) || eval->Stat(0x54) == 0)) {
                ProductionFacilityType2CIterator it(system, 1);
                last = FirstNodeOfKind(&team, &it);
            }
        }
    }
    if (team.First()) order = NewTeamOrder(kScrapOrder, m_view, &team);
    return order;
}

int AiTask5::ComputeFacilityCount() {
    int result = 0;
    m_x48 = 0;
    const int need = NeedForTargets();
    if (need < 1) {
        if (need < 0) result = 8;
        return result;
    }
    const int count = m_owner->ChooseClass(&m_classKey, 0x2a, 0x8000, 0x4000, 2);
    if ((m_classKey.m_value & 0xff000000) != 0 && count != 0) m_x48 = need / count;
    if (0 < m_x48) {
        if (m_x4c < m_x48) m_x48 = m_x4c;
        AiSystemEval* eval = m_owner->m_systems.FindEval(m_system);
        if (eval) {
            int limit = eval->Stat(0x84);
            if (m_x44 < eval->Stat(0x84)) limit = m_x44;
            if (limit < m_x48) m_x48 = limit;
            return 10;
        }
    }
    return result;
}

OrderBase* AiTask5::BuildCapitalOrder() {
    OrderBase* order = nullptr;
    bool found = false;
    ObjectKeyNode* node = m_targets.First();
    while (node) {
        if (found) break;
        AiSystemEval* eval = m_owner->m_systems.FindEval(KeyOfNode(node));
        found = false;
        if (eval) {
            const bool several = 1 < m_targets.Count();
            if (eval->Stat(0x5c) < 1) {
                ObjectKeyNode* next = NextNode(node);
                ObjectKey key = KeyOfNode(node);
                m_targets.RemoveKey(&key);
                eval->m_flags28.m_value &= ~kSystemClaimed;
                node = next;
            } else if ((eval->m_flags28.m_value & 0x1000) == 0) {
                node = NextNode(node);
            } else if (eval->Stat(0x5c) < 2) {
                found = several;
                if (found) {
                    m_system = KeyOfNode(node);
                    ObjectKey key = KeyOfNode(node);
                    m_targets.RemoveKey(&key);
                    eval->m_flags28.m_value &= ~kSystemClaimed;
                }
                node = nullptr;
            } else {
                m_system = KeyOfNode(node);
                found = true;
            }
        }
    }
    if (!found) return order;
    ObjectKeyValueSet team;
    GameObjNotifier* system = GetObjectInFamilies_90_98(m_view, &m_system);
    if (system) {
        ManufacturingFacility29Iterator it(system, 1);
        it.Begin();
        if (it.Current()) {
            ObjectKey key;
            GetObjectKey(it.Current(), &key);
            ObjectKeyValueNode* made = NewOrNull<ObjectKeyValueNode>(key, 0);
            if (made) {
                team.Add(made);
                order = NewTeamOrder(kScrapOrder, m_view, &team);
            }
        }
    }
    return order;
}

OrderBase* AiTask5::BuildManufactureOrder212() {
    ObjectKeyValueSet team;
    OrderBase* order = nullptr;
    if (ai::KeyInFamilies(m_manager, 0xa4, 0xa6)) {
        order = static_cast<OrderBase*>(CreateMessageByKind(kManufactureOrder212));
        if (order) {
            ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(m_manager, 0);
            if (node) {
                team.Add(node);
                order->m_side = m_view;
                order->SetTeam(&team);
                ManufactureOrderBase* manufacture = static_cast<ManufactureOrderBase*>(order);
                manufacture->m_item.m_value = m_classKey.m_value;
                manufacture->m_count = m_x48;
            }
        }
    }
    return order;
}

OrderBase* AiTask5::BuildDestinationOrder(ObjectKey* target) {
    ObjectKeyValueSet team;
    OrderBase* order = nullptr;
    if (IsSystemKey(*target) && ai::KeyInFamilies(m_manager, 0xa0, 0xa2)) {
        order = static_cast<OrderBase*>(CreateMessageByKind(kDestinationOrder));
        if (order) {
            ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(m_manager, 0);
            if (node) {
                team.Add(node);
                order->m_side = m_view;
                order->SetTeam(&team);
                order->SetTarget(target);
            }
        }
    }
    return order;
}

OrderBase* AiTask5::BuildManufactureOrder210() {
    ObjectKeyValueSet team;
    OrderBase* order = nullptr;
    if (ai::KeyInFamilies(m_manager, 0xa0, 0xa2)) {
        order = static_cast<OrderBase*>(CreateMessageByKind(kManufactureOrder210));
        if (order) {
            ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(m_manager, 0);
            if (node) {
                team.Add(node);
                order->m_side = m_view;
                order->SetTeam(&team);
                ManufactureOrderBase* manufacture = static_cast<ManufactureOrderBase*>(order);
                manufacture->m_item.m_value = m_classKey.m_value;
                manufacture->m_count = m_x48;
            }
        }
    }
    return order;
}

void AiTask5::AddTargetSystem() {
    AiRankedSelector sectorSelector;
    ObjectKey sector;
    ObjectKey system;
    ObjectKey scratch;
    bool found = false;
    m_system.Reset();
    if (!(m_targets.Count() < 2)) return;
    const int need = NeedForTargets();
    if (0 < need) {
        sectorSelector.Add(m_owner->RankSectors(0x40, 0, 0, 0, 0, 0, 0x13, 1));
        sector = *sectorSelector.PickBest(&scratch);
        for (;;) {
            if (!IsSectorKey(sector) || found) {
                sectorSelector.Reset();
                break;
            }
            AiSectorEval* sectorEval = m_owner->m_sectors.FindEval(sector);
            if (!sectorEval || sectorEval->Stat(0xbc) <= 0) {
                sector.Reset();
                continue;
            }
            m_selector.Add(m_owner->RankSystemsOfSector(&sector, 0, 0, 1, 0x400003, 0, 0x40000000, 5, 1));
            system = *m_selector.PickBest(&scratch);
            while (IsSystemKey(system) && !found) {
                AiSystemEval* eval = m_owner->m_systems.FindEval(system);
                if (eval && 0 < eval->Stat(0x5c) && eval->Stat(0xd4) <= need) {
                    m_system = system;
                    found = true;
                }
                system = *m_selector.PickBest(&scratch);
            }
            m_selector.Reset();
            sector = *sectorSelector.PickBest(&scratch);
        }
    }
    if (IsSystemKey(m_system)) {
        AiSystemEval* eval = m_owner->m_systems.FindEval(m_system);
        if (eval) {
            ObjectKeyNode* node = NewOrNull<ObjectKeyNode>(m_system);
            if (node) {
                m_targets.Add(node);
                eval->m_flags28.m_value |= kSystemClaimed;
                eval->MarkChanged();
            }
        }
    } else {
        bool anyTargetNeedsFacilities = false;
        if (m_targets.Count() < 2) {
            ObjectKeyNode* first = m_targets.First();
            if (first) {
                AiSystemEval* eval = m_owner->m_systems.FindEval(KeyOfNode(first));
                if (eval && 0 < eval->Stat(0x5c)) anyTargetNeedsFacilities = true;
            }
            if (!anyTargetNeedsFacilities) {
                const int count = m_owner->ChooseClass(&m_classKey, 0x2a, 0x8000, 0x4000, 2);
                if (count <= need) m_owner->m_bitsA |= 4;
            }
        }
    }
}

int AiTask5::Need() {
    AiSideController* owner = m_owner;
    int need = (((m_x24 * owner->Stat(0x184)) / 100) * 0x46) / 100 - owner->Stat(0x2c4);
    if (owner->Stat(0x184) - owner->Stat(0x188) < need) need = 0;
    return need;
}

int AiTask5::NeedForTargets() {
    int used = 0;
    for (ObjectKeyNode* node = m_targets.First(); node; node = NextNode(node)) {
        AiSystemEval* eval = m_owner->m_systems.FindEval(KeyOfNode(node));
        if (eval) used += eval->Stat(0xd4);
    }
    const int total = m_owner->Stat(0x184);
    int need = (((m_x24 * total) / 100) * 0x1e) / 100 - used;
    if (total - m_owner->Stat(0x188) < need) need = 0;
    return need;
}

void AiTask5::RefreshTargets() {
    ObjectKeySet pending;
    ObjectKey best;
    int bestStat = 0;
    int budget = 1;
    for (;;) {
        best.Reset();
        for (ObjectKeyNode* node = m_targets.First(); node; node = NextNode(node)) {
            AiSystemEval* eval = m_owner->m_systems.FindEval(KeyOfNode(node));
            if (eval && bestStat < eval->Stat(0x5c)) {
                ObjectKey key = KeyOfNode(node);
                if ((eval->m_flags30.m_value & kSystemDropped) != 0 && m_dropped.Find(&key) == nullptr) break;
                key = KeyOfNode(node);
                if (pending.Find(&key) == nullptr && 0 < eval->Stat(0x90)) {
                    bestStat = eval->Stat(0x5c);
                    best = KeyOfNode(eval);
                }
            }
        }
        if (IsSystemKey(best)) {
            ObjectKeyNode* node = NewOrNull<ObjectKeyNode>(best);
            if (node) {
                pending.Add(node);
                --budget;
            }
        } else {
            budget = 0;
        }
        if (budget < 1) {
            for (ObjectKeyNode* node = m_dropped.First(); node; node = NextNode(node)) {
                AiSystemEval* eval = m_owner->m_systems.FindEval(KeyOfNode(node));
                if (eval) eval->m_flags30.m_value &= ~kSystemDropped;
            }
            m_dropped.Clear();
            ObjectKeyNode* node = pending.First();
            while (node) {
                AiSystemEval* eval = m_owner->m_systems.FindEval(KeyOfNode(node));
                if (eval) eval->m_flags30.m_value |= kSystemDropped;
                ObjectKeyNode* next = NextNode(node);
                ObjectKey key = KeyOfNode(node);
                pending.Detach(&key);
                m_dropped.Add(node);
                node = next;
            }
            return;
        }
    }
}
