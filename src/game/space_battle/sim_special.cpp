#include <algorithm>
#include <cmath>

#include "space_battle/math2d.h"
#include "space_battle/sim_internal.h"

namespace reb::battle::sim
{
namespace
{
using namespace tuning;

bool InTractorRange(const BattleState& state, UnitIndex ship, UnitIndex target, float factor)
{
  const float reach = state.Profile(ship).tractorRange * factor + state.Profile(target).radius;
  return DistanceSq(state.units[ship].position, state.units[target].position) <= reach * reach;
}

bool SideHasDeathStar(const BattleState& state, Side side)
{
  return std::any_of(state.stations.begin(), state.stations.end(), [&](const StationState& station) {
    return state.units[station.unit].side == side && state.InSpace(station.unit);
  });
}

int32_t SpoolTicksFor(const BattleState& state, UnitIndex unit)
{
  const UnitProfile& profile = state.Profile(unit);
  const Side side = profile.side;
  if (profile.kind == UnitKind::DeathStar) {
    return Ticks(kDeathStarOwnSpoolSeconds);
  }
  const SideParams& params = state.snapshot.params.sides[SideIndex(side)];
  float seconds = std::clamp(kSpoolBaseSeconds + params.retreatDays * kSpoolSecondsPerRetreatDay, kSpoolMinSeconds,
                             kSpoolMaxSeconds);
  const ShipState* ship = state.Ship(unit);
  const bool primaryWorks = profile.hyperdrive > 0 && (ship == nullptr || ship->criticals.hyperdrivePrimary == 0);
  const int32_t rating = primaryWorks ? profile.hyperdrive : profile.hyperdriveBackup;
  seconds *= static_cast<float>(std::max(rating, 1)) / kHyperdriveReferenceRating;
  // The Death Star's gravity lengthens Imperial jumps, so the Alliance gets its chance at the run (README o).
  if (SideHasDeathStar(state, side)) {
    seconds *= kDeathStarSpoolFactor;
  }
  if (state.sides[SideIndex(side)].coordinatedJumpTicks > 0) {
    seconds *= 1.0f - kCoordinatedJumpSpeedup;
  }
  return std::max(1, Ticks(seconds));
}

void Jump(BattleState& state, UnitIndex unit)
{
  UnitState& self = state.units[unit];
  self.fate = UnitFate::Withdrawn;
  self.inSpace = false;
  self.orders.clear();
  state.sides[SideIndex(self.side)].everWithdrew = true;
  AddEffect(state, ViewEffectKind::Jump, self.position, state.Profile(unit).radius * 2.0f, unit);
  LogEvent(state, self.side, UnitName(state, unit) + " jumped to hyperspace", unit);
  if (SquadronState* squadron = state.Squadron(unit)) {
    squadron->place = SquadronPlace::Withdrawn;
    return;
  }
  if (ShipState* ship = state.Ship(unit)) {
    ship->tractorTarget = kNoUnit;
  }
  for (SquadronState& squadron : state.squadrons) {
    if (squadron.carrier == unit && squadron.place == SquadronPlace::Docked && state.units[squadron.unit].fate == UnitFate::Active) {
      squadron.place = SquadronPlace::Withdrawn;
      state.units[squadron.unit].fate = UnitFate::Withdrawn;
    }
  }
  // Squadrons it left flying need another hangar, as if it had been lost.
  RehomeSquadrons(state, unit);
}

bool DoctrineWantsOut(const BattleState& state, UnitIndex unit)
{
  const ShipState& ship = *state.Ship(unit);
  const float hull = state.HullFraction(unit);
  switch (state.EffectiveShipDoctrine(unit).withdraw) {
    case WithdrawRule::HullBelow25:
      return hull < 0.25f;
    case WithdrawRule::HullBelow50:
      return hull < 0.5f;
    case WithdrawRule::HullBelow75:
      return hull < 0.75f;
    case WithdrawRule::ShieldsCollapse:
      return state.Profile(unit).shieldMax > 0.0f && ship.shield <= kShieldsCollapsedShare * state.Profile(unit).shieldMax;
    case WithdrawRule::Never:
    case WithdrawRule::OnOrderOnly:
      return false;
  }
  return false;
}

void Spool(BattleState& state, UnitIndex unit, int32_t& progress, int32_t total)
{
  progress = std::min(total, progress + 1);
  if (progress >= total && CanJump(state, unit)) {
    Jump(state, unit);
  }
}

// The commander's combat skill, as the original's second-commander skill term (combat / 20).
float CommanderSkill(const BattleState& state, Side side)
{
  const SideState& self = state.sides[SideIndex(side)];
  if (self.commander == kNoUnit) {
    return 0.0f;
  }
  const auto* character = std::get_if<CharacterEntry>(&state.snapshot.units[self.commander]);
  const bool aboard = character != nullptr && state.Valid(character->ship)
                      && state.units[character->ship].fate != UnitFate::Destroyed;
  return aboard ? self.commanderCombat / kTrenchCombatPerSkill : 0.0f;
}

void EndTrenchRun(BattleState& state, StationState& station)
{
  if (GroupState* flight = state.Group(station.runFlight)) {
    const auto isRun = [](const UnitOrder& order) { return order.kind == OrderKind::Run; };
    std::erase_if(flight->orders, isRun);
    for (UnitIndex member : flight->members) {
      std::erase_if(state.units[member].orders, isRun);
      if (SquadronState* squadron = state.Squadron(member)) {
        squadron->runTicks = 0;
        squadron->nextThinkTick = static_cast<int32_t>(state.tick);
      }
    }
  }
  station.runFlight = kNoGroup;
}

// At the port each surviving craft takes its shot, from its torpedoes and the commander's skill, starting from the
// original formula: (torpedoes x 0.5333 + skill) x 0.5 percent, where the original's skill term is combat / 20 + 1.
bool TakeTheShot(BattleState& state, SquadronState& squadron, float skill)
{
  bool hit = false;
  for (int i = 0; i < squadron.craftCount; ++i) {
    CraftState& craft = state.craft[squadron.firstCraft + i];
    if (!craft.alive) {
      continue;
    }
    const float chance = (static_cast<float>(craft.torpedoes) * kTrenchTorpedoFactor + 1.0f + skill) * 0.005f
                         * kTrenchShotScale;
    hit = state.rng.Chance(chance) || hit;
    craft.torpedoes = 0;
  }
  return hit;
}

void FlyTheTrench(BattleState& state, StationState& station, SquadronState& squadron)
{
  const Side side = state.units[squadron.unit].side;
  const float skill = CommanderSkill(state, side);
  // Each craft survives the whole run by its agility and the commander's skill: the A-wing's agility shines.
  const float survive = std::clamp((TurnPoints(state, squadron.unit) + skill) / kTrenchFullSurvivalPoints,
                                   kTrenchMinSurvival, 1.0f);
  const float loss = 1.0f - std::pow(survive, 1.0f / static_cast<float>(Ticks(kTrenchRunSeconds)));
  for (int i = 0; i < squadron.craftCount && loss > 0.0f; ++i) {
    CraftState& craft = state.craft[squadron.firstCraft + i];
    if (craft.alive && state.rng.Chance(loss)) {
      DamageCraft(state, squadron.unit, squadron.firstCraft + i, craft.hull + craft.shield + 1.0f);
    }
  }
  if (squadron.alive <= 0 || --squadron.runTicks > 0) {
    return;
  }
  const UnitIndex target = station.unit;
  if (TakeTheShot(state, squadron, skill)) {
    LogEvent(state, side, UnitName(state, squadron.unit) + ": a direct hit on the exhaust port!", target);
    EndTrenchRun(state, station);
    DestroyUnit(state, target);
    return;
  }
  LogEvent(state, side, UnitName(state, squadron.unit) + ": the shot missed the exhaust port", squadron.unit);
  EndTrenchRun(state, station);
}

void UpdateTrenchRun(BattleState& state, StationState& station)
{
  const GroupState* flight = state.Group(station.runFlight);
  if (flight == nullptr) {
    station.runFlight = kNoGroup;
    return;
  }
  const Vec2 center = state.units[station.unit].position;
  const float entry = state.Profile(station.unit).radius + kTrenchEntryDistance;
  bool active = false;
  const std::vector<UnitIndex> members = flight->members;
  for (UnitIndex member : members) {
    SquadronState* squadron = state.Squadron(member);
    if (squadron == nullptr || squadron->place != SquadronPlace::InSpace || squadron->task != SquadronTask::TrenchRun
        || squadron->target != station.unit) {
      continue;
    }
    active = true;
    if (squadron->runTicks > 0) {
      FlyTheTrench(state, station, *squadron);
      if (station.runFlight == kNoGroup) {
        return;
      }
      continue;
    }
    if (DistanceSq(squadron->centroid, center) > entry * entry) {
      continue;
    }
    // The conditions are checked as the flight dives in; once in the trench it flies on whatever happens around it.
    if (!TrenchRunPossible(state, station.unit)) {
      LogEvent(state, state.units[member].side, flight->name + ": the Death Star run is called off", member);
      EndTrenchRun(state, station);
      return;
    }
    squadron->runTicks = Ticks(kTrenchRunSeconds);
    LogEvent(state, state.units[member].side, UnitName(state, member) + " enters the trench", member);
  }
  if (!active && std::none_of(members.begin(), members.end(), [&](UnitIndex member) {
        const SquadronState* squadron = state.Squadron(member);
        return squadron != nullptr && squadron->place == SquadronPlace::Docked && squadron->alive > 0;
      })) {
    EndTrenchRun(state, station);
  }
}

} // namespace

void BeginWithdrawal(BattleState& state, UnitIndex unit)
{
  const UnitState& self = state.units[unit];
  if (!state.snapshot.params.sides[SideIndex(self.side)].canRetreat || self.fate != UnitFate::Active) {
    return;
  }
  if (ShipState* ship = state.Ship(unit)) {
    if (!ship->withdrawing && self.inSpace) {
      ship->withdrawing = true;
      ship->spoolTicks = 0;
      ship->spoolTotal = SpoolTicksFor(state, unit);
      LogEvent(state, self.side, UnitName(state, unit) + ": spooling hyperdrive", unit);
    }
  } else if (SquadronState* squadron = state.Squadron(unit)) {
    if (squadron->withdrawing) {
      return;
    }
    if (state.Profile(unit).CanJumpAlone() && squadron->place == SquadronPlace::InSpace) {
      squadron->withdrawing = true;
      squadron->spoolTicks = 0;
      squadron->spoolTotal = SpoolTicksFor(state, unit);
    } else if (squadron->place == SquadronPlace::InSpace) {
      // Fighters without a hyperdrive dock with a departing ship, or stay behind (§6.10).
      UnitState& mutableSelf = state.units[unit];
      mutableSelf.orders.clear();
      UnitOrder recover;
      recover.kind = OrderKind::Recover;
      mutableSelf.orders.push_back(recover);
      squadron->withdrawing = true;
    } else if (squadron->place == SquadronPlace::Docked && squadron->carrier == kNoUnit) {
      // A planet's garrison has no departing hangar to leave in: it launches and jumps on its own, or stays to defend.
      if (state.Profile(unit).CanJumpAlone()) {
        LaunchSquadron(state, unit);
        squadron->withdrawing = true;
        squadron->spoolTicks = 0;
        squadron->spoolTotal = SpoolTicksFor(state, unit);
      }
    } else if (squadron->place == SquadronPlace::Docked) {
      squadron->stayDocked = true;
    }
  } else if (state.Station(unit) != nullptr) {
    StationState& station = state.stations[self.slot];
    if (!station.withdrawing) {
      station.withdrawing = true;
      station.spoolTicks = 0;
      station.spoolTotal = SpoolTicksFor(state, unit);
      LogEvent(state, self.side, UnitName(state, unit) + ": preparing to withdraw", unit);
    }
  }
}

void UpdateTractors(BattleState& state)
{
  for (ShipState& ship : state.ships) {
    ship.tractorHeld = 0.0f;
  }
  for (ShipState& ship : state.ships) {
    const UnitIndex unit = ship.unit;
    if (!state.InSpace(unit)) {
      ship.tractorTarget = kNoUnit;
      continue;
    }
    if (ship.tractorTarget != kNoUnit
        && (!state.InSpace(ship.tractorTarget) || !TractorReady(state, unit)
            || !InTractorRange(state, unit, ship.tractorTarget, kTractorBreakFactor))) {
      if (state.InSpace(ship.tractorTarget)) {
        LogEvent(state, state.units[unit].side,
                 UnitName(state, unit) + ": tractor lock on " + UnitName(state, ship.tractorTarget) + " broken", unit);
      }
      ship.tractorTarget = kNoUnit;
    }
    if (ship.tractorTarget == kNoUnit && TractorReady(state, unit)) {
      const std::vector<UnitOrder>& orders = state.ActiveOrders(unit);
      UnitIndex wanted = kNoUnit;
      if (!orders.empty() && orders.front().kind == OrderKind::Tractor && !orders.front().targets.empty()) {
        wanted = orders.front().targets.front();
      } else {
        // Without an order, tractor ships hold the biggest enemy that is trying to jump out.
        float best = 0.0f;
        for (const ShipState& other : state.ships) {
          if (state.units[other.unit].side == state.units[unit].side || !state.InSpace(other.unit) || !other.withdrawing
              || !InTractorRange(state, unit, other.unit, 1.0f)) {
            continue;
          }
          if (state.Profile(other.unit).hullMax > best) {
            best = state.Profile(other.unit).hullMax;
            wanted = other.unit;
          }
        }
      }
      if (state.InSpace(wanted) && InTractorRange(state, unit, wanted, 1.0f)) {
        ship.tractorTarget = wanted;
        LogEvent(state, state.units[unit].side, UnitName(state, unit) + ": tractor lock on " + UnitName(state, wanted), unit);
      }
    }
    if (ship.tractorTarget != kNoUnit) {
      state.Ship(ship.tractorTarget)->tractorHeld += TractorPower(state, unit);
    }
  }
}

void UpdateWithdrawal(BattleState& state)
{
  for (ShipState& ship : state.ships) {
    const UnitIndex unit = ship.unit;
    if (!state.InSpace(unit)) {
      continue;
    }
    const Side side = state.units[unit].side;
    if (!ship.withdrawing && state.sides[SideIndex(side)].holdLineTicks <= 0 && DoctrineWantsOut(state, unit)) {
      BeginWithdrawal(state, unit);
    }
    if (ship.withdrawing) {
      Spool(state, unit, ship.spoolTicks, ship.spoolTotal);
    }
  }
  for (SquadronState& squadron : state.squadrons) {
    if (squadron.withdrawing && squadron.spoolTotal > 0 && squadron.place == SquadronPlace::InSpace) {
      Spool(state, squadron.unit, squadron.spoolTicks, squadron.spoolTotal);
    }
  }
  for (StationState& station : state.stations) {
    if (station.withdrawing && state.InSpace(station.unit)) {
      Spool(state, station.unit, station.spoolTicks, station.spoolTotal);
    }
  }
}

void UpdateDeathStars(BattleState& state)
{
  for (StationState& station : state.stations) {
    const UnitIndex unit = station.unit;
    if (!state.InSpace(unit)) {
      continue;
    }
    const Side side = state.units[unit].side;
    if (station.chargeTicks > 0 && --station.chargeTicks == 0) {
      LogEvent(state, side, UnitName(state, unit) + ": superlaser charged", unit);
    }
    if (station.runFlight != kNoGroup) {
      UpdateTrenchRun(state, station);
      if (!state.InSpace(unit)) {
        continue;
      }
    }
    if (station.firingAt != kNoUnit && --station.fireDelayTicks <= 0) {
      const UnitIndex target = station.firingAt;
      station.firingAt = kNoUnit;
      if (state.InSpace(target)) {
        AddEffect(state, ViewEffectKind::Superlaser, state.units[target].position, state.Profile(target).radius * 3.0f,
                  target);
        LogEvent(state, side, UnitName(state, target) + " destroyed by the superlaser", target);
        DestroyUnit(state, target);
      }
    }
  }
}

} // namespace reb::battle::sim
