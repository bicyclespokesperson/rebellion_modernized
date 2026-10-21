#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

// Everything a battle needs from the strategic game, as plain data (docs/design/tactical_combat.md §11.1).
// BattleSetup (app/battle_handoff.cpp) builds it from the strategic objects; every battle engine consumes it. It is
// also the calibration corpus format and the first part of a mid-battle save, so it is self-contained: no pointers
// to strategic objects, only their keys.
//
// Units are listed in the order BattleSetup registers them. A unit's index in `units` is its UnitIndex; references
// between units (carrier, ship a character is aboard) use that index, -1 for none.
namespace reb::battle
{

using UnitIndex = int32_t;
inline constexpr UnitIndex kNoUnit = -1;

// 0 = Alliance, 1 = Empire: the strategic game's NotAlliance() encoding, also the legacy simulator's side index.
enum class Side : uint8_t { Alliance = 0, Empire = 1 };

inline constexpr int kArcCount = 4;    // Fore, Aft, Port, Starboard: the record's grid row order
inline constexpr int kWeaponKinds = 3; // Turbolaser, Ion, Laser: the record's grid column order
enum class Arc : uint8_t { Fore = 0, Aft = 1, Port = 2, Starboard = 3 };
enum class WeaponKind : uint8_t { Turbolaser = 0, Ion = 1, Laser = 2 };

// The record's 4 x 3 weapon grid (firepower points, not gun counts) and the range of each weapon kind in range points.
struct WeaponGrid {
  std::array<std::array<int32_t, kWeaponKinds>, kArcCount> points{};
  std::array<int32_t, kWeaponKinds> range{};

  bool operator==(const WeaponGrid&) const = default;
};

// The strategic key of a unit and its identity for presentation.
struct UnitIdentity {
  uint32_t family = 0;
  uint32_t id = 0;
  Side side = Side::Alliance;
  uint32_t typeBits = 0;  // record type bits (low 16 bits), as the legacy registration passes them
  int32_t typeIndex = -1; // index in the tactical ship-type table (app/battle_handoff.cpp), -1 if unknown
  std::string name;
  std::string className;
  // Strategic Battle Alert knowledge, indexed by the viewing side.
  std::array<bool, 2> briefingKnown{true, true};

  bool operator==(const UnitIdentity&) const = default;
};

struct CriticalHits {
  int32_t speed = 0;
  int32_t shieldRecharge = 0;
  int32_t weaponRecharge = 0;
  int32_t tractor = 0;
  int32_t hyperdrivePrimary = 0;
  int32_t hyperdriveBackup = 0;

  bool operator==(const CriticalHits&) const = default;
};

struct CapitalShipEntry {
  UnitIdentity identity;
  int32_t hullMax = 0;
  int32_t hull = 0;
  int32_t shieldMax = 0;      // record field +0x60
  int32_t sublight = 0;       // record field +0x64
  int32_t maneuverability = 0; // record field +0x68
  int32_t detection = 0;
  int32_t hyperdrive = 0;       // primary drive rating, lower is faster; 0 = none
  int32_t hyperdriveBackup = 0; // 0 = none
  int32_t shieldRecharge = 0;
  int32_t weaponRecharge = 0;
  int32_t damageControl = 0;
  int32_t tractorPower = 0;
  int32_t tractorRange = 0;
  int32_t gravityWell = 0;
  int32_t interdiction = 0;
  int32_t fighterCapacity = 0;
  int32_t hangarFree = 0; // as the legacy registration computes it
  int32_t troopCapacity = 0;
  int32_t gridTotal = 0;  // as the record stores it
  WeaponGrid weapons;
  CriticalHits criticals;

  bool operator==(const CapitalShipEntry&) const = default;
};

// Per-craft values; the squadron multiplies them by its size where the strategic game does.
struct SquadronEntry {
  UnitIdentity identity;
  UnitIndex carrier = kNoUnit; // kNoUnit: based in the system
  int32_t squadSize = 0;
  int32_t currentSize = 0;
  int32_t shieldPerCraft = 0;
  int32_t sublight = 0;
  int32_t maneuverability = 0;
  int32_t detection = 0;
  int32_t hyperdrive = 0;       // 0 = cannot jump on its own
  int32_t hyperdriveBackup = 0;
  int32_t torpedoesPerCraft = 0;
  int32_t torpedoRange = 0;
  WeaponGrid weaponsPerCraft;

  bool operator==(const SquadronEntry&) const = default;
};

struct CharacterEntry {
  UnitIdentity identity;
  UnitIndex ship = kNoUnit;
  bool major = false;
  bool admiral = false;   // the side's first commander (command kind 2)
  bool commander = false; // the side's second commander (command kind 1)
  bool injured = false;
  int32_t leadership = 0; // raw skill values
  int32_t combat = 0;

  bool operator==(const CharacterEntry&) const = default;
};

// A special force or troop regiment aboard a ship.
struct PassengerEntry {
  UnitIdentity identity;
  UnitIndex ship = kNoUnit;
  bool troopRegiment = false;

  bool operator==(const PassengerEntry&) const = default;
};

struct DeathStarEntry {
  UnitIdentity identity;
  bool completed = false;
  bool shieldFacility = false; // a Death Star Shield facility is in the system
  int32_t hullMax = 0;
  int32_t hull = 0;
  int32_t hangarFree = 0;

  bool operator==(const DeathStarEntry&) const = default;
};

using UnitEntry = std::variant<CapitalShipEntry, SquadronEntry, CharacterEntry, PassengerEntry, DeathStarEntry>;

// A planetary battery in the system (tactical_combat.md §6.13): a KDY-150 ion cannon or an LNR series turbolaser
// battery, owned by the side that controls it. Batteries are not units: they fire from the planet's surface, cannot be
// destroyed or ordered in space combat, and report no results.
struct DefenseEntry {
  UnitIdentity identity;
  WeaponKind weapon = WeaponKind::Turbolaser; // Ion for the KDY-150, Turbolaser for the LNR series
  int32_t attackStrength = 0;                 // the record's "Weapons Rating", on the bombardment scale

  bool operator==(const DefenseEntry&) const = default;
};

struct SideParams {
  // The battle vote's "time": a side withdraws when its strength falls below this fraction of the enemy's.
  // 0 = fight on; 10000 = leave at once (the Retreat vote).
  float withdrawThreshold = 0.0f;
  bool canRetreat = false;
  float retreatDays = 0.0f; // QueryRetreat's retreat time, unscaled
  bool human = false;       // a human controller votes for this side

  bool operator==(const SideParams&) const = default;
};

struct BattleParams {
  std::string systemName;
  uint32_t systemPictureId = 0; // SystemSD picture id (the backdrop planet)
  bool systemDestroyed = false;
  // Who held the system before the other side arrived: the defender, and the side a stalemate leaves the system to.
  // Snapshots recorded by the game before 2026-10-20 stored the last side to arrive here instead.
  Side heldFirstBy = Side::Alliance;
  int32_t controlSide = 0;           // 0 none, 1 Alliance, 2 Empire
  bool opponentIsAi = true;          // the legacy setup's human-vs-AI flag
  int32_t humanIsNotAlliance = 0;    // as the legacy setup receives it
  uint32_t seed = 0;                 // hashed from the system key and game tick (never drawn from the campaign RNG)
  std::array<SideParams, 2> sides;

  bool operator==(const BattleParams&) const = default;
};

struct BattleSnapshot {
  static constexpr int kVersion = 4;
  BattleParams params;
  std::vector<UnitEntry> units;
  std::vector<DefenseEntry> defenses; // version 4; earlier snapshots have none

  bool operator==(const BattleSnapshot&) const = default;
};

// Line-oriented text form: readable, diffable, and stable across platforms (snapshot.cpp).
std::string SerializeSnapshot(const BattleSnapshot& snapshot);
std::optional<BattleSnapshot> ParseSnapshot(std::string_view text);

} // namespace reb::battle
