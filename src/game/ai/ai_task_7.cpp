#include "ai/ai_records.h"
#include "ai/ai_side_controller.h"
#include "ai/ai_system_record.h"
#include "ai/ai_task_family.h"
#include "ai/ai_task_types.h"
#include "ai/ai_task_util.h"
#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "fleet/fight_squad.h"
#include "galaxy/system.h"
#include "gameobj/object_lookup.h"

using namespace aitask;

namespace {

constexpr FamilyParams kParams = {
    0x50, 0x1e0, 0x14, 0xd0, 0xd0, 0x58, 0x20, 0x11, 0x200003, 4, 0x200000, 0xb4, 0x4000, 8, 0xb, 9,
    FacilityKind::kManufacturing29, 0x400000, FacilityKind::kManufacturing2A, 0x3800000, false, true, true};

constexpr int kScrapOrder = 0x200;
constexpr int kMoveOrder = 0x201;
constexpr int kStageTargets = 0x3ec;
constexpr int kStageSquadrons = 0x3f1;
constexpr int kStageShips = 0x3f2;
constexpr uint32_t kSystemClaimed = 0x200000;
constexpr int kUnreachableCost = 100000;
constexpr int kBudgetKinds[] = {9, 10, 11};

bool IsFleetKey(const ObjectKey& key) { return ai::KeyInFamilies(key, 8, 0x10) != 0; }
bool IsSquadronKey(const ObjectKey& key) { return ai::KeyInFamilies(key, 0x1c, 0x20) != 0; }

// Every squadron of the system; the one with the least value.
GameObjNotifier* CheapestSquadron(GameObjNotifier* system, int* value) {
    GameObjNotifier* cheapest = nullptr;
    FightSquadIterator it(system, 1);
    for (it.Begin(); it.Current(); it.Advance()) {
        const int cost = static_cast<FightSquad*>(it.Current())->GetGridTotal();
        if (cost < *value) {
            *value = static_cast<FightSquad*>(it.Current())->GetGridTotal();
            cheapest = it.Current();
        }
    }
    return cheapest;
}

void AddToTeam(ObjectKeyValueSet* team, GameObjNotifier* object) {
    ObjectKey key;
    GetObjectKey(object, &key);
    ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(key, 0);
    if (node) team->Add(node);
}

}  // namespace

AiFacilityTaskBase::AiFacilityTaskBase(int id, int view)
    : AiStagedTask(id, view), m_selector(), m_manager(), m_unit(), m_system(), m_classKey(), m_targets(), m_dropped() {
    m_classKey.m_value = 0;
    m_count = 0;
    m_cap = 0;
    m_limit = 0;
    m_targets.Clear();
    m_dropped.Clear();
}

FamilyFields AiFacilityTaskBase::Fields() {
    return {m_owner, m_view, m_x24, &m_selector, &m_targets, &m_dropped, &m_system, &m_manager, &m_classKey, &m_count, &m_cap, &m_limit};
}

void AiFacilityTaskBase::Read(istream* in) {
    AiTaskBase::Read(in);
    ReadValue32(in, &m_stage);
    ReadValue32(in, &m_step);
    ReadValue32(in, &m_count);
    ReadValue32(in, &m_cap);
    ReadValue32(in, &m_limit);
    m_selector.Read(in);
    m_manager.Read(in);
    m_unit.Read(in);
    m_system.Read(in);
    m_targets.Load(in);
    m_dropped.Load(in);
}

void AiFacilityTaskBase::Write(ostream* out) {
    AiTaskBase::Write(out);
    WriteValue32(out, &m_stage);
    WriteValue32(out, &m_step);
    WriteValue32(out, &m_count);
    WriteValue32(out, &m_cap);
    WriteValue32(out, &m_limit);
    m_selector.Write(out);
    m_manager.Write(out);
    m_unit.Write(out);
    m_system.Write(out);
    m_targets.Save(out);
    m_dropped.Save(out);
}

AiTask7::AiTask7(int view) : AiFacilityTaskBase(7, view) {
    m_stage = 0;
    m_step = 0;
    m_count = 0;
    m_cap = 0;
    m_limit = 0;
    m_x24 = 0x11;
}

AiTask7::~AiTask7() { REB_KEEP_VPTR_STORE(); }

OrderBase* AiTask7::Step() {
    if (m_flags.Get() != 1) {
        m_stage = 0;
        m_step = 0;
        m_finished = 1;
        return nullptr;
    }
    if (m_stage == kStageTargets) return StepTargets();
    if (m_stage == kStageSquadrons) return StepSquadrons();
    if (m_stage != kStageShips) {
        m_stage = kStageSquadrons;
        m_step = 0;
        return nullptr;
    }
    return StepShips();
}

OrderBase* AiTask7::StepSquadrons() {
    OrderBase* order = nullptr;
    switch (m_step) {
    case 1:
        if (PickFleetSystem() == 0) {
            m_step = 0;
            m_stage = kStageShips;
            return nullptr;
        }
        m_step = 2;
        return nullptr;
    case 2:
        m_step = PickFleetTarget() != 0 ? 3 : 4;
        return nullptr;
    case 3:
        order = BuildMoveOrder();
        if (order == nullptr) {
            m_step = 4;
            return nullptr;
        }
        break;
    case 4:
        order = BuildSquadronScrapOrder();
        break;
    default:
        m_step = 1;
        return nullptr;
    }
    m_step = 0;
    m_stage = kStageShips;
    m_finished = 1;
    return order;
}

OrderBase* AiTask7::StepShips() {
    OrderBase* order = nullptr;
    switch (m_step) {
    case 2:
        m_step = PickShipSystem() != 0 ? 6 : 10;
        return nullptr;
    default:
        m_step = 5;
        return nullptr;
    case 5:
        m_step = ChooseManager() != 0 ? 2 : 10;
        return nullptr;
    case 6:
        m_step = ComputeShipCount();
        if (m_step != 0) return nullptr;
        m_stage = kStageTargets;
        m_finished = 1;
        return nullptr;
    case 7:
        order = BuildShipOrder();
        break;
    case 10:
        if (IsShipQuotaExceeded() == 0) {
            m_step = 0;
            m_stage = kStageTargets;
            return nullptr;
        }
        m_step = 7;
        return nullptr;
    case 0xb: {
        OrderBase* destination = BuildFleetDestinationOrder(&m_unit);
        if (destination) {
            m_step = 0xc;
            m_finished = 1;
            ++m_priority;
            return destination;
        }
        break;
    }
    case 0xc:
        order = BuildManufactureOrder211();
        break;
    }
    m_step = 0;
    m_stage = kStageTargets;
    m_finished = 1;
    return order;
}

OrderBase* AiTask7::StepTargets() {
    OrderBase* order = nullptr;
    switch (m_step) {
    case 2:
        if (SelectTarget() != 0) {
            m_step = 5;
            return nullptr;
        }
        break;
    default:
        m_step = 2;
        return nullptr;
    case 5:
        m_owner->m_bitsA &= ~8u;
        if (m_owner->m_bitB == 8 && PickSupplySystem() != 0) {
            m_owner->AdvanceBitCursor();
            m_step = 8;
            return nullptr;
        }
        m_step = 0xd;
        return nullptr;
    case 6:
        m_step = ComputeFacilityCount();
        if (m_step != 0) return nullptr;
        m_stage = kStageSquadrons;
        m_finished = 1;
        return nullptr;
    case 8:
        order = BuildScrapOrder();
        if (order) {
            m_finished = 1;
            ++m_priority;
        }
        m_step = 6;
        return order;
    case 9:
        order = BuildCapitalOrder();
        break;
    case 0xb: {
        OrderBase* destination = BuildDestinationOrder(&m_system);
        if (destination) {
            m_step = 0xc;
            m_finished = 1;
            ++m_priority;
            return destination;
        }
        break;
    }
    case 0xc:
        order = BuildManufactureOrder210();
        break;
    case 0xd:
        AddTargetSystem();
        RefreshTargets();
        break;
    }
    m_step = 0;
    m_stage = kStageSquadrons;
    m_finished = 1;
    return order;
}

int AiTask7::PickFleetSystem() {
    ObjectKey scratch;
    m_selector.Add(m_owner->RankSectors(0x2000000, 0, 0, 0, 0, 0, 0x22, 1));
    m_manager = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    m_selector.Add(m_owner->RankSystemsOfSector(&m_manager, 0, 0, 1, 2, 0, 0x40000, 0x35, 1));
    m_manager = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    if (IsSystemKey(m_manager)) {
        AiSystemEval* eval = m_owner->m_systems.FindEval(m_manager);
        if (eval && 0 < eval->Stat(0x11c)) return 1;
    }
    return 0;
}

int AiTask7::PickFleetTarget() {
    ObjectKey scratch;
    m_unit.Reset();
    auto pickFleet = [&](uint32_t flags) {
        m_selector.Add(m_owner->RankFleetsBySectorDistance(&m_manager, flags, 2, 2));
        m_selector.Add(m_owner->RankFleets(flags, 2, 0x17, 1));
        m_unit = *m_selector.PickBest(&scratch);
        m_selector.Reset();
    };
    pickFleet(0x100009);
    if (IsFleetKey(m_unit)) return 1;
    pickFleet(0x100005);
    if (IsFleetKey(m_unit)) return 1;

    m_selector.Add(m_owner->RankSectorsByDistance(&m_manager, 0x10000000, 0, 0, 0, 0, 0, 2));
    m_unit = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    m_selector.Add(m_owner->RankSystemsOfSector(&m_unit, 0x140000, 0, 1, 2, 0, 0, 0x26, 2));
    m_unit = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    if (IsSystemKey(m_unit)) return 1;

    bool found = false;
    m_selector.Add(m_owner->RankSectorsByX3c(0, 0, 0x8000000, 0, 0, 0, 2));
    m_selector.Add(m_owner->RankSectorsByDistance(&m_manager, 0, 0, 0x8000000, 0, 0, 0, 2));
    ObjectKey sector = *m_selector.PickBest(&scratch);
    for (;;) {
        if (!IsSectorKey(sector) || found) {
            m_selector.Reset();
            return found;
        }
        AiSectorEval* eval = m_owner->m_sectors.FindEval(sector);
        if (eval) {
            AiSystemRecord* record = static_cast<AiSystemRecord*>(m_owner->m_systemRecords.FindById(eval->m_x38));
            if (record) {
                m_unit = *record->ChooseReinforcementSource(&scratch);
                if (IsSystemKey(m_unit)) found = true;
            }
        }
        if (found) continue;
        sector = *m_selector.PickBest(&scratch);
    }
}

OrderBase* AiTask7::BuildMoveOrder() {
    ObjectKeyValueSet team;
    OrderBase* order = nullptr;
    int budget = 0;
    int capacity = 0;
    bool stop = false;
    if (IsSystemKey(m_unit)) {
        AiSystemEval* eval = m_owner->m_systems.FindEval(m_unit);
        if (eval) budget = -eval->Stat(0xe0);
    } else if (IsFleetKey(m_unit)) {
        AiFleetEval* fleet = m_owner->m_fleets.FindEval(m_unit);
        if (fleet) budget = fleet->Stat(0xac);
    }
    AiSystemEval* eval = m_owner->m_systems.FindEval(m_manager);
    if (eval) capacity = eval->Stat(0x11c);
    if (capacity <= budget) budget = capacity;
    GameObjNotifier* system = GetObjectInFamilies_90_98(m_view, &m_manager);
    if (system) {
        ObjectKey best;
        while (0 < budget && !stop) {
            int cheapest = 1000000;
            GameObjNotifier* squadron = CheapestSquadron(system, &cheapest);
            if (squadron) GetObjectKey(squadron, &best);
            --budget;
            if (IsSquadronKey(best)) {
                ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(best, 0);
                if (node) team.Add(node);
            } else {
                stop = true;
            }
        }
        if (team.First()) {
            order = static_cast<OrderBase*>(CreateMessageByKind(kMoveOrder));
            if (order) {
                order->m_side = m_view;
                order->SetTeam(&team);
                order->SetTarget(&m_unit);
            }
        }
    }
    return order;
}

OrderBase* AiTask7::BuildSquadronScrapOrder() {
    ObjectKeyValueSet team;
    OrderBase* order = nullptr;
    bool stop = false;
    AiSystemEval* eval = m_owner->m_systems.FindEval(m_manager);
    GameObjNotifier* system = GetObjectInFamilies_90_98(m_view, &m_manager);
    if (eval && system) {
        int budget = eval->Stat(0xe0);
        ObjectKey best;
        while (0 < budget && !stop) {
            int cheapest = 1000000;
            GameObjNotifier* squadron = CheapestSquadron(system, &cheapest);
            if (squadron) GetObjectKey(squadron, &best);
            --budget;
            if (IsSquadronKey(best)) {
                ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(best, 0);
                if (node) team.Add(node);
            } else {
                stop = true;
            }
        }
        if (team.First()) order = NewTeamOrder(kScrapOrder, m_view, &team);
    }
    return order;
}

int AiTask7::ChooseManager() {
    int result = 0;
    bool found = false;
    m_cap = 0;
    m_owner->m_key1c.Reset();
    ObjectKeyNode* node = m_targets.First();
    while (node && !found) {
        m_manager = KeyOfNode(node);
        if (!IsSystemKey(m_manager)) {
            ObjectKeyNode* next = NextNode(node);
            ObjectKey key = KeyOfNode(node);
            m_targets.RemoveKey(&key);
            node = next;
            continue;
        }
        AiSystemEval* eval = m_owner->m_systems.FindEval(m_manager);
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
            m_cap = eval->Stat(0x58);
            m_owner->m_key1c = m_manager;
            found = true;
        }
        node = NextNode(node);
    }
    const ObjectKey* fallbacks[] = {&m_owner->m_key20, &m_owner->m_key24};
    for (const ObjectKey* fallback : fallbacks) {
        if (found) break;
        m_manager = *fallback;
        if (!IsSystemKey(m_manager)) continue;
        AiSystemEval* eval = m_owner->m_systems.FindEval(m_manager);
        if (eval && (eval->m_flags30.m_value & 1) != 0 && (eval->m_flags28.m_value & kSystemClaimed) != 0 &&
            0 < eval->Stat(0x58) && (eval->m_flags28.m_value & 0x200) != 0 && (eval->m_flags28.m_value & 3) == 0) {
            m_cap = eval->Stat(0x58);
            found = true;
        }
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

int AiTask7::PickShipSystem() {
    int result = 0;
    ObjectKey scratch;
    auto pick = [&](uint32_t sectorRequire, uint32_t systemRequire, uint32_t systemForbid) {
        m_selector.Add(m_owner->RankSectorsByDistance(&m_manager, sectorRequire, 0, 0, 0, 0, 0, 2));
        m_unit = *m_selector.PickBest(&scratch);
        m_selector.Reset();
        m_selector.Add(m_owner->RankSystemsOfSector(&m_unit, systemRequire, 0, 1, systemForbid, 0, 0, 0x26, 2));
        m_unit = *m_selector.PickBest(&scratch);
        m_selector.Reset();
    };
    pick(0x4000000, 0x40000, 0x4000003);
    if (!IsSystemKey(m_unit)) pick(0x40000000, 0x40000, 3);
    if (!IsSystemKey(m_unit)) pick(0x1000000, 0x40000, 3);
    if (IsSystemKey(m_unit)) {
        AiSystemEval* eval = m_owner->m_systems.FindEval(m_unit);
        if (eval && eval->Stat(0xe0) < 0) result = 1;
    } else if (0 < Need()) {
        for (const int kind : kBudgetKinds) m_owner->RaiseBudget(kind);
    }
    return result;
}

int AiTask7::ComputeShipCount() {
    int result = 0;
    m_count = 0;
    const int need = Need();
    if (need < 1) {
        if (need < 0) result = 7;
        return result;
    }
    AiSystemEval* eval = m_owner->m_systems.FindEval(m_unit);
    // BUG(original): without a system record the two arguments are the task's own address.
    int mode = static_cast<int>(reinterpret_cast<intptr_t>(this));
    int order = mode;
    if (eval) {
        m_count = -eval->Stat(0xe0);
        if ((eval->m_flags28.m_value & 0x83e00000) == 0) {
            mode = 1;
            order = 2;
        } else {
            mode = 0x400;
            order = 1;
        }
    }
    const int classes = m_owner->ChooseClass(&m_classKey, 0x28, 0x200000, mode, order);
    if ((m_classKey.m_value & 0xff000000) != 0 && classes != 0) {
        const int total = classes * m_count;
        if (total - need != 0 && need <= total) m_count = need / classes;
    }
    if (0 < m_count) {
        if (m_cap < m_count) m_count = m_cap;
        m_owner->m_key1c.Reset();
        return 0xb;
    }
    return result;
}

int AiTask7::IsShipQuotaExceeded() {
    return (((m_x24 * m_owner->Stat(0x184)) / 100) * 0x5a) / 100 - m_owner->Stat(0x1e0) < 0;
}

OrderBase* AiTask7::BuildShipOrder() {
    ObjectKeyValueSet team;
    OrderBase* order = nullptr;
    ObjectKey scratch;
    m_unit.Reset();
    m_manager.Reset();
    m_selector.Add(m_owner->RankSectors(0x2000000, 0, 0, 0, 0, 0, 0x22, 1));
    m_manager = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    m_selector.Add(m_owner->RankSystemsOfSector(&m_manager, 0, 0, 1, 0, 0, 0x40000, 0x35, 1));
    m_manager = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    if (!IsSystemKey(m_manager)) {
        m_selector.Add(m_owner->RankSectors(0x8000000, 0, 0, 0x40000000, 0, 0, 0x19, 2));
        m_manager = *m_selector.PickBest(&scratch);
        m_selector.Reset();
        m_selector.Add(m_owner->RankSystemsOfSector(&m_manager, 0x80000, 0, 0, 0x80000000, 0, 0x40000, 0x17, 2));
        m_manager = *m_selector.PickBest(&scratch);
        m_selector.Reset();
    }
    if (!IsSystemKey(m_manager)) {
        m_selector.Add(m_owner->RankSectors(0x8000000, 0, 0, 0, 0, 0, 0x19, 2));
        m_manager = *m_selector.PickBest(&scratch);
        m_selector.Reset();
        m_selector.Add(m_owner->RankSystemsOfSector(&m_manager, 0x80000, 0, 0, 0, 0, 0x40000, 0x17, 2));
        m_manager = *m_selector.PickBest(&scratch);
        m_selector.Reset();
    }
    GameObjNotifier* system = IsSystemKey(m_manager) ? GetObjectInFamilies_90_98(m_view, &m_manager) : nullptr;
    if (system) {
        int cheapest = kUnreachableCost;
        GameObjNotifier* squadron = CheapestSquadron(system, &cheapest);
        if (squadron) AddToTeam(&team, squadron);
    }
    if (!team.First()) return order;

    auto pickFleet = [&](uint32_t flags) {
        m_selector.Add(m_owner->RankFleetsBySectorDistance(&m_manager, flags, 2, 2));
        m_selector.Add(m_owner->RankFleets(flags, 2, 0x17, 1));
        m_unit = *m_selector.PickBest(&scratch);
        m_selector.Reset();
    };
    bool moveToTarget = false;
    pickFleet(0x100009);
    if (IsFleetKey(m_unit)) {
        moveToTarget = true;
    } else {
        pickFleet(0x100005);
        if (IsFleetKey(m_unit)) {
            moveToTarget = true;
        } else {
            m_selector.Add(m_owner->RankSectorsByX3c(0, 0, 0x8000000, 0, 0, 0, 2));
            m_selector.Add(m_owner->RankSectorsByDistance(&m_manager, 0, 0, 0x8000000, 0, 0, 0, 2));
            ObjectKey sector = *m_selector.PickBest(&scratch);
            bool found = false;
            for (;;) {
                if (!IsSectorKey(sector) || found) break;
                AiSectorEval* eval = m_owner->m_sectors.FindEval(sector);
                if (eval) {
                    AiSystemRecord* record = static_cast<AiSystemRecord*>(m_owner->m_systemRecords.FindById(eval->m_x38));
                    if (record) {
                        m_unit = *record->ChooseReinforcementSource(&scratch);
                        if (IsSystemKey(m_unit)) found = true;
                    }
                }
                if (found) continue;
                sector = *m_selector.PickBest(&scratch);
            }
            m_selector.Reset();
            if (found) {
                moveToTarget = true;
            } else {
                return NewTeamOrder(kScrapOrder, m_view, &team);
            }
        }
    }
    if (moveToTarget) {
        order = static_cast<OrderBase*>(CreateMessageByKind(kMoveOrder));
        if (order) {
            order->m_side = m_view;
            order->SetTeam(&team);
            order->SetTarget(&m_unit);
        }
    }
    return order;
}

int AiTask7::SelectTarget() { return aitask::SelectTarget(Fields(), kParams); }

int AiTask7::PickSupplySystem() { return aitask::PickSupplySystemNear(Fields(), kParams); }

OrderBase* AiTask7::BuildScrapOrder() { return aitask::BuildScrapOrder(Fields(), kParams); }

int AiTask7::ComputeFacilityCount() { return aitask::ComputeCount(Fields(), kParams); }

OrderBase* AiTask7::BuildCapitalOrder() {
    return aitask::BuildCapitalOrder(Fields(), kParams, FacilityKind::kManufacturing28);
}

OrderBase* AiTask7::BuildFleetDestinationOrder(ObjectKey* target) {
    return aitask::BuildDestinationOrder(Fields(), target, 0xa2);
}

OrderBase* AiTask7::BuildManufactureOrder211() { return aitask::BuildManufactureOrder(Fields(), 0x211, 0xa2); }

OrderBase* AiTask7::BuildDestinationOrder(ObjectKey* target) { return aitask::BuildDestinationOrder(Fields(), target, 0xa0); }

OrderBase* AiTask7::BuildManufactureOrder210() { return aitask::BuildManufactureOrder(Fields(), 0x210, 0xa0); }

void AiTask7::AddTargetSystem() { aitask::AddTargetSystem(Fields(), kParams); }

int AiTask7::Need() { return aitask::Need(Fields(), kParams); }

int AiTask7::NeedForTargets() { return aitask::NeedForTargets(Fields(), kParams); }

void AiTask7::RefreshTargets() { aitask::RefreshTargets(Fields(), kParams); }
