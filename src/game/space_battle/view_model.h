#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "space_battle/commands.h"
#include "space_battle/snapshot.h"

// What the presentation needs from a running battle, rebuilt every rendered frame and interpolated between
// simulation ticks. The port layer (src/port/native/space_battle) draws only from this and never reads simulation
// internals; the HUD and the UI smoke checks read it too. Positions are meters on the battle plane, headings radians
// (0 = +x, counter-clockwise).
namespace reb::battle
{

enum class ViewUnitKind : uint8_t { CapitalShip, Squadron, DeathStar };

enum ViewUnitFlag : uint32_t {
  kViewWithdrawing = 1U << 0, // spooling its hyperdrive
  kViewWithdrawn = 1U << 1,
  kViewDestroyed = 1U << 2,
  kViewDocked = 1U << 3,
  kViewTractored = 1U << 4,
  kViewTractoring = 1U << 5,
  kViewDisrupted = 1U << 6, // ion disruption
  kViewInEnemyWell = 1U << 7,
  kViewFlagship = 1U << 8,
  kViewShieldsDown = 1U << 9,
  kViewUnderPlanetaryGuns = 1U << 10, // inside the enemy's planetary battery cover
};

struct ViewUnit {
  UnitIndex unit = kNoUnit;
  ViewUnitKind kind = ViewUnitKind::CapitalShip;
  Side side = Side::Alliance;
  int32_t typeIndex = -1; // tactical ship-type table index: selects the art
  GroupId group = kNoGroup;
  Vec2 position;
  float heading = 0.0f;
  float radius = 0.0f; // meters, for selection and silhouettes
  float hullFraction = 1.0f;
  float shieldFraction = 1.0f;
  int32_t craft = 0; // squadrons: craft left
  int32_t craftMax = 0;
  float spool = 0.0f; // 0-1 hyperdrive spool progress while withdrawing
  uint32_t flags = 0;
  CriticalHits criticals;
  std::string intent; // one line: what it is doing and why (§5.3)
  // For the tactical overlay's target lines and projected positions (§10.1).
  UnitIndex target = kNoUnit; // the unit it is shooting at or assigned to
  bool firing = false;        // the target is in range and arc
  Vec2 velocity;              // m/s
  float hullMax = 0.0f;
  bool jumpBlocked = false;
  bool deathStarShielded = false;
  bool deathStarRun = false;
  bool inTrench = false;
};

// One fighter craft, for drawing a squadron as individual ships.
struct ViewCraft {
  UnitIndex squadron = kNoUnit;
  Vec2 position;
  float heading = 0.0f;
};

enum class ViewShotKind : uint8_t { Turbolaser, Ion, Laser, Torpedo };

// A projectile in flight. The simulation decided the hit when it fired; the bolt only flies to `to`.
struct ViewShot {
  ViewShotKind kind = ViewShotKind::Turbolaser;
  Side side = Side::Alliance;
  Vec2 from;
  Vec2 to;
  float progress = 0.0f; // 0 at the muzzle, 1 at impact
  bool hit = false;
  bool shieldImpact = false;
  int64_t fireTick = -1;
  int64_t impactTick = -1;
  UnitIndex shooter = kNoUnit; // kNoUnit for a planetary battery
  UnitIndex target = kNoUnit;
  int32_t targetCraft = -1;
  int16_t battery = -1; // index in ViewModel::defenses when a planetary battery fired it
};

enum class ViewEffectKind : uint8_t { Explosion, ShieldFlash, HullHit, CriticalHit, Disruption, Jump, Superlaser };

struct ViewEffect {
  ViewEffectKind kind = ViewEffectKind::Explosion;
  Vec2 position;
  float size = 0.0f; // meters
  float age = 0.0f;  // seconds since it started
  UnitIndex unit = kNoUnit;
  int64_t startTick = -1;
};

struct ViewTractor {
  UnitIndex from = kNoUnit;
  UnitIndex to = kNoUnit;
};

struct ViewWell {
  UnitIndex projector = kNoUnit;
  Vec2 center;
  float radius = 0.0f;
  Side side = Side::Alliance;
  bool active = true;
};

// A planetary battery (§6.13), standing on the planet's limb. It fires at enemy capital ships inside its side's cover
// (ViewModel::coverRadius from the planet's centre) and cannot be destroyed or ordered.
struct ViewDefense {
  Side side = Side::Alliance;
  WeaponKind weapon = WeaponKind::Turbolaser; // Turbolaser: LNR series, hull damage; Ion: KDY-150, shields and engines
  std::string name;                           // the class name, such as "LNR Series II"
  Vec2 site;
  float dps = 0.0f;
  UnitIndex target = kNoUnit;    // what it fired at or is about to
  float sinceFired = -1.0f;      // seconds since its last salvo; negative before the first
  std::array<bool, 2> briefingKnown{true, true};
};

struct ViewGroup {
  GroupId id = kNoGroup;
  Side side = Side::Alliance;
  bool flight = false;
  std::string name;
  std::vector<UnitIndex> members;
  ShipDoctrine shipDoctrine;
  FlightDoctrine flightDoctrine;
};

struct ViewSide {
  float strength = 0.0f; // the simulator's strength estimate (§9)
  float initialStrength = 0.0f;
  int32_t signalsReady = 0;
  int32_t signalsMax = 0;
  bool withdrawing = false;
};

struct ViewEvent {
  float time = 0.0f; // battle seconds
  Side side = Side::Alliance;
  std::string text;
  UnitIndex unit = kNoUnit; // the unit it is about, for centring the camera
};

struct ViewDeployZone {
  Vec2 center;
  float radius = 0.0f; // 0: no zone
};

enum class ViewEndReason : uint8_t { None, Elimination, Stalemate, TimeLimit };

struct ViewModel {
  int64_t tick = 0;
  float battleSeconds = 0.0f;
  Vec2 boundsMin;
  Vec2 boundsMax;
  Vec2 planetCenter; // the system's planet, behind the defender (§4)
  float planetRadius = 0.0f;
  std::vector<ViewDefense> defenses; // planetary batteries (§6.13)
  float coverRadius = 0.0f;          // from the planet's centre: what the batteries reach
  std::vector<ViewUnit> units;
  std::vector<ViewCraft> craft;
  std::vector<ViewShot> shots;
  std::vector<ViewEffect> effects;
  std::vector<ViewTractor> tractors;
  std::vector<ViewWell> wells;
  std::vector<ViewGroup> groups;
  std::array<ViewSide, 2> sides;
  float superlaserCharge = -1.0f; // 0-1, negative when there is no operational Death Star
  bool deathStarRunOpen = false;  // the Alliance may order a Death Star run now (§6.12)
  std::array<ViewDeployZone, 2> deployZones;
  std::vector<ViewEvent> recentEvents;
  bool decided = false;
  ViewEndReason endReason = ViewEndReason::None;
  bool finished = false;
};

// Detail for the selected unit's card and overlays: arcs, ranges and doctrine.
struct ViewUnitDetail {
  UnitIndex unit = kNoUnit;
  std::string name;
  std::string className;
  std::array<bool, 2> briefingKnown{true, true};
  WeaponGrid weapons;
  float tractorRange = 0.0f; // meters
  ShipDoctrine shipDoctrine;
  FlightDoctrine flightDoctrine;
  DoctrineOverrideMask overrides = 0;
  int32_t torpedoes = 0;
  int32_t hangarUsed = 0;
  int32_t hangarCapacity = 0;
};

} // namespace reb::battle
