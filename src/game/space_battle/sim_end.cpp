#include <algorithm>
#include <cmath>

#include "space_battle/math2d.h"
#include "space_battle/sim_internal.h"

namespace reb::battle::sim
{
namespace
{
using namespace tuning;

void CountDown(int32_t& ticks)
{
  ticks = std::max(0, ticks - 1);
}

// A superlaser with a capital ship to aim at, charging or waiting for its order, is progress still to come: no
// stalemate while it can fire (the time limit still ends the battle).
bool SuperlaserPending(const BattleState& state)
{
  for (const StationState& station : state.stations) {
    const auto* entry = std::get_if<DeathStarEntry>(&state.snapshot.units[station.unit]);
    if (entry == nullptr || !entry->completed || !state.InSpace(station.unit)) {
      continue;
    }
    const Side side = state.units[station.unit].side;
    const bool target = std::any_of(state.ships.begin(), state.ships.end(), [&](const ShipState& ship) {
      return state.units[ship.unit].side != side && state.InSpace(ship.unit);
    });
    if (target) {
      return true;
    }
  }
  return false;
}

UnitIndex ShipOf(const BattleState& state, UnitIndex character)
{
  const auto* entry = std::get_if<CharacterEntry>(&state.snapshot.units[character]);
  return entry != nullptr ? entry->ship : kNoUnit;
}

// Squadrons in a hangar hold no space: when the battle ends with nothing of their side flying and the enemy in
// space, they cannot come out again and are lost with it (§6.11). Returns whether any were.
bool LoseHangars(BattleState& state, Side side)
{
  int lost = 0;
  for (SquadronState& squadron : state.squadrons) {
    if (state.units[squadron.unit].side == side && squadron.place == SquadronPlace::Docked
        && state.units[squadron.unit].fate == UnitFate::Active) {
      DestroyUnit(state, squadron.unit);
      ++lost;
    }
  }
  if (lost > 0) {
    LogEvent(state, side, "Nothing left in space to cover the hangars: " + std::to_string(lost) + " squadrons lost");
  }
  return lost > 0;
}

} // namespace

void UpdateSides(BattleState& state)
{
  for (Side side : {Side::Alliance, Side::Empire}) {
    SideState& self = state.sides[SideIndex(side)];
    if (self.admiral != kNoUnit) {
      if (state.InSpace(ShipOf(state, self.admiral))) {
        self.commandRating = std::clamp(self.leadership / kLeadershipForFullRating, 0.0f, 1.0f);
      } else if (self.signalsMax > 0 || self.commandRating > 0.0f) {
        // The flagship is gone, and with it the admiral's command and signals (§5.6).
        self.commandRating = 0.0f;
        self.signalsMax = 0;
        self.signalsReady = 0;
        LogEvent(state, side, "The flagship is lost: no more command signals");
      }
    }
    if (self.signalsReady < self.signalsMax) {
      if (--self.signalRechargeTicks <= 0) {
        ++self.signalsReady;
        self.signalRechargeTicks = self.signalsReady < self.signalsMax ? Ticks(kSignalRechargeSeconds) : 0;
      }
    }
    CountDown(self.concentrateTicks);
    CountDown(self.holdLineTicks);
    CountDown(self.scrambleTicks);
    CountDown(self.coordinatedJumpTicks);
    self.strength = SideStrength(state, side);
  }
}

void UpdateEnding(BattleState& state)
{
  if (state.finished || state.endReason != EndReason::None) {
    return;
  }
  for (Side side : {Side::Alliance, Side::Empire}) {
    SideState& self = state.sides[SideIndex(side)];
    const float durable = DurableTotal(state, side);
    const float progress = std::max(kCraftHull, self.durableLow * kStalemateProgressShare);
    if (durable <= self.durableLow - progress) {
      self.durableLow = durable;
      state.lastDurableDropTick = state.tick;
    }
  }
  if (!SidePresent(state, Side::Alliance) || !SidePresent(state, Side::Empire)) {
    state.endReason = EndReason::Elimination;
  } else if (state.tick - state.lastDurableDropTick >= Ticks(kStalemateSeconds) && !SuperlaserPending(state)) {
    state.endReason = EndReason::Stalemate;
  } else if (state.tick >= state.options.tickLimit) {
    state.endReason = EndReason::TimeLimit;
  }
  if (state.endReason == EndReason::None) {
    return;
  }
  for (Side side : {Side::Alliance, Side::Empire}) {
    if (!SideInSpace(state, side) && SideInSpace(state, Opponent(side)) && LoseHangars(state, side)) {
      state.endReason = EndReason::Elimination;
    }
  }
  const bool alliance = SidePresent(state, Side::Alliance);
  state.finished = state.outroTicks == 0;
  state.outroEndTick = state.tick + state.outroTicks;
  const Side held = state.snapshot.params.heldFirstBy;
  switch (state.endReason) {
    case EndReason::Stalemate:
      LogEvent(state, held, "Stalemate: neither side can break the other");
      break;
    case EndReason::TimeLimit:
      LogEvent(state, held, "Time limit reached");
      break;
    default:
      LogEvent(state, alliance ? Side::Alliance : Side::Empire, "The battle is over");
      break;
  }
}

BattleResultSet BuildResults(const BattleState& state)
{
  BattleResultSet results;
  results.outcome.alliancePresent = SidePresent(state, Side::Alliance);
  results.outcome.empirePresent = SidePresent(state, Side::Empire);
  results.outcome.allianceWithdrew
      = state.sides[SideIndex(Side::Alliance)].everWithdrew && !results.outcome.alliancePresent;
  results.outcome.empireWithdrew = state.sides[SideIndex(Side::Empire)].everWithdrew && !results.outcome.empirePresent;
  for (const ShipState& ship : state.ships) {
    const UnitState& unit = state.units[ship.unit];
    ShipResult result;
    result.unit = ship.unit;
    result.fate = unit.fate;
    result.hull = unit.fate == UnitFate::Destroyed ? 0 : std::max(1, static_cast<int32_t>(std::ceil(ship.hull)));
    result.criticals = ship.criticals;
    results.ships.push_back(result);
  }
  for (const SquadronState& squadron : state.squadrons) {
    const UnitState& unit = state.units[squadron.unit];
    SquadronResult result;
    result.unit = squadron.unit;
    result.fate = unit.fate;
    if (unit.fate == UnitFate::Active && squadron.place == SquadronPlace::Docked) {
      result.fate = UnitFate::Docked;
    }
    result.size = unit.fate == UnitFate::Destroyed ? 0 : squadron.alive;
    result.carrier = squadron.carrier;
    results.squadrons.push_back(result);
  }
  for (const StationState& station : state.stations) {
    const UnitState& unit = state.units[station.unit];
    DeathStarResult result;
    result.unit = station.unit;
    result.alive = unit.fate != UnitFate::Destroyed && station.hull > 0.0f;
    result.withdrew = unit.fate == UnitFate::Withdrawn;
    result.hull = result.alive ? std::max(1, static_cast<int32_t>(std::ceil(station.hull))) : 0;
    results.deathStars.push_back(result);
  }
  return results;
}

void LogEvent(BattleState& state, Side side, std::string text, UnitIndex unit)
{
  state.events.push_back({state.tick, side, std::move(text), unit});
  if (state.events.size() > static_cast<size_t>(kEventLogSize)) {
    state.events.erase(state.events.begin());
  }
}

void AddEffect(BattleState& state, ViewEffectKind kind, Vec2 position, float size, UnitIndex unit)
{
  state.effects.push_back({kind, position, size, state.tick, unit});
}

} // namespace reb::battle::sim
