#include "ai/ai_records.h"
#include "ai/ai_session_random.h"
#include "ai/ai_side_controller.h"
#include "ai/ai_system_record.h"
#include "ai/ai_task_family.h"
#include "ai/ai_task_types.h"
#include "ai/ai_task_util.h"
#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "economy/manu_mgr.h"
#include "fleet/fight_squad.h"
#include "galaxy/system.h"
#include "gameobj/object_lookup.h"

using namespace aitask;

namespace {

constexpr FamilyParams kParams = {
    0x50, 0x254, 0x14, 0xd0, 0xd0, 0x58, 0x20, 0x11, 0x200003, 4, 0x200000, 0xb4, 0x4000, 0x10, 9, 7,
    FacilityKind::kManufacturing29, 0x400000, FacilityKind::kManufacturing2A, 0x3800000, false, false, true, false};

constexpr int kScrapOrder = 0x200;
constexpr int kStageFleets = 0x3f2;
constexpr int kStageTargets = 0x3ec;
constexpr uint32_t kSystemClaimed = 0x200000;

constexpr uint32_t kUnitFlags = 0x1000;
constexpr int kUnreachableCost = 100000;

}  // namespace

AiTask10::AiTask10(int view)
    : AiStagedTask(10, view), m_selector(), m_manager(), m_unit(), m_system(), m_classKey(), m_targets(), m_dropped() {
    m_classKey.m_value = 0;
    m_stage = 0;
    m_step = 0;
    m_count = 0;
    m_cap = 0;
    m_limit = 0;
    m_recordId = 0;
    m_recordCursor = 0;
    m_targets.Clear();
    m_dropped.Clear();
    m_x24 = 8;
}

AiTask10::~AiTask10() { REB_KEEP_VPTR_STORE(); }

FamilyFields AiTask10::Fields() {
    return {m_owner, m_view, m_x24, &m_selector, &m_targets, &m_dropped, &m_system, &m_manager, &m_classKey, &m_count, &m_cap, &m_limit};
}

void AiTask10::Read(istream* in) {
    AiTaskBase::Read(in);
    ReadValue32(in, &m_stage);
    ReadValue32(in, &m_step);
    ReadValue32(in, &m_count);
    ReadValue32(in, &m_cap);
    ReadValue32(in, &m_limit);
    ReadValue32(in, &m_recordId);
    ReadValue32(in, &m_recordCursor);
    m_selector.Read(in);
    m_manager.Read(in);
    m_unit.Read(in);
    m_system.Read(in);
    m_targets.Load(in);
    m_dropped.Load(in);
}

void AiTask10::Write(ostream* out) {
    AiTaskBase::Write(out);
    WriteValue32(out, &m_stage);
    WriteValue32(out, &m_step);
    WriteValue32(out, &m_count);
    WriteValue32(out, &m_cap);
    WriteValue32(out, &m_limit);
    WriteValue32(out, &m_recordId);
    WriteValue32(out, &m_recordCursor);
    m_selector.Write(out);
    m_manager.Write(out);
    m_unit.Write(out);
    m_system.Write(out);
    m_targets.Save(out);
    m_dropped.Save(out);
}

OrderBase* AiTask10::Step() {
    if (m_flags.Get() != 1) {
        m_stage = 0;
        m_step = 0;
        m_finished = 1;
        return nullptr;
    }
    if (m_stage != kStageTargets) {
        if (m_stage != kStageFleets) {
            m_stage = kStageFleets;
            m_step = 0;
            return nullptr;
        }
        return StepFleets();
    }
    return StepTargets();
}

OrderBase* AiTask10::StepFleets() {
    OrderBase* order = nullptr;
    switch (m_step) {
    case 2:
        m_step = NextRecord() != 0 ? 4 : 8;
        return nullptr;
    case 3:
        m_step = ChooseManager() != 0 ? 2 : 8;
        return nullptr;
    case 4:
        m_step = ComputeSquadronCount();
        if (m_step != 0) return nullptr;
        m_stage = kStageTargets;
        m_finished = 1;
        return nullptr;
    case 5:
        order = BuildSquadronScrapOrder();
        break;
    default:
        m_step = 3;
        return nullptr;
    case 8:
        if (IsOverQuota() == 0) {
            m_step = 0;
            m_stage = kStageTargets;
            return nullptr;
        }
        m_step = 5;
        return nullptr;
    case 9: {
        OrderBase* destination = BuildFleetDestinationOrder(&m_unit);
        if (destination) {
            m_step = 10;
            m_finished = 1;
            ++m_priority;
            return destination;
        }
        break;
    }
    case 10:
        order = BuildManufactureOrder211();
        break;
    }
    m_step = 0;
    m_stage = kStageTargets;
    m_finished = 1;
    return order;
}

OrderBase* AiTask10::StepTargets() {
    OrderBase* order = nullptr;
    switch (m_step) {
    case 2:
        if (SelectTarget() != 0) {
            m_step = 3;
            return nullptr;
        }
        break;
    case 3:
        m_owner->m_bitsA &= ~0x10u;
        if (m_owner->m_bitB == 0x10 && PickSupplySystem() != 0) {
            m_owner->AdvanceBitCursor();
            m_step = 6;
            return nullptr;
        }
        m_step = 0xb;
        return nullptr;
    case 4:
        m_step = ComputeFacilityCount();
        if (m_step != 0) return nullptr;
        m_stage = kStageFleets;
        m_finished = 1;
        return nullptr;
    default:
        m_step = 2;
        return nullptr;
    case 6:
        order = BuildScrapOrder();
        if (order) {
            m_finished = 1;
            ++m_priority;
        }
        m_step = 4;
        return order;
    case 7:
        order = BuildCapitalOrder();
        break;
    case 9: {
        OrderBase* destination = BuildDestinationOrder(&m_system);
        if (destination) {
            m_step = 10;
            m_finished = 1;
            ++m_priority;
            return destination;
        }
        break;
    }
    case 10:
        order = BuildManufactureOrder210();
        break;
    case 0xb:
        AddTargetSystem();
        RefreshTargets();
        break;
    }
    m_step = 0;
    m_stage = kStageFleets;
    m_finished = 1;
    return order;
}

int AiTask10::ChooseManager() {
    int result = 0;
    bool found = false;
    m_owner->m_key20.Reset();
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
        if (0 < eval->Stat(0x58) && (eval->m_flags28.m_value & 0x200) != 0 && (eval->m_flags28.m_value & 3) == 0) {
            m_owner->m_key20 = m_manager;
            found = true;
        }
        node = NextNode(node);
    }
    const ObjectKey* fallbacks[] = {&m_owner->m_key1c, &m_owner->m_key24};
    for (const ObjectKey* fallback : fallbacks) {
        if (found) break;
        m_manager = *fallback;
        if (!IsSystemKey(m_manager)) continue;
        AiSystemEval* eval = m_owner->m_systems.FindEval(m_manager);
        if (eval && (eval->m_flags30.m_value & 1) != 0 && (eval->m_flags28.m_value & kSystemClaimed) != 0 &&
            0 < eval->Stat(0x58) && (eval->m_flags28.m_value & 0x200) != 0 && (eval->m_flags28.m_value & 3) == 0)
            found = true;
    }
    if (!found) {
        m_manager.Reset();
        return 0;
    }
    System* system = static_cast<System*>(GetObjectInFamilies_90_98(m_view, &m_manager));
    if (system) {
        GameObjNotifier* manager = system->FindManufacturingManagerBySlot123(0);
        if (manager) {
            ObjectKey key;
            GetObjectKey(manager, &key);
            m_manager = key;
            result = 1;
        }
    }
    return result;
}

int AiTask10::NextRecord() {
    int found = 0;
    ObjectKey scratch;
    m_count = 0;
    m_unit.Reset();
    AiSystemRecord* record = static_cast<AiSystemRecord*>(m_owner->m_systemRecords.FindById(m_recordCursor));
    if (!record) record = static_cast<AiSystemRecord*>(m_owner->m_systemRecords.First());
    if (record) {
        do {
            if (found) break;
            m_unit = *record->ChooseReinforcementSource(&scratch);
            if (IsSystemKey(m_unit)) {
                found = 1;
                m_count = record->m_x9c;
            } else {
                record = static_cast<AiSystemRecord*>(record->m_successor);
            }
        } while (record);
        if (record) {
            m_recordId = record->m_id;
            AiSystemRecord* next = static_cast<AiSystemRecord*>(record->m_successor);
            m_recordCursor = next ? next->m_id : 0;
            return found;
        }
    }
    m_recordId = 0;
    m_recordCursor = 0;
    m_selector.Add(m_owner->RankFleets(0x100005, 0xc02, 0x17, 2));
    m_unit = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    if (ai::KeyInFamilies(m_unit, 8, 0x10) && m_owner->m_fleets.FindEval(m_unit)) {
        m_count = 1;
        found = 1;
    }
    return found;
}

int AiTask10::ComputeSquadronCount() {
    int result = 0;
    const int need = Need();
    if (need < 1) {
        if (need < 0) result = 5;
        return result;
    }
    const int roll = SessionRandomNext(10);
    const int classes = m_owner->ChooseClass(&m_classKey, 0x28, 0x200000, roll > 5 ? 0x8000 : 0x400, 1);
    if ((m_classKey.m_value & 0xff000000) != 0 && classes != 0) {
        const int total = classes * m_count;
        if (total - need != 0 && need <= total) m_count = need / classes;
    }
    if (0 < m_count) {
        ManuMgr* manager = static_cast<ManuMgr*>(GetObjectInFamilies_a0_b0(m_view, &m_manager));
        if (manager) {
            const int hosted = manager->CountHostFacilities(0);
            if (hosted < m_count) m_count = hosted;
        }
        m_owner->m_key20.Reset();
        return 9;
    }
    return result;
}

int AiTask10::IsOverQuota() { return Need() < 0; }

OrderBase* AiTask10::BuildSquadronScrapOrder() {
    ObjectKeyValueSet team;
    OrderBase* order = nullptr;
    ObjectKey scratch;
    m_selector.Add(m_owner->RankFleets(5, 2, 9, 1));
    m_unit = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    m_selector.Add(m_owner->RankShipsOfFleet(&m_unit, 0x24000, 0x800, 6, 1));
    m_unit = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    if (ai::KeyInFamilies(m_unit, 0x14, 0x1c)) {
        GameObjNotifier* ship = GetObjectInFamilies_14_1c(m_view, &m_unit);
        if (ship) {
            int cheapest = kUnreachableCost;
            GameObjNotifier* cheapestSquadron = nullptr;
            FightSquadIterator it(ship, 1);
            for (it.Begin(); it.Current(); it.Advance()) {
                const int cost = static_cast<FightSquad*>(it.Current())->GetGridTotal();
                if (cost < cheapest) {
                    cheapest = static_cast<FightSquad*>(it.Current())->GetGridTotal();
                    cheapestSquadron = it.Current();
                }
            }
            if (cheapestSquadron) {
                ObjectKey key;
                GetObjectKey(cheapestSquadron, &key);
                ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(key, 0);
                if (node) team.Add(node);
            }
        }
    }
    if (team.First()) order = NewTeamOrder(kScrapOrder, m_view, &team);
    return order;
}

int AiTask10::SelectTarget() { return aitask::SelectTarget(Fields(), kParams); }

int AiTask10::PickSupplySystem() { return aitask::PickSupplySystemNear(Fields(), kParams); }

OrderBase* AiTask10::BuildScrapOrder() { return aitask::BuildScrapOrder(Fields(), kParams); }

int AiTask10::ComputeFacilityCount() { return aitask::ComputeCount(Fields(), kParams); }

OrderBase* AiTask10::BuildCapitalOrder() {
    return aitask::BuildCapitalOrder(Fields(), kParams, FacilityKind::kManufacturing28);
}

OrderBase* AiTask10::BuildFleetDestinationOrder(ObjectKey* target) {
    return aitask::BuildDestinationOrder(Fields(), target, 0xa2, 1, 0xff);
}

OrderBase* AiTask10::BuildManufactureOrder211() { return aitask::BuildManufactureOrder(Fields(), 0x211, 0xa2); }

OrderBase* AiTask10::BuildDestinationOrder(ObjectKey* target) { return aitask::BuildDestinationOrder(Fields(), target, 0xa0); }

OrderBase* AiTask10::BuildManufactureOrder210() { return aitask::BuildManufactureOrder(Fields(), 0x210, 0xa0); }

void AiTask10::AddTargetSystem() { aitask::AddTargetSystem(Fields(), kParams); }

int AiTask10::Need() { return aitask::Need(Fields(), kParams); }

int AiTask10::NeedForTargets() { return aitask::NeedForTargets(Fields(), kParams); }

void AiTask10::RefreshTargets() { aitask::RefreshTargets(Fields(), kParams); }
