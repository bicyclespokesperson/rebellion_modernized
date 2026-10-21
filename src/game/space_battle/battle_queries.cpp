#include "space_battle/battle_queries.h"

#include <algorithm>
#include <cmath>

#include "space_battle/math2d.h"

namespace reb::battle
{
namespace
{
using namespace tuning;

float CritFactor(int32_t hits)
{
  return std::max(0.0f, 1.0f - kCritSystemLoss * static_cast<float>(hits));
}

float Sublight(const UnitProfile& profile)
{
  return profile.maxSpeed / kCapitalSpeedPerSublight;
}

bool TractorStopped(const BattleState& state, UnitIndex unit)
{
  const ShipState* ship = state.Ship(unit);
  return ship != nullptr && ship->tractorHeld > 0.0f && ship->tractorHeld >= Sublight(state.Profile(unit));
}

float IonValue(const BattleState& state, UnitIndex target)
{
  const ShipState* ship = state.Ship(target);
  return ship != nullptr && ship->shield > 0.0f ? 1.0f : kIonValueShieldsDown;
}

float ShipDpsNow(const BattleState& state, UnitIndex shooter, UnitIndex target)
{
  const UnitProfile& profile = state.Profile(shooter);
  const UnitState& self = state.units[shooter];
  const UnitState& other = state.units[target];
  const bool fighter = other.kind == UnitKind::Squadron;
  float reachRadius = state.Profile(target).radius;
  if (const SquadronState* squadron = state.Squadron(target)) {
    reachRadius = squadron->spread;
  }
  const float distance = std::max(0.0f, Distance(self.position, other.position) - reachRadius);
  const float bearing = AngleDelta(self.heading, AngleOf(other.position - self.position));
  const float evasion = fighter ? kFighterTargetAccuracy * Evasion(TurnPoints(state, shooter), TurnPoints(state, target))
                                      * (1.0f - CommanderBonus(state, target, kCommanderEvasionBonus))
                                : 1.0f;
  float total = 0.0f;
  for (int i = 0; i < profile.batteryCount; ++i) {
    const BatteryProfile& battery = profile.batteries[i];
    if (distance > battery.range || !InArc(battery.arc, bearing)) {
      continue;
    }
    if (fighter) {
      total += battery.dpsVsFighter * evasion;
    } else {
      const float ion = battery.kind == WeaponKind::Ion ? IonValue(state, target) : 1.0f;
      total += battery.dpsVsCapital * ion * ShipAccuracy(profile, distance, battery.range);
    }
  }
  return total * WeaponOutput(state, shooter);
}

float ShipDpsBestArc(const BattleState& state, UnitIndex shooter, UnitIndex target)
{
  const UnitProfile& profile = state.Profile(shooter);
  const bool fighter = state.units[target].kind == UnitKind::Squadron;
  const float evasion = fighter ? kFighterTargetAccuracy * Evasion(TurnPoints(state, shooter), TurnPoints(state, target))
                                : 1.0f;
  std::array<float, kArcCount> arcs{};
  for (int i = 0; i < profile.batteryCount; ++i) {
    const BatteryProfile& battery = profile.batteries[i];
    const auto arc = static_cast<size_t>(battery.arc);
    if (fighter) {
      arcs[arc] += battery.dpsVsFighter * evasion;
    } else {
      const float ion = battery.kind == WeaponKind::Ion ? IonValue(state, target) : 1.0f;
      arcs[arc] += battery.dpsVsCapital * ion * ShipAccuracy(profile, 0.5f * battery.range, battery.range);
    }
  }
  return *std::max_element(arcs.begin(), arcs.end()) * WeaponOutput(state, shooter);
}

float SquadronDps(const BattleState& state, UnitIndex shooter, UnitIndex target, DpsGeometry geometry)
{
  const SquadronState& squadron = *state.Squadron(shooter);
  const UnitProfile& profile = state.Profile(shooter);
  if (geometry == DpsGeometry::Now
      && Distance(squadron.centroid, state.units[target].position)
             > profile.maxRange + squadron.spread + state.Profile(target).radius + kSquadronEngageSlack) {
    return 0.0f;
  }
  const float accuracyBonus = 1.0f + CommanderBonus(state, shooter, kCommanderAccuracyBonus);
  float perCraft = 0.0f;
  if (state.units[target].kind == UnitKind::Squadron) {
    perCraft = profile.craftGunVsFighter * kCraftVsCraftAccuracy
               * Evasion(TurnPoints(state, shooter), TurnPoints(state, target));
  } else if (state.units[target].kind == UnitKind::CapitalShip) {
    const ShipState& ship = *state.Ship(target);
    const float ion = profile.craftIonVsCapital * (ship.shield > 0.0f ? 1.0f : kIonValueShieldsDown);
    float torpedo = 0.0f;
    int torpedoes = 0;
    for (int i = 0; i < squadron.craftCount; ++i) {
      const CraftState& craft = state.craft[squadron.firstCraft + i];
      torpedoes += craft.alive ? craft.torpedoes : 0;
    }
    if (torpedoes > 0) {
      torpedo = kTorpedoDamage / kTorpedoSeconds * (ship.shield > 0.0f ? kTorpedoVsShields : 1.0f) * kTorpedoAccuracy;
    }
    perCraft = (profile.craftGunVsCapital + ion) * kCraftVsShipAccuracy + torpedo;
  }
  return static_cast<float>(squadron.alive) * perCraft * kCraftDutyCycle * std::min(accuracyBonus, 1.5f);
}

} // namespace

PowerFactors CurrentPower(const BattleState& state, UnitIndex unit)
{
  const ShipState* ship = state.Ship(unit);
  if (ship == nullptr) {
    return kPowerRouting[0];
  }
  const PowerFactors& to = kPowerRouting[static_cast<size_t>(ship->power)];
  if (ship->powerTicks <= 0) {
    return to;
  }
  const PowerFactors& from = kPowerRouting[static_cast<size_t>(ship->powerFrom)];
  return {0.5f * (from.weapons + to.weapons), 0.5f * (from.shields + to.shields), 0.5f * (from.engines + to.engines)};
}

float WeaponOutput(const BattleState& state, UnitIndex unit)
{
  const ShipState* ship = state.Ship(unit);
  if (ship == nullptr) {
    return 1.0f;
  }
  const float hull = kHullOutputFloor + (1.0f - kHullOutputFloor) * state.HullFraction(unit);
  return hull * CritFactor(ship->criticals.weaponRecharge) * CurrentPower(state, unit).weapons;
}

float ShieldRegen(const BattleState& state, UnitIndex unit)
{
  const ShipState* ship = state.Ship(unit);
  if (ship == nullptr) {
    return 0.0f;
  }
  return state.Profile(unit).shieldRecharge * state.HullFraction(unit) * CritFactor(ship->criticals.shieldRecharge)
         * CurrentPower(state, unit).shields;
}

float MaxSpeed(const BattleState& state, UnitIndex unit)
{
  const UnitProfile& profile = state.Profile(unit);
  if (profile.kind == UnitKind::Squadron) {
    return profile.maxSpeed;
  }
  const ShipState* ship = state.Ship(unit);
  if (ship == nullptr || ship->engineOffTicks > 0) {
    return 0.0f;
  }
  const float rating = state.sides[SideIndex(profile.side)].commandRating;
  float speed = profile.maxSpeed * (1.0f + kAdmiralMaxSpeedBonus * rating) * CurrentPower(state, unit).engines
                * CritFactor(ship->criticals.speed);
  const float sublight = Sublight(profile);
  if (ship->tractorHeld > 0.0f) {
    speed = ship->tractorHeld >= sublight ? 0.0f : speed * (1.0f - ship->tractorHeld / sublight);
  }
  return speed;
}

float TurnPoints(const BattleState& state, UnitIndex unit)
{
  const UnitProfile& profile = state.Profile(unit);
  return profile.kind == UnitKind::CapitalShip ? profile.turnPoints + state.AdmiralTurnTerm(profile.side)
                                               : profile.turnPoints;
}

float TurnRate(const BattleState& state, UnitIndex unit)
{
  const UnitProfile& profile = state.Profile(unit);
  if (profile.kind == UnitKind::Squadron) {
    return profile.turnPoints * kTurnRatePerPoint;
  }
  const ShipState* ship = state.Ship(unit);
  if (ship == nullptr) {
    return 0.0f;
  }
  float rate = TurnPoints(state, unit) * kTurnRatePerPoint * CurrentPower(state, unit).engines
               * std::max(kCritSystemLoss, CritFactor(ship->criticals.speed));
  if (ship->engineOffTicks > 0 || TractorStopped(state, unit)) {
    rate *= kStoppedTurnFactor;
  }
  return rate;
}

float TractorPower(const BattleState& state, UnitIndex unit)
{
  const ShipState* ship = state.Ship(unit);
  const UnitProfile& profile = state.Profile(unit);
  if (ship == nullptr || profile.tractorPower <= 0.0f || ship->systemsOffTicks > 0) {
    return 0.0f;
  }
  return profile.tractorPower * state.HullFraction(unit) * CritFactor(ship->criticals.tractor);
}

bool TractorReady(const BattleState& state, UnitIndex ship)
{
  return state.InSpace(ship) && TractorPower(state, ship) > 0.0f;
}

bool WellActive(const BattleState& state, UnitIndex unit)
{
  const ShipState* ship = state.Ship(unit);
  return ship != nullptr && state.InSpace(unit) && state.Profile(unit).gravityWell && ship->systemsOffTicks <= 0;
}

bool InEnemyWell(const BattleState& state, UnitIndex unit)
{
  const UnitState& self = state.units[unit];
  for (const ShipState& ship : state.ships) {
    const UnitState& projector = state.units[ship.unit];
    if (projector.side != self.side && WellActive(state, ship.unit)
        && DistanceSq(projector.position, self.position) <= kGravityWellRadius * kGravityWellRadius) {
      return true;
    }
  }
  return false;
}

bool CanJump(const BattleState& state, UnitIndex unit, JumpBlocker* blocker)
{
  JumpBlocker why = JumpBlocker::None;
  const UnitProfile& profile = state.Profile(unit);
  if (!state.snapshot.params.sides[SideIndex(profile.side)].canRetreat) {
    why = JumpBlocker::SideCannotRetreat;
  } else if (const ShipState* ship = state.Ship(unit)) {
    const bool primary = profile.hyperdrive > 0 && ship->criticals.hyperdrivePrimary == 0;
    const bool backup = profile.hyperdriveBackup > 0 && ship->criticals.hyperdriveBackup == 0;
    if (!primary && !backup) {
      why = JumpBlocker::NoHyperdrive;
    } else if (TractorStopped(state, unit)) {
      why = JumpBlocker::Tractored;
    } else if (MaxSpeed(state, unit) <= 0.0f) {
      why = JumpBlocker::Engines;
    }
  } else if (profile.kind == UnitKind::Squadron && !profile.CanJumpAlone()) {
    why = JumpBlocker::NoHyperdrive;
  }
  if (why == JumpBlocker::None && InEnemyWell(state, unit)) {
    why = JumpBlocker::GravityWell;
  }
  if (blocker != nullptr) {
    *blocker = why;
  }
  return why == JumpBlocker::None;
}

float ShipAccuracy(const UnitProfile& shooter, float distance, float range)
{
  const float detection = std::clamp(shooter.detection / kDetectionForNoFalloff, 0.0f, 1.0f);
  const float reach = range > 0.0f ? std::clamp(distance / range, 0.0f, 1.0f) : 1.0f;
  return std::clamp(kAccuracyNear - kAccuracyFalloff * (1.0f - detection) * reach, 0.05f, 0.99f);
}

float Evasion(float shooterTurnPoints, float targetTurnPoints)
{
  if (targetTurnPoints <= 0.0f) {
    return 1.0f;
  }
  return std::clamp(shooterTurnPoints / targetTurnPoints, kEvasionFloor, 1.0f);
}

float CommanderBonus(const BattleState& state, UnitIndex squadron, float perSkill)
{
  const SquadronState* self = state.Squadron(squadron);
  const SideState& side = state.sides[SideIndex(state.units[squadron].side)];
  if (self == nullptr || side.commander == kNoUnit) {
    return 0.0f;
  }
  const auto* character = std::get_if<CharacterEntry>(&state.snapshot.units[side.commander]);
  if (character == nullptr || !state.InSpace(character->ship)
      || DistanceSq(state.units[character->ship].position, self->centroid) > kCommanderRadius * kCommanderRadius) {
    return 0.0f;
  }
  return perSkill * std::clamp(side.commanderCombat / kCombatForFullBonus, 0.0f, 1.0f);
}

float ExpectedDps(const BattleState& state, UnitIndex shooter, UnitIndex target, DpsGeometry geometry)
{
  if (!state.InSpace(shooter) || !state.InSpace(target) || state.units[target].kind == UnitKind::DeathStar) {
    return 0.0f;
  }
  switch (state.units[shooter].kind) {
    case UnitKind::CapitalShip:
      return geometry == DpsGeometry::Now ? ShipDpsNow(state, shooter, target) : ShipDpsBestArc(state, shooter, target);
    case UnitKind::Squadron:
      return SquadronDps(state, shooter, target, geometry);
    default:
      return 0.0f;
  }
}

float EffectiveHealth(const BattleState& state, UnitIndex unit)
{
  if (const ShipState* ship = state.Ship(unit)) {
    return ship->hull + ship->shield;
  }
  if (const SquadronState* squadron = state.Squadron(unit)) {
    float total = 0.0f;
    for (int i = 0; i < squadron->craftCount; ++i) {
      const CraftState& craft = state.craft[squadron->firstCraft + i];
      total += craft.alive ? craft.hull + craft.shield : 0.0f;
    }
    return total;
  }
  if (const StationState* station = state.Station(unit)) {
    return station->hull;
  }
  return 0.0f;
}

float MaxEffectiveHealth(const BattleState& state, UnitIndex unit)
{
  const UnitProfile& profile = state.Profile(unit);
  if (profile.kind == UnitKind::Squadron) {
    return static_cast<float>(profile.craftMax) * (kCraftHull + profile.craftShield);
  }
  return profile.hullMax + profile.shieldMax;
}

float Regeneration(const BattleState& state, UnitIndex unit)
{
  if (state.Ship(unit) != nullptr) {
    return ShieldRegen(state, unit);
  }
  if (const SquadronState* squadron = state.Squadron(unit); squadron && state.Profile(unit).craftShield > 0.0f) {
    return static_cast<float>(squadron->alive) * kCraftShieldRechargePerSecond;
  }
  return 0.0f;
}

float TimeToKill(const BattleState& state, UnitIndex unit, float dps)
{
  const float net = dps - Regeneration(state, unit);
  return net > 0.0f ? EffectiveHealth(state, unit) / net : -1.0f;
}

float PriorityWeight(const TargetPriorities& priorities, uint32_t tags, int8_t* rank, TargetTag* tag)
{
  const int count = std::min<int>(priorities.count, TargetPriorities::kMaxTags);
  for (int i = 0; i < count; ++i) {
    if ((tags & TagBit(priorities.tags[i])) != 0) {
      if (rank != nullptr) {
        *rank = static_cast<int8_t>(i);
      }
      if (tag != nullptr) {
        *tag = priorities.tags[i];
      }
      return kPriorityWeights[i];
    }
  }
  if (rank != nullptr) {
    *rank = -1;
  }
  if (tag != nullptr) {
    *tag = TargetTag::Nearest;
  }
  return kUnlistedTargetWeight;
}

float SideStrength(const BattleState& state, Side side)
{
  // Batteries add firepower but no durability: they cannot be destroyed, and they cannot hold the system alone.
  float firepower = PlanetaryFirepower(state, side);
  float durability = 0.0f;
  for (UnitIndex i = 0; i < static_cast<UnitIndex>(state.units.size()); ++i) {
    const UnitState& unit = state.units[i];
    if (unit.side != side || !state.Present(i)) {
      continue;
    }
    const UnitProfile& profile = state.Profile(i);
    switch (unit.kind) {
      case UnitKind::CapitalShip:
        firepower += profile.firepower * WeaponOutput(state, i);
        break;
      case UnitKind::Squadron:
        firepower += profile.firepower * state.HullFraction(i);
        break;
      case UnitKind::DeathStar:
        firepower += profile.firepower;
        break;
      case UnitKind::Passive:
        continue;
    }
    durability += EffectiveHealth(state, i);
  }
  return std::sqrt(firepower * durability);
}

float DurableTotal(const BattleState& state, Side side)
{
  float total = 0.0f;
  for (const ShipState& ship : state.ships) {
    if (state.units[ship.unit].side == side && state.Present(ship.unit)) {
      total += ship.hull;
    }
  }
  for (const SquadronState& squadron : state.squadrons) {
    if (state.units[squadron.unit].side != side || !state.Present(squadron.unit)) {
      continue;
    }
    for (int i = 0; i < squadron.craftCount; ++i) {
      const CraftState& craft = state.craft[squadron.firstCraft + i];
      total += craft.alive ? craft.hull : 0.0f;
    }
  }
  for (const StationState& station : state.stations) {
    if (state.units[station.unit].side == side && state.Present(station.unit)) {
      total += station.hull;
    }
  }
  return total;
}

bool SidePresent(const BattleState& state, Side side)
{
  if (SideInSpace(state, side)) {
    return true;
  }
  return std::any_of(state.squadrons.begin(), state.squadrons.end(), [&](const SquadronState& squadron) {
    return state.units[squadron.unit].side == side && !squadron.launched && state.Present(squadron.unit);
  });
}

bool SideInSpace(const BattleState& state, Side side)
{
  return std::any_of(state.units.begin(), state.units.end(), [&](const UnitState& unit) {
    return unit.side == side && unit.kind != UnitKind::Passive && unit.inSpace;
  });
}

bool CoveredWhenDocked(const BattleState& state, UnitIndex squadron)
{
  const Side side = state.units[squadron].side;
  for (UnitIndex i = 0; i < static_cast<UnitIndex>(state.units.size()); ++i) {
    const UnitState& unit = state.units[i];
    if (i != squadron && unit.side == side && unit.kind != UnitKind::Passive && unit.inSpace) {
      return true;
    }
  }
  return false;
}

int FightersInSpace(const BattleState& state, Side side)
{
  int count = 0;
  for (const SquadronState& squadron : state.squadrons) {
    if (state.units[squadron.unit].side == side && squadron.place == SquadronPlace::InSpace) {
      count += squadron.alive;
    }
  }
  return count;
}

Vec2 DeploymentCenter(const BattleState& state, Side side)
{
  return state.sides[SideIndex(side)].deploymentCenter;
}

Vec2 ClampToDeployment(const BattleState& state, Side side, Vec2 position)
{
  const Vec2 center = DeploymentCenter(state, side);
  const Vec2 offset = position - center;
  const float radius = state.sides[SideIndex(side)].deploymentRadius;
  return LengthSq(offset) <= radius * radius ? position : center + Normalized(offset) * radius;
}

float PlanetaryCoverRadius()
{
  return kPlanetRadius + kPlanetaryCoverReach;
}

bool HasPlanetaryCover(const BattleState& state, Side side)
{
  return std::any_of(state.defenseProfiles.begin(), state.defenseProfiles.end(),
                     [&](const DefenseProfile& defense) { return defense.side == side; });
}

bool InPlanetaryCover(const BattleState& state, Side side, Vec2 position, float radius)
{
  const float reach = PlanetaryCoverRadius() + radius;
  return HasPlanetaryCover(state, side) && DistanceSq(position, state.planetCenter) <= reach * reach;
}

bool PlanetaryTarget(const BattleState& state, Side side, UnitIndex unit)
{
  return state.InSpace(unit) && state.units[unit].kind == UnitKind::CapitalShip && state.units[unit].side != side
         && InPlanetaryCover(state, side, state.units[unit].position, state.Profile(unit).radius);
}

float PlanetaryFirepower(const BattleState& state, Side side)
{
  float total = 0.0f;
  for (const DefenseProfile& defense : state.defenseProfiles) {
    total += defense.side == side ? defense.dps * (defense.weapon == WeaponKind::Ion ? 0.5f : 1.0f) : 0.0f;
  }
  return total;
}

float PlanetaryAccuracy(float distanceFromSurface)
{
  UnitProfile gun;
  gun.detection = kPlanetaryDetection;
  return ShipAccuracy(gun, std::max(0.0f, distanceFromSurface), kPlanetaryCoverReach);
}

float PlanetaryDpsOn(const BattleState& state, UnitIndex unit)
{
  const Side enemy = Opponent(state.units[unit].side);
  if (!PlanetaryTarget(state, enemy, unit)) {
    return 0.0f;
  }
  const float surface = Distance(state.units[unit].position, state.planetCenter) - kPlanetRadius
                        - state.Profile(unit).radius;
  float total = 0.0f;
  for (const DefenseProfile& defense : state.defenseProfiles) {
    if (defense.side == enemy) {
      total += defense.dps * (defense.weapon == WeaponKind::Ion ? IonValue(state, unit) : 1.0f);
    }
  }
  return total * PlanetaryAccuracy(surface);
}

bool SuperlaserReady(const BattleState& state, UnitIndex unit)
{
  const StationState* station = state.Station(unit);
  const auto* entry = std::get_if<DeathStarEntry>(&state.snapshot.units[unit]);
  return station != nullptr && entry != nullptr && entry->completed && state.InSpace(unit) && station->chargeTicks <= 0
         && station->firingAt == kNoUnit;
}

bool TrenchRunPossible(const BattleState& state, UnitIndex unit)
{
  const auto* entry = std::get_if<DeathStarEntry>(&state.snapshot.units[unit]);
  if (entry == nullptr || !entry->completed || entry->shieldFacility || !state.InSpace(unit)) {
    return false;
  }
  const Side side = state.units[unit].side;
  return FightersInSpace(state, Opponent(side)) > FightersInSpace(state, side);
}

} // namespace reb::battle
