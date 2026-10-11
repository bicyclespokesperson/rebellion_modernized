#include <new>

#include "tactical/tactical_externals_2b.h"
#include "tactical/tactical_facade.h"
#include "tactical/tactical_game.h"
#include "tactical/tactical_registry.h"
#include "tactical/tactical_ship.h"
#include "tactical/tactical_side.h"

namespace
{

constexpr int kCapitalGroupKind = 4;
constexpr int kCapitalShipClass = 0;
constexpr int kStateActive = 2;
constexpr int kStateWithdrawing = 5;
constexpr int kStateDocked = 1;
constexpr int kStateLanding = 3;
constexpr float kDeathStarReady = 100.0f;
constexpr float kDeathStarBonus = 20000000.0f;
constexpr float kRankStep = 1.0f;

TargetPriorityNode* newPriorityNode(float priority, TacticalShipBase* unit)
{
  void* memory = ::operator new(sizeof(TargetPriorityNode));
  return memory ? new (memory) TargetPriorityNode(priority, unit) : nullptr;
}

TacticalSideBase* opposingSide(HeadlessSimulator* simulator, int sideIndex)
{
  return simulator->m_sides[sideIndex == 0 ? 1 : 0];
}

int kindOfGroup(ShipGroup* group)
{
  return TacticalUnitKind(group);
}

TacticalDeathStar* asDeathStar(TacticalUnit* deathStar)
{
  return static_cast<TacticalDeathStar*>(deathStar);
}

} // namespace

TargetPriorityTree* TacticalAiSide::BuildTargetList()
{
  TargetPriorityTree deferred;
  TargetPriorityTree* list = &m_targets[0];
  list->Clear();
  if (m_attackEvaluatorKind == 0)
    ResetEvaluators();

  const int index = SideIndex();
  TacticalSideBase* enemy = opposingSide(GetHeadlessSimulator(), index);
  for (const auto& entry : enemy->m_fleet.Entries())
  {
    auto* node = static_cast<UnitRefNode*>(entry.get());
    TacticalShipBase* unit = node->Value();
    if (!unit)
      continue;
    const float score = EvaluateAttack(0, unit);
    TargetPriorityNode* priorityNode = newPriorityNode(-score, unit);
    if (EvaluateUnit4(unit) == 1)
    {
      list->Insert(priorityNode);
    }
    else if (unit->m_state == kStateActive)
    {
      deferred.Insert(priorityNode);
    }
    else if (priorityNode)
    {
      delete priorityNode;
    }
  }
  for (const auto& entry : enemy->m_hangars.Entries())
  {
    auto* node = static_cast<UnitRefNode*>(entry.get());
    TacticalShipBase* unit = node->Value();
    if (!unit || unit->m_state != kStateActive)
      continue;
    const float score = EvaluateAttack(0, unit);
    TargetPriorityNode* priorityNode = newPriorityNode(-score, unit);
    if (EvaluateUnit4(unit) == 1)
    {
      list->Insert(priorityNode);
    }
    else if (unit->m_state == kStateActive)
    {
      deferred.Insert(priorityNode);
    }
    else if (priorityNode)
    {
      delete priorityNode;
    }
  }
  for (const auto& entry : GetHeadlessSimulator()->m_launched.Entries())
  {
    auto* node = static_cast<UnitRefNode*>(entry.get());
    TacticalShipBase* unit = node->Value();
    if (!unit || unit->m_side == index)
      continue;
    const int state = unit->m_state;
    if (state != kStateDocked && state != kStateLanding)
      continue;
    const float score = EvaluateAttack(0, unit);
    list->Insert(newPriorityNode(-score, unit));
  }
  for (auto entry = deferred.Entries().begin(); entry != deferred.Entries().end();)
  {
    auto current = entry++;
    AvlNode* node = current->get();
    deferred.Remove(node);
    TargetPriorityNode* last = static_cast<TargetPriorityNode*>(list->Last());
    if (last)
      static_cast<TargetPriorityNode*>(node)->m_priority = last->m_priority - -kRankStep;
    list->Insert(node);
  }
  return list;
}

int TacticalAiSide::HasNoShipsAtAll()
{
  if (m_unk80 == 0 || m_state != 0)
    return 0;
  m_strength = 0;
  const int index = SideIndex();
  TacticalSideBase* enemy = opposingSide(GetHeadlessSimulator(), index);
  if (enemy->HasDisabledUnit())
    return 0;
  for (const auto& entry : m_hangars.Entries())
  {
    auto* node = static_cast<UnitRefNode*>(entry.get());
    if (node->Value()->TacticalShipBase::UpdateHyperdriveReady() == 1)
      return 1;
  }
  for (const auto& entry : m_fleet.Entries())
  {
    auto* node = static_cast<UnitRefNode*>(entry.get());
    if (node->Value()->TacticalShipBase::UpdateHyperdriveReady() == 1)
      return 1;
  }
  if (SideIndex() != 1)
    return 0;
  TacticalUnit* deathStar = GetTacticalRegistry()->GetDeathStar();
  if (!deathStar)
    return 0;
  if (static_cast<TacticalDeathStar*>(deathStar)->TacticalDeathStar::IsOperational() != 0)
    return 1;
  if (asDeathStar(deathStar)->m_hasFacility != 0)
    return 1;
  return 0;
}

int TacticalAiSide::ResetEvaluators()
{
  m_unk1c = 0;
  if (m_attackEvaluator)
  {
    delete m_attackEvaluator;
    m_attackEvaluator = nullptr;
    m_attackEvaluatorKind = 0;
  }
  if (m_formationEvaluator)
  {
    delete m_formationEvaluator;
    m_formationEvaluator = nullptr;
    m_formationEvaluatorKind = 0;
  }
  m_formationEvaluatorKind = 3;
  void* memory = ::operator new(sizeof(FormationEvaluator));
  m_formationEvaluator = memory ? new (memory) FormationEvaluator() : nullptr;
  m_attackEvaluatorKind = 4;
  memory = ::operator new(sizeof(AttackEvaluator));
  m_attackEvaluator = memory ? new (memory) AttackEvaluator() : nullptr;
  return m_attackEvaluatorKind;
}

int TacticalAiSide::Refresh(int a)
{
  int count = 0;
  m_strength = 0;
  for (const auto& entry : m_fleet.Entries())
  {
    auto* node = static_cast<UnitRefNode*>(entry.get());
    bool engaged = false;
    const int index = SideIndex();
    GroupRefTree& enemyGroups = GetHeadlessSimulator()->m_sides[index == 1 ? 0 : 1]->m_groups;
    for (const auto& entry : enemyGroups.Entries())
    {
      auto* groupNode = static_cast<GroupRefNode*>(entry.get());
      TacticalShipBase* leader = groupNode->Value()->m_formationA.FirstUnit();
      if (leader && static_cast<int>(leader->m_id) == node->Key())
      {
        const float groupStrength = groupNode->Value()->MemberStrengthAgainst(node->Value());
        if (!(node->Value()->GetShieldRecharge() > groupStrength))
        {
          engaged = true;
          break;
        }
      }
    }
    if (!engaged)
      m_strength = node->Value()->TacticalShipBase::AttackRatingVsClass(0, a) + m_strength;
    ++count;
  }
  if (SideIndex() == 1)
  {
    for (const auto& entry : GetHeadlessSimulator()->m_launched.Entries())
    {
      auto* node = static_cast<UnitRefNode*>(entry.get());
      m_strength = node->Value()->TacticalShipBase::AttackRatingVsClass(0, a) + m_strength;
      ++count;
    }
  }
  for (const auto& entry : m_hangars.Entries())
  {
    auto* node = static_cast<UnitRefNode*>(entry.get());
    m_strength = node->Value()->TacticalShipBase::AttackRatingVsClass(0, a) + m_strength;
    ++count;
  }
  TacticalUnit* deathStar = GetTacticalRegistry()->GetDeathStar();
  if (deathStar && static_cast<TacticalDeathStar*>(deathStar)->TacticalDeathStar::IsOperational())
  {
    if (SideIndex() == 1)
    {
      m_strength = kDeathStarBonus + m_strength;
      return count + 1;
    }
    bool carried = GetHeadlessSimulator()->HeadlessSimulator::SideZeroHasMoreFighters() != 0;
    if (!carried)
    {
      for (const auto& entry : m_groups.Entries())
      {
        auto* groupNode = static_cast<GroupRefNode*>(entry.get());
        if (groupNode->Value()->m_unk68 == 6 || groupNode->Value()->m_mode == 6)
        {
          carried = true;
          break;
        }
      }
    }
    if (carried)
      m_strength = kDeathStarBonus + m_strength;
  }
  return count;
}

int TacticalAiSide::SplitIntoGroups(TargetPriorityTree* targets, int least, int most)
{
  targets->First();
  int total = targets->Count() & 0xffff;
  int root = 1;
  if (most >= 1)
  {
    do
    {
      if (root * root > total)
        break;
      ++root;
    } while (root <= most);
  }
  int groups;
  if (total > 1)
  {
    groups = root - 1;
  }
  else
  {
    groups = 1;
  }
  const int available = most - least + 1;
  if (groups >= available)
    groups = available;

  const int perGroup = total / groups;
  int index = 0;
  int created = 0;
  FighterGroup* group = nullptr;
  for (const auto& entry : targets->Entries())
  {
    auto* node = static_cast<TargetPriorityNode*>(entry.get());
    TacticalShipBase* unit = node->m_unit.m_unit;
    if (index % perGroup == 0 && created < groups)
    {
      ++created;
      void* memory = ::operator new(sizeof(FighterGroup));
      group = memory ? new (memory) FighterGroup(static_cast<int>(unit->m_id)) : nullptr;
      GetHeadlessSimulator();
      RegisterFighterGroup(group);
      m_groups.AddGroup(group);
      group->SetSeedHandle(static_cast<int>(unit->m_id));
      unit->m_groupHandle = group->m_id;
    }
    else
    {
      group->AddMember(static_cast<int>(unit->m_id));
    }
    ++index;
  }
  return index > 0 ? groups : 0;
}

int TacticalAiSide::RecordFleet(UnitRefTree*)
{
  RecordFleetPriorities();
  return 1;
}

int TacticalAiSide::Virtual11()
{
  if (HasNoShipsAtAll())
  {
    if (m_unkb0 == 1)
      return 1;
    Refresh(0);
    if (!(m_strength < 0.0f) && !(m_strength > 0.0f))
      return 1;
    HeadlessSimulator* simulator = GetHeadlessSimulator();
    const int index = SideIndex();
    TacticalSideBase* enemy = simulator->m_sides[index == 0 ? 1 : 0];
    enemy->Refresh(0);
    const float enemyStrength = GetHeadlessSimulator()->HeadlessSimulator::OpponentStrength(SideIndex());
    if (enemyStrength > 0.0f)
    {
      const float ratio = m_strength / enemyStrength;
      if (!(ratio >= m_unk24))
        return 1;
      if (!(ratio < 0.0f) && !(ratio > 0.0f))
        return 1;
    }
  }
  return 0;
}

int TacticalAiSide::StepSide(float step)
{
  HeadlessSimulator* simulator = GetHeadlessSimulator();
  TacticalUnit* deathStar = GetTacticalRegistry()->GetDeathStar();
  if (SideIndex() == 0)
  {
    if (deathStar && static_cast<TacticalDeathStar*>(deathStar)->TacticalDeathStar::IsReady() == 1)
    {
      const int enemyHangars = simulator->m_sides[1]->m_hangars.CountNodes();
      if (enemyHangars < m_hangars.CountNodes())
      {
        for (const auto& entry : m_groups.Entries())
        {
          auto* node = static_cast<GroupRefNode*>(entry.get());
          if (kindOfGroup(node->Value()) == kCapitalGroupKind)
          {
            simulator->DeathStarAttackRun(node->Value());
            break;
          }
        }
      }
    }
  }
  else
  {
    if (deathStar && static_cast<TacticalDeathStar*>(deathStar)->TacticalDeathStar::IsOperational())
    {
      if (asDeathStar(deathStar)->m_charge < kDeathStarReady || asDeathStar(deathStar)->m_charge > kDeathStarReady)
      {
        static_cast<TacticalDeathStar*>(deathStar)->TacticalDeathStar::Charge(step);
      }
      if (!(asDeathStar(deathStar)->m_charge < kDeathStarReady) &&
          !(asDeathStar(deathStar)->m_charge > kDeathStarReady))
      {
        BuildTargetList();
        for (const auto& entry : m_targets[0].Entries())
        {
          AvlNode* node = entry.get();
          TacticalShipBase* unit = static_cast<TargetPriorityNode*>(node)->m_unit.m_unit;
          if (unit->GetUnitClass() == kCapitalShipClass)
          {
            static_cast<TacticalDeathStar*>(deathStar)->TacticalDeathStar::ResetCharge(static_cast<int>(unit->m_id));
            GetHeadlessSimulator()->HeadlessSimulator::RetireUnit(unit);
            break;
          }
        }
      }
    }
    for (const auto& entry : GetHeadlessSimulator()->m_launched.Entries())
    {
      auto* node = static_cast<UnitRefNode*>(entry.get());
      TacticalHangar* launchInterface = GetTacticalRegistry()->FindLaunchInterface(node->Value()->m_carrierHandle);
      if (launchInterface)
        launchInterface->TakeOut(node->Value());
      node->Value()->m_state = kStateActive;
      m_hangars.AddUnit(node->Value());
    }
    GetHeadlessSimulator()->m_launched.Clear();
  }

  BuildTargetList();
  ReprioritiseTargets(step);
  for (const auto& entry : m_groups.Entries())
  {
    auto* node = static_cast<GroupRefNode*>(entry.get());
    TacticalShipBase* unit = node->Value()->m_formationA.FirstUnit();
    if (unit)
    {
      const float damage = node->Value()->MemberStrengthAgainst(unit) * step;
      unit->TacticalShipBase::TakeDamage(damage);
      const float health = unit->m_hull;
      if (!(health < 0.0f) && !(health > 0.0f))
        GetHeadlessSimulator()->HeadlessSimulator::RetireUnit(unit);
    }
  }

  if (Virtual11() == 1)
  {
    auto hangarEntry = m_hangars.Entries().begin();
    while (hangarEntry != m_hangars.Entries().end())
    {
      auto current = hangarEntry++;
      auto* node = static_cast<UnitRefNode*>(current->get());
      if (node->Value()->TacticalShipBase::UpdateHyperdriveReady() == 0)
      {
        if (GetHeadlessSimulator()->HeadlessSimulator::DockInHangar(node->Value()) == 0)
        {
          GetHeadlessSimulator()->HeadlessSimulator::RetireUnit(node->Value());
        }
      }
      else
      {
        GetHeadlessSimulator()->HeadlessSimulator::DockAtDeathStar(node->Value());
      }
    }
    if (SideIndex() == 1 && deathStar && static_cast<TacticalDeathStar*>(deathStar)->TacticalDeathStar::IsOperational())
    {
      asDeathStar(deathStar)->m_fired = 1;
    }
    auto fleetEntry = m_fleet.Entries().begin();
    while (fleetEntry != m_fleet.Entries().end())
    {
      auto current = fleetEntry++;
      auto* node = static_cast<UnitRefNode*>(current->get());
      if (node->Value()->TacticalShipBase::UpdateHyperdriveReady() == 1)
      {
        GetHeadlessSimulator()->HeadlessSimulator::DockAtDeathStar(node->Value());
      }
      else
      {
        GetHeadlessSimulator()->HeadlessSimulator::RetireUnit(node->Value());
      }
    }
    m_state = kStateWithdrawing;
  }

  int active = 0;
  for (const auto& entry : m_fleet.Entries())
  {
    auto* node = static_cast<UnitRefNode*>(entry.get());
    if (node->Value()->m_state == kStateActive)
      ++active;
    node->Value()->TacticalShipBase::Recharge(step);
  }
  for (const auto& entry : m_hangars.Entries())
  {
    auto* node = static_cast<UnitRefNode*>(entry.get());
    if (node->Value()->m_state == kStateActive)
      ++active;
    node->Value()->TacticalShipBase::Recharge(step);
  }
  if (active != 0)
    return 0;
  if (SideIndex() != 1 || !deathStar)
    return 1;
  if (static_cast<TacticalDeathStar*>(deathStar)->TacticalDeathStar::IsOperational() != 0)
    return 0;
  return 1;
}

TacticalVector g_formationOrigin{};
