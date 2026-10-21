// Planetary batteries (tactical_combat.md §6.13): the KDY-150 ion cannons and LNR series turbolaser batteries of the
// system fire from the planet's surface at enemy capital ships inside their cover. They cannot be destroyed or ordered:
// they shoot at the intruder nearest the planet, or at the side's Concentrate Fire target when it is in reach.
#include <algorithm>
#include <cmath>

#include "space_battle/math2d.h"
#include "space_battle/sim_internal.h"

namespace reb::battle::sim
{
namespace
{
using namespace tuning;

float DistanceFromSurface(const BattleState& state, UnitIndex unit)
{
  return Distance(state.units[unit].position, state.planetCenter) - kPlanetRadius - state.Profile(unit).radius;
}

// An ion cannon moves on from a ship it has already stopped (shields down, engines out) to the next intruder.
bool WorthFiringOn(const BattleState& state, const DefenseProfile& defense, UnitIndex unit)
{
  if (!PlanetaryTarget(state, defense.side, unit)) {
    return false;
  }
  const ShipState& ship = *state.Ship(unit);
  return defense.weapon != WeaponKind::Ion || ship.shield > 0.0f || ship.engineOffTicks == 0;
}

bool SideHasFired(const BattleState& state, Side side)
{
  for (size_t i = 0; i < state.defenses.size(); ++i) {
    if (state.defenseProfiles[i].side == side && state.defenses[i].lastFireTick >= 0) {
      return true;
    }
  }
  return false;
}

UnitIndex ChooseTarget(const BattleState& state, const DefenseProfile& defense)
{
  const SideState& side = state.sides[SideIndex(defense.side)];
  if (side.concentrateTicks > 0 && WorthFiringOn(state, defense, side.concentrateTarget)) {
    return side.concentrateTarget;
  }
  UnitIndex best = kNoUnit;
  float bestDistance = 0.0f;
  for (const ShipState& ship : state.ships) {
    if (!WorthFiringOn(state, defense, ship.unit)) {
      continue;
    }
    const float distance = DistanceFromSurface(state, ship.unit);
    if (best == kNoUnit || distance < bestDistance) {
      best = ship.unit;
      bestDistance = distance;
    }
  }
  return best;
}

void Fire(BattleState& state, int index, UnitIndex target)
{
  const DefenseProfile& defense = state.defenseProfiles[static_cast<size_t>(index)];
  DefenseState& self = state.defenses[static_cast<size_t>(index)];
  const SideState& side = state.sides[SideIndex(defense.side)];
  const bool opening = !SideHasFired(state, defense.side);
  PendingShot shot;
  shot.fireTick = state.tick;
  shot.target = target;
  shot.battery = static_cast<int16_t>(index);
  shot.side = defense.side;
  shot.kind = defense.weapon == WeaponKind::Ion ? ShotKind::Ion : ShotKind::Turbolaser;
  shot.from = state.planetCenter + FromAngle(defense.siteAngle) * kPlanetRadius;
  shot.damage = defense.dps * kPlanetarySalvoSeconds;
  shot.shieldImpact = state.Ship(target)->shield > 0.0f;
  const float bonus = side.concentrateTicks > 0 && target == side.concentrateTarget ? kConcentrateAccuracyBonus : 0.0f;
  shot.hit = state.rng.Chance(std::min(kMaxHitChance, PlanetaryAccuracy(DistanceFromSurface(state, target)) + bonus));
  const float speed = kBoltSpeed[static_cast<size_t>(defense.weapon)];
  AimShot(state, shot, state.units[target].position, state.units[target].velocity, state.Profile(target).radius, speed,
          static_cast<int>(state.units.size()) * kMaxBatteries + index);
  QueueShot(state, shot, speed);
  self.lastFireTick = state.tick;
  if (opening) {
    LogEvent(state, defense.side, "Planetary defenses open fire on " + UnitName(state, target), target);
  }
}

} // namespace

void InitDefenses(BattleState& state)
{
  const std::vector<DefenseEntry>& entries = state.snapshot.defenses;
  state.defenseProfiles.clear();
  state.defenses.assign(entries.size(), DefenseState{});
  // The emplacements face the battle: spread evenly over the limb either side of the direction to the battle's centre.
  const float facing = AngleOf(Vec2{} - state.planetCenter);
  const auto count = static_cast<float>(entries.size());
  for (size_t i = 0; i < entries.size(); ++i) {
    const DefenseEntry& entry = entries[i];
    DefenseProfile profile;
    profile.side = entry.identity.side;
    profile.weapon = entry.weapon == WeaponKind::Ion ? WeaponKind::Ion : WeaponKind::Turbolaser;
    profile.dps = kPlanetaryDpsPerRootAttack * std::sqrt(static_cast<float>(std::max(0, entry.attackStrength)));
    const float slot = count > 1.0f ? (static_cast<float>(i) + 0.5f) / count * 2.0f - 1.0f : 0.0f;
    profile.siteAngle = WrapAngle(facing + slot * kPlanetarySiteSpread);
    state.defenseProfiles.push_back(profile);
    // Staggered so that a planet's batteries do not all fire on the same tick.
    state.defenses[i].cooldown = Ticks(kPlanetarySalvoSeconds) * static_cast<int32_t>(i + 1)
                                 / static_cast<int32_t>(entries.size() + 1);
  }
}

void FireDefenses(BattleState& state)
{
  for (size_t i = 0; i < state.defenses.size(); ++i) {
    DefenseState& defense = state.defenses[i];
    if (defense.cooldown > 0) {
      --defense.cooldown;
      continue;
    }
    const DefenseProfile& profile = state.defenseProfiles[i];
    defense.target = profile.dps > 0.0f ? ChooseTarget(state, profile) : kNoUnit;
    if (defense.target == kNoUnit) {
      defense.cooldown = Ticks(kBatteryRetargetSeconds);
      continue;
    }
    Fire(state, static_cast<int>(i), defense.target);
    defense.cooldown = Ticks(kPlanetarySalvoSeconds);
  }
}

} // namespace reb::battle::sim
