#pragma once
#include <string>

#include "space_battle/battle_queries.h"
#include "space_battle/battle_state.h"

// The step functions of the simulation, one file per system (sim_*.cpp). Only Battle and the tests call these.
namespace reb::battle::sim
{

inline constexpr int32_t Ticks(float seconds)
{
  return static_cast<int32_t>(seconds * kTicksPerSecond + 0.5f);
}

// sim_setup.cpp: profiles, units, groups and deployment from state.snapshot and state.options.
void Initialize(BattleState& state);
GroupId CreateGroup(BattleState& state, Side side, bool flight, std::string name);
void SetGroup(BattleState& state, UnitIndex unit, GroupId group);

// sim_orders.cpp
bool ValidateCommand(const BattleState& state, const Command& command);
void ApplyCommand(BattleState& state, const Command& command);
void PopFinishedOrders(BattleState& state);

// sim_step.cpp: one tick of everything below, in order.
void Step(BattleState& state);

// sim_targeting.cpp (§5.3)
void ScoreGroupTargets(BattleState& state, GroupState& group);
void AllocateFire(BattleState& state, GroupState& group);
// Best target for one battery now, or kNoUnit; `craft` receives the craft index for a squadron target.
UnitIndex SelectBatteryTarget(const BattleState& state, UnitIndex ship, int battery, int32_t* craft);
float ShipTargetScore(const BattleState& state, UnitIndex ship, const ScoredTarget& target);

// sim_movement.cpp (§5.2, §6.2-6.3)
void ThinkGroupMovement(BattleState& state, GroupState& group);
void ThinkShip(BattleState& state, UnitIndex ship);
void MoveShips(BattleState& state);
float ChooseHeading(const BattleState& state, UnitIndex ship, Facing facing, Vec2 goal, float* firepower);

// sim_weapons.cpp (§6.3-6.6)
void FireBatteries(BattleState& state);
void ResolveShots(BattleState& state);
void UpdateShipSystems(BattleState& state);
// `planetary`: a KDY-150's ion fire, whose disruption goes for the engines first (§6.13).
void DamageShip(BattleState& state, UnitIndex target, float damage, ShotKind kind, Subsystem focus,
                bool planetary = false);
void DamageCraft(BattleState& state, UnitIndex squadron, int32_t craft, float damage);
void DestroyUnit(BattleState& state, UnitIndex unit);
void QueueShot(BattleState& state, PendingShot shot, float speed);
// Where a decided shot flies: leading the target, onto its hull for a hit, past it for a miss. No random draws.
void AimShot(const BattleState& state, PendingShot& shot, Vec2 aim, Vec2 velocity, float radius, float speed, int salt);

// sim_fighters.cpp (§6.7-6.8)
void ThinkSquadrons(BattleState& state);
void FlyCraft(BattleState& state);
void FireCraft(BattleState& state);
void UpdateHangars(BattleState& state);
void UpdateSquadronBounds(BattleState& state);
void LaunchSquadron(BattleState& state, UnitIndex squadron);
void DockSquadron(BattleState& state, UnitIndex squadron, UnitIndex carrier, bool rearm);
int32_t FreeHangarSlots(const BattleState& state, UnitIndex carrier);
void RehomeSquadrons(BattleState& state, UnitIndex lostCarrier);

// sim_special.cpp (§6.9-6.12)
void UpdateTractors(BattleState& state);
void UpdateWithdrawal(BattleState& state);
void UpdateDeathStars(BattleState& state);
void BeginWithdrawal(BattleState& state, UnitIndex unit);

// sim_planet.cpp (§6.13)
void InitDefenses(BattleState& state);
void FireDefenses(BattleState& state);

// sim_end.cpp (§6.11)
void UpdateSides(BattleState& state);
void UpdateEnding(BattleState& state);
BattleResultSet BuildResults(const BattleState& state);

// Events and cosmetic effects.
void LogEvent(BattleState& state, Side side, std::string text, UnitIndex unit = kNoUnit);
void AddEffect(BattleState& state, ViewEffectKind kind, Vec2 position, float size, UnitIndex unit);
std::string UnitName(const BattleState& state, UnitIndex unit);

} // namespace reb::battle::sim
