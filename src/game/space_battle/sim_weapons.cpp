#include <algorithm>
#include <cmath>

#include "space_battle/math2d.h"
#include "space_battle/sim_internal.h"

namespace reb::battle::sim
{
namespace
{
using namespace tuning;

enum class CritSystem : uint8_t { ShieldRecharge, WeaponRecharge, Tractor, Engines, Hyperdrive };
constexpr std::array<const char*, 5> kSystemNames{"shield recharge", "weapon recharge", "tractor beam", "engines",
                                                  "hyperdrive"};

ShotKind ShotKindOf(WeaponKind kind)
{
  switch (kind) {
    case WeaponKind::Turbolaser:
      return ShotKind::Turbolaser;
    case WeaponKind::Ion:
      return ShotKind::Ion;
    case WeaponKind::Laser:
      return ShotKind::Laser;
  }
  return ShotKind::Turbolaser;
}

int32_t RetargetTicks(const UnitProfile& profile)
{
  const float detection = std::clamp(profile.detection / kDetectionForNoFalloff, 0.0f, 1.0f);
  return std::max(1, Ticks(kBatteryRetargetSeconds * (1.0f - 0.5f * detection)));
}

// A miss flies past the target: a deterministic offset, so cosmetics never draw on the gameplay generator.
Vec2 MissOffset(int64_t tick, int index, float radius)
{
  const auto hash = static_cast<uint32_t>((tick * 2654435761LL) ^ (index * 40503));
  const float angle = static_cast<float>(hash % 6283U) * 0.001f;
  return FromAngle(angle) * (radius * 1.4f + 40.0f);
}

int32_t& SystemHits(CriticalHits& hits, CritSystem system)
{
  switch (system) {
    case CritSystem::ShieldRecharge:
      return hits.shieldRecharge;
    case CritSystem::WeaponRecharge:
      return hits.weaponRecharge;
    case CritSystem::Tractor:
      return hits.tractor;
    case CritSystem::Engines:
      return hits.speed;
    case CritSystem::Hyperdrive:
      return hits.hyperdrivePrimary;
  }
  return hits.speed;
}

bool SystemCanTakeHit(const UnitProfile& profile, const CriticalHits& hits, CritSystem system)
{
  switch (system) {
    case CritSystem::ShieldRecharge:
      return profile.shieldMax > 0.0f && hits.shieldRecharge < kCritSystemMax;
    case CritSystem::WeaponRecharge:
      return profile.batteryCount > 0 && hits.weaponRecharge < kCritSystemMax;
    case CritSystem::Tractor:
      return profile.tractorPower > 0.0f && hits.tractor < kCritSystemMax;
    case CritSystem::Engines:
      return profile.maxSpeed > 0.0f && hits.speed < kCritSystemMax;
    case CritSystem::Hyperdrive:
      return (profile.hyperdrive > 0 && hits.hyperdrivePrimary == 0)
             || (profile.hyperdriveBackup > 0 && hits.hyperdriveBackup == 0);
  }
  return false;
}

CritSystem FocusSystem(Subsystem focus, bool* any)
{
  *any = true;
  switch (focus) {
    case Subsystem::Shields:
      return CritSystem::ShieldRecharge;
    case Subsystem::Weapons:
      return CritSystem::WeaponRecharge;
    case Subsystem::Tractor:
      return CritSystem::Tractor;
    case Subsystem::Engines:
      return CritSystem::Engines;
    case Subsystem::Hyperdrive:
      return CritSystem::Hyperdrive;
    case Subsystem::Hull:
      break;
  }
  *any = false;
  return CritSystem::ShieldRecharge;
}

void ApplyCritical(BattleState& state, UnitIndex unit, CritSystem system)
{
  ShipState& ship = *state.Ship(unit);
  const UnitProfile& profile = state.Profile(unit);
  if (system == CritSystem::Hyperdrive) {
    // One hit per drive: the primary first, then the backup (§6.5).
    if (profile.hyperdrive > 0 && ship.criticals.hyperdrivePrimary == 0) {
      ship.criticals.hyperdrivePrimary = 1;
    } else {
      ship.criticals.hyperdriveBackup = 1;
    }
    const bool stranded = !SystemCanTakeHit(profile, ship.criticals, CritSystem::Hyperdrive);
    LogEvent(state, profile.side, UnitName(state, unit) + (stranded ? ": hyperdrive knocked out" : ": hyperdrive hit"), unit);
  } else {
    int32_t& hits = SystemHits(ship.criticals, system);
    ++hits;
    if (hits >= kCritSystemMax) {
      LogEvent(state, profile.side, UnitName(state, unit) + ": " + kSystemNames[static_cast<size_t>(system)] + " disabled", unit);
    }
  }
  AddEffect(state, ViewEffectKind::CriticalHit, state.units[unit].position, profile.radius * 0.4f, unit);
}

void RollCritical(BattleState& state, UnitIndex unit, Subsystem focus, float chance)
{
  if (!state.rng.Chance(std::min(chance, kMaxCritChance))) {
    return;
  }
  const UnitProfile& profile = state.Profile(unit);
  const CriticalHits& hits = state.Ship(unit)->criticals;
  bool focused = false;
  const CritSystem focusSystem = FocusSystem(focus, &focused);
  std::array<float, 5> weights{};
  float total = 0.0f;
  for (size_t i = 0; i < weights.size(); ++i) {
    const auto system = static_cast<CritSystem>(i);
    if (SystemCanTakeHit(profile, hits, system)) {
      weights[i] = kCritWeights[i] * (focused && system == focusSystem ? kSubsystemCritMultiplier : 1.0f);
      total += weights[i];
    }
  }
  if (total <= 0.0f) {
    return;
  }
  float roll = state.rng.Unit() * total;
  for (size_t i = 0; i < weights.size(); ++i) {
    if (weights[i] <= 0.0f) {
      continue;
    }
    roll -= weights[i];
    if (roll < 0.0f || i + 1 == weights.size()) {
      ApplyCritical(state, unit, static_cast<CritSystem>(i));
      return;
    }
  }
}

// A planetary ion cannon (§6.13) takes the engines whenever they are running; otherwise it drains a weapon arc.
void Disrupt(BattleState& state, UnitIndex unit, bool planetary)
{
  ShipState& ship = *state.Ship(unit);
  const UnitProfile& profile = state.Profile(unit);
  const bool wasDisrupted = ship.engineOffTicks > 0 || ship.systemsOffTicks > 0;
  const bool engines = planetary ? ship.engineOffTicks == 0 : state.rng.Chance(kIonEngineChance);
  if (engines) {
    const int32_t duration = Ticks(state.rng.Between(kIonEngineMinSeconds, kIonEngineMaxSeconds));
    if (ship.engineOffTicks == 0) {
      ship.engineOffTicks = duration;
      if (planetary) {
        LogEvent(state, profile.side, UnitName(state, unit) + ": engines knocked out by planetary ion fire", unit);
      }
    }
  } else if (profile.batteryCount > 0) {
    const auto arc = static_cast<Arc>(state.rng.Range(0, kArcCount - 1));
    for (int i = 0; i < profile.batteryCount; ++i) {
      if (profile.batteries[i].arc == arc) {
        ship.batteryCooldown[i]
            = std::max(ship.batteryCooldown[i], Ticks(kSalvoSeconds[static_cast<size_t>(profile.batteries[i].kind)]));
      }
    }
  }
  // Preserve charged jump progress and let each engine outage expire even under sustained ion fire.
  ship.systemsOffTicks = std::max(ship.systemsOffTicks, Ticks(kIonSystemsSeconds));
  ship.tractorTarget = kNoUnit;
  if (!wasDisrupted && !(planetary && engines)) {
    LogEvent(state, profile.side, UnitName(state, unit) + ": disrupted by ion fire", unit);
  }
  AddEffect(state, ViewEffectKind::Disruption, state.units[unit].position, profile.radius, unit);
}

void FireBattery(BattleState& state, UnitIndex unit, int index, UnitIndex target, int32_t craft, float output)
{
  const UnitState& self = state.units[unit];
  const UnitProfile& profile = state.Profile(unit);
  const BatteryProfile& battery = profile.batteries[index];
  const float salvo = kSalvoSeconds[static_cast<size_t>(battery.kind)];
  const SideState& side = state.sides[SideIndex(self.side)];
  const float signalBonus = side.concentrateTicks > 0 && target == side.concentrateTarget ? kConcentrateAccuracyBonus
                                                                                          : 0.0f;
  PendingShot shot;
  shot.fireTick = state.tick;
  shot.shooter = unit;
  shot.target = target;
  shot.targetCraft = craft;
  shot.side = self.side;
  shot.kind = ShotKindOf(battery.kind);
  shot.focus = state.EffectiveShipDoctrine(unit).subsystem;
  shot.from = self.position + FromAngle(self.heading + ArcCenter(battery.arc)) * (profile.radius * 0.6f);
  float accuracy = 0.0f;
  Vec2 aim;
  Vec2 velocity;
  float radius = 0.0f;
  if (craft >= 0) {
    const CraftState& victim = state.craft[craft];
    aim = victim.position;
    velocity = FromAngle(victim.heading) * state.Profile(target).maxSpeed;
    radius = kCraftRadius;
    shot.damage = battery.dpsVsFighter * salvo * output;
    accuracy = kFighterTargetAccuracy * Evasion(TurnPoints(state, unit), TurnPoints(state, target))
               * (1.0f - CommanderBonus(state, target, kCommanderEvasionBonus));
  } else {
    const UnitState& other = state.units[target];
    aim = other.position;
    velocity = other.velocity;
    radius = state.Profile(target).radius;
    shot.damage = battery.dpsVsCapital * salvo * output;
    const float distance = std::max(0.0f, Distance(self.position, other.position) - radius);
    accuracy = ShipAccuracy(profile, distance, battery.range);
    shot.shieldImpact = state.Ship(target)->shield > 0.0f;
  }
  shot.hit = state.rng.Chance(std::min(kMaxHitChance, accuracy + signalBonus));
  const float speed = kBoltSpeed[static_cast<size_t>(battery.kind)];
  AimShot(state, shot, aim, velocity, radius, speed, unit * kMaxBatteries + index);
  QueueShot(state, shot, speed);
}

void RepairOne(BattleState& state, UnitIndex unit)
{
  ShipState& ship = *state.Ship(unit);
  CriticalHits& hits = ship.criticals;
  std::array<int32_t*, 6> systems{&hits.shieldRecharge, &hits.weaponRecharge,    &hits.tractor,
                                  &hits.speed,          &hits.hyperdrivePrimary, &hits.hyperdriveBackup};
  int total = 0;
  for (int32_t* system : systems) {
    total += *system;
  }
  if (total <= 0) {
    return;
  }
  int roll = state.rng.Range(0, total - 1);
  for (size_t i = 0; i < systems.size(); ++i) {
    roll -= *systems[i];
    if (roll < 0) {
      --*systems[i];
      static constexpr std::array<const char*, 6> kNames{"shield recharge", "weapon recharge", "tractor beam",
                                                         "engines",         "hyperdrive",      "backup hyperdrive"};
      LogEvent(state, state.units[unit].side, UnitName(state, unit) + ": repaired " + kNames[i], unit);
      return;
    }
  }
}

} // namespace

void AimShot(const BattleState& state, PendingShot& shot, Vec2 aim, Vec2 velocity, float radius, float speed, int salt)
{
  const float flight = Distance(shot.from, aim) / speed;
  shot.to = aim + velocity * flight;
  if (shot.hit) {
    shot.to = shot.to + (shot.from - shot.to) * std::min(0.9f, radius * 0.5f / std::max(1.0f, Distance(shot.from, shot.to)));
  } else {
    shot.to = shot.to + MissOffset(state.tick, salt, radius);
  }
}

void QueueShot(BattleState& state, PendingShot shot, float speed)
{
  const float flight = Distance(shot.from, shot.to) / std::max(speed, 1.0f);
  shot.impactTick = state.tick + std::max<int64_t>(1, static_cast<int64_t>(std::ceil(flight * kTicksPerSecond)));
  state.shots.push_back(shot);
}

void FireBatteries(BattleState& state)
{
  for (ShipState& ship : state.ships) {
    const UnitIndex unit = ship.unit;
    if (!state.units[unit].inSpace) {
      continue;
    }
    const UnitProfile& profile = state.Profile(unit);
    const float output = WeaponOutput(state, unit);
    for (int i = 0; i < profile.batteryCount; ++i) {
      if (ship.batteryCooldown[i] > 0) {
        --ship.batteryCooldown[i];
        continue;
      }
      int32_t craft = -1;
      const UnitIndex target = output > 0.0f ? SelectBatteryTarget(state, unit, i, &craft) : kNoUnit;
      ship.batteryTarget[i] = target;
      if (target == kNoUnit) {
        ship.batteryCooldown[i] = RetargetTicks(profile);
        continue;
      }
      FireBattery(state, unit, i, target, craft, output);
      ship.batteryCooldown[i] = Ticks(kSalvoSeconds[static_cast<size_t>(profile.batteries[i].kind)]);
    }
  }
}

void ResolveShots(BattleState& state)
{
  size_t kept = 0;
  for (size_t i = 0; i < state.shots.size(); ++i) {
    const PendingShot shot = state.shots[i];
    if (shot.impactTick > state.tick) {
      state.shots[kept++] = shot;
      continue;
    }
    if (!shot.hit || !state.InSpace(shot.target)) {
      continue;
    }
    if (shot.targetCraft >= 0) {
      DamageCraft(state, shot.target, shot.targetCraft, shot.damage);
    } else if (state.units[shot.target].kind == UnitKind::CapitalShip) {
      DamageShip(state, shot.target, shot.damage, shot.kind, shot.focus, shot.battery >= 0);
    }
  }
  state.shots.resize(kept);
}

void DamageShip(BattleState& state, UnitIndex target, float damage, ShotKind kind, Subsystem focus, bool planetary)
{
  ShipState& ship = *state.Ship(target);
  const UnitProfile& profile = state.Profile(target);
  if (!state.units[target].inSpace || damage <= 0.0f) {
    return;
  }
  ship.lastDamageTick = static_cast<int32_t>(state.tick);
  const bool hadShields = ship.shield >= kShieldsDownEventShare * profile.shieldMax && ship.shield > 0.0f;
  float hullDamage = 0.0f;
  float absorbed = 0.0f;
  if (kind == ShotKind::Torpedo) {
    // A quarter against shields, to the shields only; full against an unshielded hull (§6.4).
    if (ship.shield > 0.0f) {
      absorbed = std::min(ship.shield, damage * kTorpedoVsShields);
      ship.shield -= absorbed;
    } else {
      hullDamage = damage;
    }
  } else {
    absorbed = std::min(ship.shield, damage);
    ship.shield -= absorbed;
    const float overflow = damage - absorbed;
    if (kind == ShotKind::Ion) {
      if (overflow > 0.0f) {
        ship.ionPool += overflow;
        const float threshold = std::max(kIonDisruptionMinimum, kIonDisruptionShare * (profile.hullMax + profile.shieldMax));
        while (ship.ionPool >= threshold && state.units[target].inSpace) {
          ship.ionPool -= threshold;
          Disrupt(state, target, planetary);
        }
      }
    } else {
      hullDamage = overflow;
    }
  }
  if (absorbed > 0.0f && profile.shieldMax > 0.0f
      && state.rng.Chance(std::min(kMaxShieldCritChance, absorbed * kShieldCritChancePerPoint))
      && ship.criticals.shieldRecharge < kCritSystemMax) {
    ApplyCritical(state, target, CritSystem::ShieldRecharge);
  }
  if (hadShields && ship.shield <= 0.0f) {
    ship.shield = 0.0f;
    LogEvent(state, profile.side, UnitName(state, target) + ": shields down", target);
  }
  if (hullDamage > 0.0f) {
    if (focus != Subsystem::Hull) {
      hullDamage *= kSubsystemHullShare;
    }
    ship.hull -= hullDamage;
    AddEffect(state, ViewEffectKind::HullHit, state.units[target].position, profile.radius * 0.3f, target);
    if (ship.hull <= 0.0f) {
      DestroyUnit(state, target);
      return;
    }
    RollCritical(state, target, focus, hullDamage * kCritChancePerHullPoint);
  } else if (absorbed > 0.0f) {
    AddEffect(state, ViewEffectKind::ShieldFlash, state.units[target].position, profile.radius, target);
  }
}

void DamageCraft(BattleState& state, UnitIndex unit, int32_t index, float damage)
{
  SquadronState& squadron = *state.Squadron(unit);
  CraftState& craft = state.craft[index];
  if (!craft.alive) {
    return;
  }
  squadron.lastAttackedTick = static_cast<int32_t>(state.tick);
  const float absorbed = std::min(craft.shield, damage);
  craft.shield -= absorbed;
  craft.hull -= damage - absorbed;
  if (craft.hull > 0.0f) {
    return;
  }
  craft.alive = false;
  craft.hull = 0.0f;
  craft.shield = 0.0f;
  --squadron.alive;
  AddEffect(state, ViewEffectKind::Explosion, craft.position, 30.0f, unit);
  if (squadron.alive <= 0) {
    DestroyUnit(state, unit);
  }
}

void DestroyUnit(BattleState& state, UnitIndex unit)
{
  UnitState& self = state.units[unit];
  if (self.fate == UnitFate::Destroyed) {
    return;
  }
  const bool wasInSpace = self.inSpace;
  self.fate = UnitFate::Destroyed;
  self.inSpace = false;
  self.orders.clear();
  const UnitProfile& profile = state.Profile(unit);
  switch (self.kind) {
    case UnitKind::CapitalShip: {
      ShipState& ship = *state.Ship(unit);
      ship.hull = 0.0f;
      ship.shield = 0.0f;
      ship.withdrawing = false;
      ship.tractorTarget = kNoUnit;
      LogEvent(state, self.side, UnitName(state, unit) + " destroyed", unit);
      break;
    }
    case UnitKind::Squadron: {
      SquadronState& squadron = *state.Squadron(unit);
      squadron.place = SquadronPlace::Destroyed;
      squadron.alive = 0;
      for (int i = 0; i < squadron.craftCount; ++i) {
        state.craft[squadron.firstCraft + i].alive = false;
      }
      LogEvent(state, self.side, UnitName(state, unit) + " lost", unit);
      break;
    }
    case UnitKind::DeathStar:
      state.stations[self.slot].hull = 0.0f;
      LogEvent(state, self.side, UnitName(state, unit) + " destroyed", unit);
      break;
    case UnitKind::Passive:
      break;
  }
  if (wasInSpace || self.kind == UnitKind::Squadron) {
    AddEffect(state, ViewEffectKind::Explosion, self.position, profile.radius * 2.0f, unit);
  }
  if (self.kind != UnitKind::Squadron) {
    // A carrier takes its docked squadrons with it (original rule); launched ones look for another home.
    for (SquadronState& squadron : state.squadrons) {
      if (squadron.carrier == unit && squadron.place == SquadronPlace::Docked) {
        DestroyUnit(state, squadron.unit);
      }
    }
    RehomeSquadrons(state, unit);
  }
}

void UpdateShipSystems(BattleState& state)
{
  for (ShipState& ship : state.ships) {
    const UnitIndex unit = ship.unit;
    if (!state.units[unit].inSpace) {
      continue;
    }
    const UnitProfile& profile = state.Profile(unit);
    const Side side = state.units[unit].side;
    const PowerRouting wanted = state.EffectiveShipDoctrine(unit).power;
    if (wanted != ship.power) {
      ship.powerFrom = ship.power;
      ship.power = wanted;
      ship.powerTicks = Ticks(kPowerSwitchSeconds * state.TransitionScale(side));
    } else if (ship.powerTicks > 0) {
      --ship.powerTicks;
    }
    ship.shield = std::min(profile.shieldMax, ship.shield + ShieldRegen(state, unit) * kTickSeconds);
    ship.engineOffTicks = std::max(0, ship.engineOffTicks - 1);
    ship.systemsOffTicks = std::max(0, ship.systemsOffTicks - 1);
    const float threshold = std::max(kIonDisruptionMinimum, kIonDisruptionShare * (profile.hullMax + profile.shieldMax));
    ship.ionPool = std::max(0.0f, ship.ionPool - kIonPoolDecay * threshold * kTickSeconds);
    const bool holdTheLine = state.sides[SideIndex(side)].holdLineTicks > 0;
    ship.damageControlTicks -= holdTheLine ? 2 : 1;
    if (ship.damageControlTicks <= 0) {
      ship.damageControlTicks = Ticks(kDamageControlSeconds);
      if (state.rng.Range(1, 100) <= static_cast<int32_t>(profile.damageControl)) {
        RepairOne(state, unit);
      }
    }
  }
}

} // namespace reb::battle::sim
