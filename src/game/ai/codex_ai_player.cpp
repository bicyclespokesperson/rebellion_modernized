#include "ai/codex_ai_player.h"
#include "ai/codex_ai_assault.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>

#include "ai/ai_order_util.h"
#include "economy/defense_facility.h"
#include "events/game_clock_hooks.h"
#include "fleet/fleet.h"
#include "fleet/troop_reg.h"
#include "galaxy/system_rules_1e.h"
#include "gameobj/object_lookup.h"

namespace
{
constexpr uint32_t kFriendly = 1;
constexpr uint32_t kInTransit = 2;
constexpr uint32_t kEnemy = 4;
constexpr uint32_t kDestroyed = 0x10;
constexpr uint32_t kEnemyFleet = 0x200;
constexpr uint32_t kExplored = 0x8000000;
constexpr uint32_t kHeadquarters = 0x80000000;
constexpr uint32_t kCoruscant = 0x40000000;
constexpr int kAssault = 0x234;
constexpr int kBombardTroops = 0x220;
constexpr int kBombardDefenses = 0x222;

bool IsOurFleet(AiFleetEval& fleet)
{
  return (fleet.m_flags.m_value & kFriendly) != 0 && fleet.Stat(0x50) > 0;
}

bool CarriesHeadquarters(AiFleetEval& fleet)
{
  const ObjectKey fleetKey = ai::KeyOfNode(&fleet);
  auto* headquarters = GetFirstObjectInFamilies_20_22(fleet.m_view);
  for (auto* parent = headquarters ? headquarters->m_parent : nullptr; parent; parent = parent->m_parent) {
    if (ai::KeyOfObject(parent).m_value == fleetKey.m_value) {
      return true;
    }
  }
  return false;
}

bool CanDeploy(AiFleetEval& fleet)
{
  return IsOurFleet(fleet) && !CarriesHeadquarters(fleet) && (fleet.m_flags.m_value & (kInTransit | 0x800)) == 0
         && ai::IsSystemKey(fleet.m_systemKey);
}

int Bombardment(AiFleetEval& fleet)
{
  const ObjectKey key = ai::KeyOfNode(&fleet);
  auto* object = static_cast<Fleet*>(GetObjectInFamilies_08_10(fleet.m_view, &key));
  return object ? object->SumUnitStat119() : 0;
}

int EnemyStrength(AiSystemEval& system)
{
  int strength = 0;
  if ((system.m_flags30.m_value & kEnemyFleet) != 0) {
    strength += system.Stat(0xe4);
  }
  if ((system.m_flags30.m_value & kEnemy) != 0) {
    strength += system.Stat(0xb0);
  }
  return std::max(0, strength);
}

bool HasSpaceSuperiority(AiFleetEval& fleet, AiSystemEval& system)
{
  const int64_t strength = fleet.Stat(0x58);
  const int64_t opposition = EnemyStrength(system);
  return strength > 0 && strength * 10 >= opposition * 14;
}

int64_t GroundDefense(AiSystemEval& system)
{
  const ObjectKey key = ai::KeyOfNode(&system);
  auto* object = GetObjectInFamilies_90_98(system.m_view, &key);
  if (!object) {
    return std::numeric_limits<int>::max();
  }
  int64_t defense = 0;
  TroopRegIterator troops(object, 1, system.m_x20);
  for (troops.Begin(); troops.m_current; troops.Advance()) {
    defense += static_cast<TroopReg*>(troops.m_current)->GetDefenseStrength();
  }
  return defense;
}

bool HasGroundSuperiority(AiFleetEval& fleet, AiSystemEval& system)
{
  return fleet.Stat(0x80) > 0 && fleet.Stat(0x6c) * int64_t{10} > GroundDefense(system) * 15;
}

bool CanBreach(AiFleetEval& fleet, AiSystemEval& system)
{
  const ObjectKey key = ai::KeyOfNode(&system);
  auto* object = static_cast<GameObj*>(GetObjectInFamilies_90_98(system.m_view, &key));
  const bool assaultAllowed = object && CountDefenseFacilities24(object, system.m_x20, 1) < GetParameter1E03();
  return (system.m_flags30.m_value & kEnemy) == 0 || (assaultAllowed && HasGroundSuperiority(fleet, system))
         || Bombardment(fleet) > system.Stat(0x68);
}

bool IsObjective(AiSystemEval& system)
{
  return (system.m_flags30.m_value & kDestroyed) == 0 && (system.m_flags2c.m_value & kExplored) != 0
         && (system.m_flags30.m_value & (kEnemy | kEnemyFleet)) != 0;
}

int Distance(AiSystemEval& first, AiSystemEval& second)
{
  const int dx = static_cast<int16_t>(first.m_position) - static_cast<int16_t>(second.m_position);
  const int dy = static_cast<int16_t>(first.m_position >> 16) - static_cast<int16_t>(second.m_position >> 16);
  return static_cast<int>(std::sqrt(static_cast<double>(dx) * dx + static_cast<double>(dy) * dy));
}

bool IsVictoryObjective(AiSystemEval& system)
{
  return (system.m_flags30.m_value & kEnemy) != 0 && (system.m_flags30.m_value & (kHeadquarters | kCoruscant)) != 0;
}
} // namespace

void CodexAiPlayer::Read(istream* in)
{
  AiPlayer::Read(in);
  m_flagship.Read(in);
  m_objective.Read(in);
  m_reinforcement.Read(in);
  ReadValue32(in, &m_nextPlanDay);
  ReadValue32(in, &m_objectiveDay);
}

void CodexAiPlayer::Write(ostream* out)
{
  AiPlayer::Write(out);
  m_flagship.Write(out);
  m_objective.Write(out);
  m_reinforcement.Write(out);
  WriteValue32(out, &m_nextPlanDay);
  WriteValue32(out, &m_objectiveDay);
}

int CodexAiPlayer::StepPlayer()
{
  // Plan only after the side's analysis has settled, without starving its staff.
  if (m_state == 3 && m_currentTask == nullptr && m_ordersSent < m_ordersPerBudget && GameHookDay() >= m_nextPlanDay) {
    m_nextPlanDay = GameHookDay() + 3;
    PlanCampaign();
  }
  return AiPlayer::StepPlayer();
}

bool CodexAiPlayer::AcceptTaskOrder(OrderBase* order)
{
  if (!m_objective.IsComplete()) {
    return true;
  }
  for (const auto& node : order->GetTeam()->Entries()) {
    const ObjectKey key = ai::KeyOfNode(node.get());
    if (key.m_value == m_flagship.m_value || key.m_value == m_reinforcement.m_value) {
      return false;
    }
    if (key.Family() >= 0x10 && key.Family() < 0x20) {
      auto* object = GetObjectInFamilies_01_ff(m_side, &key);
      for (auto* parent = object ? object->m_parent : nullptr; parent; parent = parent->m_parent) {
        const ObjectKey parentKey = ai::KeyOfObject(parent);
        if (parentKey.m_value == m_flagship.m_value || parentKey.m_value == m_reinforcement.m_value) {
          return false;
        }
      }
    }
  }
  return true;
}

bool CodexAiPlayer::IssueOrder(int kind, const ObjectKey& unit, const ObjectKey& target)
{
  ObjectKeyValueSet team;
  ai::AddToTeam(&team, unit);
  std::unique_ptr<OrderBase> order(ai::NewOrder(kind, m_side, &team, &target));
  StatusCode status;
  if (!order || order->CheckValid(&status) == 0) {
    return false;
  }
  if (std::getenv("REB_AI_TRACE")) {
    std::fprintf(stderr, "[codex] day=%d side=%d order=%x unit=%08x target=%08x\n", GameHookDay(), m_side, kind,
                 unit.m_value, target.m_value);
  }
  Send(order.release());
  ++m_ordersSent;
  return true;
}

AiFleetEval* CodexAiPlayer::ChooseFlagship()
{
  auto* current = m_model.m_fleets.FindEval(m_flagship);
  const bool keepCurrent = current && IsOurFleet(*current) && current->Stat(0x58) > 0 && !CarriesHeadquarters(*current);
  if (keepCurrent && (current->m_flags.m_value & kInTransit) != 0) {
    return current;
  }
  AiFleetEval* best = nullptr;
  for (const auto& node : m_model.m_fleets.Entries()) {
    auto& fleet = *static_cast<AiFleetEval*>(node.get());
    if (CanDeploy(fleet) && (!best || fleet.Stat(0x58) > best->Stat(0x58))) {
      best = &fleet;
    }
  }
  if (keepCurrent && (!best || best->Stat(0x58) <= int64_t{2} * current->Stat(0x58))) {
    return current;
  }
  m_flagship = best ? ai::KeyOfNode(best) : ObjectKey();
  m_reinforcement.Reset();
  m_objective.Reset();
  return best;
}

AiSystemEval* CodexAiPlayer::ChooseObjective(AiFleetEval& flagship)
{
  auto* origin = m_model.m_systems.FindEval(flagship.m_systemKey);
  if (!origin) {
    return nullptr;
  }
  for (const auto& node : m_model.m_systems.Entries()) {
    auto& system = *static_cast<AiSystemEval*>(node.get());
    if ((system.m_flags30.m_value & (kFriendly | kEnemyFleet)) == (kFriendly | kEnemyFleet)
        && (system.m_flags30.m_value & (kHeadquarters | kCoruscant)) != 0) {
      m_objective = ai::KeyOfNode(&system);
      m_objectiveDay = GameHookDay();
      return &system;
    }
  }
  auto* current = m_model.m_systems.FindEval(m_objective);
  if (current && IsObjective(*current) && GameHookDay() - m_objectiveDay < 180) {
    return current;
  }

  int64_t totalStrength = 0;
  int64_t totalBombardment = 0;
  for (const auto& node : m_model.m_fleets.Entries()) {
    auto& fleet = *static_cast<AiFleetEval*>(node.get());
    if (IsOurFleet(fleet) && !CarriesHeadquarters(fleet)) {
      totalStrength += fleet.Stat(0x58);
      totalBombardment += Bombardment(fleet);
    }
  }
  AiSystemEval* best = nullptr;
  int64_t bestScore = std::numeric_limits<int64_t>::min();
  for (const auto& node : m_model.m_systems.Entries()) {
    auto& system = *static_cast<AiSystemEval*>(node.get());
    if (!IsObjective(system) || totalStrength * 7 < static_cast<int64_t>(EnemyStrength(system)) * 14) {
      continue;
    }
    if (system.Stat(0x64) >= 2 && totalBombardment <= system.Stat(0x68)) {
      continue;
    }
    const bool defendedHome = (system.m_flags30.m_value & kFriendly) != 0;
    int64_t value = 2000 + system.Stat(0xcc) * 3 + system.Stat(0x50) * 100 + system.Stat(0x54) * 100;
    if (IsVictoryObjective(system)) {
      value += 20000;
    }
    if (defendedHome) {
      value += (system.m_flags30.m_value & (kHeadquarters | kCoruscant)) != 0 ? 40000 : 4000;
    }
    const int64_t score
        = value * 100 / (100 + Distance(*origin, system)) + (HasSpaceSuperiority(flagship, system) ? 1200 : 0);
    if (score > bestScore) {
      bestScore = score;
      best = &system;
    }
  }
  m_objective = best ? ai::KeyOfNode(best) : ObjectKey();
  m_objectiveDay = GameHookDay();
  return best;
}

bool CodexAiPlayer::Reinforce(AiFleetEval& flagship, AiSystemEval& target)
{
  auto* origin = m_model.m_systems.FindEval(flagship.m_systemKey);
  if (!origin || (origin->m_flags30.m_value & kFriendly) == 0) {
    return false;
  }
  auto* inbound = m_model.m_fleets.FindEval(m_reinforcement);
  if (inbound && IsOurFleet(*inbound) && (inbound->m_flags.m_value & kInTransit) != 0) {
    return true;
  }
  m_reinforcement.Reset();
  AiFleetEval* best = nullptr;
  int64_t bestScore = 0;
  for (const auto& node : m_model.m_fleets.Entries()) {
    auto& fleet = *static_cast<AiFleetEval*>(node.get());
    if (!CanDeploy(fleet) || ai::KeyOfNode(&fleet).m_value == m_flagship.m_value) {
      continue;
    }
    auto* system = m_model.m_systems.FindEval(fleet.m_systemKey);
    if (!system || (system->m_flags30.m_value & kFriendly) == 0 || EnemyStrength(*system) > 0) {
      continue;
    }
    if ((system->m_flags30.m_value & (kHeadquarters | kCoruscant)) != 0) {
      continue;
    }
    const int64_t value
        = fleet.Stat(0x58) + fleet.Stat(0x80) * 500 + (!CanBreach(flagship, target) ? Bombardment(fleet) * 500 : 0);
    const int64_t score = value * 100 / (100 + Distance(*origin, *system));
    if (score > bestScore) {
      bestScore = score;
      best = &fleet;
    }
  }
  if (best && IssueOrder(ai::kMoveOrder, ai::KeyOfNode(best), m_flagship)) {
    m_reinforcement = ai::KeyOfNode(best);
    return true;
  }
  return false;
}

bool CodexAiPlayer::LoadTroops(AiFleetEval& flagship, AiSystemEval& system, int wanted)
{
  if (flagship.Stat(0xa8) <= 0 || flagship.Stat(0x80) >= wanted) {
    return false;
  }
  AiSystemEval* source = nullptr;
  int nearest = std::numeric_limits<int>::max();
  for (const auto& node : m_model.m_systems.Entries()) {
    auto& candidate = *static_cast<AiSystemEval*>(node.get());
    const bool home = (candidate.m_flags30.m_value & (kHeadquarters | kCoruscant)) != 0;
    const int reserve = std::max(home ? 6 : 2, candidate.Stat(0x98) + 1);
    if ((candidate.m_flags30.m_value & kFriendly) == 0 || EnemyStrength(candidate) > 0
        || candidate.Stat(0x90) <= reserve) {
      continue;
    }
    const int distance = Distance(system, candidate);
    if (distance < nearest) {
      source = &candidate;
      nearest = distance;
    }
  }
  if (source) {
    const ObjectKey sourceKey = ai::KeyOfNode(source);
    auto* object = GetObjectInFamilies_90_98(m_side, &sourceKey);
    if (!object) {
      return false;
    }
    TroopRegIterator troops(object, 1);
    for (troops.Begin(); troops.m_current; troops.Advance()) {
      if (IssueOrder(ai::kMoveOrder, ai::KeyOfObject(troops.m_current), m_flagship)) {
        return true;
      }
    }
  }
  return false;
}

bool CodexAiPlayer::Invade(AiFleetEval& flagship, AiSystemEval& system)
{
  if ((system.m_flags30.m_value & kEnemy) == 0) {
    return false;
  }
  const ObjectKey systemKey = ai::KeyOfNode(&system);
  // Ground combat superiority alone is not enough to hold a newly captured system. Match the troop
  // reserve calculated by the campaign planner so the AI does not launch an uprising-prone assault.
  if (ai::CanLaunchOccupation(flagship.Stat(0x80), system.Stat(0x118), HasGroundSuperiority(flagship, system))
      && IssueOrder(kAssault, m_flagship, systemKey)) {
    return true;
  }
  if (Bombardment(flagship) > system.Stat(0x68)) {
    if ((system.Stat(0x64) > 0 || system.Stat(0x74) > 0) && IssueOrder(kBombardDefenses, m_flagship, systemKey)) {
      return true;
    }
    if (GroundDefense(system) > 0 && IssueOrder(kBombardTroops, m_flagship, systemKey)) {
      return true;
    }
  }
  return false;
}

void CodexAiPlayer::PlanCampaign()
{
  auto* flagship = ChooseFlagship();
  if (!flagship || !CanDeploy(*flagship)) {
    return;
  }
  auto* target = ChooseObjective(*flagship);
  if (!target) {
    return;
  }
  auto* origin = m_model.m_systems.FindEval(flagship->m_systemKey);
  if (!origin) {
    return;
  }
  if (std::getenv("REB_AI_TRACE") && GameHookDay() % 30 < 3) {
    std::fprintf(stderr,
                 "[codex-plan] day=%d side=%d at=%08x objective=%08x strength=%d enemy=%d troops=%d attack=%d "
                 "shields=%d bombard=%d need=%d\n",
                 GameHookDay(), m_side, flagship->m_systemKey.m_value, m_objective.m_value, flagship->Stat(0x58),
                 EnemyStrength(*target), flagship->Stat(0x80), flagship->Stat(0x6c), target->Stat(0x68),
                 Bombardment(*flagship), target->Stat(0x118));
  }
  if (flagship->m_systemKey.m_value == m_objective.m_value) {
    if (Invade(*flagship, *target)) {
      return;
    }
  } else if ((origin->m_flags30.m_value & kFriendly) != 0) {
    const int troopsWanted = ai::RequiredOccupationTroops(target->Stat(0x118));
    if (LoadTroops(*flagship, *origin, troopsWanted)) {
      return;
    }
    if ((!HasSpaceSuperiority(*flagship, *target) || !CanBreach(*flagship, *target)
         || flagship->Stat(0x80) < troopsWanted)
        && Reinforce(*flagship, *target)) {
      return;
    }
  }
  if (flagship->m_systemKey.m_value != m_objective.m_value && HasSpaceSuperiority(*flagship, *target)
      && CanBreach(*flagship, *target)) {
    IssueOrder(ai::kMoveOrder, m_flagship, m_objective);
    return;
  }
  if ((origin->m_flags30.m_value & kFriendly) != 0) {
    // Release an unproductive campaign so the staff can develop and deploy forces.
    m_objective.Reset();
    return;
  }
  // Refit at the closest safe friendly port when an occupation needs reinforcements.
  if ((origin->m_flags30.m_value & kFriendly) == 0) {
    AiSystemEval* port = nullptr;
    int bestDistance = std::numeric_limits<int>::max();
    for (const auto& node : m_model.m_systems.Entries()) {
      auto& system = *static_cast<AiSystemEval*>(node.get());
      if ((system.m_flags30.m_value & kFriendly) == 0 || EnemyStrength(system) != 0) {
        continue;
      }
      const int distance = Distance(*origin, system);
      if (distance < bestDistance) {
        port = &system;
        bestDistance = distance;
      }
    }
    if (port) {
      IssueOrder(ai::kMoveOrder, m_flagship, ai::KeyOfNode(port));
    }
  }
}
