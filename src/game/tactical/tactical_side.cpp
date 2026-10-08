#include "tactical/tactical_side.h"

#include <new>

#include "tactical/tactical_externals_2b.h"
#include "tactical/tactical_game.h"
#include "tactical/tactical_unit_access.h"

namespace {

constexpr float kFleetPriorityScale = -10000.0f;
constexpr float kRankStep = 1.0f;
constexpr float kAssignmentLimit = 180.0f;
constexpr int kFighterClass = 1;
constexpr int kStateActive = 2;

SimUnitNode* firstUnitNode(UnitRefTree& tree) { return reinterpret_cast<SimUnitList*>(&tree)->First(); }

TargetPriorityNode* newPriorityNode(float priority, SimUnit* unit) {
    void* memory = ::operator new(sizeof(TargetPriorityNode));
    return memory ? new (memory) TargetPriorityNode(priority, unit) : nullptr;
}

TargetPriorityNode* firstNode(TargetPriorityTree& tree) { return static_cast<TargetPriorityNode*>(tree.First()); }

TargetPriorityNode* successor(TargetPriorityNode* node) { return static_cast<TargetPriorityNode*>(node->m_successor); }

}  // namespace

// ---- scorers ----------------------------------------------------------------------------------------------------------

AttackEvaluatorBase::~AttackEvaluatorBase() {}

FormationEvaluatorBase::~FormationEvaluatorBase() {}

SideOrderTree::~SideOrderTree() {}

int SideOrderTree::IsLess(AvlNode* a, AvlNode* b) {
    struct OrderKey {
        uint8_t m_node[0x18];
        float m_primary;       // +0x18
        void* m_vptr;          // +0x1c
        uint32_t m_first;      // +0x20
        uint32_t m_second;     // +0x24
        float m_third;         // +0x28
        float m_secondary;     // +0x2c
    };
    const OrderKey* left = reinterpret_cast<const OrderKey*>(a);
    const OrderKey* right = reinterpret_cast<const OrderKey*>(b);
    if (left->m_primary == right->m_primary) {
        if (left->m_secondary == right->m_secondary) {
            const bool firstLess = left->m_first < right->m_first;
            if (left->m_first != right->m_first) return firstLess;
            const bool secondLess = left->m_second < right->m_second;
            if (left->m_second != right->m_second) return secondLess;
            if (left->m_third < right->m_third) return true;
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
    for (SimUnitNode* node = firstUnitNode(m_fleet); node; node = node->Next()) {
        if (unitField<int>(node->Value(), 0x150) != 0) return 1;
    }
    return 0;
}

void TacticalSideBase::RecordFleetPriorities() {
    const int count = Refresh(0);
    m_previousStrength = m_strength;
    m_unitCount = count;
    for (SimUnitNode* node = firstUnitNode(m_fleet); node; node = node->Next()) {
        const float strength = unitVirtuals(node->Value())->Slot36();
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

float TacticalSideBase::EvaluateAttack(int a, SimUnit* unit) {
    float result = 0.0f;
    if (unit && m_attackEvaluator) result = m_attackEvaluator->Evaluate(a, unit);
    return result;
}

int TacticalSideBase::EvaluateUnit4(SimUnit* unit) {
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
        if (unitVirtuals(node->m_unit.m_unit)->GetUnitClass() == kFighterClass) {
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
        SimUnit* first = node->Value()->FirstUnit();
        if (first && FUN_005b7ce0(first) != 0) {
            flaggedGroups.AddGroup(node->Value());
        } else {
            plainGroups.AddGroup(node->Value());
        }
    }
    if (plainGroups.FirstNode()) m_formationEvaluator->Assign(this, &plainGroups, list, kAssignmentLimit);
    if (flaggedGroups.FirstNode()) m_formationEvaluator->Assign(this, &flaggedGroups, &sorted, kAssignmentLimit);
}
