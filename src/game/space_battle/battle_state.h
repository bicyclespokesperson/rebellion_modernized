#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "space_battle/battle.h"
#include "space_battle/rng.h"
#include "space_battle/ship_profile.h"

// The simulation's internals: flat, index-addressed state that the step functions mutate and that commanders, the
// view model, saves and the state hash read. Battle::State() exposes it read-only, which is how a commander (the
// battle AI) perceives the battle; it never writes here, it only submits Commands.
//
// Every unit of the snapshot has a UnitState at its snapshot index. Capital ships, squadrons and Death Stars also have
// a kind-specific record (ships, squadrons, stations) at UnitState::slot; squadrons own a contiguous range of craft.
namespace reb::battle
{

// ---- orders ----------------------------------------------------------------------------------------------------------

enum class OrderKind : uint8_t { Attack, Move, Approach, Hold, Escort, Tractor, Strike, Recover, Recall, Withdraw, Run };

// A live order (§5.4) as queued on a unit or a group. Doctrine, power, group, launch and signal orders take effect at
// once and are never queued.
struct UnitOrder {
  OrderKind kind = OrderKind::Hold;
  std::vector<UnitIndex> targets; // Attack: in order; Approach, Escort, Tractor, Strike: [0]
  Vec2 position;                  // Move
  float facing = 0.0f;            // Move, when hasFacing
  bool hasFacing = false;
  Approach approach = Approach::Direct;
};

// ---- intents (§5.3) ----------------------------------------------------------------------------------------------------

enum class IntentKind : uint8_t {
  None,
  Engaging,      // firing on `target`
  Closing,       // moving to engage `target`
  NoTarget,      // nothing in range; `range` is the held engagement range
  Holding,       // Hold order or Reserve role
  Moving,        // Move order
  Screening,     // `target` is the ship it protects
  Escorting,
  Withdrawing,   // spooling; `progress`
  Trapped,       // spooling but cannot jump: `blocker` says why
  Docked,
  Rearming,
  Returning,     // to its carrier
  Intercepting,  // `target` is an enemy squadron
  Striking,      // `target` is a capital ship
  Patrolling,
  Tractoring,    // holding `target`
  TrenchRun,
  Disrupted,     // engines down from ion fire
  Destroyed,
  Withdrawn,
  HoldingCover,  // Under the guns: `target` is outside the planetary batteries' cover
};

enum class JumpBlocker : uint8_t { None, NoHyperdrive, Engines, Tractored, GravityWell, SideCannotRetreat };

struct Intent {
  IntentKind kind = IntentKind::None;
  UnitIndex target = kNoUnit;
  int8_t priorityRank = -1; // rank of the matched priority tag, -1 when none matched
  TargetTag tag = TargetTag::Nearest;
  int8_t arc = -1;          // the strongest arc bearing on the target, -1 when none
  float timeToKill = -1.0f; // seconds at the committed rate, -1 when it cannot be estimated
  int16_t othersAssigned = 0;
  float range = 0.0f;
  float progress = 0.0f;
  JumpBlocker blocker = JumpBlocker::None;
  bool fromOrder = false; // an explicit order, rather than doctrine, chose the target
};

// ---- units -------------------------------------------------------------------------------------------------------------

struct UnitState {
  UnitKind kind = UnitKind::Passive;
  Side side = Side::Alliance;
  UnitFate fate = UnitFate::Active;
  bool inSpace = false; // on the plane: false while docked, destroyed or gone
  int32_t slot = -1;
  Vec2 position;
  Vec2 previousPosition;
  float heading = 0.0f;
  float previousHeading = 0.0f;
  Vec2 velocity;
  GroupId group = kNoGroup;
  // Per-unit doctrine values; only the fields set in `overrides` replace the group's doctrine.
  ShipDoctrine shipDoctrine;
  FlightDoctrine flightDoctrine;
  DoctrineOverrideMask overrides = 0;
  std::vector<UnitOrder> orders; // unit orders; when empty the unit follows its group
  Intent intent;
};

struct ShipState {
  UnitIndex unit = kNoUnit;
  float hull = 0.0f;
  float shield = 0.0f;
  CriticalHits criticals;
  float speed = 0.0f;
  float desiredHeading = 0.0f;
  float desiredSpeed = 0.0f;
  Vec2 goal;
  Vec2 slotOffset; // its place in the group's formation, relative to the formation's centre
  UnitIndex primaryTarget = kNoUnit; // the allocator's (or an order's) target
  std::array<int32_t, kMaxBatteries> batteryCooldown{}; // ticks until the battery can fire
  std::array<UnitIndex, kMaxBatteries> batteryTarget{};
  PowerRouting power = PowerRouting::Balanced;
  PowerRouting powerFrom = PowerRouting::Balanced;
  int32_t powerTicks = 0; // ticks left in a routing transition
  int32_t engineOffTicks = 0;  // ion disruption
  int32_t systemsOffTicks = 0; // ion: tractor and gravity-well projectors down
  float ionPool = 0.0f;
  int32_t damageControlTicks = 0;
  UnitIndex tractorTarget = kNoUnit;
  float tractorHeld = 0.0f; // combined power of the enemy tractors holding this ship (recomputed each tick)
  bool withdrawing = false;
  int32_t spoolTicks = 0; // progress
  int32_t spoolTotal = 0;
  int32_t launchTicks = 0; // carriers: until the next launch
  int32_t nextThinkTick = 0;
  int32_t lastDamageTick = -1000000;
};

enum class SquadronPlace : uint8_t { Docked, InSpace, Destroyed, Withdrawn };
enum class SquadronTask : uint8_t { Idle, Intercept, Escort, Strike, Patrol, Return, TrenchRun };

struct SquadronState {
  UnitIndex unit = kNoUnit;
  int32_t firstCraft = 0;
  int32_t craftCount = 0; // craft slots (squadron size)
  int32_t alive = 0;
  int32_t initialAlive = 0;
  SquadronPlace place = SquadronPlace::Docked;
  UnitIndex carrier = kNoUnit; // home hangar; kNoUnit = the system (planet)
  SquadronTask task = SquadronTask::Idle;
  UnitIndex target = kNoUnit;
  Vec2 anchor; // the centre it circles when not attacking, or the escorted unit's position
  bool stayDocked = false;    // Recall, Hold in hangar, losses or a departing side: do not relaunch
  bool launchOrdered = false; // Launch: on order
  bool stayAfterDock = false; // what the flight returns for: rearm and relaunch, or stay aboard
  bool noHangar = false;      // it wanted to dock but nothing had room: it fights on
  // It has flown in this battle. Back in a hangar it no longer holds space for its side (§6.11), unlike a squadron
  // still waiting for its first launch.
  bool launched = false;
  int32_t rearmTicks = 0;
  int32_t nextThinkTick = 0;
  int32_t lastAttackedTick = -1000000;
  Vec2 centroid;
  float spread = 0.0f; // bounding radius around the centroid
  bool withdrawing = false;
  int32_t spoolTicks = 0;
  int32_t spoolTotal = 0;
  int32_t runTicks = 0; // on the trench run
};

enum class CraftPhase : uint8_t { Formation, Approach, BreakOff, Extend, Pursue };

struct CraftState {
  Vec2 position;
  Vec2 previousPosition;
  float heading = 0.0f;
  float previousHeading = 0.0f;
  float hull = 0.0f;
  float shield = 0.0f;
  int16_t torpedoes = 0;
  bool alive = false;
  CraftPhase phase = CraftPhase::Formation;
  int16_t phaseTicks = 0;
  float breakHeading = 0.0f;
  int32_t targetCraft = -1; // dogfights: index into BattleState::craft
  int16_t gunTicks = 0;
  int16_t torpedoTicks = 0;
};

struct StationState {
  UnitIndex unit = kNoUnit;
  float hull = 0.0f;
  int32_t chargeTicks = 0; // ticks until the superlaser is charged; 0 = charged
  UnitIndex firingAt = kNoUnit;
  int32_t fireDelayTicks = 0;
  int32_t launchTicks = 0;
  bool withdrawing = false;
  int32_t spoolTicks = 0;
  int32_t spoolTotal = 0;
  GroupId runFlight = kNoGroup; // the one flight allowed on the trench run
};

// ---- planetary batteries (§6.13) --------------------------------------------------------------------------------------------

// A battery of the snapshot's `defenses`, derived at initialisation and never saved.
struct DefenseProfile {
  Side side = Side::Alliance;
  WeaponKind weapon = WeaponKind::Turbolaser;
  float dps = 0.0f;       // against capital ships, before accuracy
  float siteAngle = 0.0f; // where it stands on the planet's limb, radians from the planet's centre
};

struct DefenseState {
  int32_t cooldown = 0; // ticks until it can fire
  UnitIndex target = kNoUnit;
  int64_t lastFireTick = -1;
};

// ---- groups --------------------------------------------------------------------------------------------------------------

// One entry of a group's scored target list (§5.3). `score` uses the group's priorities; a unit with its own
// priorities rescales `stateFactor` by its own tag weight.
struct ScoredTarget {
  UnitIndex unit = kNoUnit;
  float score = 0.0f;       // priority weight x state factor, before per-ship efficiency
  float stateFactor = 0.0f; // how close it is to dying, and the finish-damaged boost
  uint32_t tags = 0;        // Tags() plus Threatening when it shoots at what this group protects
  int8_t rank = -1;
  TargetTag tag = TargetTag::Nearest;
};

struct GroupState {
  GroupId id = kNoGroup;
  Side side = Side::Alliance;
  bool flight = false;
  std::string name;
  std::vector<UnitIndex> members;
  ShipDoctrine shipDoctrine;
  FlightDoctrine flightDoctrine;
  std::vector<UnitOrder> orders;
  int32_t nextThinkTick = 0;
  bool committed = false; // a Reserve group leaves its station once it has been given an order
  UnitIndex focus = kNoUnit;
  Vec2 approachReference;
  UnitIndex approachTarget = kNoUnit;
  Vec2 goal;
  bool hasGoal = false;
  float goalHeading = 0.0f;
  float speed = 0.0f;        // formation speed: the slowest member's
  float frameHeading = 0.0f; // the formation's forward direction
  std::vector<ScoredTarget> candidates; // the latest scored target list, best first
};

// ---- sides ------------------------------------------------------------------------------------------------------------------

struct SideState {
  Vec2 deploymentCenter;
  float deploymentRadius = 0.0f;
  UnitIndex admiral = kNoUnit;   // character
  UnitIndex commander = kNoUnit; // character
  float leadership = 0.0f;
  float commanderCombat = 0.0f;
  float commandRating = 0.0f; // 0-1 while the admiral's ship is in the battle
  int32_t signalsReady = 0;
  int32_t signalsMax = 0;
  int32_t signalRechargeTicks = 0;
  UnitIndex concentrateTarget = kNoUnit;
  int32_t concentrateTicks = 0;
  int32_t holdLineTicks = 0;
  int32_t scrambleTicks = 0;
  int32_t coordinatedJumpTicks = 0;
  bool fleetWithdrawal = false;
  float durableLow = 0.0f; // the lowest hull-and-craft total so far, for the stalemate rule
  float strength = 0.0f;
  float initialStrength = 0.0f;
  bool everWithdrew = false; // at least one unit got away
  int32_t planetLaunchTicks = 0; // the defender's system-based squadrons launch from the planet's limb
};

// ---- shots and events -----------------------------------------------------------------------------------------------------

enum class ShotKind : uint8_t { Turbolaser, Ion, Laser, Torpedo };

// A shot in flight. The hit was decided when it was fired; damage lands at `impactTick`.
struct PendingShot {
  int64_t fireTick = 0;
  int64_t impactTick = 0;
  UnitIndex shooter = kNoUnit; // kNoUnit for a planetary battery
  UnitIndex target = kNoUnit;
  int32_t targetCraft = -1;
  int16_t battery = -1; // index in `defenses` when a planetary battery fired it
  Side side = Side::Alliance;
  ShotKind kind = ShotKind::Turbolaser;
  Subsystem focus = Subsystem::Hull;
  bool hit = false;
  bool shieldImpact = false;
  float damage = 0.0f;
  Vec2 from;
  Vec2 to;
};

struct BattleEvent {
  int64_t tick = 0;
  Side side = Side::Alliance;
  std::string text;
  UnitIndex unit = kNoUnit;
};

enum class EndReason : uint8_t { None, Elimination, Stalemate, TimeLimit };

// Cosmetic only: never saved, hashed or read by the simulation.
struct Effect {
  ViewEffectKind kind = ViewEffectKind::Explosion;
  Vec2 position;
  float size = 0.0f;
  int64_t tick = 0;
  UnitIndex unit = kNoUnit;
};

// ---- the whole battle ---------------------------------------------------------------------------------------------------

struct BattleState {
  BattleSnapshot snapshot;
  BattleOptions options;
  std::vector<UnitProfile> profiles; // derived from the snapshot; never saved
  std::vector<DefenseProfile> defenseProfiles; // derived from the snapshot's defenses; never saved

  int64_t tick = 0;
  Pcg32 rng;
  Side attacker = Side::Empire;
  Vec2 arrivalDirection{1.0f, 0.0f}; // from the battle centre toward where the attacker came from
  Vec2 planetCenter;

  std::vector<UnitState> units;
  std::vector<ShipState> ships;
  std::vector<SquadronState> squadrons;
  std::vector<CraftState> craft;
  std::vector<StationState> stations;
  std::vector<DefenseState> defenses; // one per snapshot defense
  std::vector<GroupState> groups;
  std::array<SideState, 2> sides;
  std::vector<PendingShot> shots;
  std::vector<Command> pending; // accepted, not yet applied
  std::vector<Command> commandLog;
  std::vector<BattleEvent> events;
  int64_t lastDurableDropTick = 0;
  bool finished = false;
  EndReason endReason = EndReason::None;
  int64_t outroTicks = 0; // live presentation only; never archived
  int64_t outroEndTick = 0;

  std::vector<Effect> effects;

  // ---- read helpers (battle_state.cpp) ----
  const UnitProfile& Profile(UnitIndex unit) const
  {
    return profiles[unit];
  }
  bool Valid(UnitIndex unit) const
  {
    return unit >= 0 && unit < static_cast<UnitIndex>(units.size());
  }
  // In the battlespace and able to act or be shot: not docked, destroyed or gone.
  bool InSpace(UnitIndex unit) const
  {
    return Valid(unit) && units[unit].inSpace;
  }
  // Still in the battle (docked squadrons count): what decides whether a side is present.
  bool Present(UnitIndex unit) const;
  const ShipState* Ship(UnitIndex unit) const;
  ShipState* Ship(UnitIndex unit);
  const SquadronState* Squadron(UnitIndex unit) const;
  SquadronState* Squadron(UnitIndex unit);
  const StationState* Station(UnitIndex unit) const;
  const GroupState* Group(GroupId id) const;
  GroupState* Group(GroupId id);

  ShipDoctrine EffectiveShipDoctrine(UnitIndex unit) const;
  FlightDoctrine EffectiveFlightDoctrine(UnitIndex unit) const;
  // The orders the unit is executing: its own, else its group's.
  const std::vector<UnitOrder>& ActiveOrders(UnitIndex unit) const;
  // Acting on its own rather than in its group's formation: it has orders of its own, or overrides a movement field.
  bool Detached(UnitIndex unit) const;

  float HullFraction(UnitIndex unit) const;
  float ShieldFraction(UnitIndex unit) const;
  // Static tags plus the state tags (shields down, damaged, withdrawing, tractored) that hold now.
  uint32_t Tags(UnitIndex unit) const;
  bool Withdrawing(UnitIndex unit) const;

  // Admiral term and speed bonus (§5.6, §6.2) for this side's capital ships.
  float AdmiralTurnTerm(Side side) const;
  float TransitionScale(Side side) const;

  float BattleSeconds() const
  {
    return static_cast<float>(tick) / kTicksPerSecond;
  }
};

Side Opponent(Side side);
int SideIndex(Side side);

} // namespace reb::battle
