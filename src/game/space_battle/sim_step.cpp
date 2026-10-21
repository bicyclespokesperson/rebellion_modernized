#include <algorithm>

#include "space_battle/sim_internal.h"

namespace reb::battle::sim
{
namespace
{
using namespace tuning;

void RememberPositions(BattleState& state)
{
  for (UnitState& unit : state.units) {
    unit.previousPosition = unit.position;
    unit.previousHeading = unit.heading;
  }
  for (CraftState& craft : state.craft) {
    craft.previousPosition = craft.position;
    craft.previousHeading = craft.heading;
  }
}

// Group and ship decisions are staggered (once a second, twice a second) to bound the per-tick cost.
void Think(BattleState& state)
{
  for (GroupState& group : state.groups) {
    if (state.tick < group.nextThinkTick) {
      continue;
    }
    group.nextThinkTick = static_cast<int32_t>(state.tick) + Ticks(kGroupThinkSeconds);
    // Squadrons choose their own targets (sim_fighters.cpp); task forces share a scored list.
    if (!group.flight) {
      ScoreGroupTargets(state, group);
      AllocateFire(state, group);
      ThinkGroupMovement(state, group);
    }
  }
  for (ShipState& ship : state.ships) {
    if (!state.InSpace(ship.unit) || state.tick < ship.nextThinkTick) {
      continue;
    }
    ship.nextThinkTick = static_cast<int32_t>(state.tick) + Ticks(kShipThinkSeconds);
    ThinkShip(state, ship.unit);
  }
  ThinkSquadrons(state);
}

void PruneEffects(BattleState& state)
{
  const int64_t oldest = state.tick - Ticks(kEffectSeconds);
  std::erase_if(state.effects, [&](const Effect& effect) { return effect.tick < oldest; });
}

} // namespace

void Step(BattleState& state)
{
  if (state.finished) {
    return;
  }
  if (state.endReason != EndReason::None) {
    RememberPositions(state);
    MoveShips(state);
    FlyCraft(state);
    UpdateSquadronBounds(state);
    std::erase_if(state.shots, [&](const PendingShot& shot) { return shot.impactTick <= state.tick; });
    ++state.tick;
    PruneEffects(state);
    state.finished = state.tick >= state.outroEndTick;
    return;
  }
  PopFinishedOrders(state);
  RememberPositions(state);
  UpdateSides(state);
  UpdateTractors(state);
  Think(state);
  MoveShips(state);
  FlyCraft(state);
  UpdateSquadronBounds(state);
  ResolveShots(state);
  FireBatteries(state);
  FireDefenses(state);
  FireCraft(state);
  UpdateShipSystems(state);
  UpdateHangars(state);
  UpdateWithdrawal(state);
  UpdateDeathStars(state);
  ++state.tick;
  PruneEffects(state);
  UpdateEnding(state);
}

} // namespace reb::battle::sim
