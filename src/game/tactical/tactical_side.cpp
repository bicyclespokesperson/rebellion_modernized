#include "tactical/tactical_side.h"

#include <compare>
#include <new>
#include <tuple>
#include <vector>

#include "tactical/tactical_externals_2b.h"
#include "tactical/tactical_game.h"
#include "tactical/tactical_ship.h"

namespace
{

constexpr float kFleetPriorityScale = -10000.0f;
constexpr float kRankStep = 1.0f;
constexpr float kAssignmentLimit = 180.0f;
constexpr int kFighterClass = 1;
constexpr float kNoInterceptTime = -1.0f;
constexpr int kStateActive = 2;

TargetPriorityNode* newPriorityNode(float priority, TacticalShipBase* unit)
{
  void* memory = ::operator new(sizeof(TargetPriorityNode));
  return memory ? new (memory) TargetPriorityNode(priority, unit) : nullptr;
}

} // namespace

// ---- scorers ----------------------------------------------------------------------------------------------------------

AttackEvaluatorBase::~AttackEvaluatorBase() = default;

FormationEvaluatorBase::~FormationEvaluatorBase() = default;

namespace
{

bool sideOrderBefore(const SideOrderNode* left, const SideOrderNode* right)
{
  if (auto order = std::weak_order(left->m_primary, right->m_primary); order != 0)
    return order < 0;
  if (auto order = std::weak_order(left->m_secondary, right->m_secondary); order != 0)
    return order < 0;
  const auto leftKey = std::tie(left->m_unitHandle, left->m_second);
  const auto rightKey = std::tie(right->m_unitHandle, right->m_second);
  if (leftKey != rightKey)
    return leftKey < rightKey;
  return std::weak_order(left->m_weight, right->m_weight) < 0;
}

} // namespace

SideOrderTree::~SideOrderTree()
{
  m_clearing = true;
  decltype(m_nodes) detached;
  detached.swap(m_nodes);
  detached.clear();
}

bool SideOrderTree::NodeOrder::operator()(const std::unique_ptr<SideOrderNode>& a,
                                          const std::unique_ptr<SideOrderNode>& b) const
{
  return sideOrderBefore(a.get(), b.get());
}

void SideOrderTree::Insert(SideOrderNode* node)
{
  if (!m_clearing && node)
  {
    std::unique_ptr<SideOrderNode> pending(node);
    try
    {
      m_nodes.insert(std::move(pending));
    }
    catch (...)
    {
      pending.release();
      throw;
    }
  }
}

// ---- the side -----------------------------------------------------------------------------------------------------------

TacticalSideBase::TacticalSideBase()
{
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

TacticalSideBase::~TacticalSideBase()
{
  delete m_attackEvaluator;
  m_attackEvaluator = nullptr;
  delete m_formationEvaluator;
  m_formationEvaluator = nullptr;
  if (m_targets[0].First())
    m_targets[0].Clear();
}

int TacticalSideBase::HasDisabledUnit()
{
  for (const auto& entry : m_fleet.Entries())
  {
    auto* node = static_cast<UnitRefNode*>(entry.get());
    if (node->Value()->m_hasGravityWell != 0)
      return 1;
  }
  return 0;
}

void TacticalSideBase::RecordFleetPriorities()
{
  const int count = Refresh(0);
  m_previousStrength = m_strength;
  m_unitCount = count;
  for (const auto& entry : m_fleet.Entries())
  {
    auto* node = static_cast<UnitRefNode*>(entry.get());
    const float strength = node->Value()->GetSpeed();
    const float spare = node->Value()->TacticalShipBase::MaxWeaponRange();
    const float priority = strength - spare * kFleetPriorityScale;
    TargetPriorityNode* priorityNode = nullptr;
    void* memory = ::operator new(sizeof(TargetPriorityNode));
    if (memory)
      priorityNode = new (memory) TargetPriorityNode(-priority, node->Value());
    if (node->Value()->TacticalShipBase::HasOnlyKind1Weapons() == 1)
    {
      m_targets[3].Insert(priorityNode);
    }
    else if (node->Value()->TacticalShipBase::HasKind1Weapons() != 0)
    {
      m_targets[2].Insert(priorityNode);
    }
    else
    {
      m_targets[1].Insert(priorityNode);
    }
  }
}

float TacticalSideBase::InterceptTime(TacticalShipBase* unit, ShipGroup* group, float* total, float* speed)
{
  if (m_attackEvaluator)
    return m_attackEvaluator->Virtual01(unit, group, total, speed);
  return kNoInterceptTime;
}

float TacticalSideBase::EvaluateAttack(int a, TacticalShipBase* unit)
{
  float result = 0.0f;
  if (unit && m_attackEvaluator)
    result = m_attackEvaluator->Evaluate(a, unit);
  return result;
}

int TacticalSideBase::EvaluateUnit4(TacticalShipBase* unit)
{
  if (m_attackEvaluator)
    return m_attackEvaluator->Virtual04(unit);
  return 0;
}

int TacticalSideBase::EvaluateUnit3(ShipGroup* group)
{
  if (m_attackEvaluator)
    return m_attackEvaluator->Virtual03(group);
  return 0;
}

void TacticalSideBase::ReprioritiseTargets(float)
{
  TargetPriorityTree* list = &m_targets[0];
  if (!list->First())
    return;

  TargetPriorityTree sorted;
  std::vector<TargetPriorityNode*> initialNodes;
  initialNodes.reserve(list->Count());
  for (const auto& entry : list->Entries())
    initialNodes.push_back(static_cast<TargetPriorityNode*>(entry.get()));
  for (TargetPriorityNode* node : initialNodes)
  {
    if (list->PositionOf(node) == list->Entries().end())
      continue;
    if (node->m_unit.m_unit->GetUnitClass() == kFighterClass)
    {
      sorted.Insert(newPriorityNode(node->m_priority, node->m_unit.m_unit));
      TargetPriorityNode* match = nullptr;
      for (const auto& entry : list->Entries())
      {
        auto* candidate = static_cast<TargetPriorityNode*>(entry.get());
        if (candidate->m_unit.SameUnit(&node->m_unit))
        {
          match = candidate;
          break;
        }
      }
      if (match)
      {
        list->Remove(match);
        delete match;
      }
    }
  }

  float rank = kRankStep;
  float base = 0.0f;
  if (sorted.Last())
    base = static_cast<TargetPriorityNode*>(sorted.Last())->m_priority;
  for (const auto& entry : list->Entries())
  {
    auto* node = static_cast<TargetPriorityNode*>(entry.get());
    sorted.Insert(newPriorityNode(base + rank, node->m_unit.m_unit));
    rank = rank - -kRankStep;
  }

  base = 0.0f;
  if (list->Last())
    base = static_cast<TargetPriorityNode*>(list->Last())->m_priority;
  rank = kRankStep;
  for (const auto& entry : sorted.Entries())
  {
    auto* node = static_cast<TargetPriorityNode*>(entry.get());
    list->Insert(newPriorityNode(base + rank, node->m_unit.m_unit));
    rank = rank - -kRankStep;
  }

  GroupRefTree plainGroups;
  GroupRefTree flaggedGroups;
  for (const auto& entry : m_groups.Entries())
  {
    auto* node = static_cast<GroupRefNode*>(entry.get());
    if (EvaluateUnit3(node->Value()) != 0)
      continue;
    TacticalShipBase* first = node->Value()->FirstUnit();
    if (first && first->TacticalShipBase::HasKind1Weapons() != 0)
    {
      flaggedGroups.AddGroup(node->Value());
    }
    else
    {
      plainGroups.AddGroup(node->Value());
    }
  }
  if (plainGroups.FirstNode())
    m_formationEvaluator->Assign(this, &plainGroups, list, kAssignmentLimit);
  if (flaggedGroups.FirstNode())
    m_formationEvaluator->Assign(this, &flaggedGroups, &sorted, kAssignmentLimit);
}
