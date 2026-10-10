#include "ai/ai_records.h"
#include "ai/ai_side_controller.h"
#include "ai/ai_task_family.h"
#include "ai/ai_task_types.h"
#include "ai/ai_task_util.h"
#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "economy/defense_facility.h"
#include "economy/manufacturing_facility.h"
#include "economy/production_facility_types.h"
#include "galaxy/system.h"
#include "gameobj/object_lookup.h"

using namespace aitask;

namespace {

constexpr FamilyParams kParams = {
    0x5a, 0x1dc, 10, 0xd8, 0xd8, 0x60, 0x80, 0x15, 0x3800003, 6, 0x2000000, 0xc4, 0x10000, 1, 0xb, 8,
    FacilityKind::kManufacturing28, 0x200000, FacilityKind::kManufacturing29, 0x400000, false, false, true, true, true};

constexpr int kScrapOrder = 0x200;
constexpr int kStageTargets = 0x3ec;
constexpr int kStageA = 0x3ed;
constexpr int kStageB = 0x3ee;
constexpr uint32_t kSystemClaimed = 0x2000000;
constexpr uint32_t kHomeSystemKey = 0x90000109;
constexpr uint32_t kShieldClassKey = 0x2d000002;
constexpr uint32_t kIonClassKey = 0x2c000001;

}  // namespace

AiTask3::AiTask3(int view) : AiFacilityTaskBase(3, view) {
    m_stage = 0;
    m_step = 0;
    m_count = 0;
    m_cap = 0;
    m_limit = 0;
    m_x24 = 9;
}

AiTask3::~AiTask3() { REB_KEEP_VPTR_STORE(); }

OrderBase* AiTask3::Step() {
    if (m_flags.Get() != 1) {
        m_stage = 0;
        m_step = 0;
        m_finished = 1;
        return nullptr;
    }
    if (m_stage == kStageTargets) return StepTargets();
    if (m_stage == kStageA) return StepA();
    if (m_stage != kStageB) {
        m_stage = kStageA;
        m_step = 0;
        return nullptr;
    }
    return StepB();
}

OrderBase* AiTask3::StepA() {
    switch (m_step) {
    case 1:
        break;
    case 2: {
        OrderBase* order = BuildDefenseScrapOrder();
        m_step = 0;
        m_stage = kStageB;
        m_finished = 1;
        return order;
    }
    case 3:
        if (IsOverQuota() == 0) {
            m_step = 0;
            m_stage = kStageB;
        } else {
            m_step = 7;
        }
        [[fallthrough]];
    case 7: {
        OrderBase* order = BuildDefenseOrder();
        m_step = 0;
        m_stage = kStageTargets;
        m_finished = 1;
        return order;
    }
    default:
        m_step = 1;
        return nullptr;
    }
    if (PickDefenseTarget() != 0) {
        m_step = 2;
        return nullptr;
    }
    m_step = 0;
    m_stage = kStageB;
    return nullptr;
}

OrderBase* AiTask3::StepB() {
    OrderBase* order = nullptr;
    switch (m_step) {
    case 1:
        m_step = PickFleetSystem() != 0 ? 6 : 9;
        return nullptr;
    default:
        m_step = 4;
        return nullptr;
    case 4:
        if (ChooseManager() != 0) {
            m_step = 1;
            return nullptr;
        }
        m_step = 0;
        m_stage = kStageTargets;
        return nullptr;
    case 5:
        m_step = ComputeClassCount();
        return nullptr;
    case 6:
        order = BuildSurplusScrapOrder();
        if (order) {
            m_finished = 1;
            ++m_priority;
        }
        m_step = 5;
        return order;
    case 7:
        order = BuildDefenseOrder();
        if (order == nullptr) {
            m_step = 9;
            return nullptr;
        }
        break;
    case 9: {
        int picked;
        if (m_owner->Stat(0x1d4) < m_owner->Stat(0x1d0)) {
            m_classKey.m_value = kShieldClassKey;
            picked = PickIonTarget();
        } else {
            m_classKey.m_value = kIonClassKey;
            picked = PickShieldTarget();
        }
        if (picked == 0) {
            m_step = 0;
            m_stage = kStageTargets;
        } else {
            m_count = 1;
            m_step = 10;
        }
    }
        [[fallthrough]];
    case 10:
        order = BuildAllScrapOrder();
        if (order) {
            m_finished = 1;
            ++m_priority;
        }
        m_step = 0xb;
        return order;
    case 0xb: {
        OrderBase* destination = BuildDestinationOrder(&m_unit);
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
    }
    m_step = 0;
    m_stage = kStageTargets;
    m_finished = 1;
    return order;
}

OrderBase* AiTask3::StepTargets() {
    OrderBase* order = nullptr;
    switch (m_step) {
    case 1:
        if (SelectTarget() != 0) {
            m_step = 4;
            return nullptr;
        }
        break;
    default:
        m_step = 1;
        return nullptr;
    case 4:
        m_owner->m_bitsA &= ~1u;
        if (m_owner->m_bitB == 1 && PickSupplySystem() != 0) {
            m_owner->AdvanceBitCursor();
            m_step = 6;
            return nullptr;
        }
        m_step = 0xd;
        return nullptr;
    case 5:
        m_step = ComputeFacilityCount();
        if (m_step != 0) return nullptr;
        m_stage = kStageA;
        m_finished = 1;
        return nullptr;
    case 6:
        order = BuildScrapOrder();
        if (order) {
            m_finished = 1;
            ++m_priority;
        }
        m_step = 5;
        return order;
    case 8:
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
    m_stage = kStageA;
    m_finished = 1;
    return order;
}

int AiTask3::PickDefenseTarget() {
    ObjectKey scratch;
    m_selector.Add(m_owner->RankSectors(0x10000, 0, 0, 0, 0, 0, 0x1d, 1));
    m_unit = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    m_selector.Add(m_owner->RankSystemsOfSector(&m_unit, 0, 0, 1, 0, 0, 0, 0x25, 1));
    m_unit = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    if (IsSystemKey(m_unit)) {
        AiSystemEval* eval = m_owner->m_systems.FindEval(m_unit);
        if (eval && 0 < eval->Stat(0xdc)) return 1;
    }
    return 0;
}

OrderBase* AiTask3::BuildDefenseScrapOrder() {
    ObjectKeyValueSet team;
    OrderBase* order = nullptr;
    int fromOthers = 0;
    int fromFirst = 0;
    AiSystemEval* eval = m_owner->m_systems.FindEval(m_unit);
    if (eval) {
        fromFirst = eval->Stat(0x64) - eval->Stat(0x6c);
        fromOthers = eval->Stat(0x74) - eval->Stat(0x7c);
    }
    GameObjNotifier* system = GetObjectInFamilies_90_98(m_view, &m_unit);
    if (system) {
        if (0 < fromFirst) {
            DefenseFacility24Iterator it(system, 1);
            for (it.Begin(); it.Current() && 0 < fromFirst; --fromFirst) {
                AddCurrentToTeam(&team, &it);
                it.Advance();
            }
        }
        if (0 < fromOthers) {
            DefenseFacility22Iterator second(system, 1);
            second.Begin();
            while (second.Current() && 0 < fromOthers) {
                AddCurrentToTeam(&team, &second);
                --fromOthers;
                second.Advance();
            }
            if (0 < fromOthers) {
                DefenseFacility23Iterator third(system, 1);
                for (third.Begin(); third.Current() && 0 < fromOthers; --fromOthers) {
                    AddCurrentToTeam(&team, &third);
                    third.Advance();
                }
            }
        }
    }
    if (team.First()) order = NewTeamOrder(kScrapOrder, m_view, &team);
    return order;
}

int AiTask3::IsOverQuota() { return (((m_x24 * m_owner->Stat(0x184)) / 100) * 0x5a) / 100 - m_owner->Stat(0x1dc) < 0; }

int AiTask3::PickFleetSystem() {
    int result = 0;
    ObjectKey scratch;
    m_unit = KeyOfValue(kHomeSystemKey);
    AiSystemEval* home = m_owner->m_systems.FindEval(m_unit);
    if ((home->m_flags30.m_value & 1) == 0 || (home->m_flags28.m_value & 2) != 0) {
        m_unit.Reset();
    } else if ((home->m_flags28.m_value & 0x40000000) == 0) {
        if (home->Stat(0x64) < m_owner->m_budget[2]) result = 1;
    } else {
        result = 1;
    }
    if (result == 0) {
        m_selector.Add(m_owner->RankSectors(0x4000, 0, 0, 0, 0, 0, 0x1c, 1));
        m_selector.Add(m_owner->RankSectorsByDistance(&m_manager, 0x4000, 0, 0, 0, 0, 0, 2));
        m_unit = *m_selector.PickBest(&scratch);
        m_selector.Reset();
        m_selector.Add(m_owner->RankSystemsOfSector(&m_unit, 0x4000, 0, 1, 0x4000003, 0, 0, 0x25, 2));
        m_unit = *m_selector.PickBest(&scratch);
        m_selector.Reset();
        if (!IsSystemKey(m_unit)) {
            m_unit = KeyOfValue(kHomeSystemKey);
            AiSystemEval* eval = m_owner->m_systems.FindEval(m_unit);
            if ((eval->m_flags30.m_value & 1) == 0 || -1 < eval->Stat(0xdc) || (eval->m_flags28.m_value & 2) != 0)
                m_unit.Reset();
            else
                result = 1;
        }
        if (!IsSystemKey(m_unit)) {
            m_selector.Add(m_owner->RankSectorsByDistance(&m_manager, 0x8000, 0, 0, 0, 0, 0, 2));
            m_unit = *m_selector.PickBest(&scratch);
            m_selector.Reset();
            m_selector.Add(m_owner->RankSystemsOfSector(&m_unit, 0x4000, 0, 1, 3, 0, 0, 0x25, 2));
            m_unit = *m_selector.PickBest(&scratch);
            m_selector.Reset();
        }
        if (IsSystemKey(m_unit)) {
            AiSystemEval* eval = m_owner->m_systems.FindEval(m_unit);
            if (eval && eval->Stat(0xdc) < 0) result = 1;
        } else {
            const int need = Need();
            if (0 < need && m_owner->m_budget[4] < 2) m_owner->RaiseBudget(4);
        }
    }
    return result;
}

int AiTask3::ChooseManager() {
    int result = 0;
    bool found = false;
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
        if (!eval) {
            ObjectKeyNode* next = NextNode(node);
            ObjectKey key = KeyOfNode(node);
            m_targets.RemoveKey(&key);
            node = next;
            continue;
        }
        if ((eval->m_flags30.m_value & 1) != 0 && (eval->m_flags28.m_value & kSystemClaimed) != 0) {
            if (0 < eval->Stat(0x60) && (eval->m_flags28.m_value & 0x800) != 0 && (eval->m_flags28.m_value & 3) == 0) found = true;
            node = NextNode(node);
            continue;
        }
        eval->m_flags28.m_value &= ~kSystemClaimed;
        eval->MarkChanged();
        ObjectKeyNode* next = NextNode(node);
        ObjectKey key = KeyOfNode(node);
        m_targets.RemoveKey(&key);
        node = next;
    }
    if (!found) {
        m_manager.Reset();
        return result;
    }
    System* system = static_cast<System*>(GetObjectInFamilies_90_98(m_view, &m_manager));
    if (system) {
        GameObjNotifier* manager = system->FindManufacturingManagerBySlot123(1);
        if (manager) {
            ObjectKey key;
            GetObjectKey(manager, &key);
            m_manager = key;
            result = 1;
        }
    }
    return result;
}

OrderBase* AiTask3::BuildSurplusScrapOrder() {
    ObjectKeyValueSet team;
    OrderBase* order = nullptr;
    ObjectKeyValueNode* last = nullptr;
    if (!IsSystemKey(m_unit)) return order;
    AiSystemEval* eval = m_owner->m_systems.FindEval(m_unit);
    GameObjNotifier* system = GetObjectInFamilies_90_98(m_view, &m_unit);
    if (system) {
        if ((eval->m_flags28.m_value & 0x200000) == 0) last = AddAllOfKind<ManufacturingFacility28Iterator>(&team, system, last);
        if ((eval->m_flags28.m_value & 0x400000) == 0) last = AddAllOfKind<ManufacturingFacility29Iterator>(&team, system, last);
        if ((eval->m_flags28.m_value & 0x3800000) == 0) last = AddAllOfKind<ManufacturingFacility2AIterator>(&team, system, last);
        if (team.First() == nullptr && eval->Stat(0x84) == 0) {
            AiSideController* owner = m_owner;
            last = nullptr;
            if (owner->Stat(0x1d0) < owner->Stat(0x1d4) || eval->Stat(0x50) == 0)
                last = AddFirstNode<ProductionFacilityType2DIterator>(&team, system);
            if (last == nullptr) {
                if (owner->Stat(0x1d4) <= owner->Stat(0x1d0) || eval->Stat(0x54) == 0)
                    last = AddFirstNode<ProductionFacilityType2CIterator>(&team, system);
                if (last == nullptr) last = AddFirstNode<ManufacturingFacility28Iterator>(&team, system);
                if (last == nullptr) last = AddFirstNode<ManufacturingFacility29Iterator>(&team, system);
                if (last == nullptr) last = AddFirstNode<ManufacturingFacility2AIterator>(&team, system);
            }
        }
    }
    if (team.First()) order = NewTeamOrder(kScrapOrder, m_view, &team);
    return order;
}

int AiTask3::ComputeClassCount() {
    int result = 9;
    m_count = 0;
    const int need = Need();
    if (need < 1) {
        if (need < 0) result = 7;
        return result;
    }
    AiSystemEval* eval = m_owner->m_systems.FindEval(m_unit);
    int classes = static_cast<int>(reinterpret_cast<intptr_t>(this));
    if (eval && 0 < eval->Stat(0x84)) {
        uint32_t required;
        uint32_t mode;
        if ((eval->m_flags28.m_value & 0x40000000) == 0) {
            if ((eval->m_flags28.m_value & 0x8000) == 0) {
                required = 0x80000;
                mode = 0x40000;
            } else {
                required = 0x20000;
                mode = 0x10000;
            }
        } else {
            required = 0x40000;
            mode = 0x10000;
        }
        classes = m_owner->ChooseClass(&m_classKey, 0x2a, required, mode, 1);
    }
    if ((m_classKey.m_value & 0xff000000) != 0 && classes != 0 && classes <= need) m_count = 1;
    if (0 < m_count) return 0xb;
    return result;
}

OrderBase* AiTask3::BuildDefenseOrder() {
    ObjectKeyValueSet team;
    OrderBase* order = nullptr;
    ObjectKey scratch;
    m_selector.Add(m_owner->RankSectors(0x10000, 0, 0, 0, 0, 0, 0x1d, 1));
    m_unit = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    m_selector.Add(m_owner->RankSystemsOfSector(&m_unit, 0, 0, 1, 0, 0, 0, 0x25, 1));
    m_unit = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    if (!IsSystemKey(m_unit)) {
        m_selector.Add(m_owner->RankSectors(0x200e0, 0, 0, 0, 0, 0, 3, 1));
        m_unit = *m_selector.PickBest(&scratch);
        m_selector.Reset();
        m_selector.Add(m_owner->RankSystemsOfSector(&m_unit, 0, 0, 1, 0x3e00000, 0, 0x40000000, 7, 1));
        m_selector.Add(m_owner->RankSystemsOfSector(&m_unit, 0, 0, 1, 0x3e00000, 0, 0x40000000, 0xb, 1));
        m_unit = *m_selector.PickBest(&scratch);
        m_selector.Reset();
    }
    if (!IsSystemKey(m_unit)) {
        m_selector.Add(m_owner->RankSectors(0x20000, 0, 0, 0, 0, 0, 3, 1));
        m_unit = *m_selector.PickBest(&scratch);
        m_selector.Reset();
        m_selector.Add(m_owner->RankSystemsOfSector(&m_unit, 0, 0, 1, 0, 0, 0x40000000, 7, 1));
        m_selector.Add(m_owner->RankSystemsOfSector(&m_unit, 0, 0, 1, 0, 0, 0x40000000, 0xb, 1));
        m_unit = *m_selector.PickBest(&scratch);
        m_selector.Reset();
    }
    GameObjNotifier* system = IsSystemKey(m_unit) ? GetObjectInFamilies_90_98(m_view, &m_unit) : nullptr;
    if (system) {
        ObjectKeyValueNode* node = AddFirstNode<DefenseFacility22Iterator>(&team, system);
        if (!node) node = AddFirstNode<DefenseFacility23Iterator>(&team, system);
        if (!node) node = AddFirstNode<DefenseFacility24Iterator>(&team, system);
    }
    if (team.First()) order = NewTeamOrder(kScrapOrder, m_view, &team);
    return order;
}

int AiTask3::SelectTarget() { return aitask::SelectTarget(Fields(), kParams); }

int AiTask3::PickSupplySystem() { return aitask::PickSupplySystemNear(Fields(), kParams); }

OrderBase* AiTask3::BuildScrapOrder() { return aitask::BuildScrapOrder(Fields(), kParams); }

int AiTask3::ComputeFacilityCount() { return aitask::ComputeCount(Fields(), kParams); }

OrderBase* AiTask3::BuildCapitalOrder() {
    return aitask::BuildCapitalOrder(Fields(), kParams, FacilityKind::kManufacturing2A);
}

int AiTask3::PickShieldTarget() {
    ObjectKey scratch;
    m_selector.Add(m_owner->RankSectorsByDistance(&m_manager, 0x100, 0, 0, 0, 0, 0, 2));
    m_unit = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    m_selector.Add(m_owner->RankSystemsOfSectorByDistance(&m_unit, &m_manager, 0x80, 0, 0, 0x3e00003, 0, 0, 2));
    m_unit = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    return IsSystemKey(m_unit) ? 1 : 0;
}

int AiTask3::PickIonTarget() {
    ObjectKey scratch;
    m_selector.Add(m_owner->RankSectorsByDistance(&m_manager, 0x200, 0, 0, 0, 0, 0, 2));
    m_unit = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    m_selector.Add(m_owner->RankSystemsOfSectorByDistance(&m_unit, &m_manager, 0x100, 0, 1, 0x3e00003, 0, 0, 2));
    m_unit = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    return IsSystemKey(m_unit) ? 1 : 0;
}

OrderBase* AiTask3::BuildAllScrapOrder() {
    ObjectKeyValueSet team;
    OrderBase* order = nullptr;
    ObjectKeyValueNode* last = nullptr;
    GameObjNotifier* system = IsSystemKey(m_unit) ? GetObjectInFamilies_90_98(m_view, &m_unit) : nullptr;
    if (system) {
        last = AddAllOfKind<ManufacturingFacility28Iterator>(&team, system, last);
        last = AddAllOfKind<ManufacturingFacility29Iterator>(&team, system, last);
        last = AddAllOfKind<ManufacturingFacility2AIterator>(&team, system, last);
    }
    if (team.First()) order = NewTeamOrder(kScrapOrder, m_view, &team);
    return order;
}

OrderBase* AiTask3::BuildDestinationOrder(ObjectKey* target) { return aitask::BuildDestinationOrder(Fields(), target, 0xa0); }

OrderBase* AiTask3::BuildManufactureOrder210() { return aitask::BuildManufactureOrder(Fields(), 0x210, 0xa0); }

void AiTask3::AddTargetSystem() { aitask::AddTargetSystem(Fields(), kParams); }

int AiTask3::Need() { return aitask::Need(Fields(), kParams); }

int AiTask3::NeedForTargets() { return aitask::NeedForTargets(Fields(), kParams); }

void AiTask3::RefreshTargets() { aitask::RefreshTargets(Fields(), kParams); }
