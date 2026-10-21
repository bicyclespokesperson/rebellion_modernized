#include "ai/claude_ai_player.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

#include "ai/ai_class_records.h"
#include "ai/ai_common.h"
#include "ai/ai_order_util.h"
#include "ai/ai_records.h"
#include "ai/ai_task_util.h"
#include "economy/defense_facility.h"
#include "economy/facility.h"
#include "economy/manufacturing_facility.h"
#include "economy/production_facility_types.h"
#include "economy/side.h"
#include "events/game_clock_hooks.h"
#include "fleet/capship.h"
#include "fleet/fight_squad.h"
#include "fleet/fleet.h"
#include "fleet/fleet_iterator.h"
#include "fleet/troop_reg.h"
#include "galaxy/system.h"
#include "galaxy/system_neighbours.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "gameobj/status_code.h"
#include "orders/order_base.h"
#include "rules/loyalty_params.h"

namespace
{

constexpr uint32_t kCoruscant = 0x90000109;
constexpr uint32_t kYavin = 0x92000121;
constexpr uint32_t kHeadquartersKey = 0x20000005;
constexpr uint32_t kFleetBlockade = 0x20;
constexpr uint32_t kStateUsable = 0x1;
constexpr uint32_t kStateDestroyed = 0x8;
constexpr uint32_t kStateEnroute = 0x10;
constexpr int kOrderBombard = 0x220;
constexpr int kOrderDestroyPlanet = 0x223;
constexpr int kOrderAssault = 0x234;
constexpr int kOrderMove = 0x201;
constexpr int kCapitalGarrison = 8;
constexpr int kCommanderOrdersPerDay = 6;
constexpr int kShieldsBlockingAssault = 2;
// Share of the side's fleet attack the commander takes; the rest stays with the staff for defense and expansion.
constexpr double kClaimShare = 0.8;
constexpr double kStrikeMargin = 1.8;
constexpr double kSiegeMargin = 1.5;
constexpr int kGatherPatienceDays = 45;
constexpr float kWithdrawRatio = 0.35f;
constexpr int kSiegePatienceDays = 40;

// REB_CLAUDE_OFF=name,name... switches commander features off for experiments.
bool FeatureOff(const char* name)
{
  const char* off = std::getenv("REB_CLAUDE_OFF");
  if (!off) {
    return false;
  }
  const std::string list = std::string(",") + off + ",";
  return list.find(std::string(",") + name + ",") != std::string::npos;
}

bool SameKey(const ObjectKey& a, const ObjectKey& b)
{
  return a.m_value == b.m_value;
}

bool IsSet(const ObjectKey& key)
{
  return key.IsComplete();
}

ObjectKey KeyOf(GameObjNotifier* object)
{
  ObjectKey key;
  GetObjectKey(object, &key);
  return key;
}

uint32_t StateFlags(GameObjNotifier* object)
{
  return static_cast<GameObj*>(object)->m_stateFlags;
}

MapPosition PositionOf(System* system)
{
  uint32_t packed = 0;
  system->GetRecordField4c(&packed);
  return MapPosition{static_cast<int16_t>(packed & 0xffff), static_cast<int16_t>(packed >> 16)};
}

GameObjNotifier* FleetOf(GameObjNotifier* object)
{
  for (int depth = 0; object && depth < 4; ++depth) {
    const uint32_t family = KeyOf(object).Family();
    if (ai::InFamilies(family, ai::kFleetFamilyFirst, ai::kFleetFamilyLast)) {
      return object;
    }
    if (ai::InFamilies(family, 0x80, 0x98)) {
      return nullptr;
    }
    object = object->m_parent;
  }
  return nullptr;
}

int TroopDefense(AiClassTable& classes, GameObjNotifier* troop);

// Upkeep of every unit below `parent` by family bucket: troops, capital ships, fighters, defenses, factories,
// mines/refineries, characters.
void AddUpkeep(AiClassTable& classes, GameObjNotifier* parent, uint32_t side, int (&upkeep)[7])
{
  static constexpr FamilyRange kWalk(0x08, 0x40);
  ChildIterator children(parent, side, &kWalk, 3);
  for (children.Begin(); children.Current(); children.Advance()) {
    const uint32_t family = KeyOf(children.Current()).Family();
    if (family >= 0x10) {
      ObjectKey recordKey;
      GetObjectRecordKey(children.Current(), &recordKey);
      const AiClassRecordBase* record = classes.Find(&recordKey);
      const int cost = record ? record->m_valueB : 0;
      const int bucket = family < 0x14 ? 0 : family < 0x1c ? 1 : family < 0x20 ? 2 : family < 0x28 ? 3
                       : family < 0x2c ? 4 : family < 0x30 ? 5 : 6;
      upkeep[bucket] += cost;
    }
    if (family < 0x1c) {
      AddUpkeep(classes, children.Current(), side, upkeep);
    }
  }
}

int TroopDefense(AiClassTable& classes, GameObjNotifier* troop)
{
  ObjectKey recordKey;
  GetObjectRecordKey(troop, &recordKey);
  auto* troopClass = static_cast<AiTroopClass*>(classes.Find(&recordKey));
  return troopClass ? troopClass->m_defenseStrength : 4;
}

} // namespace

// ---- persistence ------------------------------------------------------------------------------------------------

namespace
{

void WriteKey(ostream* out, const ObjectKey& key)
{
  WriteValue32(out, &key.m_value);
}

void ReadKey(istream* in, ObjectKey* key)
{
  ReadValue32(in, &key->m_value);
}

void WriteDayMap(ostream* out, const std::map<uint32_t, int>& map)
{
  const auto count = static_cast<uint32_t>(map.size());
  WriteValue32(out, &count);
  for (const auto& [key, day] : map) {
    WriteValue32(out, &key);
    WriteValue32(out, &day);
  }
}

void ReadDayMap(istream* in, std::map<uint32_t, int>* map)
{
  uint32_t count = 0;
  ReadValue32(in, &count);
  map->clear();
  for (uint32_t index = 0; index < count; ++index) {
    uint32_t key = 0;
    int day = 0;
    ReadValue32(in, &key);
    ReadValue32(in, &day);
    (*map)[key] = day;
  }
}

constexpr uint32_t kSaveVersion = 1;

} // namespace

void ClaudeAiPlayer::Read(istream* in)
{
  AiPlayer::Read(in);
  uint32_t version = 0;
  ReadValue32(in, &version);
  ReadKey(in, &m_flagship);
  ReadKey(in, &m_target);
  ReadKey(in, &m_scout);
  ReadKey(in, &m_scoutTarget);
  ReadKey(in, &m_knownHeadquarters);
  ReadValue32(in, &m_phase);
  ReadValue32(in, &m_phaseDay);
  ReadValue32(in, &m_lastSiegeOrderDay);
  ReadValue32(in, &m_lastPlanDay);
  ReadDayMap(in, &m_avoidUntil);
  ReadDayMap(in, &m_visitedDay);
  ReadValue32(in, &m_strongestEnemyAttack);
  ReadValue32(in, &m_strongestEnemyHull);
  ReadValue32(in, &m_lastScoutLossDay);
  ReadValue32(in, &m_lastHeadquartersMoveDay);
  ReadDayMap(in, &m_enemySeenDay);
  ReadDayMap(in, &m_capturedDay);
}

void ClaudeAiPlayer::Write(ostream* out)
{
  AiPlayer::Write(out);
  WriteValue32(out, &kSaveVersion);
  WriteKey(out, m_flagship);
  WriteKey(out, m_target);
  WriteKey(out, m_scout);
  WriteKey(out, m_scoutTarget);
  WriteKey(out, m_knownHeadquarters);
  WriteValue32(out, &m_phase);
  WriteValue32(out, &m_phaseDay);
  WriteValue32(out, &m_lastSiegeOrderDay);
  WriteValue32(out, &m_lastPlanDay);
  WriteDayMap(out, m_avoidUntil);
  WriteDayMap(out, m_visitedDay);
  WriteValue32(out, &m_strongestEnemyAttack);
  WriteValue32(out, &m_strongestEnemyHull);
  WriteValue32(out, &m_lastScoutLossDay);
  WriteValue32(out, &m_lastHeadquartersMoveDay);
  WriteDayMap(out, m_enemySeenDay);
  WriteDayMap(out, m_capturedDay);
}

// ---- turn integration -------------------------------------------------------------------------------------------

int ClaudeAiPlayer::StepPlayer()
{
  if (m_state == 3 && m_currentTask == nullptr && GameHookDay() != m_lastPlanDay) {
    m_lastPlanDay = GameHookDay();
    m_ordersThisDay = 0;
    Command();
  }
  return AiPlayer::StepPlayer();
}

void ClaudeAiPlayer::BeginVote(int id, ObjectKey* key)
{
  AiPlayer::BeginVote(id, key);
  if (!key || !ai::IsSystemKey(*key)) {
    return;
  }
  // The vote is the strength ratio below which our ships withdraw from the battle. The staff votes up to 0.9 and
  // ignores planetary fighters, which makes winning fleets flee; we commit, and never leave the capital.
  static const bool keepStaffVote = FeatureOff("vote");
  const SystemInfo* info = FindSystem(*key);
  if (!keepStaffVote) {
    m_voteTime.m_value = info && IsOwnCapital(*info) ? 0.0f : kWithdrawRatio;
  }
  static const bool tracing = std::getenv("REB_AI_TRACE") != nullptr;
  if (!tracing) {
    return;
  }
  auto* system = static_cast<System*>(GetObjectInFamilies_90_98(m_side, key));
  if (!system) {
    return;
  }
  BattleProbe probe;
  probe.day = GameHookDay();
  CountForces(system, &probe.own, &probe.enemy);
  m_battles[key->m_value] = probe;
}

void ClaudeAiPlayer::CountForces(System* system, SideCount* own, SideCount* enemy)
{
  FleetIterator fleets(system, 3);
  for (fleets.Begin(); fleets.Current(); fleets.Advance()) {
    SideCount* count = fleets.Current()->ControlSide() == static_cast<uint32_t>(m_side) ? own : enemy;
    if (AiFleetEval* eval = m_model.m_fleets.FindEval(KeyOf(fleets.Current()))) {
      count->readyAttack += eval->Stat(0x58);
      count->attack += eval->Stat(0xb4);
      count->hull += eval->Stat(0x54);
    }
    CapShipIterator ships(fleets.Current(), 3);
    for (ships.Begin(); ships.Current(); ships.Advance()) {
      ++count->ships;
      FightSquadIterator squadrons(ships.Current(), 3);
      count->squadrons += squadrons.Count();
    }
  }
}

void ClaudeAiPlayer::ReportBattles()
{
  for (auto position = m_battles.begin(); position != m_battles.end();) {
    if (GameHookDay() - position->second.day < 2) {
      ++position;
      continue;
    }
    ObjectKey key;
    key.m_value = position->first;
    SideCount own;
    SideCount enemy;
    if (auto* system = static_cast<System*>(GetObjectInFamilies_90_98(m_side, &key))) {
      CountForces(system, &own, &enemy);
    }
    const BattleProbe& before = position->second;
    Trace("battle at %08x: own %.0f(%.0f)/%.0f %d ships %d sq vs enemy %.0f(%.0f)/%.0f %d ships %d sq -> own %.0f/%.0f "
          "%d ships %d sq, enemy %.0f/%.0f %d ships",
          key.m_value, before.own.readyAttack, before.own.attack, before.own.hull, before.own.ships,
          before.own.squadrons, before.enemy.readyAttack, before.enemy.attack, before.enemy.hull, before.enemy.ships,
          before.enemy.squadrons, own.attack, own.hull, own.ships, own.squadrons, enemy.attack, enemy.hull,
          enemy.ships);
    position = m_battles.erase(position);
  }
}

bool ClaudeAiPlayer::AcceptTaskOrder(OrderBase* order)
{
  for (const auto& node : order->m_team.Entries()) {
    ObjectKey key;
    node->GetKey(&key);
    if (key.m_value == kHeadquartersKey) {
      return false;
    }
  }
  return !OrderTouchesClaimedFleet(order);
}

bool ClaudeAiPlayer::OrderTouchesClaimedFleet(OrderBase* order) const
{
  if (m_claimed.empty()) {
    return false;
  }
  for (const auto& node : order->m_team.Entries()) {
    ObjectKey key;
    node->GetKey(&key);
    GameObjNotifier* object = nullptr;
    const uint32_t family = key.Family();
    if (ai::InFamilies(family, ai::kFleetFamilyFirst, ai::kFleetFamilyLast)) {
      object = GetObjectInFamilies_08_10(m_side, &key);
    } else if (ai::InFamilies(family, 0x10, 0x40)) {
      object = GetObjectInFamilies_01_ff(m_side, &key);
    }
    GameObjNotifier* fleet = object ? FleetOf(object) : nullptr;
    if (fleet && IsClaimed(KeyOf(fleet))) {
      return true;
    }
  }
  return false;
}

bool ClaudeAiPlayer::Every(int* lastDay, int days) const
{
  if (GameHookDay() - *lastDay < days) {
    return false;
  }
  *lastDay = GameHookDay();
  return true;
}

bool ClaudeAiPlayer::IsBenched(const ObjectKey& unit, int refusals) const
{
  const auto found = m_refusals.find(unit.m_value);
  return found != m_refusals.end() && found->second.count >= refusals && GameHookDay() - found->second.day < 30;
}

bool ClaudeAiPlayer::IsClaimed(const ObjectKey& fleet) const
{
  return std::find(m_claimed.begin(), m_claimed.end(), fleet.m_value) != m_claimed.end();
}

bool ClaudeAiPlayer::Issue(int kind, std::vector<ObjectKey> team, const ObjectKey* target, const char* what)
{
  if (team.empty() || m_ordersThisDay >= kCommanderOrdersPerDay || m_ordersSent >= m_ordersPerBudget) {
    return false;
  }
  ObjectKeyValueSet keys;
  for (const auto& key : team) {
    ai::AddToTeam(&keys, key);
  }
  OrderBase* order = target ? ai::NewOrder(kind, m_side, &keys, target) : aitask::NewTeamOrder(kind, m_side, &keys);
  if (!order) {
    return false;
  }
  StatusCode status;
  if (order->CheckValid(&status) == 0) {
    // Only refused moves mean a unit is stuck; refused sieges just wait for troops or a blockade.
    int count = 1;
    if (kind == kOrderMove) {
      Refusals& refusals = m_refusals[team.front().m_value];
      count = ++refusals.count;
      refusals.day = GameHookDay();
    }
    if (count <= 3 || count % 20 == 0) {
      Trace("refused %s (kind %x, %zu units, first %08x, target %08x): status %d/%d (%d times)", what, kind, team.size(),
            team.front().m_value, target ? target->m_value : 0, static_cast<int>(status.m_first),
            static_cast<int>(status.m_second), count);
    }
    delete order;
    return false;
  }
  m_refusals.erase(team.front().m_value);
  Trace("order %s (kind %x, %zu units, first %08x, target %08x)", what, kind, team.size(), team.front().m_value,
        target ? target->m_value : 0);
  Send(order);
  ++m_ordersSent;
  ++m_ordersThisDay;
  return true;
}

void ClaudeAiPlayer::Trace(const char* format, ...) const
{
  static const bool enabled = std::getenv("REB_AI_TRACE") != nullptr;
  if (!enabled) {
    return;
  }
  std::fprintf(stderr, "[claude side %d day %d] ", m_side, GameHookDay());
  va_list arguments;
  va_start(arguments, format);
  std::vfprintf(stderr, format, arguments);
  va_end(arguments);
  std::fputc('\n', stderr);
}

// ---- the picture of the war -------------------------------------------------------------------------------------

ClaudeAiPlayer::FleetInfo* ClaudeAiPlayer::FindFleet(const ObjectKey& key)
{
  for (auto& info : m_fleets) {
    if (SameKey(info.key, key)) {
      return &info;
    }
  }
  return nullptr;
}

ClaudeAiPlayer::SystemInfo* ClaudeAiPlayer::FindSystem(const ObjectKey& key)
{
  const auto found = m_systems.find(key.m_value);
  return found == m_systems.end() ? nullptr : &found->second;
}

void ClaudeAiPlayer::Survey()
{
  const int view = m_side;
  const auto enemy = static_cast<uint32_t>(Enemy());
  m_fleets.clear();
  m_systems.clear();

  for (const auto& node : m_model.m_fleets.Entries()) {
    auto* eval = static_cast<AiFleetEval*>(node.get());
    if ((eval->m_flags.m_value & 1) == 0) {
      continue;
    }
    FleetInfo info;
    eval->GetKey(&info.key);
    info.fleet = static_cast<Fleet*>(GetObjectInFamilies_08_10(view, &info.key));
    if (!info.fleet || (StateFlags(info.fleet) & kStateDestroyed) != 0
        || info.fleet->ControlSide() != static_cast<uint32_t>(m_side)) {
      continue;
    }
    GameObjNotifier* parent = info.fleet->m_parent;
    if (!parent || !ai::IsSystemKey(KeyOf(parent))) {
      continue;
    }
    info.system = static_cast<System*>(parent);
    info.systemKey = KeyOf(parent);
    info.enroute = (StateFlags(info.fleet) & kStateEnroute) != 0;
    info.force.attack = eval->Stat(0xb4);
    info.force.hull = eval->Stat(0x54);
    info.deathStar = (eval->m_flags.m_value & 0x10) != 0;
    info.bombard = info.fleet->SumUnitStat119();
    CapShipIterator ships(info.fleet, 3);
    for (ships.Begin(); ships.Current(); ships.Advance()) {
      ++info.ships;
      auto* ship = static_cast<CapShip*>(ships.Current());
      TroopRegIterator assigned(ship, 3);
      TroopRegIterator ready(ship, 1);
      info.troops += ready.Count();
      info.troopRoom += std::max(0, ship->GetTroopCapacity() - assigned.Count());
      FightSquadIterator squadrons(ship, 3);
      info.hangarRoom += std::max(0, ship->GetFighterCapacity() - squadrons.Count());
    }
    if (info.ships > 0) {
      m_fleets.push_back(info);
    }
  }

  for (const auto& node : m_model.m_systems.Entries()) {
    auto* eval = static_cast<AiSystemEval*>(node.get());
    SystemInfo info;
    eval->GetKey(&info.key);
    info.system = static_cast<System*>(GetObjectInFamilies_90_98(view, &info.key));
    if (!info.system || (StateFlags(info.system) & kStateDestroyed) != 0) {
      continue;
    }
    System* system = info.system;
    info.controller = static_cast<int>(system->ControlSide());
    info.explored = (system->m_flags & system_flag::kExplored) != 0;
    info.populated = (system->m_flags & system_flag::kPopulated) != 0;
    FleetIterator fleets(system, 3);
    for (fleets.Begin(); fleets.Current(); fleets.Advance()) {
      const uint32_t side = fleets.Current()->ControlSide();
      if (side == static_cast<uint32_t>(m_side)) {
        if ((StateFlags(fleets.Current()) & kStateEnroute) == 0) {
          info.ownFleetPresent = true;
        }
        continue;
      }
      if (side != enemy) {
        continue;
      }
      AiFleetEval* fleetEval = m_model.m_fleets.FindEval(KeyOf(fleets.Current()));
      if (fleetEval && (fleetEval->m_flags.m_value & 0x4000) != 0) {
        info.enemyFleets.attack += fleetEval->Stat(0xb4);
        info.enemyFleets.hull += fleetEval->Stat(0x54);
      }
    }
    if (!info.explored) {
      m_systems[info.key.m_value] = info;
      continue;
    }
    const auto controller = static_cast<uint32_t>(info.controller);
    if (info.controller == static_cast<int>(enemy)) {
      info.planetFighters = eval->Stat(0xb0);
    }
    if (info.controller == 1 || info.controller == 2) {
      TroopRegIterator troops(system, 3, controller);
      for (troops.Begin(); troops.Current(); troops.Advance()) {
        ++info.defenders;
        info.defenderDefense += TroopDefense(m_model.m_classes, troops.Current());
      }
      DefenseFacility22Iterator ionBatteries(system, 3, controller);
      DefenseFacility23Iterator batteries(system, 3);
      info.batteries = ionBatteries.Count();
      for (batteries.Begin(); batteries.Current(); batteries.Advance()) {
        info.batteries += batteries.Current()->ControlSide() == controller ? 1 : 0;
      }
      DefenseFacility24Iterator shields(system, 3, controller);
      for (shields.Begin(); shields.Current(); shields.Advance()) {
        if (StateFlags(shields.Current()) & kStateUsable) {
          ++info.shields;
          info.shieldStrength += static_cast<int>(static_cast<DefenseFacility*>(shields.Current())->GetShieldStrength());
        }
      }
      ManufacturingFacilityIterator factories(system, 3);
      for (factories.Begin(); factories.Current(); factories.Advance()) {
        if (factories.Current()->ControlSide() == controller) {
          ++info.productionFacilities;
        }
      }
      info.resourceFacilities = CountProductionFacilityType2C(system, controller, 3)
                                + CountProductionFacilityType2D(system, controller, 3);
      // Facilities still under construction already hold their energy slot.
      FacilityIterator facilities(system, 3);
      info.freeEnergy = std::min(system->m_energy - system->m_energyAllocated, system->m_energy - facilities.Count());
      info.headquarters = CountAllFacilities(system, 1, 3) != 0;
    }
    if (info.enemyFleets.attack > 0 && (info.ownFleetPresent || info.controller == m_side)) {
      m_enemySeenDay[info.key.m_value] = GameHookDay();
    }
    info.blockadedByUs = (system->m_flags & system_flag::kBlockade) != 0 && info.ownFleetPresent
                         && info.controller != m_side;
    if (info.ownFleetPresent || info.controller == m_side) {
      m_visitedDay[info.key.m_value] = GameHookDay();
      if (info.enemyFleets.Value() > static_cast<double>(m_strongestEnemyAttack) * m_strongestEnemyHull) {
        m_strongestEnemyAttack = static_cast<int>(info.enemyFleets.attack);
        m_strongestEnemyHull = static_cast<int>(info.enemyFleets.hull);
      }
    }
    m_systems[info.key.m_value] = info;
  }

  for (auto position = m_capturedDay.begin(); position != m_capturedDay.end();) {
    const SystemInfo* info = FindSystem(ObjectKey::Make(position->first >> 24, position->first & 0xffffff));
    if (info && info->controller != m_side && GameHookDay() > position->second + 1) {
      Trace("lost conquest %08x after %d days: controller %d, uprising %d, enemy fleets %.0f, support %d",
            position->first, GameHookDay() - position->second, info->controller,
            (info->system->m_flags & system_flag::kUprising) != 0, info->enemyFleets.attack,
            info->system->GetLoyaltyFor(m_side));
      position = m_capturedDay.erase(position);
    } else {
      ++position;
    }
  }
  if (m_side == 2) {
    for (const auto& [key, info] : m_systems) {
      if (info.headquarters && info.controller != 2 && (info.ownFleetPresent || !IsSet(m_knownHeadquarters))) {
        if (!SameKey(m_knownHeadquarters, info.key)) {
          Trace("alliance headquarters seen at %08x", info.key.m_value);
        }
        m_knownHeadquarters = info.key;
      }
    }
    if (const SystemInfo* known = FindSystem(m_knownHeadquarters);
        known && known->ownFleetPresent && !known->headquarters) {
      Trace("alliance headquarters left %08x", known->key.m_value);
      m_knownHeadquarters.Reset();
    }
  }
}

int ClaudeAiPlayer::TravelDaysBetween(const ObjectKey& from, const ObjectKey& to) const
{
  const auto a = m_systems.find(from.m_value);
  const auto b = m_systems.find(to.m_value);
  if (a == m_systems.end() || b == m_systems.end()) {
    return 999;
  }
  const MapPosition pa = PositionOf(a->second.system);
  const MapPosition pb = PositionOf(b->second.system);
  return TravelDays(&pa, &pb);
}

bool ClaudeAiPlayer::IsObjective(const SystemInfo& info) const
{
  if (m_side == 1) {
    return info.key.m_value == kCoruscant;
  }
  return info.controller != 2 && (info.headquarters || SameKey(info.key, m_knownHeadquarters));
}

bool ClaudeAiPlayer::IsOwnCapital(const SystemInfo& info) const
{
  if (m_side == 1) {
    return info.headquarters && info.controller == 1;
  }
  return info.key.m_value == kCoruscant;
}

bool ClaudeAiPlayer::IntelIsStale(const SystemInfo& info) const
{
  if (info.ownFleetPresent || info.controller == m_side) {
    return false;
  }
  const auto seen = m_visitedDay.find(info.key.m_value);
  return seen == m_visitedDay.end() || GameHookDay() - seen->second > 150;
}

const ClaudeAiPlayer::SystemInfo* ClaudeAiPlayer::OwnCapital() const
{
  for (const auto& [key, info] : m_systems) {
    if (info.controller == m_side && IsOwnCapital(info)) {
      return &info;
    }
  }
  return nullptr;
}

ClaudeAiPlayer::Force ClaudeAiPlayer::EnemyForceAt(const SystemInfo& info) const
{
  Force force = info.enemyFleets;
  if (info.controller == Enemy()) {
    // The enemy's home fleet keeps coming back to its capital, so a quiet scouting report there means little.
    // Unseen worlds keep whatever the view last held (often nothing): assume part of the strongest enemy group we
    // have met is there.
    const bool capital = m_side == 1 ? info.key.m_value == kCoruscant : IsObjective(info);
    const bool stale = IntelIsStale(info);
    const double share = capital ? 1.0 : stale ? 0.35 : 0.0;
    force.attack = std::max(force.attack, share * m_strongestEnemyAttack);
    force.hull = std::max(force.hull, share * m_strongestEnemyHull);
    if (capital && stale) {
      const Force potential = PotentialForce();
      force.attack = std::max(force.attack, potential.attack);
      force.hull = std::max(force.hull, potential.hull);
    }
  }
  force.attack += info.planetFighters;
  if (force.attack > 0 && force.hull <= 0) {
    force.hull = 1000;
  }
  return force;
}

ClaudeAiPlayer::Force ClaudeAiPlayer::PotentialForce() const
{
  Force force;
  for (const auto& info : m_fleets) {
    if (IsClaimed(info.key) && !SameKey(info.key, m_scout)) {
      force.Add(info.force);
    }
  }
  return force;
}

int ClaudeAiPlayer::TroopsNeeded(const SystemInfo& info) const
{
  if (info.controller != Enemy()) {
    return 0;
  }
  // Each battery kills one landing regiment; a failed assault still trades regiments, so repeated landings work.
  return info.batteries + info.defenders + 1;
}

bool ClaudeAiPlayer::CanBreakShields(const SystemInfo& info, int bombard) const
{
  return bombard > info.shieldStrength;
}

double ClaudeAiPlayer::TargetValue(const SystemInfo& info) const
{
  double value = 10;
  if (info.populated) {
    value += 10;
  }
  value += 12.0 * info.productionFacilities + 4.0 * info.resourceFacilities;
  if (info.enemyFleets.attack > 0) {
    value += 20;
  }
  if (info.controller == m_side) {
    value += 40;
    // Enemy fleets retake fresh conquests soon after the strike group moves on.
    if (const auto taken = m_capturedDay.find(info.key.m_value);
        taken != m_capturedDay.end() && GameHookDay() - taken->second < 300) {
      value += 60;
    }
    if (IsOwnCapital(info)) {
      value += 400;
    } else if (const SystemInfo* capital = OwnCapital()) {
      // Enemy fleets in our space near the capital are the prelude to an attack on it.
      value += 300.0 * std::max(0.0, 1.0 - TravelDaysBetween(info.key, capital->key) / 25.0);
    }
  }
  if (IsObjective(info)) {
    value += 500;
  } else if (info.controller == Enemy() && info.populated) {
    // Conquests of worlds that hate us revolt and are lost again.
    value *= 0.3 + info.system->GetLoyaltyFor(m_side) / 100.0;
  }
  return value;
}

ObjectKey ClaudeAiPlayer::NearestOwnSystem(const ObjectKey& from, bool needTroops) const
{
  ObjectKey best;
  int bestDays = 1 << 30;
  for (const auto& [key, info] : m_systems) {
    if (info.controller != m_side || info.enemyFleets.attack > 0) {
      continue;
    }
    if (needTroops && info.defenders <= info.system->m_troopRegRequired + 1) {
      continue;
    }
    const int days = TravelDaysBetween(from, info.key);
    if (days < bestDays) {
      bestDays = days;
      best = info.key;
    }
  }
  return best;
}

// ---- command ----------------------------------------------------------------------------------------------------

void ClaudeAiPlayer::Command()
{
  ReportBattles();
  Survey();
  RefreshClaims();
  ChooseFlagship();
  RunStrikeGroup();
  RunScout();
  HideHeadquarters();
  if (Every(&m_lastStatusDay, 25)) {
    const FleetInfo* flagship = FindFleet(m_flagship);
    const Force potential = PotentialForce();
    if (const SystemInfo* capital = OwnCapital()) {
      DefenseFacility24Iterator shields(capital->system, 3);
      Trace("capital %08x: shields %d existing / %d usable, batteries %d, garrison %d, energy %d free %d, fleets %.0f",
            capital->key.m_value, shields.Count(), capital->shields, capital->batteries, capital->defenders,
            capital->system->m_energy, capital->freeEnergy, capital->enemyFleets.attack);
    }
    Trace("status: fleets %zu claimed %zu potential %.0f/%.0f flagship %08x (%.0f/%.0f, troops %d, bombard %d) "
          "target %08x phase %d hq %08x",
          m_fleets.size(), m_claimed.size(), potential.attack, potential.hull, m_flagship.m_value,
          flagship ? flagship->force.attack : 0.0, flagship ? flagship->force.hull : 0.0,
          flagship ? flagship->troops : 0, flagship ? flagship->bombard : 0, m_target.m_value, m_phase,
          m_knownHeadquarters.m_value);
    if (auto* side = static_cast<Side*>(GetSideObject(m_side, m_side))) {
      int ownSystems = 0;
      for (const auto& [key, info] : m_systems) {
        ownSystems += info.controller == m_side ? 1 : 0;
      }
      int upkeep[7] = {};
      for (const auto& [key, info] : m_systems) {
        AddUpkeep(m_model.m_classes, info.system, static_cast<uint32_t>(m_side), upkeep);
      }
      Trace("upkeep: troops %d ships %d fighters %d defenses %d factories %d mines %d people %d", upkeep[0], upkeep[1],
            upkeep[2], upkeep[3], upkeep[4], upkeep[5], upkeep[6]);
      Trace("economy: systems %d maint capacity %u allocated %u required %d raw %d refined %d fleet maint %d troop maint %d",
            ownSystems, side->m_maintState.m_first, side->m_maintState.m_second, side->m_maintRequired,
            side->m_rawMaterials, side->m_refinedMaterials, m_model.Stat(0x24c), m_model.Stat(0x250));
    }
  }
}

void ClaudeAiPlayer::RefreshClaims()
{
  std::vector<const FleetInfo*> combat;
  double total = 0;
  for (const auto& info : m_fleets) {
    if (info.force.attack > 0) {
      combat.push_back(&info);
      total += info.force.attack;
    }
  }
  // Home guard: fleets sitting at the capital stay with the staff until they match half the strongest enemy group
  // seen (and a fifth of our own strength).
  std::vector<uint32_t> guard;
  static const bool noGuard = FeatureOff("guard");
  if (const SystemInfo* capital = noGuard ? nullptr : OwnCapital()) {
    const double wanted = std::max(0.5 * m_strongestEnemyAttack, 0.2 * total);
    std::vector<const FleetInfo*> atCapital;
    for (const FleetInfo* info : combat) {
      if (!info->enroute && SameKey(info->systemKey, capital->key) && !SameKey(info->key, m_flagship)) {
        atCapital.push_back(info);
      }
    }
    std::stable_sort(atCapital.begin(), atCapital.end(),
                     [](const FleetInfo* a, const FleetInfo* b) { return a->force.attack < b->force.attack; });
    double guarded = 0;
    for (const FleetInfo* info : atCapital) {
      if (guarded >= wanted) {
        break;
      }
      guard.push_back(info->key.m_value);
      guarded += info->force.attack;
    }
    total -= guarded;
  }
  std::stable_sort(combat.begin(), combat.end(), [this](const FleetInfo* a, const FleetInfo* b) {
    const bool aFlag = SameKey(a->key, m_flagship);
    const bool bFlag = SameKey(b->key, m_flagship);
    if (aFlag != bFlag) {
      return aFlag;
    }
    return a->force.attack > b->force.attack;
  });
  m_claimed.clear();
  double claimed = 0;
  for (const FleetInfo* info : combat) {
    if (claimed >= kClaimShare * total) {
      break;
    }
    if (std::find(guard.begin(), guard.end(), info->key.m_value) != guard.end()) {
      continue;
    }
    m_claimed.push_back(info->key.m_value);
    claimed += info->force.attack;
  }
  if (IsSet(m_scout) && FindFleet(m_scout)) {
    m_claimed.push_back(m_scout.m_value);
  }
}



void ClaudeAiPlayer::ChooseFlagship()
{
  if (FindFleet(m_flagship) && !IsBenched(m_flagship, 8)) {
    return;
  }
  if (IsSet(m_flagship)) {
    Trace("flagship %08x lost or stuck", m_flagship.m_value);
  }
  const FleetInfo* best = nullptr;
  for (const auto& info : m_fleets) {
    if (!IsClaimed(info.key) || SameKey(info.key, m_scout) || IsBenched(info.key, 3)) {
      continue;
    }
    if (!best || info.force.Value() > best->force.Value()) {
      best = &info;
    }
  }
  if (best) {
    Trace("new flagship %08x (attack %.0f hull %.0f)", best->key.m_value, best->force.attack, best->force.hull);
    m_flagship = best->key;
    SetPhase(kGather);
  } else {
    m_flagship.Reset();
  }
}

void ClaudeAiPlayer::SetPhase(Phase phase)
{
  m_phase = phase;
  m_phaseDay = GameHookDay();
}

void ClaudeAiPlayer::AbandonTarget(int days)
{
  if (IsSet(m_target)) {
    Trace("abandon target %08x for %d days", m_target.m_value, days);
    m_avoidUntil[m_target.m_value] = GameHookDay() + days;
  }
  m_target.Reset();
  SetPhase(kGather);
}

bool ClaudeAiPlayer::PickTarget(FleetInfo& flagship)
{
  const Force potential = PotentialForce();
  const int today = GameHookDay();
  const SystemInfo* best = nullptr;
  double bestScore = 0;
  for (const auto& [key, info] : m_systems) {
    if (!info.explored && !IsObjective(info)) {
      continue;
    }
    const bool enemyHeld = info.controller == Enemy();
    const bool ourContested = info.controller == m_side && info.enemyFleets.attack > 0;
    if (!enemyHeld && !ourContested) {
      continue;
    }
    if (const auto avoid = m_avoidUntil.find(key); avoid != m_avoidUntil.end() && avoid->second > today) {
      continue;
    }
    const Force enemy = EnemyForceAt(info);
    if (potential.Value() < kStrikeMargin * enemy.Value()) {
      continue;
    }
    double value = TargetValue(info);
    if (enemyHeld && info.shields >= kShieldsBlockingAssault && !CanBreakShields(info, flagship.bombard)) {
      const bool deathStarTarget = flagship.deathStar && info.headquarters && m_side == 2;
      if (!deathStarTarget) {
        if (info.enemyFleets.attack <= 0 && !IsObjective(info)) {
          continue;
        }
        value *= IsObjective(info) ? 0.6 : 0.3;
      }
    }
    const int days = TravelDaysBetween(flagship.systemKey, info.key);
    const double odds = enemy.Value() > 0 ? potential.Value() / (potential.Value() + enemy.Value()) : 1.0;
    const double score = value * odds / (8.0 + days);
    if (score > bestScore) {
      bestScore = score;
      best = &info;
    }
  }
  if (!best) {
    return false;
  }
  if (!SameKey(m_target, best->key)) {
    Trace("target %08x (controller %d, enemy %.0f/%.0f, defenders %d, batteries %d, shields %d/%d, hq %d) score %.2f",
          best->key.m_value, best->controller, best->enemyFleets.attack, best->enemyFleets.hull, best->defenders,
          best->batteries, best->shields, best->shieldStrength, best->headquarters, bestScore);
  }
  m_target = best->key;
  return true;
}

void ClaudeAiPlayer::RunStrikeGroup()
{
  FleetInfo* flagship = FindFleet(m_flagship);
  if (!flagship) {
    return;
  }
  SystemInfo* target = FindSystem(m_target);
  if (m_phase == kGather) {
    // Re-plan freely while gathering: the picture changes as the group grows.
    if (!PickTarget(*flagship)) {
      m_target.Reset();
    }
    target = FindSystem(m_target);
    RunGather(*flagship);
    return;
  }
  if (!target) {
    AbandonTarget(30);
    return;
  }
  if (m_phase == kStrike) {
    if (flagship->enroute) {
      return;
    }
    if (SameKey(flagship->systemKey, m_target)) {
      m_strikeFailures = 0;
      SetPhase(kSiege);
    } else {
      Trace("strike on %08x stopped at %08x", m_target.m_value, flagship->systemKey.m_value);
      if (++m_strikeFailures >= 3) {
        m_strikeFailures = 0;
        AbandonTarget(60);
      } else {
        SetPhase(kGather);
      }
      return;
    }
  }
  if (!SameKey(flagship->systemKey, m_target) || flagship->enroute) {
    SetPhase(kGather);
    return;
  }
  RunSiege(*flagship, *target);
}

void ClaudeAiPlayer::CallJoiners(const FleetInfo& flagship, const ObjectKey& rally)
{
  const bool flagshipSettled = !flagship.enroute && SameKey(flagship.systemKey, rally);
  for (const auto& info : m_fleets) {
    if (!IsClaimed(info.key) || SameKey(info.key, flagship.key) || SameKey(info.key, m_scout) || info.enroute
        || IsBenched(info.key, 3)) {
      continue;
    }
    if (SameKey(info.systemKey, rally)) {
      if (flagshipSettled) {
        Issue(kOrderMove, {info.key}, &flagship.key, "merge into flagship");
      }
      continue;
    }
    Issue(kOrderMove, {info.key}, &rally, "join rally");
  }
}

std::vector<ObjectKey> ClaudeAiPlayer::SpareCargo(const SystemInfo& info, Cargo cargo) const
{
  // A freshly conquered world is held by the regiments that took it; taking them away invites an uprising.
  if (const auto taken = m_capturedDay.find(info.key.m_value);
      cargo == Cargo::kTroops && taken != m_capturedDay.end() && GameHookDay() - taken->second < 300) {
    return {};
  }
  const bool capital = IsOwnCapital(info);
  int keep = 0;
  std::vector<ObjectKey> units;
  const auto side = static_cast<uint32_t>(m_side);
  if (cargo == Cargo::kTroops) {
    keep = std::max(info.system->m_troopRegRequired, capital ? kCapitalGarrison : 1);
    TroopRegIterator troops(info.system, 1, side);
    for (troops.Begin(); troops.Current(); troops.Advance()) {
      units.push_back(KeyOf(troops.Current()));
    }
  } else {
    keep = capital ? 4 : 1;
    FightSquadIterator squadrons(info.system, 1, side);
    for (squadrons.Begin(); squadrons.Current(); squadrons.Advance()) {
      units.push_back(KeyOf(squadrons.Current()));
    }
  }
  units.resize(std::max(0, static_cast<int>(units.size()) - keep));
  return units;
}

// Moves spare troops or fighters onto the flagship: from the system it is in, or (`summon`) from other systems up to
// 30 days away. Returns how many units were ordered aboard.
int ClaudeAiPlayer::Embark(FleetInfo& flagship, Cargo cargo, int wanted, bool summon)
{
  int& room = cargo == Cargo::kTroops ? flagship.troopRoom : flagship.hangarRoom;
  if (flagship.enroute || room <= 0 || wanted <= 0) {
    return 0;
  }
  std::vector<std::pair<int, const SystemInfo*>> sources;
  for (const auto& [key, info] : m_systems) {
    if (info.controller != m_side || info.enemyFleets.attack > 0) {
      continue;
    }
    const int days = SameKey(info.key, flagship.systemKey) ? 0 : TravelDaysBetween(info.key, flagship.systemKey);
    if (summon ? days > 0 && days <= 30 : days == 0) {
      sources.emplace_back(days, &info);
    }
  }
  std::stable_sort(sources.begin(), sources.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  int sent = 0;
  int orders = 0;
  for (const auto& [days, info] : sources) {
    if (sent >= wanted || room <= 0 || orders >= 2) {
      break;
    }
    std::vector<ObjectKey> team = SpareCargo(*info, cargo);
    team.resize(std::min({static_cast<int>(team.size()), wanted - sent, room}));
    const int count = static_cast<int>(team.size());
    const char* what = cargo == Cargo::kTroops ? (days == 0 ? "embark troops" : "summon troops")
                                               : (days == 0 ? "embark fighters" : "summon fighters");
    if (count > 0 && Issue(kOrderMove, std::move(team), &flagship.key, what)) {
      sent += count;
      room -= count;
      ++orders;
    }
  }
  return sent;
}

void ClaudeAiPlayer::RunGather(FleetInfo& flagship)
{
  const SystemInfo* here = FindSystem(flagship.systemKey);
  ObjectKey rally = flagship.systemKey;
  if (!here || here->controller != m_side) {
    rally = NearestOwnSystem(flagship.systemKey, false);
  }
  if (!IsSet(m_target)) {
    // Nothing worth attacking: build up at the capital, where the group also deters raids.
    if (const SystemInfo* capital = OwnCapital(); capital && capital->enemyFleets.attack <= 0) {
      rally = capital->key;
    }
  }
  if (!IsSet(rally)) {
    rally = flagship.systemKey;
  }
  if (!flagship.enroute && !SameKey(flagship.systemKey, rally)) {
    Issue(kOrderMove, {flagship.key}, &rally, "flagship to rally");
    return;
  }
  CallJoiners(flagship, rally);
  if (flagship.enroute) {
    return;
  }

  SystemInfo* target = FindSystem(m_target);
  const int wantedTroops = target ? TroopsNeeded(*target) : 4;
  if (flagship.troops < wantedTroops + 2) {
    const int loaded = Embark(flagship, Cargo::kTroops, wantedTroops + 2 - flagship.troops, false);
    if (flagship.troops + loaded < wantedTroops) {
      Embark(flagship, Cargo::kTroops, wantedTroops - flagship.troops - loaded, true);
    }
  }
  Embark(flagship, Cargo::kFighters, flagship.hangarRoom, false);
  Embark(flagship, Cargo::kFighters, flagship.hangarRoom, true);
  if (!target) {
    return;
  }
  const Force gathered = flagship.force;
  const Force enemy = EnemyForceAt(*target);
  const int waited = GameHookDay() - m_phaseDay;
  const bool groundReady = target->controller != Enemy() || flagship.troops >= wantedTroops
                           || target->shields >= kShieldsBlockingAssault;
  const bool strong = gathered.Value() >= kStrikeMargin * enemy.Value();
  const bool patientEnough = waited >= kGatherPatienceDays && gathered.Value() >= kSiegeMargin * enemy.Value();
  if (!((strong && groundReady) || patientEnough)) {
    if (waited > 3 * kGatherPatienceDays) {
      AbandonTarget(60);
    }
    return;
  }
  // Fleets still waiting at the rally are merged first (CallJoiners); the flagship sails alone, so one stale fleet
  // cannot invalidate the whole order.
  if (Issue(kOrderMove, {flagship.key}, &m_target, "strike")) {
    SetPhase(kStrike);
  }
}

void ClaudeAiPlayer::RunSiege(FleetInfo& flagship, SystemInfo& target)
{
  const int today = GameHookDay();
  if (target.controller == m_side && target.enemyFleets.attack <= 0) {
    Trace("secured %08x", target.key.m_value);
    m_capturedDay[target.key.m_value] = GameHookDay();
    m_target.Reset();
    SetPhase(kGather);
    return;
  }
  const Force enemy = EnemyForceAt(target);
  if (target.enemyFleets.attack > 0) {
    if (flagship.force.Value() < 0.8 * enemy.Value()) {
      const ObjectKey home = NearestOwnSystem(flagship.systemKey, false);
      AbandonTarget(90);
      if (IsSet(home)) {
        Issue(kOrderMove, {flagship.key}, &home, "retreat");
      }
    }
    return;
  }
  if (today - m_phaseDay > kSiegePatienceDays) {
    AbandonTarget(90);
    return;
  }
  CallJoiners(flagship, flagship.systemKey);
  if (target.controller != Enemy()) {
    // Neutral now (e.g. the garrison was wiped out): any troops claim it.
    if (flagship.troops > 0 && (flagship.fleet->m_fleetFlags & kFleetBlockade) != 0 && today - m_lastSiegeOrderDay >= 2
        && Issue(kOrderAssault, {flagship.key}, &target.key, "occupy")) {
      m_lastSiegeOrderDay = today;
    }
    return;
  }
  if ((flagship.fleet->m_fleetFlags & kFleetBlockade) == 0 || today - m_lastSiegeOrderDay < 2) {
    return;
  }
  if (m_side == 2 && flagship.deathStar && target.headquarters && target.key.m_value != kCoruscant
      && Issue(kOrderDestroyPlanet, {flagship.key}, &target.key, "death star on headquarters")) {
    m_lastSiegeOrderDay = today;
    return;
  }
  const int needed = TroopsNeeded(target);
  if (target.shields < kShieldsBlockingAssault && flagship.troops >= needed && flagship.troops > 0) {
    if (Issue(kOrderAssault, {flagship.key}, &target.key, "assault")) {
      m_lastSiegeOrderDay = today;
    }
    return;
  }
  const bool worthBombarding = target.shields + target.batteries + target.defenders > 0 || target.headquarters;
  if (worthBombarding && CanBreakShields(target, flagship.bombard)
      && Issue(kOrderBombard, {flagship.key}, &target.key, "bombard")) {
    m_lastSiegeOrderDay = today;
    return;
  }
  if (flagship.troops < needed) {
    Embark(flagship, Cargo::kTroops, needed - flagship.troops, true);
  }
}

void ClaudeAiPlayer::RunScout()
{
  static const bool off = FeatureOff("scout");
  if (off) {
    return;
  }
  FleetInfo* scout = FindFleet(m_scout);
  if (!scout && IsSet(m_scout)) {
    // Lone scouts die to planetary defenses; do not feed another one into the same system soon.
    if (IsSet(m_scoutTarget)) {
      Trace("scout %08x lost near %08x", m_scout.m_value, m_scoutTarget.m_value);
      m_avoidUntil[m_scoutTarget.m_value] = std::max(m_avoidUntil[m_scoutTarget.m_value], GameHookDay() + 300);
    }
    m_lastScoutLossDay = GameHookDay();
  }
  if (!scout) {
    m_scout.Reset();
    m_scoutTarget.Reset();
    if (GameHookDay() - m_lastScoutLossDay < 60) {
      return;
    }
    const FleetInfo* best = nullptr;
    for (const auto& info : m_fleets) {
      if (IsClaimed(info.key) || info.enroute || SameKey(info.key, m_flagship) || info.ships > 2) {
        continue;
      }
      const SystemInfo* at = FindSystem(info.systemKey);
      if (at && IsOwnCapital(*at)) {
        continue;
      }
      if (!best || info.force.Value() < best->force.Value()) {
        best = &info;
      }
    }
    if (!best) {
      return;
    }
    m_scout = best->key;
    m_claimed.push_back(m_scout.m_value);
    scout = FindFleet(m_scout);
    Trace("scout %08x", m_scout.m_value);
  }
  if (scout->enroute) {
    return;
  }
  const int today = GameHookDay();
  const SystemInfo* best = nullptr;
  double bestScore = 0;
  for (const auto& [key, info] : m_systems) {
    if (info.controller != Enemy() || SameKey(info.key, scout->systemKey) || !IntelIsStale(info)) {
      continue;
    }
    if (const auto avoid = m_avoidUntil.find(key); avoid != m_avoidUntil.end() && avoid->second > today) {
      continue;
    }
    // The Empire hunts the hidden headquarters; the Alliance knows where Coruscant is and only scouts its next target.
    const bool hunting = m_side == 2 && !IsSet(m_knownHeadquarters);
    const bool enemyCapital = m_side == 1 && info.key.m_value == kCoruscant;
    if ((!hunting && !SameKey(info.key, m_target)) || enemyCapital) {
      continue;
    }
    double priority = SameKey(info.key, m_target) ? 6 : 1;
    if (hunting && info.populated) {
      priority *= 2;
    }
    const double score = priority / (5.0 + TravelDaysBetween(scout->systemKey, info.key));
    if (score > bestScore) {
      bestScore = score;
      best = &info;
    }
  }
  if (best && Issue(kOrderMove, {scout->key}, &best->key, "scout")) {
    m_scoutTarget = best->key;
  }
}

// ---- the Alliance headquarters ------------------------------------------------------------------------------------

int ClaudeAiPlayer::DaysToEnemy(const SystemInfo& info) const
{
  int nearest = 1 << 30;
  for (const auto& [key, other] : m_systems) {
    if (other.controller == Enemy()) {
      nearest = std::min(nearest, TravelDaysBetween(info.key, other.key));
    }
  }
  return nearest;
}

// The Empire only finds the headquarters by bringing a fleet into its system (and it is shown Yavin at the start).
// Keep it away from any system the enemy has seen, deep inside our space; the staff's own moves are vetoed.
void ClaudeAiPlayer::HideHeadquarters()
{
  static const bool off = FeatureOff("hq");
  if (off || m_side != 1 || !Every(&m_lastHeadquartersCheckDay, 10)) {
    return;
  }
  const int today = GameHookDay();
  ObjectKey hqKey;
  hqKey.m_value = kHeadquartersKey;
  GameObjNotifier* hq = GetObjectInFamilies_20_22(m_side, &hqKey);
  if (!hq || (StateFlags(hq) & (kStateDestroyed | kStateEnroute)) != 0 || !hq->m_parent
      || today - m_lastHeadquartersMoveDay < 150) {
    return;
  }
  const SystemInfo* here = FindSystem(KeyOf(hq->m_parent));
  if (!here) {
    return;
  }
  auto compromised = [&](const SystemInfo& info) {
    if (info.key.m_value == kYavin) {
      return true;
    }
    const auto seen = m_enemySeenDay.find(info.key.m_value);
    return seen != m_enemySeenDay.end() && today - seen->second < 600;
  };
  const bool threatened = compromised(*here) || DaysToEnemy(*here) < 15;
  if (!threatened) {
    return;
  }
  const SystemInfo* best = nullptr;
  double bestScore = DaysToEnemy(*here) + (compromised(*here) ? -1000.0 : 0.0);
  for (const auto& [key, info] : m_systems) {
    if (info.controller != m_side || !info.populated || info.freeEnergy <= 0 || compromised(info)
        || info.enemyFleets.attack > 0 || (info.system->m_flags & system_flag::kUprising) != 0
        || info.system->GetLoyaltyFor(m_side) < 60) {
      continue;
    }
    const double score = DaysToEnemy(info) + 5.0 * info.shields + 1.0 * info.defenders;
    if (score > bestScore + 5) {
      bestScore = score;
      best = &info;
    }
  }
  if (best && Issue(kOrderMove, {hqKey}, &best->key, "move headquarters")) {
    m_lastHeadquartersMoveDay = today;
  }
}
