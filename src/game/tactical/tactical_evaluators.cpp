#include <cmath>
#include <new>

#include "tactical/tactical_externals_2b.h"
#include "tactical/tactical_formation.h"
#include "tactical/tactical_game.h"
#include "tactical/tactical_registry.h"
#include "tactical/tactical_groups.h"
#include "tactical/tactical_side.h"
#include "tactical/tactical_ship.h"
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

UnitRefNode* firstUnitNode(UnitRefTree& tree) { return tree.FirstNode(); }

bool differs(float a, float b) { return a < b || a > b; }

template <class Unit, class Tree>
int countAboard(Tree& tree, int handle) {
    int count = 0;
    for (AvlNode* node = tree.FirstItem(); node; node = node->m_successor) {
        if (static_cast<Unit*>(node)->m_shipHandle == handle) ++count;
    }
    return count;
}

}  // namespace

float AttackEvaluatorBase::Virtual01(TacticalShipBase* unit, ShipGroup* group, float* total, float* speed) {
    unit->GetUnitClass();
    *speed = 0.0f;
    if (!unit || !group) return kNoTime;

    *total = group->MemberStrengthAgainst(unit) + *total;
    float scale = unit->m_shield + unit->m_hull * kScaleSlack;
    if (kMinimumDistance > scale) scale = kMinimumDistance;
    const float unitSpeed = unit->GetShieldRecharge();
    *speed = unitSpeed;
    const double remaining = static_cast<double>(*total) - static_cast<double>(unitSpeed);
    if (!(remaining > 0.0)) return kNoTime;

    float ratio = static_cast<float>(static_cast<double>(scale) / remaining);
    const TacticalVector& position = unit->m_position;
    const TacticalVector& anchor = group->m_formation->m_position;
    const float dx = position.m_x - anchor.m_x;
    const float dz = position.m_z - anchor.m_z;
    const float dy = position.m_y - anchor.m_y;
    if (differs(dx, g_formationOrigin.m_x) || differs(dy, g_formationOrigin.m_y) || differs(dz, g_formationOrigin.m_z)) {
        const float formationSpeed = group->m_formation->Virtual13();
        float distance = 0.0f;
        if (differs(formationSpeed, 0.0f)) distance = std::sqrt(dy * dy + dz * dz + dx * dx) / formationSpeed;
        if (kMinimumDistance > distance) distance = kMinimumDistance;
        ratio = distance * kDistanceWeight + ratio;
    }
    return ratio;
}

int AttackEvaluator::Virtual04(TacticalShipBase* unit) {
    if (unit->m_state == kStateActive) {
        const float value = unit->GetWeaponRecharge();
        if (differs(value, 0.0f)) return 1;
    }
    return 0;
}

int AttackEvaluator::Virtual03(ShipGroup* group) {
    const int mode = group->m_mode;
    if (mode == kModeAttacking || mode == kModeReturning || mode == kModeMoving) return 1;
    if (TacticalUnitKind(group) == kCapitalGroupKind) {
        for (UnitRefNode* node = firstUnitNode(*group); node; node = node->Next()) {
            if (node->Value()->m_state == kStateDocked) return 1;
            if (node->Value()->m_state == kStateLanding) return 1;
        }
    }
    TacticalShipBase* leader = group->m_formationA.FirstUnit();
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

float AttackEvaluator::Evaluate(int a, TacticalShipBase* unit) {
    float score = 0.0f;
    if (!unit) return score;
    HeadlessSimulator* simulator = GetHeadlessSimulator();
    score = static_cast<float>(countAboard<TacticalSpecialForceUnit>(simulator->m_specialForces, static_cast<int>(unit->m_id))) * kAboardWeight + score;
    simulator = GetHeadlessSimulator();
    score = static_cast<float>(countAboard<TacticalCharacterUnit>(simulator->m_characters, static_cast<int>(unit->m_id))) * kAboardWeight + score;
    score = unit->TacticalShipBase::AttackRatingVsClass(a, 0) * kStrengthWeight + score;
    score = score - unit->TacticalShipBase::ShieldRechargeFraction() * kPenaltyWeight;
    score = score - unit->TacticalShipBase::ScaleByHullFull() * kPenaltyWeight;
    if (unit->m_state == kStateDocked || unit->m_state == kStateLanding) score = score - kPenaltyWeight;
    const int side = unit->m_side;
    SideOrderTree& orders = reinterpret_cast<TacticalSideBase*>(GetHeadlessSimulator()->m_sides[side])->m_orders;
    float result = score;
    for (AvlNode* node = orders.First(); node; node = node->m_successor) {
        const SideOrderNode* order = reinterpret_cast<const SideOrderNode*>(node);
        if (order->m_unitHandle == static_cast<uint32_t>(static_cast<int>(unit->m_id))) result = order->m_weight * kOrderWeight + result;
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
                TacticalShipBase* targetUnit = target->m_unit.m_unit;
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
        TacticalShipBase* chosen = pick ? pick->m_unit.m_unit : firstTarget(targets)->m_unit.m_unit;
        FUN_005afa00(GetHeadlessSimulator(), chosen, leftover->Value());
    }
    return 1;
}
