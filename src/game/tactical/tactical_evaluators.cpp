#include <cmath>
#include <new>

#include "tactical/tactical_externals_2b.h"
#include "tactical/tactical_formation.h"
#include "tactical/tactical_game.h"
#include "tactical/tactical_registry.h"
#include "tactical/tactical_groups.h"
#include "tactical/tactical_side.h"
#include "tactical/tactical_unit_access.h"
#include "tactical/tactical_units.h"

namespace {

constexpr int kModeAttacking = 3;
constexpr int kModeReturning = 6;
constexpr int kModeMoving = 2;
constexpr int kCapitalGroupKind = 4;
constexpr int kStateDocked = 1;
constexpr int kStateLanding = 3;
constexpr int kStateActive = 2;
constexpr float kInterceptHorizon = 300.0f;
constexpr float kNoTime = -1.0f;
constexpr float kStrengthWeight = 15.0f;
constexpr float kPenaltyWeight = 0.01f;
constexpr float kOrderWeight = 0.03f;
constexpr float kAboardWeight = 1.0f;
constexpr float kScaleSlack = 0.65f;
constexpr float kDistanceWeight = 0.001f;
constexpr float kMinimumDistance = 1.0f;
constexpr int kRadiusOffset = 0x3c0;
constexpr int kDamageOffset = 0x3cc;
constexpr int kUnitPositionOffset = 0x30;
constexpr int kFormationPositionOffset = 0x3c;

// The simulator's two record trees (characters and special forces) hold the units that travel aboard a ship; their first
// node is reached through the second virtual slot after the tree interface.
class SimRecordTree : public IdAvlTree {
public:
    virtual AvlNode* FirstRecord();  // slot 3
};

SimUnitNode* firstUnitNode(UnitRefTree& tree) { return FirstNodeOf(*reinterpret_cast<SimUnitList*>(&tree)); }

bool differs(float a, float b) { return a < b || a > b; }

int countAboard(IdAvlTree* tree, int shipHandleOffset, int handle) {
    int count = 0;
    for (AvlNode* node = reinterpret_cast<SimRecordTree*>(tree)->FirstRecord(); node; node = node->m_successor) {
        if (*reinterpret_cast<int*>(reinterpret_cast<char*>(node) + shipHandleOffset) == handle) ++count;
    }
    return count;
}

}  // namespace

float AttackEvaluatorBase::Virtual01(SimUnit* unit, ShipGroup* group, float* total, float* speed) {
    unitVirtuals(unit)->GetUnitClass();
    *speed = 0.0f;
    if (!unit || !group) return kNoTime;

    *total = group->MemberStrengthAgainst(unit) + *total;
    float scale = unitField<float>(unit, kRadiusOffset) + unitField<float>(unit, kDamageOffset) * kScaleSlack;
    if (kMinimumDistance > scale) scale = kMinimumDistance;
    const float unitSpeed = unitVirtuals(unit)->Slot25();
    *speed = unitSpeed;
    const double remaining = static_cast<double>(*total) - static_cast<double>(unitSpeed);
    if (!(remaining > 0.0)) return kNoTime;

    float ratio = static_cast<float>(static_cast<double>(scale) / remaining);
    const float* position = reinterpret_cast<const float*>(reinterpret_cast<char*>(unit) + kUnitPositionOffset);
    const float* anchor = reinterpret_cast<const float*>(reinterpret_cast<char*>(group->m_formation) + kFormationPositionOffset);
    const float dx = position[0] - anchor[0];
    const float dz = position[2] - anchor[2];
    const float dy = position[1] - anchor[1];
    if (differs(dx, g_formationOrigin.m_x) || differs(dy, g_formationOrigin.m_y) || differs(dz, g_formationOrigin.m_z)) {
        const float formationSpeed = group->m_formation->Virtual13();
        float distance = 0.0f;
        if (differs(formationSpeed, 0.0f)) distance = std::sqrt(dy * dy + dz * dz + dx * dx) / formationSpeed;
        if (kMinimumDistance > distance) distance = kMinimumDistance;
        ratio = distance * kDistanceWeight + ratio;
    }
    return ratio;
}

int AttackEvaluator::Virtual04(SimUnit* unit) {
    if (unitState(unit) == kStateActive) {
        const float value = unitVirtuals(unit)->Slot24();
        if (differs(value, 0.0f)) return 1;
    }
    return 0;
}

int AttackEvaluator::Virtual03(ShipGroup* group) {
    const int mode = group->m_mode;
    if (mode == kModeAttacking || mode == kModeReturning || mode == kModeMoving) return 1;
    if (TacticalUnitKind(group) == kCapitalGroupKind) {
        for (SimUnitNode* node = firstUnitNode(*group); node; node = node->Next()) {
            if (unitState(node->Value()) == kStateDocked) return 1;
            if (unitState(node->Value()) == kStateLanding) return 1;
        }
    }
    SimUnit* leader = group->m_formationA.FirstUnit();
    if (!leader) return 0;
    if (!Virtual04(leader)) return 0;

    float total = 0.0f;
    float speed = 0.0f;
    float time = Virtual01(leader, group, &total, &speed);
    if (time < kInterceptHorizon || !(time >= kInterceptHorizon)) {
        if (differs(time, kNoTime)) return 1;
    }
    const int side = group->m_side;
    GroupRefTree& groups = reinterpret_cast<TacticalSideBase*>(GetHeadlessSimulator()->m_sides[side])->m_groups;
    for (GroupRefNode* node = static_cast<GroupRefNode*>(groups.FirstNode()); node;
         node = static_cast<GroupRefNode*>(node->Next())) {
        if (node->m_id == group->m_id) continue;
        if (node->Value()->m_formationA.FirstUnit() != leader) continue;
        time = Virtual01(leader, node->Value(), &total, &speed);
    }
    if (!(time < kInterceptHorizon)) return 0;
    if (differs(time, kNoTime)) return 1;
    return 0;
}

float AttackEvaluator::Evaluate(int a, SimUnit* unit) {
    float score = 0.0f;
    if (!unit) return score;
    HeadlessSimulator* simulator = GetHeadlessSimulator();
    score = static_cast<float>(countAboard(&simulator->m_specialForces, 0x48, unitHandle(unit))) * kAboardWeight + score;
    simulator = GetHeadlessSimulator();
    score = static_cast<float>(countAboard(&simulator->m_characters, 0x5c, unitHandle(unit))) * kAboardWeight + score;
    score = FUN_005b7a80(unit, a, 0) * kStrengthWeight + score;
    score = score - FUN_005b1710(unit) * kPenaltyWeight;
    score = score - FUN_005b1640(unit) * kPenaltyWeight;
    if (unitState(unit) == kStateDocked || unitState(unit) == kStateLanding) score = score - kPenaltyWeight;
    const int side = unitSide(unit);
    SideOrderTree& orders = reinterpret_cast<TacticalSideBase*>(GetHeadlessSimulator()->m_sides[side])->m_orders;
    float result = score;
    for (AvlNode* node = orders.First(); node; node = node->m_successor) {
        const SideOrderNode* order = reinterpret_cast<const SideOrderNode*>(node);
        if (order->m_unitHandle == static_cast<uint32_t>(unitHandle(unit))) result = order->m_weight * kOrderWeight + result;
    }
    return result;
}

// ---- the formation evaluator ---------------------------------------------------------------------------------------------

namespace {

constexpr float kFarPriority = 100000.0f;

ScoredGroupNode* newScored(float priority, ShipGroup* group) {
    void* memory = ::operator new(sizeof(ScoredGroupNode));
    return memory ? new (memory) ScoredGroupNode(priority, group) : nullptr;
}

TargetPriorityNode* firstTarget(TargetPriorityTree* tree) { return static_cast<TargetPriorityNode*>(tree->First()); }

TargetPriorityNode* nextTarget(TargetPriorityNode* node) { return static_cast<TargetPriorityNode*>(node->m_successor); }

GroupRefNode* firstGroup(GroupRefTree& tree) { return static_cast<GroupRefNode*>(tree.FirstNode()); }

GroupRefNode* nextGroup(GroupRefNode* node) { return static_cast<GroupRefNode*>(node->Next()); }

ScoredGroupNode* previousScored(ScoredGroupNode* node) { return static_cast<ScoredGroupNode*>(node->Predecessor()); }

bool timeIsUsable(float time, float limit) { return time >= 0.0f && time <= limit; }

}  // namespace

int GroupRefSlot::SameGroup(GroupRefSlot* other) {
    if (m_group && other->m_group) return m_group->m_id == other->m_group->m_id;
    return 0;
}

int GroupRefSlot::GroupBefore(GroupRefSlot* other) {
    if (m_group && other->m_group) return m_group->m_id < other->m_group->m_id;
    return 0;
}

ScoredGroupNode::ScoredGroupNode(float priority, ShipGroup* group) {
    m_priority = priority;
    m_group.m_group = group;
}

ScoredGroupNode::~ScoredGroupNode() {}

ScoredGroupTree::~ScoredGroupTree() {}

int ScoredGroupTree::IsLess(AvlNode* a, AvlNode* b) {
    ScoredGroupNode* left = static_cast<ScoredGroupNode*>(a);
    ScoredGroupNode* right = static_cast<ScoredGroupNode*>(b);
    if (!differs(left->m_priority, right->m_priority)) return left->m_group.GroupBefore(&right->m_group);
    if (left->m_priority < right->m_priority) return 1;
    return 0;
}

int FormationEvaluator::Assign(TacticalSideBase* side, GroupRefTree* groups, TargetPriorityTree* targets, float limit) {
    if (!targets->First()) return 0;
    if (!groups->FirstNode()) return 0;
    GroupRefTree remaining(*groups);

    for (TargetPriorityNode* target = firstTarget(targets); target; target = nextTarget(target)) {
        if (side->EvaluateUnit4(target->m_unit.m_unit) == 0) continue;
        ScoredGroupTree scored;
        for (GroupRefNode* node = firstGroup(remaining); node; node = nextGroup(node)) {
            float total = 0.0f;
            float speed = 0.0f;
            const float time = side->InterceptTime(target->m_unit.m_unit, node->Value(), &total, &speed);
            ScoredGroupNode* entry;
            if (timeIsUsable(time, limit)) {
                entry = newScored(time, node->Value());
            } else {
                entry = newScored(kFarPriority - total, node->Value());
            }
            scored.Insert(entry);
        }
        ScoredGroupNode* last = static_cast<ScoredGroupNode*>(scored.Last());
        ScoredGroupNode* first = static_cast<ScoredGroupNode*>(scored.First());
        if (!first) return 0;
        const bool oneChoice = !differs(first->m_priority, last->m_priority) && first->m_group.SameGroup(&last->m_group);
        if (oneChoice) {
            FUN_005afa00(GetHeadlessSimulator(), target->m_unit.m_unit, first->m_group.m_group);
            remaining.DeleteById(static_cast<ScoredGroupNode*>(scored.First())->m_group.m_group->m_id);
            return 1;
        }

        float strength = first->m_group.m_group->MemberStrengthAgainst(target->m_unit.m_unit);
        float speed;
        for (ScoredGroupNode* candidate = last; candidate; candidate = previousScored(candidate)) {
            if (!differs(candidate->m_priority, first->m_priority) && candidate->m_group.SameGroup(&first->m_group)) break;
            const float time = side->InterceptTime(target->m_unit.m_unit, candidate->m_group.m_group, &strength, &speed);
            if (!timeIsUsable(time, limit)) continue;
            for (ScoredGroupNode* assigned = static_cast<ScoredGroupNode*>(scored.Last()); assigned;
                 assigned = previousScored(assigned)) {
                SimUnit* targetUnit = target->m_unit.m_unit;
                FUN_005afa00(GetHeadlessSimulator(), targetUnit, assigned->m_group.m_group);
                remaining.DeleteById(assigned->m_group.m_group->m_id);
                if (!differs(assigned->m_priority, candidate->m_priority) && assigned->m_group.SameGroup(&candidate->m_group)) break;
            }
            break;
        }
    }

    for (GroupRefNode* leftover = firstGroup(remaining); leftover; leftover = nextGroup(leftover)) {
        TargetPriorityNode* pick = firstTarget(targets);
        if (pick) {
            do {
                if (side->EvaluateUnit4(pick->m_unit.m_unit) == 1) break;
                pick = nextTarget(pick);
            } while (pick);
        }
        SimUnit* chosen = pick ? pick->m_unit.m_unit : firstTarget(targets)->m_unit.m_unit;
        FUN_005afa00(GetHeadlessSimulator(), chosen, leftover->Value());
    }
    return 1;
}
