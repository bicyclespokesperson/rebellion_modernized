#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "space_battle/commands.h"
#include "space_battle/snapshot.h"

// Per-unit behaviour derived from the snapshot's record values (tactical_combat.md §7.1): archetype, target tags,
// batteries, ranges, speed, turn rate and size. Nothing here is authored per class except the radius exceptions in
// tuning.h, keyed by the presentation type index.
namespace reb::battle
{

enum class UnitKind : uint8_t { Passive, CapitalShip, Squadron, DeathStar }; // Passive: characters and passengers

enum class Archetype : uint8_t {
  BowLine,
  BroadsideLine,
  Picket,
  EscortFrigate,
  Carrier,
  Interdictor,
  Transport,
  Interceptor, // fighters: guns only
  Multirole,   // guns and torpedoes
  Bomber,      // ion cannons
  Station,     // the Death Star
};

enum class SizeClass : uint8_t { Fighter, Light, Medium, Heavy, Super, DeathStar };
enum class FacingStyle : uint8_t { Bow, Broadside, Omni };

inline constexpr int kMaxBatteries = kArcCount * kWeaponKinds;

constexpr uint32_t TagBit(TargetTag tag)
{
  return 1U << static_cast<uint32_t>(tag);
}

struct BatteryProfile {
  Arc arc = Arc::Fore;
  WeaponKind kind = WeaponKind::Turbolaser;
  float points = 0.0f;
  float range = 0.0f;        // meters
  float dpsVsCapital = 0.0f; // at full output, before accuracy; ion counts against shields only
  float dpsVsFighter = 0.0f; // before accuracy and evasion
};

struct UnitProfile {
  UnitKind kind = UnitKind::Passive;
  Side side = Side::Alliance;
  Archetype archetype = Archetype::Transport;
  SizeClass size = SizeClass::Light;
  FacingStyle facing = FacingStyle::Omni;
  uint32_t staticTags = 0; // TagBit of every size and function tag that applies
  float radius = 0.0f;
  float maxSpeed = 0.0f;     // m/s, balanced power, no admiral, no damage
  float acceleration = 0.0f; // m/s^2
  float turnPoints = 0.0f;   // capital ships: maneuverability (the admiral term is added in battle); craft: +1
  float hullMax = 0.0f;
  float shieldMax = 0.0f;
  float shieldRecharge = 0.0f; // per second at full hull
  float detection = 0.0f;
  float damageControl = 0.0f; // percent chance per repair interval
  int32_t hyperdrive = 0;
  int32_t hyperdriveBackup = 0;
  float tractorPower = 0.0f;
  float tractorRange = 0.0f; // meters
  bool gravityWell = false;
  int32_t hangarCapacity = 0;
  int32_t troopCapacity = 0;

  int32_t batteryCount = 0;
  std::array<BatteryProfile, kMaxBatteries> batteries{};
  std::array<float, kArcCount> arcVsCapital{}; // anti-capital DPS of each arc (ion included)
  std::array<float, kArcCount> arcVsFighter{};
  float bestArcVsCapital = 0.0f;
  float bestArcVsFighter = 0.0f;
  float maxRange = 0.0f;
  float closeRange = 0.0f;   // every weapon reaches
  float optimalRange = 0.0f; // most of the anti-capital firepower reaches
  float firepower = 0.0f;    // for the strength estimate (§9), at full health

  // Squadrons, per craft.
  int32_t craftMax = 0;
  float craftShield = 0.0f;
  int32_t torpedoesPerCraft = 0;
  float torpedoRange = 0.0f;
  float craftGunVsFighter = 0.0f; // DPS
  float craftGunVsCapital = 0.0f;
  float craftIonVsCapital = 0.0f;
  float craftGunRange = 0.0f;
  float craftIonRange = 0.0f;

  bool IsShip() const
  {
    return kind == UnitKind::CapitalShip;
  }
  bool IsSquadron() const
  {
    return kind == UnitKind::Squadron;
  }
  bool CanJumpAlone() const
  {
    return hyperdrive > 0 || hyperdriveBackup > 0;
  }
};

std::vector<UnitProfile> BuildProfiles(const BattleSnapshot& snapshot);

float WeaponFactorVsCapital(WeaponKind kind);
float WeaponFactorVsFighter(WeaponKind kind);
// Arc centres relative to the bow: fore 0, port +90 degrees (left), aft 180, starboard -90.
float ArcCenter(Arc arc);
bool InArc(Arc arc, float relativeBearing);
const char* ArchetypeName(Archetype archetype);
const char* TagName(TargetTag tag);

} // namespace reb::battle
