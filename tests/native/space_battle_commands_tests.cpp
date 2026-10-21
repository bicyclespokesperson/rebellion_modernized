#include "space_battle/unit_names.h"
#include "space_battle_test_support.h"

#include "space_battle/doctrine.h"
#include "space_battle/tuning.h"

using namespace sbtest;
namespace tuning = reb::battle::tuning;

namespace
{
constexpr float kHulkHull = 8000.0f;

// An unarmed, immobile, unshielded hulk: hull lost is exactly the damage dealt.
CapitalShipEntry Hulk(Side side, const std::string& name = "Hulk")
{
  CapitalShipEntry ship = BaseShip(side, name);
  ship.hullMax = ship.hull = static_cast<int32_t>(kHulkHull);
  ship.shieldMax = 0;
  ship.shieldRecharge = 0;
  ship.sublight = 0;
  ship.weaponRecharge = 0;
  ship.hyperdrive = 0;
  ship.hyperdriveBackup = 0;
  return ship;
}

GroupId GroupOf(const Battle& battle, UnitIndex unit)
{
  return battle.State().units[unit].group;
}

void SetShipDoctrine(Battle& battle, Side side, std::vector<GroupId> groups, std::vector<UnitIndex> units,
                     const ShipDoctrine& doctrine, DoctrineOverrideMask fields)
{
  OrderSetShipDoctrine order;
  order.doctrine = doctrine;
  order.fields = fields;
  SB_CHECK(battle.Submit(MakeCommand(side, order, std::move(units), std::move(groups))));
}

Vec2 Position(const Battle& battle, UnitIndex unit)
{
  return battle.State().units[unit].position;
}

std::string IntentOf(const Battle& battle, UnitIndex unit)
{
  ViewModel view;
  battle.BuildViewModel(1.0f, view);
  for (const ViewUnit& entry : view.units) {
    if (entry.unit == unit) {
      return entry.intent;
    }
  }
  return {};
}

// §11.7: a broadside cruiser on Auto facing deals more damage per minute than the same ship held bow-on.
void AutoFacingBeatsBowOnForBroadsideShips()
{
  float dealt[2] = {0.0f, 0.0f};
  for (const Facing facing : {Facing::Auto, Facing::BowOn}) {
    SnapshotBuilder builder;
    const UnitIndex shooter = builder.Add(Broadside(Side::Alliance));
    const UnitIndex hulk = builder.Add(Hulk(Side::Empire));
    Battle battle(builder.Snapshot(), Uncommanded());
    Place(battle, shooter, {-3200.0f, 0.0f}, 0.0f);
    Place(battle, hulk, {0.0f, 0.0f}, kPi);
    ShipDoctrine doctrine;
    doctrine.facing = facing;
    doctrine.approach = Approach::Hold; // compare facing alone, at one range
    SetShipDoctrine(battle, Side::Alliance, {GroupOf(battle, shooter)}, {}, doctrine, kShipFacing | kShipApproach);
    battle.RunHeadless(60 * kTicksPerSecond);
    dealt[facing == Facing::Auto ? 0 : 1] = kHulkHull - battle.State().Ship(hulk)->hull;
  }
  std::printf("hull damage in a minute: Auto %.0f, bow-on %.0f\n", dealt[0], dealt[1]);
  SB_CHECK(dealt[0] > 1.3f * dealt[1]);
}

void HooksSwingAroundTheTargetsFlank()
{
  for (const Approach approach : {Approach::LeftHook, Approach::RightHook, Approach::Direct}) {
    SnapshotBuilder builder;
    const UnitIndex first = builder.Add(Broadside(Side::Alliance, "First"));
    const UnitIndex second = builder.Add(Broadside(Side::Alliance, "Second"));
    const UnitIndex hulk = builder.Add(Hulk(Side::Empire));
    Battle battle(builder.Snapshot(), Uncommanded());
    Place(battle, first, {-9000.0f, -600.0f}, 0.0f);
    Place(battle, second, {-9000.0f, 600.0f}, 0.0f);
    Place(battle, hulk, {0.0f, 0.0f}, kPi);
    ShipDoctrine doctrine;
    doctrine.approach = approach;
    SetShipDoctrine(battle, Side::Alliance, {GroupOf(battle, first)}, {}, doctrine, kShipApproach);
    battle.RunHeadless(90 * kTicksPerSecond);
    const Vec2 centroid = (Position(battle, first) + Position(battle, second)) * 0.5f;
    const float bearing = AngleOf(centroid);
    std::printf("approach %d ends at bearing %.0f degrees from the target\n", static_cast<int>(approach),
                bearing / tuning::kDegrees);
    // Coming from the west (180 degrees), a left hook ends on the target's north flank, a right hook on its south.
    switch (approach) {
      case Approach::LeftHook:
        SB_CHECK(bearing > 60.0f * tuning::kDegrees && bearing < 150.0f * tuning::kDegrees);
        break;
      case Approach::RightHook:
        SB_CHECK(bearing < -60.0f * tuning::kDegrees && bearing > -150.0f * tuning::kDegrees);
        break;
      default:
        SB_CHECK(std::fabs(bearing) > 125.0f * tuning::kDegrees); // broadsides rake sideways once in range
        break;
    }
  }
}

// A task force moves at the speed of its slowest ship unless its doctrine says break formation (§5.1).
void TaskForcesKeepTheSlowestSpeed()
{
  float gaps[2] = {0.0f, 0.0f};
  for (const bool breakFormation : {false, true}) {
    SnapshotBuilder builder;
    const UnitIndex slow = builder.Add(Wedge(Side::Alliance, "Slow"));
    const UnitIndex fast = builder.Add(Picket(Side::Alliance, "Fast"));
    const UnitIndex hulk = builder.Add(Hulk(Side::Empire));
    Battle battle(builder.Snapshot(), Uncommanded());
    Place(battle, slow, {-15000.0f, -500.0f}, 0.0f);
    Place(battle, fast, {-15000.0f, 500.0f}, 0.0f);
    Place(battle, hulk, {15000.0f, 0.0f}, kPi);
    SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderAssignGroup{GroupOf(battle, slow)}, {fast})));
    SB_CHECK(GroupOf(battle, fast) == GroupOf(battle, slow));
    ShipDoctrine doctrine;
    doctrine.role = Role::Line;
    doctrine.range = EngagementRange::Optimal;
    doctrine.approach = Approach::Direct;
    doctrine.breakFormation = breakFormation;
    SetShipDoctrine(battle, Side::Alliance, {GroupOf(battle, slow)}, {}, doctrine,
                    kShipRole | kShipRange | kShipApproach | kShipBreakFormation);
    battle.RunHeadless(40 * kTicksPerSecond);
    gaps[breakFormation ? 1 : 0] = Position(battle, fast).x - Position(battle, slow).x;
    const float slowTravel = Position(battle, slow).x + 15000.0f;
    SB_CHECK(slowTravel > 6000.0f);
  }
  std::printf("lead of the fast ship after 40 s: %.0f m in formation, %.0f m breaking formation\n", gaps[0], gaps[1]);
  SB_CHECK(std::fabs(gaps[0]) < 1200.0f);
  SB_CHECK(gaps[1] > 2500.0f);
}

void PowerRoutingTransitionsOverFiveSeconds()
{
  for (const bool admiral : {false, true}) {
    SnapshotBuilder builder;
    const UnitIndex ship = builder.Add(Wedge(Side::Alliance));
    if (admiral) {
      builder.AddCharacter(Side::Alliance, ship, true, false, 100, 50);
    }
    builder.Add(Hulk(Side::Empire));
    Battle battle(builder.Snapshot(), Uncommanded());
    const float before = MaxSpeed(battle.State(), ship);
    SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderSetPower{PowerRouting::Engines}, {}, {GroupOf(battle, ship)})));
    battle.Step();
    SB_CHECK_NEAR(MaxSpeed(battle.State(), ship), before * 0.5f * (1.0f + 1.4f), 1e-2f);
    SB_CHECK_NEAR(WeaponOutput(battle.State(), ship), 0.5f * (1.0f + 0.8f), 1e-4f);
    const int transition = sim::Ticks(tuning::kPowerSwitchSeconds * (admiral ? 0.6f : 1.0f));
    for (int i = 0; i < transition; ++i) {
      battle.Step();
    }
    SB_CHECK_NEAR(MaxSpeed(battle.State(), ship), before * 1.4f, 1e-2f);
    SB_CHECK_NEAR(ShieldRegen(battle.State(), ship), battle.State().Profile(ship).shieldRecharge * 0.8f, 1e-3f);
  }
}

void CommandSignals()
{
  SnapshotBuilder builder;
  const UnitIndex flagship = builder.Add(Wedge(Side::Alliance, "Flagship"));
  const UnitIndex escort = builder.Add(Wedge(Side::Alliance, "Escort"));
  builder.AddCharacter(Side::Alliance, flagship, true, false, 65, 40);
  const UnitIndex near = builder.Add(Hulk(Side::Empire, "Near"));
  const UnitIndex named = builder.Add(Hulk(Side::Empire, "Named"));
  Battle battle(builder.Snapshot(), Uncommanded());
  Place(battle, flagship, {-3000.0f, 0.0f}, 0.0f);
  Place(battle, escort, {-3000.0f, 1200.0f}, 0.0f);
  Place(battle, near, {0.0f, 0.0f}, kPi);
  Place(battle, named, {0.0f, 9000.0f}, kPi);
  const SideState& alliance = battle.State().sides[0];
  SB_CHECK(alliance.signalsMax == 3 && alliance.signalsReady == 3); // 1 + leadership / 30
  battle.RunHeadless(3 * kTicksPerSecond);
  SB_CHECK(battle.State().Ship(flagship)->primaryTarget == near);
  // Concentrate fire: every ship in range shifts to the named target.
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderSignal{SignalKind::ConcentrateFire, named})));
  battle.RunHeadless(2 * kTicksPerSecond);
  SB_CHECK(battle.State().Ship(flagship)->primaryTarget == named);
  SB_CHECK(battle.State().Ship(escort)->primaryTarget == named);
  SB_CHECK(battle.State().units[flagship].intent.fromOrder);
  // Hold the line: no automatic withdrawal while it lasts.
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderSignal{SignalKind::HoldTheLine, kNoUnit})));
  Mutable(battle).Ship(escort)->hull = 400.0f;
  battle.RunHeadless(5 * kTicksPerSecond);
  SB_CHECK(!battle.State().Ship(escort)->withdrawing);
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderSignal{SignalKind::Scramble, kNoUnit})));
  SB_CHECK(battle.State().sides[0].signalsReady == 0);
  SB_CHECK(!battle.Submit(MakeCommand(Side::Alliance, OrderSignal{SignalKind::Scramble, kNoUnit})));
  battle.RunHeadless(static_cast<int64_t>(tuning::kHoldTheLineSeconds) * kTicksPerSecond);
  SB_CHECK(battle.State().Ship(escort)->withdrawing || battle.State().units[escort].fate != UnitFate::Active);
  // Charges recharge slowly.
  battle.RunHeadless(static_cast<int64_t>(tuning::kSignalRechargeSeconds) * kTicksPerSecond);
  SB_CHECK(battle.State().sides[0].signalsReady >= 1);
  // Losing the flagship loses the signals.
  Mutable(battle).Ship(flagship)->shield = 0.0f;
  sim::DamageShip(Mutable(battle), flagship, 100000.0f, ShotKind::Turbolaser, Subsystem::Hull);
  SB_CHECK(!battle.Finished());
  battle.Step();
  SB_CHECK(battle.State().sides[0].signalsMax == 0 && battle.State().sides[0].signalsReady == 0);
  SB_CHECK(!battle.Submit(MakeCommand(Side::Alliance, OrderSignal{SignalKind::HoldTheLine, kNoUnit})));
}

void CoordinatedJumpSpeedsSpooling()
{
  SnapshotBuilder builder;
  const UnitIndex ship = builder.Add(Wedge(Side::Alliance));
  builder.AddCharacter(Side::Alliance, ship, true, false, 50, 40);
  builder.Add(Hulk(Side::Empire));
  Battle battle(builder.Snapshot(), Uncommanded());
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderWithdraw{}, {ship})));
  const int32_t plain = battle.State().Ship(ship)->spoolTotal;
  SB_CHECK(plain > 0);
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderSignal{SignalKind::CoordinatedJump, kNoUnit})));
  SB_CHECK(battle.State().Ship(ship)->spoolTotal < plain);
  SB_CHECK_NEAR(static_cast<float>(battle.State().Ship(ship)->spoolTotal), 0.6f * static_cast<float>(plain), 2.0f);
}

void HoldAndQueuedOrders()
{
  SnapshotBuilder builder;
  const UnitIndex ship = builder.Add(Wedge(Side::Alliance));
  const UnitIndex hulk = builder.Add(Hulk(Side::Empire));
  const UnitIndex other = builder.Add(Hulk(Side::Empire, "Other"));
  Battle battle(builder.Snapshot(), Uncommanded());
  Place(battle, ship, {-3000.0f, 0.0f}, 0.0f);
  Place(battle, hulk, {0.0f, 0.0f}, kPi);
  Place(battle, other, {-3000.0f, 6000.0f}, kPi);
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderHold{}, {}, {GroupOf(battle, ship)})));
  battle.RunHeadless(30 * kTicksPerSecond);
  SB_CHECK(Distance(Position(battle, ship), {-3000.0f, 0.0f}) < 300.0f);
  SB_CHECK(battle.State().Ship(hulk)->hull < kHulkHull); // it held, and fired
  // Move, then (queued) attack the other hulk.
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderMove{{-6000.0f, 2000.0f}, std::nullopt}, {ship})));
  Command attack = MakeCommand(Side::Alliance, OrderAttack{{other}}, {ship});
  attack.queue = true;
  SB_CHECK(battle.Submit(attack));
  SB_CHECK(battle.State().units[ship].orders.size() == 2);
  bool attacked = false;
  for (int tick = 0; tick < 120 * kTicksPerSecond && !attacked; ++tick) {
    battle.Step();
    const UnitState& unit = battle.State().units[ship];
    attacked = unit.orders.size() == 1 && unit.orders.front().kind == OrderKind::Attack
               && battle.State().Ship(ship)->primaryTarget == other;
  }
  SB_CHECK(attacked);
  // Attack overrides priorities: the closer hulk is ignored.
  battle.RunHeadless(2 * kTicksPerSecond);
  SB_CHECK(battle.State().Ship(ship)->primaryTarget == other);
  SB_CHECK(battle.State().units[ship].intent.fromOrder);
}

void EscortStaysBetweenTheShipAndTheThreat()
{
  SnapshotBuilder builder;
  const UnitIndex charge = builder.Add(Wedge(Side::Alliance, "Charge"));
  const UnitIndex escort = builder.Add(Picket(Side::Alliance, "Escort"));
  const UnitIndex threat = builder.Add(Hulk(Side::Empire, "Threat"));
  Battle battle(builder.Snapshot(), Uncommanded());
  Place(battle, charge, {0.0f, 0.0f}, 0.0f);
  Place(battle, escort, {-3000.0f, 0.0f}, 0.0f);
  Place(battle, threat, {12000.0f, 0.0f}, kPi);
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderHold{}, {charge})));
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderEscort{charge}, {escort})));
  battle.RunHeadless(40 * kTicksPerSecond);
  const Vec2 at = Position(battle, escort);
  std::printf("escort at (%.0f, %.0f), screening %.0f m toward the threat\n", at.x, at.y, at.x);
  const float station = tuning::kScreenDistance + battle.State().Profile(charge).radius;
  SB_CHECK(at.x > 0.5f * station && at.x < 1.5f * station);
  SB_CHECK(IntentOf(battle, escort).find("Escorting Charge") == 0);
}

void ReserveHoldsUntilCommitted()
{
  SnapshotBuilder builder;
  const UnitIndex transport = builder.Add(TransportShip(Side::Alliance));
  builder.Add(Wedge(Side::Empire));
  Battle battle(builder.Snapshot(), Uncommanded());
  const Vec2 start = Position(battle, transport);
  battle.RunHeadless(40 * kTicksPerSecond);
  SB_CHECK(Distance(Position(battle, transport), start) < 100.0f);
  SB_CHECK(IntentOf(battle, transport) == "Holding position");
  SB_CHECK(battle.Submit(
      MakeCommand(Side::Alliance, OrderMove{start + Vec2{0.0f, 3000.0f}, std::nullopt}, {}, {GroupOf(battle, transport)})));
  battle.RunHeadless(20 * kTicksPerSecond);
  SB_CHECK(Distance(Position(battle, transport), start) > 1000.0f);
}

void NativeNamesAppearInBattleText()
{
  SnapshotBuilder builder;
  auto ship = Wedge(Side::Alliance, "");
  ship.identity.typeIndex = 20;
  const UnitIndex first = builder.Add(ship);
  ship.identity.name = " 4";
  const UnitIndex numbered = builder.Add(ship);
  ship.identity.name = "Executor";
  const UnitIndex named = builder.Add(ship);
  ship.identity.name.clear();
  ship.identity.side = Side::Empire;
  const UnitIndex enemy = builder.Add(ship);
  auto squadron = XwingLike(Side::Alliance, first, "");
  squadron.identity.typeIndex = 29;
  const UnitIndex fighter = builder.Add(squadron);
  const auto& snapshot = builder.Snapshot();
  SB_CHECK(NameUnit(snapshot, first).name == "Imperial Star Destroyer 1");
  SB_CHECK(NameUnit(snapshot, numbered).name == "Imperial Star Destroyer 4");
  SB_CHECK(NameUnit(snapshot, named).name == "Executor");
  SB_CHECK(NameUnit(snapshot, named).ownName);
  SB_CHECK(NameUnit(snapshot, enemy).name == "Imperial Star Destroyer 1");
  SB_CHECK(NameUnit(snapshot, fighter).name == "X-wing Squadron 1");
  Battle battle(snapshot, Uncommanded());
  SB_CHECK(sim::UnitName(battle.State(), numbered) == "Imperial Star Destroyer 4");
  Place(battle, first, {-3000.0f, 0.0f}, 0.0f);
  Place(battle, enemy, {0.0f, 0.0f}, kPi);
  battle.RunHeadless(20 * kTicksPerSecond);
  SB_CHECK(IntentOf(battle, first).find("Imperial Star Destroyer 1") != std::string::npos);
  bool launched = false;
  for (const auto& event : battle.State().events) {
    if (event.text == "X-wing Squadron 1 launched") {
      launched = true;
    }
  }
  SB_CHECK(launched);
  Intent& fallback = Mutable(battle).units[first].intent;
  fallback.kind = IntentKind::Closing;
  fallback.target = enemy;
  fallback.priorityRank = -1;
  fallback.fromOrder = false;
  SB_CHECK(IntentOf(battle, first) == "Closing on Imperial Star Destroyer 1: best available target");
}

void PrioritiesPickTheTaggedTarget()
{
  SnapshotBuilder builder;
  const UnitIndex shooter = builder.Add(Wedge(Side::Alliance));
  const UnitIndex heavy = builder.Add(Broadside(Side::Empire, "Heavy"));
  const UnitIndex carrier = builder.Add(CarrierShip(Side::Empire, "Carrier"));
  builder.Add(TieLike(Side::Empire, carrier));
  Battle battle(builder.Snapshot(), Uncommanded());
  Place(battle, shooter, {-3000.0f, 0.0f}, 0.0f);
  Place(battle, heavy, {0.0f, 0.0f}, kPi);
  Place(battle, carrier, {500.0f, 2500.0f}, kPi);
  battle.RunHeadless(2 * kTicksPerSecond);
  SB_CHECK(battle.State().Ship(shooter)->primaryTarget == heavy); // default: capitals by size first
  ShipDoctrine doctrine;
  doctrine.priorities = MakePriorities({TargetTag::Carrier, TargetTag::Nearest}, false);
  SetShipDoctrine(battle, Side::Alliance, {GroupOf(battle, shooter)}, {}, doctrine, kShipPriorities);
  battle.RunHeadless(2 * kTicksPerSecond);
  SB_CHECK(battle.State().Ship(shooter)->primaryTarget == carrier);
  const std::string intent = IntentOf(battle, shooter);
  std::printf("intent: %s\n", intent.c_str());
  SB_CHECK(intent.find("Targeting Carrier: priority 1, Carriers") == 0 || intent.find("Closing on Carrier") == 0);
}

// A per-unit override of a movement field detaches the ship from its group's formation (§5.1).
// Turbolasers with a few short-ranged lasers: Close and Optimal ranges differ a lot.
CapitalShipEntry Gunline(Side side, const std::string& name)
{
  CapitalShipEntry ship = BaseShip(side, name);
  SetArc(ship, Arc::Fore, 100, 0, 20);
  SetArc(ship, Arc::Aft, 40, 0, 20);
  SetArc(ship, Arc::Port, 80, 0, 20);
  SetArc(ship, Arc::Starboard, 80, 0, 20);
  ship.weapons.range = {50, 0, 18};
  return ship;
}

void UnitOverridesActOnTheirOwn()
{
  SnapshotBuilder builder;
  const UnitIndex first = builder.Add(Gunline(Side::Alliance, "First"));
  const UnitIndex second = builder.Add(Gunline(Side::Alliance, "Second"));
  const UnitIndex hulk = builder.Add(Hulk(Side::Empire));
  Battle battle(builder.Snapshot(), Uncommanded());
  SB_CHECK(GroupOf(battle, first) == GroupOf(battle, second));
  Place(battle, first, {-9000.0f, -500.0f}, 0.0f);
  Place(battle, second, {-9000.0f, 500.0f}, 0.0f);
  Place(battle, hulk, {0.0f, 0.0f}, kPi);
  ShipDoctrine doctrine;
  doctrine.range = EngagementRange::Close;
  doctrine.approach = Approach::Direct;
  SetShipDoctrine(battle, Side::Alliance, {}, {first}, doctrine, kShipRange | kShipApproach);
  SB_CHECK(battle.State().Detached(first) && !battle.State().Detached(second));
  SB_CHECK(battle.Detail(first).overrides == (kShipRange | kShipApproach));
  SB_CHECK(battle.Detail(first).shipDoctrine.range == EngagementRange::Close);
  battle.RunHeadless(60 * kTicksPerSecond);
  const float closeDistance = Distance(Position(battle, first), {0.0f, 0.0f});
  const float optimalDistance = Distance(Position(battle, second), {0.0f, 0.0f});
  std::printf("override: Close ship at %.0f m, Optimal ship at %.0f m\n", closeDistance, optimalDistance);
  SB_CHECK(closeDistance + 800.0f < optimalDistance);
  OrderSetShipDoctrine reset;
  reset.fields = 0;
  reset.clear = kShipRange | kShipApproach;
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, reset, {first})));
  SB_CHECK(!battle.State().Detached(first));
}

void AllBatteriesOnTargetWhenItsShieldsAreDown()
{
  SnapshotBuilder builder;
  CapitalShipEntry gunship = Picket(Side::Alliance, "Gunship");
  SetArc(gunship, Arc::Fore, 60, 0, 140);
  gunship.weapons.range = {40, 0, 18};
  const UnitIndex shooter = builder.Add(gunship);
  const UnitIndex target = builder.Add(Wedge(Side::Empire, "Target"));
  const UnitIndex carrier = builder.Add(CarrierShip(Side::Empire));
  const UnitIndex fighters = builder.Add(TieLike(Side::Empire, carrier));
  Battle battle(builder.Snapshot(), Uncommanded());
  BattleState& state = Mutable(battle);
  Place(battle, shooter, {0.0f, 0.0f}, 0.0f);
  Place(battle, target, {1500.0f, 0.0f}, kPi);
  Place(battle, carrier, {9000.0f, 0.0f}, kPi);
  sim::LaunchSquadron(state, fighters);
  sim::UpdateSquadronBounds(state);
  const SquadronState& squadron = *state.Squadron(fighters);
  for (int i = 0; i < squadron.craftCount; ++i) {
    state.craft[squadron.firstCraft + i].position = {900.0f, 100.0f};
  }
  sim::UpdateSquadronBounds(state);
  state.Ship(target)->shield = 0.0f;
  state.Ship(shooter)->primaryTarget = target;
  GroupState& group = state.groups[static_cast<size_t>(GroupOf(battle, shooter))];
  sim::ScoreGroupTargets(state, group);
  const UnitProfile& profile = state.Profile(shooter);
  int laser = -1;
  for (int i = 0; i < profile.batteryCount; ++i) {
    laser = profile.batteries[i].arc == Arc::Fore && profile.batteries[i].kind == WeaponKind::Laser ? i : laser;
  }
  SB_CHECK(laser >= 0);
  int32_t craft = -1;
  SB_CHECK(sim::SelectBatteryTarget(state, shooter, laser, &craft) == fighters); // lasers prefer fighters
  group.shipDoctrine.allBatteriesOnTarget = true;
  SB_CHECK(sim::SelectBatteryTarget(state, shooter, laser, &craft) == target);
}

void SquadronAttackOrdersEngage()
{
  SnapshotBuilder builder;
  builder.Params().heldFirstBy = Side::Empire;
  builder.Add(Broadside(Side::Alliance));
  const UnitIndex red = builder.Add(XwingLike(Side::Alliance, kNoUnit, "Red"));
  const UnitIndex wedge = builder.Add(Wedge(Side::Empire));
  const UnitIndex ties = builder.Add(TieLike(Side::Empire, wedge));
  Battle battle(builder.Snapshot(), Uncommanded());
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderAttack{{wedge}}, {red})));
  battle.Step();
  SB_CHECK(battle.State().Squadron(red)->task == SquadronTask::Strike);
  SB_CHECK(battle.State().Squadron(red)->target == wedge);
  sim::LaunchSquadron(Mutable(battle), ties);
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderAttack{{ties}}, {red})));
  battle.Step();
  SB_CHECK(battle.State().Squadron(red)->task == SquadronTask::Intercept);
  SB_CHECK(battle.State().Squadron(red)->target == ties);
}

void DoctrineShieldCollapseWithdraws()
{
  SnapshotBuilder builder;
  const UnitIndex ship = builder.Add(Wedge(Side::Alliance));
  builder.Add(Hulk(Side::Empire));
  Battle battle(builder.Snapshot(), Uncommanded());
  ShipDoctrine doctrine;
  doctrine.withdraw = WithdrawRule::ShieldsCollapse;
  SetShipDoctrine(battle, Side::Alliance, {GroupOf(battle, ship)}, {}, doctrine, kShipWithdraw);
  battle.Step();
  SB_CHECK(!battle.State().Ship(ship)->withdrawing);
  Mutable(battle).Ship(ship)->shield = 0.0f;
  battle.Step();
  SB_CHECK(battle.State().Ship(ship)->withdrawing);
}

// Briefing: regrouping, doctrine and deployment take effect at once, before the first Step.
void BriefingChangesShowImmediately()
{
  SnapshotBuilder builder;
  const UnitIndex first = builder.Add(Wedge(Side::Alliance, "First"));
  const UnitIndex second = builder.Add(Picket(Side::Alliance, "Second"));
  builder.Add(Wedge(Side::Empire));
  Battle battle(builder.Snapshot(), Uncommanded());
  SB_CHECK(GroupOf(battle, first) != GroupOf(battle, second));
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderAssignGroup{kNoGroup}, {first, second})));
  const GroupId merged = GroupOf(battle, first);
  SB_CHECK(merged == GroupOf(battle, second));
  ViewModel view;
  battle.BuildViewModel(0.0f, view);
  SB_CHECK(std::any_of(view.groups.begin(), view.groups.end(),
                       [&](const ViewGroup& group) { return group.id == merged && group.members.size() == 2; }));
  SB_CHECK(view.deployZones[0].radius > 0.0f && view.deployZones[1].radius > 0.0f);
  const Vec2 zone = view.deployZones[0].center;
  const Vec2 before = Position(battle, second) - Position(battle, first);
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderDeploy{zone + Vec2{0.0f, 20000.0f}, 0.5f * kPi}, {}, {merged})));
  const Vec2 centroid = (Position(battle, first) + Position(battle, second)) * 0.5f;
  SB_CHECK(Distance(centroid, zone) <= tuning::kDeployZoneRadius + 1.0f);
  SB_CHECK(centroid.y > zone.y + 0.5f * tuning::kDeployZoneRadius);
  const Vec2 after = Position(battle, second) - Position(battle, first);
  SB_CHECK_NEAR(Length(after), Length(before), 50.0f); // the formation keeps its shape
  SB_CHECK_NEAR(battle.State().units[first].heading, 0.5f * kPi, 1e-3f);
  battle.Step();
  SB_CHECK(!battle.Submit(MakeCommand(Side::Alliance, OrderDeploy{zone, std::nullopt}, {}, {merged})));
  battle.BuildViewModel(0.0f, view);
  SB_CHECK(view.deployZones[0].radius == 0.0f);
}

void MovedSelectionsKeepTheirShape()
{
  SnapshotBuilder builder;
  const UnitIndex first = builder.Add(Wedge(Side::Alliance, "First"));
  const UnitIndex second = builder.Add(Picket(Side::Alliance, "Second"));
  builder.Add(Hulk(Side::Empire));
  Battle battle(builder.Snapshot(), Uncommanded());
  Place(battle, first, {-6000.0f, 0.0f}, 0.0f);
  Place(battle, second, {-6000.0f, 2000.0f}, 0.0f);
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderMove{{-6000.0f, -8000.0f}, -0.5f * kPi}, {first, second})));
  const Vec2 a = battle.State().units[first].orders.front().position;
  const Vec2 b = battle.State().units[second].orders.front().position;
  // Turned a quarter to the right: the pair, side by side along y, now lies along x.
  SB_CHECK_NEAR(a.x, -7000.0f, 1.0f);
  SB_CHECK_NEAR(b.x, -5000.0f, 1.0f);
  SB_CHECK_NEAR(a.y, -8000.0f, 1.0f);
  SB_CHECK_NEAR(b.y, -8000.0f, 1.0f);
}

void TractorOrderStopsTheTarget()
{
  SnapshotBuilder builder;
  const UnitIndex holder = builder.Add(Wedge(Side::Alliance, "Holder"));
  CapitalShipEntry slow = Broadside(Side::Empire, "Slow");
  slow.sublight = 3;
  const UnitIndex target = builder.Add(slow);
  Battle battle(builder.Snapshot(), Uncommanded());
  Place(battle, holder, {0.0f, 0.0f}, 0.0f);
  Place(battle, target, {1500.0f, 0.0f}, kPi);
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderTractor{target}, {holder})));
  battle.Step();
  battle.Step();
  SB_CHECK(battle.State().Ship(holder)->tractorTarget == target);
  SB_CHECK(battle.State().Ship(target)->tractorHeld >= 3.0f - 1e-3f); // power 3 >= sub-light 3: stopped
  SB_CHECK(MaxSpeed(battle.State(), target) == 0.0f);
  SB_CHECK(IntentOf(battle, holder).find("Holding Slow in a tractor beam") == 0
           || IntentOf(battle, holder).find("Targeting Slow") == 0);
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderTractor{kNoUnit}, {holder})));
  SB_CHECK(battle.State().Ship(holder)->tractorTarget == kNoUnit);
}

} // namespace

int main()
{
  AutoFacingBeatsBowOnForBroadsideShips();
  HooksSwingAroundTheTargetsFlank();
  TaskForcesKeepTheSlowestSpeed();
  PowerRoutingTransitionsOverFiveSeconds();
  CommandSignals();
  CoordinatedJumpSpeedsSpooling();
  HoldAndQueuedOrders();
  EscortStaysBetweenTheShipAndTheThreat();
  ReserveHoldsUntilCommitted();
  NativeNamesAppearInBattleText();
  PrioritiesPickTheTaggedTarget();
  UnitOverridesActOnTheirOwn();
  AllBatteriesOnTargetWhenItsShieldsAreDown();
  SquadronAttackOrdersEngage();
  DoctrineShieldCollapseWithdraws();
  BriefingChangesShowImmediately();
  MovedSelectionsKeepTheirShape();
  TractorOrderStopsTheTarget();
  std::puts("space battle commands passed");
  return 0;
}
