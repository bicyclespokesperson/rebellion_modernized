#include "tactical/tactical_side.h"

#include "core/vptr_store.h"

#include <new>

#include "tactical/tactical_externals_2b.h"
#include "tactical/tactical_game.h"
#include "tactical/tactical_ship.h"

namespace {

constexpr float kFleetPriorityScale = -10000.0f;
constexpr float kRankStep = 1.0f;
constexpr float kAssignmentLimit = 180.0f;
constexpr int kFighterClass = 1;
constexpr float kNoInterceptTime = -1.0f;
constexpr int kStateActive = 2;

UnitRefNode* firstUnitNode(UnitRefTree& tree) { return tree.FirstNode(); }

TargetPriorityNode* newPriorityNode(float priority, TacticalShipBase* unit) {
    void* memory = ::operator new(sizeof(TargetPriorityNode));
    return memory ? new (memory) TargetPriorityNode(priority, unit) : nullptr;
}

TargetPriorityNode* firstNode(TargetPriorityTree& tree) { return static_cast<TargetPriorityNode*>(tree.First()); }

TargetPriorityNode* successor(TargetPriorityNode* node) { return static_cast<TargetPriorityNode*>(node->m_successor); }

}  // namespace

// ---- scorers ----------------------------------------------------------------------------------------------------------

AttackEvaluatorBase::~AttackEvaluatorBase() { REB_KEEP_VPTR_STORE(); }

FormationEvaluatorBase::~FormationEvaluatorBase() { REB_KEEP_VPTR_STORE(); }

SideOrderTree::~SideOrderTree() {}

int SideOrderTree::IsLess(AvlNode* a, AvlNode* b) {
    const SideOrderNode* left = reinterpret_cast<const SideOrderNode*>(a);
    const SideOrderNode* right = reinterpret_cast<const SideOrderNode*>(b);
    if (left->m_primary == right->m_primary) {
        if (left->m_secondary == right->m_secondary) {
            const bool firstLess = left->m_unitHandle < right->m_unitHandle;
            if (left->m_unitHandle != right->m_unitHandle) return firstLess;
            const bool secondLess = left->m_second < right->m_second;
            if (left->m_second != right->m_second) return secondLess;
            if (left->m_weight < right->m_weight) return true;
        } else if (left->m_secondary < right->m_secondary) {
            return true;
        }
    } else if (left->m_primary < right->m_primary) {
        return true;
    }
    return false;
}

// ---- the side -----------------------------------------------------------------------------------------------------------

TacticalSideBase::TacticalSideBase() {
    m_channelA.m_data = nullptr;
    m_channelB.m_data = nullptr;
    m_unk48 = 0;
    m_unk4c = 0;
    m_unk50 = 0;
    m_unk54 = -1.0f;
    m_unk20 = 0;
    m_unk14 = 1;
    m_unk7c = 0;
    m_unk80 = 0;
    m_unk1c = 0;
    m_unk24 = 0.33f;
    m_state = 0;
    m_previousStrength = 0;
    m_strength = 0;
    m_unitCount = 0;
    m_unk40 = 0;
    m_unk18 = 0;
    m_attackEvaluatorKind = 0;
    m_attackEvaluator = nullptr;
    m_formationEvaluatorKind = 0;
    m_formationEvaluator = nullptr;
}

TacticalSideBase::~TacticalSideBase() {
    delete m_attackEvaluator;
    m_attackEvaluator = nullptr;
    delete m_formationEvaluator;
    m_formationEvaluator = nullptr;
    if (m_targets[0].First()) m_targets[0].Clear();
}

int TacticalSideBase::HasDisabledUnit() {
    for (UnitRefNode* node = firstUnitNode(m_fleet); node; node = node->Next()) {
        if (node->Value()->m_hasGravityWell != 0) return 1;
    }
    return 0;
}

void TacticalSideBase::RecordFleetPriorities() {
    const int count = Refresh(0);
    m_previousStrength = m_strength;
    m_unitCount = count;
    for (UnitRefNode* node = firstUnitNode(m_fleet); node; node = node->Next()) {
        const float strength = node->Value()->GetSpeed();
        const float spare = FUN_005b18d0(node->Value());
        const float priority = strength - spare * kFleetPriorityScale;
        TargetPriorityNode* entry = nullptr;
        void* memory = ::operator new(sizeof(TargetPriorityNode));
        if (memory) entry = new (memory) TargetPriorityNode(-priority, node->Value());
        if (FUN_005b7d10(node->Value()) == 1) {
            m_targets[3].Insert(entry);
        } else if (FUN_005b7ce0(node->Value()) != 0) {
            m_targets[2].Insert(entry);
        } else {
            m_targets[1].Insert(entry);
        }
    }
}

float TacticalSideBase::InterceptTime(TacticalShipBase* unit, ShipGroup* group, float* total, float* speed) {
    if (m_attackEvaluator) return m_attackEvaluator->Virtual01(unit, group, total, speed);
    return kNoInterceptTime;
}

float TacticalSideBase::EvaluateAttack(int a, TacticalShipBase* unit) {
    float result = 0.0f;
    if (unit && m_attackEvaluator) result = m_attackEvaluator->Evaluate(a, unit);
    return result;
}

int TacticalSideBase::EvaluateUnit4(TacticalShipBase* unit) {
    if (m_attackEvaluator) return m_attackEvaluator->Virtual04(unit);
    return 0;
}

int TacticalSideBase::EvaluateUnit3(ShipGroup* group) {
    if (m_attackEvaluator) return m_attackEvaluator->Virtual03(group);
    return 0;
}

void TacticalSideBase::ReprioritiseTargets(float) {
    TargetPriorityTree* list = &m_targets[0];
    if (!list->First()) return;

    TargetPriorityTree sorted;
    for (TargetPriorityNode* node = firstNode(*list); node;) {
        TargetPriorityNode* next = successor(node);
        if (node->m_unit.m_unit->GetUnitClass() == kFighterClass) {
            sorted.Insert(newPriorityNode(node->m_priority, node->m_unit.m_unit));
            TargetPriorityNode* match = firstNode(*list);
            while (match && !match->m_unit.SameUnit(&node->m_unit)) match = successor(match);
            if (match) {
                list->Remove(match);
                delete match;
            }
        }
        node = next;
    }

    float rank = kRankStep;
    float base = 0.0f;
    if (sorted.Last()) base = static_cast<TargetPriorityNode*>(sorted.Last())->m_priority;
    for (TargetPriorityNode* node = firstNode(*list); node; node = successor(node)) {
        sorted.Insert(newPriorityNode(base + rank, node->m_unit.m_unit));
        rank = rank - -kRankStep;
    }

    base = 0.0f;
    if (list->Last()) base = static_cast<TargetPriorityNode*>(list->Last())->m_priority;
    rank = kRankStep;
    for (TargetPriorityNode* node = firstNode(sorted); node; node = successor(node)) {
        list->Insert(newPriorityNode(base + rank, node->m_unit.m_unit));
        rank = rank - -kRankStep;
    }

    GroupRefTree plainGroups;
    GroupRefTree flaggedGroups;
    for (GroupRefNode* node = static_cast<GroupRefNode*>(m_groups.FirstNode()); node;
         node = static_cast<GroupRefNode*>(node->Next())) {
        if (EvaluateUnit3(node->Value()) != 0) continue;
        TacticalShipBase* first = node->Value()->FirstUnit();
        if (first && FUN_005b7ce0(first) != 0) {
            flaggedGroups.AddGroup(node->Value());
        } else {
            plainGroups.AddGroup(node->Value());
        }
    }
    if (plainGroups.FirstNode()) m_formationEvaluator->Assign(this, &plainGroups, list, kAssignmentLimit);
    if (flaggedGroups.FirstNode()) m_formationEvaluator->Assign(this, &flaggedGroups, &sorted, kAssignmentLimit);
}
