#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

#include "space_battle/snapshot.h"

// The command model of tactical_combat.md §5: doctrine for task forces (capital ships) and flights (squadrons),
// per-unit overrides, live orders and commander signals. The player's UI, the battle AI and the headless commander
// all control a side only through these types, and every accepted Command goes into the order log (replays, saves).
namespace reb::battle
{

using GroupId = int32_t;
inline constexpr GroupId kNoGroup = -1;

struct Vec2 {
  float x = 0.0f; // meters on the battle plane
  float y = 0.0f;
};

// ---- capital-ship doctrine (§5.2) ---------------------------------------------------------------------------------

enum class Role : uint8_t { Line, Strike, Flank, Screen, Carrier, Picket, Reserve };
enum class EngagementRange : uint8_t { Close, Optimal, StandOff };
// UnderGuns: as Direct, but never leaving the side's planetary battery cover (§6.13); Direct without batteries.
enum class Approach : uint8_t { Direct, LeftHook, RightHook, Pincer, Hold, UnderGuns };
enum class Facing : uint8_t { Auto, BowOn, Broadside, StrongestToThreat };
enum class FireAllocation : uint8_t { Auto, Concentrate, Spread };
enum class Subsystem : uint8_t { Hull, Shields, Engines, Weapons, Hyperdrive, Tractor };
enum class PowerRouting : uint8_t { Balanced, Weapons, Shields, Engines };
enum class WithdrawRule : uint8_t { Never, HullBelow25, HullBelow50, HullBelow75, ShieldsCollapse, OnOrderOnly };
enum class Formation : uint8_t { LineAbreast, Column, Wedge, Ring, Loose };

// Target tags (§5.3), computed from unit data and state.
enum class TargetTag : uint8_t {
  // size
  Fighter,
  Light,
  Medium,
  Heavy,
  Super,
  DeathStar,
  // function
  Carrier,
  Transport,
  Interdictor,
  Tractor,
  Picket,
  Bomber,
  Flagship,
  // state
  ShieldsDown,
  Damaged,
  Withdrawing,
  Tractored,
  Threatening,
  // fallback
  Nearest,
};

struct TargetPriorities {
  static constexpr int kMaxTags = 4;
  std::array<TargetTag, kMaxTags> tags{TargetTag::Nearest, TargetTag::Nearest, TargetTag::Nearest, TargetTag::Nearest};
  uint8_t count = 1;
  bool finishDamaged = false;
};

struct ShipDoctrine {
  Role role = Role::Line;
  EngagementRange range = EngagementRange::Optimal;
  Approach approach = Approach::Direct;
  Facing facing = Facing::Auto;
  TargetPriorities priorities;
  FireAllocation allocation = FireAllocation::Auto;
  Subsystem subsystem = Subsystem::Hull;
  PowerRouting power = PowerRouting::Balanced;
  WithdrawRule withdraw = WithdrawRule::HullBelow25;
  Formation formation = Formation::LineAbreast;
  bool breakFormation = false;
  bool allBatteriesOnTarget = false;
  UnitIndex screenTarget = kNoUnit; // Role::Screen: the ship to protect
};

// ---- flight doctrine -----------------------------------------------------------------------------------------------

enum class FlightMission : uint8_t { Intercept, Escort, Strike, Patrol, HoldInHangar };
enum class DogfightRule : uint8_t { Engage, AvoidUnlessAttacked };
enum class RearmRule : uint8_t { WhenTorpedoesSpent, AtHalfLosses, Never };
enum class LaunchRule : uint8_t { Immediately, WhenEnemyFightersCommit, OnOrder };

struct FlightDoctrine {
  FlightMission mission = FlightMission::Intercept;
  TargetPriorities priorities;
  DogfightRule dogfight = DogfightRule::Engage;
  RearmRule rearm = RearmRule::WhenTorpedoesSpent;
  LaunchRule launch = LaunchRule::Immediately;
  UnitIndex escortTarget = kNoUnit; // Escort: a ship or a squadron of another flight
  Vec2 patrolCenter;
  float patrolRadius = 0.0f;
};

// Which fields of a unit's doctrine override its group's (§5.1): bit i = i-th field in declaration order.
using DoctrineOverrideMask = uint32_t;

enum ShipDoctrineField : DoctrineOverrideMask {
  kShipRole = 1U << 0,
  kShipRange = 1U << 1,
  kShipApproach = 1U << 2,
  kShipFacing = 1U << 3,
  kShipPriorities = 1U << 4,
  kShipAllocation = 1U << 5,
  kShipSubsystem = 1U << 6,
  kShipPower = 1U << 7,
  kShipWithdraw = 1U << 8,
  kShipFormation = 1U << 9,
  kShipBreakFormation = 1U << 10,
  kShipAllBatteriesOnTarget = 1U << 11,
  kShipScreenTarget = 1U << 12,
};

enum FlightDoctrineField : DoctrineOverrideMask {
  kFlightMission = 1U << 0,
  kFlightPriorities = 1U << 1,
  kFlightDogfight = 1U << 2,
  kFlightRearm = 1U << 3,
  kFlightLaunch = 1U << 4,
  kFlightEscortTarget = 1U << 5,
  kFlightPatrolCenter = 1U << 6,
  kFlightPatrolRadius = 1U << 7,
};

// ---- orders (§5.4) and signals (§5.6) -------------------------------------------------------------------------------

enum class SignalKind : uint8_t { ConcentrateFire, HoldTheLine, Scramble, CoordinatedJump };

struct OrderAttack {
  std::vector<UnitIndex> targets; // in order; replaces priorities until they are gone
};
struct OrderMove {
  Vec2 position;
  std::optional<float> facing; // radians, 0 = +x, counter-clockwise
};
struct OrderApproach {
  UnitIndex target = kNoUnit;
  Approach approach = Approach::Direct;
};
struct OrderHold {};
struct OrderEscort {
  UnitIndex target = kNoUnit;
};
struct OrderTractor {
  UnitIndex target = kNoUnit; // kNoUnit releases
};
struct OrderLaunch {};
struct OrderRecall {};
struct OrderStrike {
  UnitIndex target = kNoUnit;
};
struct OrderRecover {};
struct OrderWithdraw {};
struct OrderSetPower {
  PowerRouting power = PowerRouting::Balanced;
};
// For group targets the fields set the group's doctrine. For unit targets they become per-unit overrides, and
// `clear` drops those fields' overrides so the unit follows its group again.
struct OrderSetShipDoctrine {
  ShipDoctrine doctrine;
  DoctrineOverrideMask fields = ~0U; // which fields to apply
  DoctrineOverrideMask clear = 0;
};
struct OrderSetFlightDoctrine {
  FlightDoctrine doctrine;
  DoctrineOverrideMask fields = ~0U;
  DoctrineOverrideMask clear = 0;
};
struct OrderAssignGroup {
  GroupId group = kNoGroup; // kNoGroup creates a new group
};
struct OrderSignal {
  SignalKind signal = SignalKind::ConcentrateFire;
  UnitIndex target = kNoUnit;
};
struct OrderFireSuperlaser {
  UnitIndex target = kNoUnit;
};
struct OrderDeathStarRun {}; // a flight attacks the Death Star (§6.12)
// Briefing only (before the first Step): place the units, keeping their formation, with their centroid at `position`,
// clamped to the side's deployment zone.
struct OrderDeploy {
  Vec2 position;
  std::optional<float> facing;
};

using OrderPayload
    = std::variant<OrderAttack, OrderMove, OrderApproach, OrderHold, OrderEscort, OrderTractor, OrderLaunch,
                   OrderRecall, OrderStrike, OrderRecover, OrderWithdraw, OrderSetPower, OrderSetShipDoctrine,
                   OrderSetFlightDoctrine, OrderAssignGroup, OrderSignal, OrderFireSuperlaser, OrderDeathStarRun,
                   OrderDeploy>;

// Who an order applies to: listed units, or whole groups (doctrine changes to a group apply to the group's doctrine,
// so members without overrides follow it).
struct CommandTarget {
  std::vector<UnitIndex> units;
  std::vector<GroupId> groups;
};

struct Command {
  int64_t tick = 0; // simulation tick at which it takes effect
  Side side = Side::Alliance;
  CommandTarget target;
  OrderPayload order;
  bool queue = false; // Shift: append after current orders instead of replacing them
};

} // namespace reb::battle
