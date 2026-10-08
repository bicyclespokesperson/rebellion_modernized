#include "ai/ai_common.h"
#include "ai/ai_evaluators.h"
#include "ai/ai_side_controller.h"
#include "ai/ai_task_types.h"
#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "economy/manufacturing_facility.h"
#include "events/game_clock_hooks.h"
#include "economy/defense_facility.h"
#include "economy/production_facility_types.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "orders/order_base.h"

using ai::KeyInFamilies;

namespace {

constexpr int kScrapOrder = 0x200;
constexpr int kMoveOrder = 0x201;
constexpr uint32_t kMovedUnitKey = 0x20000005;
constexpr int kInitialNextTime = 200;
constexpr int kRetryDelayDays = 300;
constexpr int kStateChoosePlan = 0x3fb;
constexpr int kStateAct = 0x3fc;

bool IsSectorKey(const ObjectKey& key) { return KeyInFamilies(key, 0x80, 0x90); }
bool IsSystemKey(const ObjectKey& key) { return KeyInFamilies(key, 0x90, 0x98); }

ObjectKey KeyOfUnit(uint32_t value) {
    ObjectKey key;
    key.m_value = value;
    return key;
}

// Adds the first object of the iterator to the team; true if a node was made.
bool AddFirstToTeam(ObjectKeyValueSet* team, ChildIterator* it) {
    it->Begin();
    if (!it->Current()) return false;
    ObjectKey key;
    GetObjectKey(it->Current(), &key);
    ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(key, 0);
    if (node) team->Add(node);
    return node != nullptr;
}

template <class Iterator>
bool AddFirstOfKind(ObjectKeyValueSet* team, GameObjNotifier* system) {
    Iterator it(system, 1);
    return AddFirstToTeam(team, &it);
}

}  // namespace

void AiStagedTask::Evaluate() {
    if (m_flags.Get() != 1 && m_flags.Get() != 2) {
        m_flags.m_value = 1;
        m_finished = 0;
    }
}

void AiStagedTask::Refused(int*, OrderBase* order) {
    m_finished = 1;
    m_stage = 0;
    m_step = 0;
    m_priority = 0;
    if (order) delete order;
}

AiTask13::AiTask13(int view) : AiStagedTask(13, view), m_selector(), m_target(), m_objects() {
    m_stage = 0;
    m_step = 0;
    m_x58 = 0;
    m_plan = 0;
    m_evaluator = nullptr;
    m_nextTime = kInitialNextTime;
}

AiTask13::~AiTask13() {
    if (m_evaluator) {
        delete m_evaluator;
        m_evaluator = nullptr;
    }
    REB_KEEP_VPTR_STORE();
}

void AiTask13::Read(istream* in) {
    AiTaskBase::Read(in);
    ReadValue32(in, &m_stage);
    ReadValue32(in, &m_step);
    m_target.Read(in);
    ReadValue32(in, &m_nextTime);
    ReadValue32(in, &m_x58);
    ReadValue32(in, &m_plan);
    m_objects.Load(in);
    if (m_plan != 0) {
        CreateEvaluator();
        if (m_evaluator) {
            m_evaluator->Read(in);
            m_evaluator->m_objects = &m_objects;
        }
    }
}

void AiTask13::Write(ostream* out) {
    AiTaskBase::Write(out);
    WriteValue32(out, &m_stage);
    WriteValue32(out, &m_step);
    m_target.Write(out);
    WriteValue32(out, &m_nextTime);
    WriteValue32(out, &m_x58);
    WriteValue32(out, &m_plan);
    m_objects.Save(out);
    if (m_evaluator) m_evaluator->Write(out);
}

void AiTask13::ForwardVote(const ObjectKey* key, GameTime* chance) {
    if (m_evaluator) m_evaluator->ScoreVote(key, reinterpret_cast<float*>(chance));
}

OrderBase* AiTask13::Step() {
    if (m_flags.Get() != 1) {
        m_stage = 0;
        m_step = 0;
        m_finished = 1;
        return nullptr;
    }
    if (m_stage == kStateChoosePlan) {
        StepPlan();
        return nullptr;
    }
    if (m_stage != kStateAct) {
        m_stage = kStateChoosePlan;
        return nullptr;
    }
    if (m_view == 1) return StepAlliance();
    StepEmpire();
    return nullptr;
}

void AiTask13::StepPlan() {
    if (m_step == 1) {
        ChoosePlan();
        m_step = 2;
        return;
    }
    if (m_step != 2) {
        m_step = 1;
        return;
    }
    int done = 1;
    if (m_evaluator) done = m_evaluator->Run();
    if (done) {
        m_step = 0;
        m_stage = kStateAct;
    }
}

OrderBase* AiTask13::StepAlliance() {
    if (m_step == 3) {
        if (ChooseTargets()) {
            m_step = 4;
            return nullptr;
        }
        m_step = 0;
        m_stage = 0;
        m_finished = 1;
        return nullptr;
    }
    if (m_step != 4) {
        if (m_step != 5) {
            m_step = 3;
            return nullptr;
        }
        OrderBase* order = BuildMoveOrder();
        m_step = 0;
        m_stage = 0;
        m_finished = 1;
        return order;
    }
    OrderBase* order = BuildScrapOrder();
    if (order) {
        m_finished = 1;
        ++m_priority;
    }
    m_step = 5;
    return order;
}

void AiTask13::StepEmpire() {
    m_finished = 1;
    m_step = 0;
    m_stage = 0;
}

void AiTask13::ChoosePlan() {
    AiSideController* owner = m_owner;
    float ratio = static_cast<float>(owner->Stat(0x258));
    const int surplus = (owner->Stat(0x25c) - owner->Stat(0x1fc)) + owner->Stat(0x200);
    if (0 < owner->Stat(0x288)) ratio = ratio / static_cast<float>(owner->Stat(0x288));
    const int limit = owner->Stat(0x210);
    const int a = owner->Stat(0x1ec);
    const int b = owner->Stat(0x1e8);
    bool keep = false;
    switch (m_plan) {
    case 2:
        if ((b < owner->Stat(0x1ac) + 1 || a < owner->Stat(0x1ac) / 2 + 1) && (ratio < 1.5 || surplus <= limit) &&
            owner->m_bitsA == 0)
            keep = owner->Stat(0x1c8) != 0;
        break;
    case 3:
        if (ratio < 1.5 && owner->m_bitsA == 0 && 0 < surplus) {
            const int base = owner->Stat(0x1ac);
            keep = base / 2 + 1 <= b && base / 4 + 1 <= a;
        }
        break;
    case 4:
        keep = owner->m_bitsA != 0;
        break;
    case 5:
        if (1.5 <= ratio && owner->m_bitsA == 0 && surplus <= limit) {
            const int base = owner->Stat(0x1ac);
            keep = base / 2 + 1 <= b && base / 4 + 1 <= a;
        }
        break;
    case 6:
        keep = 1.5 <= ratio && owner->m_bitsA == 0 && 0 < surplus;
        break;
    case 7:
        if ((b < owner->Stat(0x1ac) + 1 || a < owner->Stat(0x1ac) / 2 + 1) && 0 < surplus &&
            (ratio < 1.5 || surplus <= limit) && owner->m_bitsA == 0)
            keep = owner->Stat(0x1c8) != 0;
        break;
    }
    if (!keep) m_plan = 0;
    if (m_plan == 0) {
        owner->m_flags.m_value &= 0xff3fffff;
        if (ratio < 1.5 || owner->m_bitsA != 0) {
            owner->m_flags.m_value |= 0x800000;
            if (owner->m_bitsA == 0) {
                const int base = owner->Stat(0x1ac);
                if ((b < base / 2 + 1 || a < base / 4 + 1) && 0 < owner->Stat(0x1c8))
                    m_plan = surplus <= limit ? 2 : 7;
                else
                    m_plan = surplus <= limit ? 1 : 3;
            } else {
                m_plan = 4;
            }
        } else if (limit < surplus) {
            m_plan = 6;
        } else {
            const int base = owner->Stat(0x1ac);
            if ((b < base / 2 + 1 || a < base / 4 + 1) && 0 < owner->Stat(0x1c8)) {
                m_plan = 2;
            } else {
                owner->m_flags.m_value |= 0x400000;
                m_plan = 5;
            }
        }
    }
    if (m_evaluator && m_evaluator->GetKind() != m_plan) {
        m_evaluator->ClearRecordFlags();
        delete m_evaluator;
        m_evaluator = nullptr;
    }
    if (!m_evaluator) {
        CreateEvaluator();
        if (m_evaluator) {
            m_evaluator->m_objects = &m_objects;
            m_owner->m_flags.m_value |= 2;
        }
    }
}

void AiTask13::CreateEvaluator() {
    switch (m_plan) {
    case 1: m_evaluator = NewOrNull<AiEvaluatorPlan1>(m_view, m_owner); break;
    case 2: m_evaluator = NewOrNull<AiEvaluatorPlan2>(m_view, m_owner); break;
    case 3: m_evaluator = NewOrNull<AiEvaluatorPlan3>(m_view, m_owner); break;
    case 4: m_evaluator = NewOrNull<AiEvaluatorPlan4>(m_view, m_owner); break;
    case 5: m_evaluator = NewOrNull<AiEvaluatorPlan5>(m_view, m_owner); break;
    case 6: m_evaluator = NewOrNull<AiEvaluatorPlan6>(m_view, m_owner); break;
    case 7: m_evaluator = NewOrNull<AiEvaluatorPlan7>(m_view, m_owner); break;
    }
}

int AiTask13::ChooseTargets() {
    ObjectKey sector;
    ObjectKey scratch = KeyOfUnit(kMovedUnitKey);
    GameObj* unit = static_cast<GameObj*>(GetObjectInFamilies_20_22(m_view, &scratch));
    if (!unit || (unit->m_stateFlags & 0x10)) return 0;

    m_selector.Add(m_owner->RankSectors(0, 0, 0x80000000, 0, 0, 0, 3, 1));
    sector = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    m_selector.Add(m_owner->RankSystemsOfSector(&sector, 0, 0, 0x80000000, 2, 0, 0, 0, 1));
    m_target = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    if (!IsSectorKey(sector) || !IsSystemKey(m_target)) return 0;

    m_target.Reset();
    AiSectorEval* sectorEval = m_owner->m_sectors.FindEval(sector);
    if (!sectorEval) return 0;
    if ((sectorEval->m_flags2c.m_value & 0x40000) == 0 && GameHookDay() < m_nextTime) return 0;

    m_selector.Add(m_owner->RankSectors(0, 0, 4, 0, 0, 0x40000, 4, 2));
    sector = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    if (!IsSectorKey(sector)) {
        m_selector.Add(m_owner->RankSectors(0, 0, 4, 0, 0, 0, 4, 2));
        sector = *m_selector.PickBest(&scratch);
        m_selector.Reset();
    }
    if (!IsSectorKey(sector)) return 0;

    m_selector.Add(m_owner->RankSystemsOfSectorRandomly(&sector, 0x20, 0, 1, 0, 0, 0x200, 1));
    m_target = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    if (!IsSystemKey(m_target)) {
        m_selector.Add(m_owner->RankSystemsOfSectorRandomly(&sector, 0, 0, 1, 0, 0, 0x200, 1));
        m_target = *m_selector.PickBest(&scratch);
        m_selector.Reset();
    }
    return IsSystemKey(m_target) ? 1 : 0;
}

OrderBase* AiTask13::BuildScrapOrder() {
    ObjectKeyValueSet team;
    if (!IsSystemKey(m_target)) return nullptr;
    AiSystemEval* eval = m_owner->m_systems.FindEval(m_target);
    GameObjNotifier* system = GetObjectInFamilies_90_98(m_view, &m_target);
    if (!eval || !system) return nullptr;
    if (eval->Stat(0x84) == 0) {
        AiSideController* owner = m_owner;
        bool found = false;
        if (owner->Stat(0x1d0) < owner->Stat(0x1d4) || eval->Stat(0x50) == 0)
            found = AddFirstOfKind<ProductionFacilityType2DIterator>(&team, system);
        if (!found && (owner->Stat(0x1d4) <= owner->Stat(0x1d0) || eval->Stat(0x54) == 0))
            found = AddFirstOfKind<ProductionFacilityType2CIterator>(&team, system);
        found = found || AddFirstOfKind<DefenseFacility22Iterator>(&team, system);
        found = found || AddFirstOfKind<DefenseFacility23Iterator>(&team, system);
        found = found || AddFirstOfKind<DefenseFacility25Iterator>(&team, system);
        found = found || AddFirstOfKind<DefenseFacility24Iterator>(&team, system);
        found = found || AddFirstOfKind<ManufacturingFacility29Iterator>(&team, system);
        found = found || AddFirstOfKind<ManufacturingFacility28Iterator>(&team, system);
        found = found || AddFirstOfKind<ManufacturingFacility2AIterator>(&team, system);
    }
    if (!team.First()) return nullptr;
    OrderBase* order = static_cast<OrderBase*>(CreateMessageByKind(kScrapOrder));
    if (order) {
        order->m_side = m_view;
        order->SetTeam(&team);
    }
    return order;
}

OrderBase* AiTask13::BuildMoveOrder() {
    ObjectKeyValueSet team;
    OrderBase* order = static_cast<OrderBase*>(CreateMessageByKind(kMoveOrder));
    if (order) {
        ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(KeyOfUnit(kMovedUnitKey), 0);
        if (node) {
            team.Add(node);
            order->m_side = m_view;
            order->SetTeam(&team);
            order->SetTarget(&m_target);
            m_nextTime = GameHookDay() + kRetryDelayDays;
        }
    }
    return order;
}
