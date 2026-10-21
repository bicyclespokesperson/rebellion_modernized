#include "space_battle/commander.h"

#include <algorithm>
#include <cmath>

#include "space_battle/battle.h"
#include "space_battle/battle_queries.h"
#include "space_battle/doctrine.h"
#include "space_battle/math2d.h"

namespace reb::battle
{
namespace
{
constexpr int64_t kRecoveryTicks = 15 * kTicksPerSecond;

Command MakeCommand(const Battle& battle, Side side, OrderPayload order)
{
  Command command;
  command.tick = battle.Tick();
  command.side = side;
  command.order = std::move(order);
  return command;
}

void ForGroup(const Battle& battle, Side side, const GroupState& group, OrderPayload order, std::vector<Command>& out)
{
  Command command = MakeCommand(battle, side, std::move(order));
  command.target.groups = {group.id};
  out.push_back(std::move(command));
}

void ForUnit(const Battle& battle, Side side, UnitIndex unit, OrderPayload order, std::vector<Command>& out)
{
  Command command = MakeCommand(battle, side, std::move(order));
  command.target.units = {unit};
  out.push_back(std::move(command));
}

bool SamePriorities(const TargetPriorities& a, const TargetPriorities& b)
{
  return a.count == b.count && a.finishDamaged == b.finishDamaged
         && std::equal(a.tags.begin(), a.tags.begin() + a.count, b.tags.begin());
}

void SetDoctrine(const Battle& battle, const GroupState& group, const ShipDoctrine& desired, std::vector<Command>& out)
{
  const ShipDoctrine& old = group.shipDoctrine;
  DoctrineOverrideMask fields = 0;
  const auto changed = [&](bool differs, DoctrineOverrideMask field) { fields |= differs ? field : 0; };
  changed(old.role != desired.role, kShipRole);
  changed(old.range != desired.range, kShipRange);
  changed(old.approach != desired.approach, kShipApproach);
  changed(old.facing != desired.facing, kShipFacing);
  changed(!SamePriorities(old.priorities, desired.priorities), kShipPriorities);
  changed(old.allocation != desired.allocation, kShipAllocation);
  changed(old.power != desired.power, kShipPower);
  changed(old.withdraw != desired.withdraw, kShipWithdraw);
  changed(old.formation != desired.formation, kShipFormation);
  changed(old.screenTarget != desired.screenTarget, kShipScreenTarget);
  if (fields != 0) {
    ForGroup(battle, group.side, group, OrderSetShipDoctrine{desired, fields}, out);
  }
}

void SetDoctrine(const Battle& battle, const GroupState& group, const FlightDoctrine& desired,
                 std::vector<Command>& out)
{
  const FlightDoctrine& old = group.flightDoctrine;
  DoctrineOverrideMask fields = 0;
  const auto changed = [&](bool differs, DoctrineOverrideMask field) { fields |= differs ? field : 0; };
  changed(old.mission != desired.mission, kFlightMission);
  changed(!SamePriorities(old.priorities, desired.priorities), kFlightPriorities);
  changed(old.dogfight != desired.dogfight, kFlightDogfight);
  changed(old.rearm != desired.rearm, kFlightRearm);
  changed(old.launch != desired.launch, kFlightLaunch);
  changed(old.escortTarget != desired.escortTarget, kFlightEscortTarget);
  if (fields != 0) {
    ForGroup(battle, group.side, group, OrderSetFlightDoctrine{desired, fields}, out);
  }
}

struct Fleet {
  uint32_t tags = 0;
  int capitals = 0;
  int fighters = 0;
  UnitIndex bomber = kNoUnit;
  UnitIndex station = kNoUnit;
  bool activeWell = false;
  bool withdrawing = false;
};

Fleet ReadFleet(const BattleState& state, Side side)
{
  Fleet fleet;
  for (UnitIndex unit = 0; unit < static_cast<UnitIndex>(state.units.size()); ++unit) {
    if (state.units[unit].side != side || !state.Present(unit)) {
      continue;
    }
    const UnitProfile& profile = state.Profile(unit);
    fleet.tags |= profile.staticTags;
    fleet.withdrawing = fleet.withdrawing || (profile.IsShip() && state.Withdrawing(unit));
    fleet.capitals += profile.IsShip();
    if (const SquadronState* squadron = state.Squadron(unit)) {
      fleet.fighters += squadron->alive;
      if (profile.archetype == Archetype::Bomber && fleet.bomber == kNoUnit) {
        fleet.bomber = unit;
      }
    }
    if (profile.kind == UnitKind::DeathStar) {
      fleet.station = unit;
    }
    fleet.activeWell = fleet.activeWell || (profile.IsShip() && WellActive(state, unit));
  }
  return fleet;
}

bool Has(const Fleet& fleet, TargetTag tag)
{
  return (fleet.tags & TagBit(tag)) != 0;
}

UnitIndex Protect(const BattleState& state, Side side, const GroupState& group)
{
  UnitIndex best = kNoUnit;
  float value = -1.0f;
  for (UnitIndex unit = 0; unit < static_cast<UnitIndex>(state.units.size()); ++unit) {
    if (state.units[unit].side != side || !state.InSpace(unit) || state.Withdrawing(unit)
        || std::find(group.members.begin(), group.members.end(), unit) != group.members.end()) {
      continue;
    }
    const UnitProfile& profile = state.Profile(unit);
    if (!profile.IsShip() && profile.kind != UnitKind::DeathStar) {
      continue;
    }
    const float score = EffectiveHealth(state, unit) * (1.0f + profile.firepower)
                        + (profile.kind == UnitKind::DeathStar ? 1e9f : 0.0f);
    if (score > value) {
      value = score;
      best = unit;
    }
  }
  return best;
}

UnitIndex LivingMember(const BattleState& state, const GroupState& group)
{
  for (UnitIndex unit : group.members) {
    if (state.Present(unit) && !state.Withdrawing(unit)) {
      return unit;
    }
  }
  return kNoUnit;
}

void PursueWithTractors(const Battle& battle, Side side, const GroupState& group, std::vector<Command>& out)
{
  const BattleState& state = battle.State();
  for (UnitIndex unit : group.members) {
    const ShipState* ship = state.Ship(unit);
    if (!state.InSpace(unit) || ship == nullptr || ship->withdrawing || !TractorReady(state, unit)) {
      continue;
    }
    UnitIndex target = kNoUnit;
    float nearest = 0.0f;
    for (const ShipState& enemy : state.ships) {
      if (state.units[enemy.unit].side == side || !state.InSpace(enemy.unit) || !enemy.withdrawing) {
        continue;
      }
      const float distance = DistanceSq(state.units[unit].position, state.units[enemy.unit].position);
      if (target == kNoUnit || distance < nearest) {
        target = enemy.unit;
        nearest = distance;
      }
    }
    const auto& orders = state.ActiveOrders(unit);
    if (target != kNoUnit && (orders.empty() || orders.front().kind != OrderKind::Tractor
                             || orders.front().targets.empty() || orders.front().targets.front() != target)) {
      ForUnit(battle, side, unit, OrderTractor{target}, out);
    }
  }
}

void PlanGroups(const Battle& battle, Side side, CommanderParams params, bool observing, bool withdrawing,
                std::vector<Command>& out)
{
  const BattleState& state = battle.State();
  const Fleet own = ReadFleet(state, side);
  const Fleet enemy = ReadFleet(state, Opponent(side));
  const float ownStrength = SideStrength(state, side);
  const float enemyStrength = SideStrength(state, Opponent(side));
  const bool breakNet = enemy.activeWell && (ownStrength < enemyStrength || withdrawing);
  const bool pursue = !withdrawing && ownStrength > enemyStrength && enemy.withdrawing;
  const bool invasion = state.snapshot.params.heldFirstBy == side && Has(enemy, TargetTag::Transport);
  // A defender with planetary batteries fights under their guns unless it is clearly the stronger (§6.13).
  const float sally = tuning::kSallyStrengthRatio[static_cast<size_t>(params.temperament)];
  const bool underGuns = side != state.attacker && HasPlanetaryCover(state, side) && ownStrength < sally * enemyStrength;
  // A stalemate leaves the system with the side that held it (§6.11), so a stronger attacker whose fight has stalled
  // commits everything that can hurt the enemy.
  const bool press = side == state.attacker && !withdrawing && ownStrength > enemyStrength
                     && state.tick - state.lastDurableDropTick >= tuning::kPressAttackSeconds * kTicksPerSecond;
  for (const GroupState& group : state.groups) {
    const UnitIndex member = LivingMember(state, group);
    if (group.side != side || member == kNoUnit) {
      continue;
    }
    const UnitProfile& profile = state.Profile(member);
    if (group.flight) {
      if (observing) {
        continue;
      }
      FlightDoctrine doctrine = DefaultFlightDoctrine(profile.archetype);
      if (own.station != kNoUnit || (enemy.station != kNoUnit && profile.archetype != Archetype::Bomber)) {
        doctrine.mission = FlightMission::Intercept;
        doctrine.priorities = MakePriorities({TargetTag::Bomber, TargetTag::Fighter, TargetTag::Nearest}, true);
      } else if (profile.archetype == Archetype::Bomber || profile.archetype == Archetype::Multirole) {
        if (breakNet) {
          doctrine.mission = FlightMission::Strike;
          doctrine.priorities = MakePriorities({TargetTag::Interdictor, TargetTag::Carrier, TargetTag::Damaged,
                                               TargetTag::Nearest}, true);
        } else if (invasion) {
          doctrine.mission = FlightMission::Strike;
          doctrine.priorities = MakePriorities({TargetTag::Transport, TargetTag::Carrier, TargetTag::Damaged,
                                               TargetTag::Nearest}, true);
        } else if (profile.archetype == Archetype::Bomber && Has(enemy, TargetTag::Carrier)) {
          doctrine.mission = FlightMission::Strike;
          doctrine.priorities = MakePriorities({TargetTag::Carrier, TargetTag::ShieldsDown, TargetTag::Damaged,
                                               TargetTag::Nearest}, true);
        } else if (profile.archetype == Archetype::Multirole && own.bomber != kNoUnit && enemy.fighters > 0) {
          doctrine.mission = FlightMission::Escort;
          doctrine.escortTarget = own.bomber;
        } else if (enemy.capitals == 0) {
          doctrine.mission = FlightMission::Intercept;
        }
      }
      if (press && (profile.craftGunVsCapital > 0.0f || profile.craftIonVsCapital > 0.0f || profile.torpedoesPerCraft > 0)) {
        doctrine.mission = FlightMission::Strike;
        doctrine.priorities = MakePriorities({TargetTag::ShieldsDown, TargetTag::Damaged, TargetTag::Nearest}, true);
      }
      if (pursue && !observing && (profile.archetype == Archetype::Bomber || profile.archetype == Archetype::Multirole)) {
        doctrine.mission = FlightMission::Strike;
        doctrine.priorities = MakePriorities({TargetTag::Withdrawing, TargetTag::Damaged, TargetTag::Nearest}, true);
      }
      doctrine.rearm = params.temperament == Temperament::Cautious ? RearmRule::AtHalfLosses
                                                                  : RearmRule::WhenTorpedoesSpent;
      SetDoctrine(battle, group, doctrine, out);
      continue;
    }
    ShipDoctrine doctrine = observing ? group.shipDoctrine : DefaultShipDoctrine(profile.archetype, group.id);
    const bool line = profile.archetype == Archetype::BowLine || profile.archetype == Archetype::BroadsideLine;
    const bool screen = profile.archetype == Archetype::Picket || profile.archetype == Archetype::EscortFrigate;
    if (!observing) {
      doctrine.facing = profile.facing == FacingStyle::Bow ? Facing::BowOn
                        : profile.facing == FacingStyle::Broadside ? Facing::Broadside : Facing::StrongestToThreat;
      doctrine.allocation = params.difficulty == CommanderDifficulty::Low ? FireAllocation::Spread
                                                                         : FireAllocation::Auto;
      if (line) {
        doctrine.range = params.temperament == Temperament::Aggressive ? EngagementRange::Close
                        : params.temperament == Temperament::Cautious ? EngagementRange::StandOff
                                                                     : EngagementRange::Optimal;
        doctrine.power = params.temperament == Temperament::Aggressive ? PowerRouting::Weapons
                                                                      : PowerRouting::Balanced;
        if (params.temperament == Temperament::Aggressive && enemy.capitals == 1
            && params.difficulty != CommanderDifficulty::Low) {
          doctrine.allocation = FireAllocation::Concentrate;
        }
        if (state.HullFraction(member) < 0.55f && ownStrength > 1.3f * enemyStrength) {
          doctrine.role = Role::Reserve;
          doctrine.power = PowerRouting::Shields;
        }
      }
      if (screen) {
        doctrine.role = enemy.fighters > 0 || own.station != kNoUnit ? Role::Screen : Role::Line;
        doctrine.formation = doctrine.role == Role::Screen ? Formation::Ring : Formation::Loose;
        if (enemy.fighters == 0) {
          doctrine.priorities = MakePriorities({TargetTag::Damaged, TargetTag::Light, TargetTag::Nearest}, true);
        }
      }
      if (line && own.station != kNoUnit) {
        doctrine.role = Role::Screen;
        doctrine.formation = Formation::Ring;
      }
      if (line && underGuns) {
        doctrine.approach = Approach::UnderGuns;
      }
      // The commander can recover a departing ship's fighters before ordering its jump.
      doctrine.withdraw = WithdrawRule::OnOrderOnly;
    }
    if (doctrine.role == Role::Screen) {
      doctrine.screenTarget = Protect(state, side, group);
    }
    if (breakNet && !screen && profile.archetype != Archetype::Carrier && profile.archetype != Archetype::Transport) {
      doctrine.role = Role::Line;
      doctrine.range = EngagementRange::Optimal;
      if (doctrine.approach == Approach::UnderGuns) {
        doctrine.approach = Approach::Direct; // the interdictor has to be reached wherever it is
      }
      doctrine.priorities = MakePriorities({TargetTag::Interdictor, TargetTag::Tractor, TargetTag::Damaged,
                                           TargetTag::Nearest}, true);
    } else if (invasion && !screen && profile.archetype != Archetype::Carrier) {
      doctrine.priorities = MakePriorities({TargetTag::Transport, TargetTag::Carrier, TargetTag::Damaged,
                                           TargetTag::Nearest}, true);
    }
    if (press && !observing && profile.archetype != Archetype::Transport && profile.archetype != Archetype::Carrier
        && profile.bestArcVsCapital > 0.0f) {
      doctrine.role = Role::Line;
      doctrine.range = EngagementRange::Optimal;
      doctrine.approach = Approach::Direct;
      doctrine.priorities = MakePriorities({TargetTag::Damaged, TargetTag::Nearest}, true);
    }
    if (pursue && !observing && !breakNet && profile.archetype != Archetype::Transport
        && profile.archetype != Archetype::Carrier && own.station == kNoUnit) {
      doctrine.role = Role::Line;
      doctrine.range = EngagementRange::Close;
      doctrine.approach = Approach::Direct;
      doctrine.priorities = MakePriorities({TargetTag::Withdrawing, TargetTag::Damaged, TargetTag::Nearest}, true);
      PursueWithTractors(battle, side, group, out);
    }
    SetDoctrine(battle, group, doctrine, out);
  }
}

void FireSuperlaser(const Battle& battle, Side side, std::vector<Command>& out)
{
  const BattleState& state = battle.State();
  for (const StationState& station : state.stations) {
    if (state.units[station.unit].side != side || !SuperlaserReady(state, station.unit)) {
      continue;
    }
    UnitIndex best = kNoUnit;
    float bestValue = 0.0f;
    for (const ShipState& ship : state.ships) {
      if (state.units[ship.unit].side == side || !state.InSpace(ship.unit)) {
        continue;
      }
      const float value = (1.0f + state.Profile(ship.unit).firepower * WeaponOutput(state, ship.unit))
                          * EffectiveHealth(state, ship.unit);
      if (value > bestValue) {
        bestValue = value;
        best = ship.unit;
      }
    }
    if (best != kNoUnit) {
      out.push_back(MakeCommand(battle, side, OrderFireSuperlaser{best}));
    }
  }
}

void SendTrenchRun(const Battle& battle, Side side, std::vector<Command>& out)
{
  const BattleState& state = battle.State();
  for (const StationState& station : state.stations) {
    if (state.units[station.unit].side == side || station.runFlight != kNoGroup
        || !TrenchRunPossible(state, station.unit)) {
      continue;
    }
    GroupId best = kNoGroup;
    int bestTorpedoes = 0;
    for (const GroupState& group : state.groups) {
      if (group.side != side || !group.flight) {
        continue;
      }
      int torpedoes = 0;
      for (UnitIndex member : group.members) {
        const SquadronState* squadron = state.Squadron(member);
        if (squadron == nullptr || squadron->place != SquadronPlace::InSpace || squadron->withdrawing) {
          continue;
        }
        for (int i = 0; i < squadron->craftCount; ++i) {
          const CraftState& craft = state.craft[squadron->firstCraft + i];
          torpedoes += craft.alive ? craft.torpedoes : 0;
        }
      }
      if (torpedoes > bestTorpedoes) {
        bestTorpedoes = torpedoes;
        best = group.id;
      }
    }
    if (best != kNoGroup) {
      ForGroup(battle, side, *state.Group(best), OrderDeathStarRun{}, out);
    }
  }
}

bool RecoverFighters(const Battle& battle, Side side, UnitIndex carrier, std::vector<Command>& out)
{
  bool waiting = false;
  const BattleState& state = battle.State();
  for (const SquadronState& squadron : state.squadrons) {
    if (state.units[squadron.unit].side != side || squadron.place != SquadronPlace::InSpace || squadron.alive == 0
        || (carrier != kNoUnit && squadron.carrier != carrier)) {
      continue;
    }
    // Independent squadrons can jump with the fleet. Recover the ones that depend on a surviving hangar.
    if (carrier == kNoUnit && state.Profile(squadron.unit).CanJumpAlone()) {
      continue;
    }
    const UnitIndex home = squadron.carrier;
    if (!state.InSpace(home) || (carrier == kNoUnit && state.Withdrawing(home))) {
      continue;
    }
    waiting = true;
    const auto& orders = state.ActiveOrders(squadron.unit);
    if (orders.empty() || orders.front().kind != OrderKind::Recover) {
      ForUnit(battle, side, squadron.unit, OrderRecover{}, out);
    }
  }
  return waiting;
}

bool InImmediateDanger(const BattleState& state, UnitIndex unit)
{
  float incoming = PlanetaryDpsOn(state, unit);
  for (const ShipState& ship : state.ships) {
    if (state.units[ship.unit].side != state.units[unit].side && state.InSpace(ship.unit)) {
      incoming += ExpectedDps(state, ship.unit, unit, DpsGeometry::Now);
    }
  }
  const float lifetime = TimeToKill(state, unit, incoming);
  return lifetime >= 0.0f && lifetime < 10.0f;
}

void WithdrawFleet(const Battle& battle, Side side, int64_t since, std::vector<Command>& out)
{
  const BattleState& state = battle.State();
  const bool waiting = RecoverFighters(battle, side, kNoUnit, out) && battle.Tick() - since < kRecoveryTicks;
  Command withdraw = MakeCommand(battle, side, OrderWithdraw{});
  for (UnitIndex unit = 0; unit < static_cast<UnitIndex>(state.units.size()); ++unit) {
    const UnitProfile& profile = state.Profile(unit);
    if (state.units[unit].side != side || !state.Present(unit) || profile.kind == UnitKind::Passive
        || state.Withdrawing(unit)) {
      continue;
    }
    if (profile.IsShip() && waiting && profile.hangarCapacity > 0 && !InImmediateDanger(state, unit)) {
      continue;
    }
    if (profile.IsSquadron() && (waiting || state.Squadron(unit)->carrier == kNoUnit) && !profile.CanJumpAlone()) {
      continue; // waits for its carrier, or is a planet's garrison that stays to defend it
    }
    if (profile.kind == UnitKind::DeathStar && waiting) {
      continue;
    }
    withdraw.target.units.push_back(unit);
  }
  if (!withdraw.target.units.empty()) {
    out.push_back(std::move(withdraw));
  }
}

void UseSignal(const Battle& battle, Side side, bool withdrawing, std::vector<Command>& out)
{
  const BattleState& state = battle.State();
  const SideState& self = state.sides[SideIndex(side)];
  if (self.signalsReady == 0 || self.commandRating <= 0.0f) {
    return;
  }
  const bool jumping = withdrawing || std::any_of(state.ships.begin(), state.ships.end(), [&](const ShipState& ship) {
    return state.units[ship.unit].side == side && state.InSpace(ship.unit) && ship.withdrawing;
  });
  if (jumping) {
    if (self.coordinatedJumpTicks <= 0) {
      out.push_back(MakeCommand(battle, side, OrderSignal{SignalKind::CoordinatedJump}));
    }
    return;
  }
  int docked = 0;
  for (const SquadronState& squadron : state.squadrons) {
    if (state.units[squadron.unit].side == side && squadron.place == SquadronPlace::Docked && !squadron.stayDocked
        && state.EffectiveFlightDoctrine(squadron.unit).mission != FlightMission::HoldInHangar
        && state.EffectiveFlightDoctrine(squadron.unit).launch == LaunchRule::Immediately) {
      docked += squadron.alive;
    }
  }
  if (docked > 0 && self.scrambleTicks <= 0) {
    out.push_back(MakeCommand(battle, side, OrderSignal{SignalKind::Scramble}));
    return;
  }
  if (self.holdLineTicks <= 0 && SideStrength(state, side) >= SideStrength(state, Opponent(side))) {
    const bool damaged = std::any_of(state.ships.begin(), state.ships.end(), [&](const ShipState& ship) {
      return state.units[ship.unit].side == side && state.InSpace(ship.unit) && !ship.withdrawing
             && state.HullFraction(ship.unit) < 0.6f;
    });
    if (damaged) {
      out.push_back(MakeCommand(battle, side, OrderSignal{SignalKind::HoldTheLine}));
      return;
    }
  }
  if (self.concentrateTicks > 0) {
    return;
  }
  UnitIndex best = kNoUnit;
  float bestScore = 0.0f;
  for (const ShipState& target : state.ships) {
    if (state.units[target.unit].side == side || !state.InSpace(target.unit)) {
      continue;
    }
    float dps = 0.0f;
    float committed = 0.0f;
    for (const ShipState& ship : state.ships) {
      if (state.units[ship.unit].side == side && state.InSpace(ship.unit) && !ship.withdrawing
          && Distance(state.units[ship.unit].position, state.units[target.unit].position) <= tuning::kConcentrateRange) {
        dps += ExpectedDps(state, ship.unit, target.unit, DpsGeometry::Now);
        committed += ExpectedDps(state, ship.unit, target.unit, DpsGeometry::BestArc);
      }
    }
    // Keep the allocator's overkill bound even when using a fleet-wide focus signal.
    if (dps <= 0.0f || (committed - Regeneration(state, target.unit)) * tuning::kOverkillWindowSeconds
                         >= EffectiveHealth(state, target.unit)) {
      continue;
    }
    const float score = dps * (1.0f + state.Profile(target.unit).firepower * WeaponOutput(state, target.unit));
    if (score > bestScore) {
      bestScore = score;
      best = target.unit;
    }
  }
  if (best != kNoUnit) {
    out.push_back(MakeCommand(battle, side, OrderSignal{SignalKind::ConcentrateFire, best}));
  }
}

void WriteInteger(std::vector<uint8_t>& out, uint64_t value, int bytes)
{
  for (int i = 0; i < bytes; ++i) {
    out.push_back(static_cast<uint8_t>(value >> (i * 8)));
  }
}

bool ReadInteger(std::span<const uint8_t>& data, uint64_t& value, int bytes)
{
  if (data.size() < static_cast<size_t>(bytes)) {
    return false;
  }
  value = 0;
  for (int i = 0; i < bytes; ++i) {
    value |= static_cast<uint64_t>(data[i]) << (i * 8);
  }
  data = data.subspan(bytes);
  return true;
}

} // namespace

std::unique_ptr<Commander> MakeBattleCommander(CommanderParams params)
{
  return std::make_unique<DefaultCommander>(params);
}

void DefaultCommander::PreserveShips(const Battle& battle, Side side, std::vector<Command>& out)
{
  const BattleState& state = battle.State();
  if (m_observing || m_withdrawTick >= 0 || !state.snapshot.params.sides[SideIndex(side)].canRetreat) {
    return;
  }
  for (const ShipState& ship : state.ships) {
    const float preserveAt = state.Profile(ship.unit).archetype == Archetype::Transport ? 0.5f
                             : m_params.temperament == Temperament::Cautious ? 0.5f
                             : m_params.temperament == Temperament::Balanced ? 0.25f : 0.15f;
    if (state.units[ship.unit].side != side || !state.InSpace(ship.unit) || ship.withdrawing
        || state.HullFraction(ship.unit) >= preserveAt) {
      continue;
    }
    auto found = std::find_if(m_preserving.begin(), m_preserving.end(),
                              [&](const Preservation& p) { return p.ship == ship.unit; });
    if (found == m_preserving.end()) {
      m_preserving.push_back({ship.unit, battle.Tick()});
      found = m_preserving.end() - 1;
    }
    const bool waiting = RecoverFighters(battle, side, ship.unit, out);
    if (!waiting || battle.Tick() - found->since >= kRecoveryTicks || InImmediateDanger(state, ship.unit)) {
      ForUnit(battle, side, ship.unit, OrderWithdraw{}, out);
    }
  }
}

void DefaultCommander::Update(const Battle& battle, Side side, std::vector<Command>& out)
{
  const BattleState& state = battle.State();
  if (!m_initialized) {
    m_initialized = true;
    m_observing = battle.Tick() > 0
                  || std::any_of(battle.CommandLog().begin(), battle.CommandLog().end(),
                                 [&](const Command& command) { return command.side == side; });
    m_nextThink = battle.Tick();
  }
  if (battle.Tick() < m_nextThink) {
    PreserveShips(battle, side, out);
    return;
  }
  const SideParams& params = state.snapshot.params.sides[SideIndex(side)];
  const int interval = m_params.difficulty == CommanderDifficulty::Low ? 5
                       : m_params.difficulty == CommanderDifficulty::Normal ? 2 : 1;
  m_nextThink = battle.Tick() + interval * kTicksPerSecond;
  const float threshold = params.human ? battle.Options().humanSideAiWithdrawThreshold : params.withdrawThreshold;
  const float enemy = SideStrength(state, Opponent(side));
  if (m_withdrawTick < 0 && params.canRetreat && threshold > 0.0f && enemy > 0.0f
      && SideStrength(state, side) < threshold * enemy) {
    m_withdrawTick = battle.Tick();
  }
  PlanGroups(battle, side, m_params, m_observing, m_withdrawTick >= 0, out);
  for (const ShipState& ship : state.ships) {
    if (state.units[ship.unit].side == side && state.InSpace(ship.unit) && ship.withdrawing) {
      RecoverFighters(battle, side, ship.unit, out);
    }
  }
  FireSuperlaser(battle, side, out);
  if (m_withdrawTick < 0) {
    SendTrenchRun(battle, side, out);
  }
  if (params.canRetreat && m_withdrawTick >= 0) {
    WithdrawFleet(battle, side, m_withdrawTick, out);
  } else {
    PreserveShips(battle, side, out);
  }
  if (m_params.difficulty != CommanderDifficulty::Low) {
    UseSignal(battle, side, m_withdrawTick >= 0, out);
  }
}

void DefaultCommander::Save(std::vector<uint8_t>& out) const
{
  out = {1, static_cast<uint8_t>(m_params.temperament), static_cast<uint8_t>(m_params.difficulty),
         static_cast<uint8_t>(m_initialized), static_cast<uint8_t>(m_observing)};
  WriteInteger(out, static_cast<uint64_t>(m_nextThink), 8);
  WriteInteger(out, static_cast<uint64_t>(m_withdrawTick + 1), 8);
  WriteInteger(out, m_preserving.size(), 4);
  for (const Preservation& p : m_preserving) {
    WriteInteger(out, static_cast<uint32_t>(p.ship), 4);
    WriteInteger(out, static_cast<uint64_t>(p.since), 8);
  }
}

bool DefaultCommander::Restore(std::span<const uint8_t> data)
{
  if (data.empty()) {
    m_initialized = false;
    m_nextThink = 0;
    m_withdrawTick = -1;
    m_preserving.clear();
    return true;
  }
  if (data.size() < 25 || data[0] != 1 || data[1] > static_cast<uint8_t>(Temperament::Cautious)
      || data[2] > static_cast<uint8_t>(CommanderDifficulty::High) || data[3] > 1 || data[4] > 1) {
    return false;
  }
  m_params = {static_cast<Temperament>(data[1]), static_cast<CommanderDifficulty>(data[2])};
  m_initialized = data[3] != 0;
  m_observing = data[4] != 0;
  data = data.subspan(5);
  uint64_t next = 0, withdraw = 0, count = 0;
  if (!ReadInteger(data, next, 8) || !ReadInteger(data, withdraw, 8) || !ReadInteger(data, count, 4)
      || next > INT64_MAX || withdraw > INT64_MAX || count > data.size() / 12 || data.size() != count * 12) {
    return false;
  }
  m_nextThink = static_cast<int64_t>(next);
  m_withdrawTick = static_cast<int64_t>(withdraw) - 1;
  m_preserving.clear();
  for (uint64_t i = 0; i < count; ++i) {
    uint64_t ship = 0, since = 0;
    if (!ReadInteger(data, ship, 4) || !ReadInteger(data, since, 8) || ship > INT32_MAX || since > INT64_MAX) {
      return false;
    }
    m_preserving.push_back({static_cast<UnitIndex>(ship), static_cast<int64_t>(since)});
  }
  return true;
}

} // namespace reb::battle
