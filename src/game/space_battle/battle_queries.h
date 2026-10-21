#pragma once
#include "space_battle/battle_state.h"
#include "space_battle/tuning.h"

// Derived, read-only values of a running battle: what a unit can do right now given damage, critical hits, power,
// disruption and commanders. The simulation, the commanders and the view model all read the battle through these,
// so they agree on every number.
namespace reb::battle
{

// Power routing in effect, blending old and new settings during a transition (§5.5).
tuning::PowerFactors CurrentPower(const BattleState& state, UnitIndex ship);
// Weapon output multiplier of a capital ship: hull, weapon criticals and power (§6.5).
float WeaponOutput(const BattleState& state, UnitIndex ship);
// Shield recharge per second of a capital ship.
float ShieldRegen(const BattleState& state, UnitIndex ship);
float MaxSpeed(const BattleState& state, UnitIndex unit); // m/s now
float TurnRate(const BattleState& state, UnitIndex unit); // rad/s now
// Turn points as the evasion term compares them (capital ships include the admiral term).
float TurnPoints(const BattleState& state, UnitIndex unit);
float TractorPower(const BattleState& state, UnitIndex ship);
bool TractorReady(const BattleState& state, UnitIndex ship);
bool WellActive(const BattleState& state, UnitIndex ship);
// Inside an active gravity well projected by the other side.
bool InEnemyWell(const BattleState& state, UnitIndex unit);
// Whether the unit could jump this tick if spooled (§6.10), and if not, why.
bool CanJump(const BattleState& state, UnitIndex unit, JumpBlocker* blocker = nullptr);

// Accuracy of a capital-ship battery against a capital ship at `distance`: falls with range, less so with detection.
float ShipAccuracy(const UnitProfile& shooter, float distance, float range);
// The original's evasion term: max(0.1, attacker turn / target turn), capped at 1 here because it scales accuracy.
float Evasion(float shooterTurnPoints, float targetTurnPoints);
float CommanderBonus(const BattleState& state, UnitIndex squadron, float perSkill);

enum class DpsGeometry : uint8_t {
  Now,       // only batteries that bear and reach now
  BestArc,   // as if the strongest arc against this target were brought to bear in range
};
// Expected damage per second `shooter` deals to `target` (accuracy included). Ion counts against shields only.
float ExpectedDps(const BattleState& state, UnitIndex shooter, UnitIndex target, DpsGeometry geometry);
float EffectiveHealth(const BattleState& state, UnitIndex unit); // hull and shields left
float MaxEffectiveHealth(const BattleState& state, UnitIndex unit);
float Regeneration(const BattleState& state, UnitIndex unit); // shield points per second
// Seconds for `dps` to destroy the unit against its regeneration; negative when it never would.
float TimeToKill(const BattleState& state, UnitIndex unit, float dps);

// Weight of the best-ranked priority tag in `tags` (8, 4, 2, 1), or the unlisted weight; `rank` is -1 then.
float PriorityWeight(const TargetPriorities& priorities, uint32_t tags, int8_t* rank, TargetTag* tag);

// Strength estimate (§9): sqrt(firepower x durability) of the side's units still in the battle.
float SideStrength(const BattleState& state, Side side);
// Hull left on ships, craft and stations: any drop counts as progress for the stalemate rule (§6.11).
float DurableTotal(const BattleState& state, Side side);
// Still in the battle (§6.11): a unit in space, or a squadron waiting for its first launch from a hangar that is
// there. A squadron that has flown and docked again does not keep its side in the battle.
bool SidePresent(const BattleState& state, Side side);
// Holding space: a unit of the side is in space.
bool SideInSpace(const BattleState& state, Side side);
// Another unit of the squadron's side is in space, so it can dock without conceding the battle.
bool CoveredWhenDocked(const BattleState& state, UnitIndex squadron);
int FightersInSpace(const BattleState& state, Side side);

// The side's deployment zone (§4): the attacker drops out of hyperspace along the arrival bearing, the defender holds
// between the planet and the arrival vector.
Vec2 DeploymentCenter(const BattleState& state, Side side);
Vec2 ClampToDeployment(const BattleState& state, Side side, Vec2 position);

// Planetary batteries (§6.13). Cover is the space within kPlanetaryCoverReach of the planet's surface.
float PlanetaryCoverRadius(); // from the planet's centre
bool HasPlanetaryCover(const BattleState& state, Side side); // the side has batteries on the planet
// Inside `side`'s cover: its batteries can reach a hull of `radius` at `position`.
bool InPlanetaryCover(const BattleState& state, Side side, Vec2 position, float radius = 0.0f);
// A capital ship the side's batteries would fire on: an enemy ship in space, inside the cover.
bool PlanetaryTarget(const BattleState& state, Side side, UnitIndex unit);
// Damage per second of all the side's batteries, ion at half (it strips shields but never destroys a ship).
float PlanetaryFirepower(const BattleState& state, Side side);
float PlanetaryAccuracy(float distanceFromSurface);
// Expected damage per second the enemy's batteries deal `unit` where it is now (accuracy included).
float PlanetaryDpsOn(const BattleState& state, UnitIndex unit);

bool SuperlaserReady(const BattleState& state, UnitIndex station);
// The trench run's conditions (§6.12): complete, no shield facility, more Alliance fighters in play than Imperial.
bool TrenchRunPossible(const BattleState& state, UnitIndex station);

} // namespace reb::battle
