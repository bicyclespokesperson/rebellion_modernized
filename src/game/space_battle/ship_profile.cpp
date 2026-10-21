#include "space_battle/ship_profile.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <optional>

#include "space_battle/math2d.h"
#include "space_battle/tuning.h"

namespace reb::battle
{
namespace
{
using namespace tuning;

constexpr int Kind(WeaponKind kind)
{
  return static_cast<int>(kind);
}

float ArcPoints(const WeaponGrid& grid, int arc)
{
  const auto& row = grid.points[arc];
  return static_cast<float>(std::accumulate(row.begin(), row.end(), 0));
}

float ArcWeighted(const WeaponGrid& grid, int arc, float (*factor)(WeaponKind))
{
  float sum = 0.0f;
  for (int kind = 0; kind < kWeaponKinds; ++kind) {
    sum += static_cast<float>(grid.points[arc][kind]) * factor(static_cast<WeaponKind>(kind));
  }
  return sum;
}

int BestArc(const WeaponGrid& grid, float (*factor)(WeaponKind))
{
  int best = 0;
  for (int arc = 1; arc < kArcCount; ++arc) {
    if (ArcWeighted(grid, arc, factor) > ArcWeighted(grid, best, factor)) {
      best = arc;
    }
  }
  return best;
}

float KindPoints(const WeaponGrid& grid, WeaponKind kind)
{
  float sum = 0.0f;
  for (int arc = 0; arc < kArcCount; ++arc) {
    sum += static_cast<float>(grid.points[arc][Kind(kind)]);
  }
  return sum;
}

SizeClass SizeFromMass(float mass)
{
  if (mass < kLightMassLimit) {
    return SizeClass::Light;
  }
  if (mass < kMediumMassLimit) {
    return SizeClass::Medium;
  }
  return mass < kHeavyMassLimit ? SizeClass::Heavy : SizeClass::Super;
}

TargetTag SizeTag(SizeClass size)
{
  switch (size) {
    case SizeClass::Fighter:
      return TargetTag::Fighter;
    case SizeClass::Light:
      return TargetTag::Light;
    case SizeClass::Medium:
      return TargetTag::Medium;
    case SizeClass::Heavy:
      return TargetTag::Heavy;
    case SizeClass::Super:
      return TargetTag::Super;
    case SizeClass::DeathStar:
      return TargetTag::DeathStar;
  }
  return TargetTag::Light;
}

std::optional<float> TableRadius(int32_t typeIndex)
{
  for (const ClassLength& entry : kClassLengths) {
    if (typeIndex >= 0 && entry.typeIndex == typeIndex) {
      return 0.5f * entry.length;
    }
  }
  return std::nullopt;
}

float ClassRadius(int32_t typeIndex, float mass)
{
  return TableRadius(typeIndex).value_or(
      std::max(kMinShipRadius, 0.5f * kFallbackLengthScale * std::pow(mass, kFallbackLengthExponent)));
}

Archetype ClassifyShip(const CapitalShipEntry& ship)
{
  const WeaponGrid& grid = ship.weapons;
  const float heavy = KindPoints(grid, WeaponKind::Turbolaser) + KindPoints(grid, WeaponKind::Ion);
  const float laser = KindPoints(grid, WeaponKind::Laser);
  if (ship.gravityWell > 0) {
    return Archetype::Interdictor;
  }
  if (heavy <= 0.0f) {
    if (ship.troopCapacity > 0 || laser <= 0.0f) {
      return Archetype::Transport;
    }
    return ship.fighterCapacity >= kCarrierArchetypeHangars ? Archetype::Carrier : Archetype::Picket;
  }
  const float laserShare = laser / (heavy + laser);
  if (laserShare >= kPicketLaserShare) {
    return Archetype::Picket;
  }
  if (laserShare >= kEscortLaserShare) {
    return Archetype::EscortFrigate;
  }
  const float fore = ArcWeighted(grid, static_cast<int>(Arc::Fore), WeaponFactorVsCapital);
  const float broadside = std::max(ArcWeighted(grid, static_cast<int>(Arc::Port), WeaponFactorVsCapital),
                                   ArcWeighted(grid, static_cast<int>(Arc::Starboard), WeaponFactorVsCapital));
  return fore >= broadside * kBowFacingRatio ? Archetype::BowLine : Archetype::BroadsideLine;
}

FacingStyle FacingFromGrid(const WeaponGrid& grid)
{
  const float fore = ArcWeighted(grid, static_cast<int>(Arc::Fore), WeaponFactorVsCapital);
  const float aft = ArcWeighted(grid, static_cast<int>(Arc::Aft), WeaponFactorVsCapital);
  const float broadside = std::max(ArcWeighted(grid, static_cast<int>(Arc::Port), WeaponFactorVsCapital),
                                   ArcWeighted(grid, static_cast<int>(Arc::Starboard), WeaponFactorVsCapital));
  const float strongest = std::max({fore, aft, broadside});
  const float weakest = std::min({fore, aft, broadside});
  if (strongest <= 0.0f || weakest >= strongest * 0.7f) {
    return FacingStyle::Omni;
  }
  return fore >= broadside * kBowFacingRatio ? FacingStyle::Bow : FacingStyle::Broadside;
}

// Optimal: the longest range that still keeps most of the anti-capital firepower in reach.
float OptimalRange(const UnitProfile& profile)
{
  float total = 0.0f;
  for (int i = 0; i < profile.batteryCount; ++i) {
    total += profile.batteries[i].dpsVsCapital;
  }
  if (total <= 0.0f) {
    return profile.closeRange;
  }
  float best = 0.0f;
  for (int i = 0; i < profile.batteryCount; ++i) {
    const float range = profile.batteries[i].range;
    float reaching = 0.0f;
    for (int j = 0; j < profile.batteryCount; ++j) {
      if (profile.batteries[j].range >= range) {
        reaching += profile.batteries[j].dpsVsCapital;
      }
    }
    if (reaching >= kOptimalRangeShare * total) {
      best = std::max(best, range);
    }
  }
  return best * kRangeMargin;
}

void FillBatteries(const WeaponGrid& grid, float weaponRecharge, UnitProfile& profile)
{
  const int capArc = BestArc(grid, WeaponFactorVsCapital);
  const int fighterArc = BestArc(grid, WeaponFactorVsFighter);
  // A ship firing only its best arc deals the legacy credit (weapon recharge x the arc's weapon mix); every other
  // bearing arc adds its own points at the same rate (§6.3).
  const float capArcPoints = ArcPoints(grid, capArc);
  const float fighterArcPoints = ArcPoints(grid, fighterArc);
  const float perPointCap = capArcPoints > 0.0f ? weaponRecharge * kLegacyStepsPerSecond / capArcPoints : 0.0f;
  const float perPointFighter
      = fighterArcPoints > 0.0f ? weaponRecharge * kLegacyStepsPerSecond / fighterArcPoints : 0.0f;
  profile.batteryCount = 0;
  float minRange = 0.0f;
  for (int arc = 0; arc < kArcCount; ++arc) {
    for (int kind = 0; kind < kWeaponKinds; ++kind) {
      const auto points = static_cast<float>(grid.points[arc][kind]);
      const float range = static_cast<float>(grid.range[kind]) * kRangePointMeters;
      if (points <= 0.0f || range <= 0.0f) {
        continue;
      }
      BatteryProfile& battery = profile.batteries[profile.batteryCount++];
      battery.arc = static_cast<Arc>(arc);
      battery.kind = static_cast<WeaponKind>(kind);
      battery.points = points;
      battery.range = range;
      battery.dpsVsCapital = points * perPointCap * WeaponFactorVsCapital(battery.kind);
      battery.dpsVsFighter = points * perPointFighter * WeaponFactorVsFighter(battery.kind) * kPointDefenceDamageScale;
      profile.arcVsCapital[arc] += battery.dpsVsCapital;
      profile.arcVsFighter[arc] += battery.dpsVsFighter;
      profile.maxRange = std::max(profile.maxRange, range);
      minRange = minRange == 0.0f ? range : std::min(minRange, range);
    }
  }
  profile.bestArcVsCapital = *std::max_element(profile.arcVsCapital.begin(), profile.arcVsCapital.end());
  profile.bestArcVsFighter = *std::max_element(profile.arcVsFighter.begin(), profile.arcVsFighter.end());
  profile.closeRange = minRange * kRangeMargin;
  profile.optimalRange = OptimalRange(profile);
}

UnitProfile MakeShipProfile(const CapitalShipEntry& ship)
{
  UnitProfile profile;
  profile.kind = UnitKind::CapitalShip;
  profile.side = ship.identity.side;
  profile.archetype = ClassifyShip(ship);
  profile.facing = FacingFromGrid(ship.weapons);
  profile.hullMax = static_cast<float>(std::max(1, ship.hullMax));
  profile.shieldMax = static_cast<float>(std::max(0, ship.shieldMax));
  profile.shieldRecharge = static_cast<float>(std::max(0, ship.shieldRecharge)) * kLegacyStepsPerSecond;
  profile.size = SizeFromMass(profile.hullMax + profile.shieldMax);
  profile.radius = ClassRadius(ship.identity.typeIndex, profile.hullMax + profile.shieldMax);
  profile.maxSpeed = static_cast<float>(std::max(0, ship.sublight)) * kCapitalSpeedPerSublight;
  profile.acceleration = profile.maxSpeed / kCapitalAccelerationSeconds;
  profile.turnPoints = static_cast<float>(std::max(0, ship.maneuverability));
  profile.detection = static_cast<float>(ship.detection);
  profile.damageControl = static_cast<float>(ship.damageControl);
  profile.hyperdrive = ship.hyperdrive;
  profile.hyperdriveBackup = ship.hyperdriveBackup;
  profile.tractorPower = static_cast<float>(std::max(0, ship.tractorPower));
  profile.tractorRange = profile.tractorPower > 0.0f ? static_cast<float>(ship.tractorRange) * kRangePointMeters : 0.0f;
  profile.gravityWell = ship.gravityWell > 0;
  profile.hangarCapacity = std::max(0, ship.fighterCapacity);
  profile.troopCapacity = std::max(0, ship.troopCapacity);
  FillBatteries(ship.weapons, static_cast<float>(std::max(0, ship.weaponRecharge)), profile);
  profile.firepower = profile.bestArcVsCapital + 0.5f * profile.bestArcVsFighter;

  profile.staticTags = TagBit(SizeTag(profile.size));
  const float heavy = KindPoints(ship.weapons, WeaponKind::Turbolaser) + KindPoints(ship.weapons, WeaponKind::Ion);
  const float laser = KindPoints(ship.weapons, WeaponKind::Laser);
  if (profile.troopCapacity > 0 && heavy <= 0.0f) {
    profile.staticTags |= TagBit(TargetTag::Transport);
  }
  if (profile.gravityWell) {
    profile.staticTags |= TagBit(TargetTag::Interdictor);
  }
  if (profile.tractorPower > 0.0f) {
    profile.staticTags |= TagBit(TargetTag::Tractor);
  }
  if (laser > 0.0f && laser >= kPicketLaserShare * (heavy + laser)) {
    profile.staticTags |= TagBit(TargetTag::Picket);
  }
  return profile;
}

float FighterDpsPerCraft(const WeaponGrid& grid, float (*factor)(WeaponKind), WeaponKind only, bool onlyThis)
{
  const int arc = static_cast<int>(Arc::Fore);
  const float total = ArcPoints(grid, arc);
  if (total <= 0.0f) {
    return 0.0f;
  }
  float sum = 0.0f;
  for (int kind = 0; kind < kWeaponKinds; ++kind) {
    const bool isOnly = static_cast<WeaponKind>(kind) == only;
    if (isOnly == onlyThis) {
      sum += static_cast<float>(grid.points[arc][kind]) * factor(static_cast<WeaponKind>(kind));
    }
  }
  return kCraftLegacyOutput * kLegacyStepsPerSecond * sum / total;
}

UnitProfile MakeSquadronProfile(const SquadronEntry& squadron)
{
  UnitProfile profile;
  profile.kind = UnitKind::Squadron;
  profile.side = squadron.identity.side;
  profile.size = SizeClass::Fighter;
  profile.facing = FacingStyle::Bow;
  const WeaponGrid& grid = squadron.weaponsPerCraft;
  const float ion = KindPoints(grid, WeaponKind::Ion);
  profile.archetype = ion > 0.0f                         ? Archetype::Bomber
                      : squadron.torpedoesPerCraft > 0 ? Archetype::Multirole
                                                         : Archetype::Interceptor;
  profile.radius = kCraftRadius;
  profile.maxSpeed = static_cast<float>(std::max(1, squadron.sublight)) * kFighterSpeedPerSublight;
  profile.acceleration = profile.maxSpeed;
  profile.turnPoints = static_cast<float>(std::max(0, squadron.maneuverability) + 1);
  profile.detection = static_cast<float>(squadron.detection);
  profile.hyperdrive = squadron.hyperdrive;
  profile.hyperdriveBackup = squadron.hyperdriveBackup;
  profile.craftMax = std::max(0, squadron.squadSize);
  profile.craftShield = static_cast<float>(std::max(0, squadron.shieldPerCraft));
  profile.torpedoesPerCraft = std::max(0, squadron.torpedoesPerCraft);
  profile.torpedoRange = static_cast<float>(squadron.torpedoRange) * kRangePointMeters;
  profile.hullMax = kCraftHull;
  profile.shieldMax = profile.craftShield;
  profile.craftGunVsFighter = FighterDpsPerCraft(grid, WeaponFactorVsFighter, WeaponKind::Ion, false) * kDogfightDamageScale;
  profile.craftGunVsCapital = FighterDpsPerCraft(grid, WeaponFactorVsCapital, WeaponKind::Ion, false);
  profile.craftIonVsCapital = FighterDpsPerCraft(grid, WeaponFactorVsCapital, WeaponKind::Ion, true);
  const auto rangeOf = [&](WeaponKind kind) {
    return grid.points[static_cast<int>(Arc::Fore)][Kind(kind)] > 0
               ? static_cast<float>(grid.range[Kind(kind)]) * kRangePointMeters
               : 0.0f;
  };
  profile.craftGunRange = std::max(rangeOf(WeaponKind::Laser), rangeOf(WeaponKind::Turbolaser));
  profile.craftIonRange = rangeOf(WeaponKind::Ion);
  profile.maxRange = std::max({profile.craftGunRange, profile.craftIonRange, profile.torpedoRange});
  profile.closeRange = profile.maxRange;
  profile.optimalRange = profile.maxRange;
  const float torpedoPotential = static_cast<float>(profile.torpedoesPerCraft) * kTorpedoDamage / 60.0f;
  profile.firepower = static_cast<float>(profile.craftMax)
                      * (profile.craftGunVsCapital + 0.5f * profile.craftIonVsCapital + torpedoPotential
                         + 0.5f * profile.craftGunVsFighter);
  profile.staticTags = TagBit(TargetTag::Fighter);
  if (ion > 0.0f) {
    profile.staticTags |= TagBit(TargetTag::Bomber);
  }
  return profile;
}

UnitProfile MakeDeathStarProfile(const DeathStarEntry& station)
{
  UnitProfile profile;
  profile.kind = UnitKind::DeathStar;
  profile.side = station.identity.side;
  profile.archetype = Archetype::Station;
  profile.size = SizeClass::DeathStar;
  profile.radius = TableRadius(station.identity.typeIndex).value_or(kDeathStarRadius);
  profile.hullMax = static_cast<float>(std::max(1, station.hullMax));
  profile.hangarCapacity = std::max(0, station.hangarFree);
  profile.firepower = kDeathStarFirepower;
  profile.staticTags = TagBit(TargetTag::DeathStar);
  return profile;
}

UnitProfile MakePassiveProfile(Side side)
{
  UnitProfile profile;
  profile.side = side;
  return profile;
}

} // namespace

float WeaponFactorVsCapital(WeaponKind kind)
{
  return kind == WeaponKind::Laser ? kLaserVsCapital : 1.0f;
}

float WeaponFactorVsFighter(WeaponKind kind)
{
  switch (kind) {
    case WeaponKind::Turbolaser:
      return kTurbolaserVsFighter;
    case WeaponKind::Ion:
      return 0.0f;
    case WeaponKind::Laser:
      return 1.0f;
  }
  return 0.0f;
}

float ArcCenter(Arc arc)
{
  switch (arc) {
    case Arc::Fore:
      return 0.0f;
    case Arc::Aft:
      return kPi;
    case Arc::Port:
      return 0.5f * kPi;
    case Arc::Starboard:
      return -0.5f * kPi;
  }
  return 0.0f;
}

bool InArc(Arc arc, float relativeBearing)
{
  return std::fabs(AngleDelta(ArcCenter(arc), relativeBearing)) <= kArcHalfWidth;
}

std::vector<UnitProfile> BuildProfiles(const BattleSnapshot& snapshot)
{
  std::vector<UnitProfile> profiles;
  profiles.reserve(snapshot.units.size());
  for (const UnitEntry& entry : snapshot.units) {
    profiles.push_back(std::visit(
        [](const auto& unit) {
          using T = std::decay_t<decltype(unit)>;
          if constexpr (std::is_same_v<T, CapitalShipEntry>) {
            return MakeShipProfile(unit);
          } else if constexpr (std::is_same_v<T, SquadronEntry>) {
            return MakeSquadronProfile(unit);
          } else if constexpr (std::is_same_v<T, DeathStarEntry>) {
            return MakeDeathStarProfile(unit);
          } else {
            return MakePassiveProfile(unit.identity.side);
          }
        },
        entry));
  }
  // Tags that depend on other units: a carrier carries squadrons in this battle; a flagship has a commander aboard.
  for (const UnitEntry& entry : snapshot.units) {
    if (const auto* squadron = std::get_if<SquadronEntry>(&entry)) {
      const UnitIndex carrier = squadron->carrier;
      if (carrier >= 0 && carrier < static_cast<UnitIndex>(profiles.size()) && profiles[carrier].IsShip()) {
        profiles[carrier].staticTags |= TagBit(TargetTag::Carrier);
      }
    } else if (const auto* character = std::get_if<CharacterEntry>(&entry)) {
      const UnitIndex ship = character->ship;
      if ((character->admiral || character->commander) && ship >= 0 && ship < static_cast<UnitIndex>(profiles.size())
          && profiles[ship].IsShip()) {
        profiles[ship].staticTags |= TagBit(TargetTag::Flagship);
      }
    }
  }
  return profiles;
}

const char* ArchetypeName(Archetype archetype)
{
  switch (archetype) {
    case Archetype::BowLine:
      return "Bow-gunned line";
    case Archetype::BroadsideLine:
      return "Broadside line";
    case Archetype::Picket:
      return "Picket";
    case Archetype::EscortFrigate:
      return "Escort frigate";
    case Archetype::Carrier:
      return "Carrier";
    case Archetype::Interdictor:
      return "Interdictor";
    case Archetype::Transport:
      return "Transport";
    case Archetype::Interceptor:
      return "Interceptor";
    case Archetype::Multirole:
      return "Multirole fighter";
    case Archetype::Bomber:
      return "Bomber";
    case Archetype::Station:
      return "Battle station";
  }
  return "";
}

const char* TagName(TargetTag tag)
{
  switch (tag) {
    case TargetTag::Fighter:
      return "Fighters";
    case TargetTag::Light:
      return "Light ships";
    case TargetTag::Medium:
      return "Medium ships";
    case TargetTag::Heavy:
      return "Heavy ships";
    case TargetTag::Super:
      return "Super-heavy ships";
    case TargetTag::DeathStar:
      return "Death Star";
    case TargetTag::Carrier:
      return "Carriers";
    case TargetTag::Transport:
      return "Transports";
    case TargetTag::Interdictor:
      return "Interdictors";
    case TargetTag::Tractor:
      return "Tractor ships";
    case TargetTag::Picket:
      return "Pickets";
    case TargetTag::Bomber:
      return "Bombers";
    case TargetTag::Flagship:
      return "Flagships";
    case TargetTag::ShieldsDown:
      return "Shields down";
    case TargetTag::Damaged:
      return "Damaged";
    case TargetTag::Withdrawing:
      return "Withdrawing";
    case TargetTag::Tractored:
      return "Tractored";
    case TargetTag::Threatening:
      return "Threatening";
    case TargetTag::Nearest:
      return "Nearest";
  }
  return "";
}

} // namespace reb::battle
