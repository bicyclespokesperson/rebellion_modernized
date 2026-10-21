#include "space_battle_test_support.h"

#include "space_battle/tuning.h"

using namespace sbtest;
namespace tuning = reb::battle::tuning;

namespace
{
CapitalShipEntry Runner(Side side, int sublight, const std::string& name = "Runner")
{
  CapitalShipEntry ship = Broadside(side, name);
  ship.sublight = sublight;
  ship.weapons = {};
  ship.weaponRecharge = 0;
  return ship;
}

CapitalShipEntry Tug(Side side, int power, const std::string& name = "Tug")
{
  CapitalShipEntry ship = Broadside(side, name);
  ship.tractorPower = power;
  ship.tractorRange = 20;
  ship.weapons = {};
  ship.weaponRecharge = 0;
  return ship;
}

void SpoolTimeUsesRetreatDistanceAndDriveRating()
{
  for (float retreatDays : {0.0f, 25.0f, 1000.0f}) {
    SnapshotBuilder builder;
    builder.Side(Side::Alliance).retreatDays = retreatDays;
    CapitalShipEntry fast = Runner(Side::Alliance, 4);
    fast.hyperdrive = 50;
    CapitalShipEntry slow = fast;
    slow.hyperdrive = 100;
    const UnitIndex first = builder.Add(fast);
    const UnitIndex second = builder.Add(slow);
    builder.Add(Wedge(Side::Empire));
    Battle battle(builder.Snapshot(), Uncommanded());
    SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderWithdraw{}, {first, second})));
    const float seconds = std::min(60.0f, 30.0f + 0.4f * retreatDays);
    SB_CHECK(battle.State().Ship(first)->spoolTotal == sim::Ticks(seconds * 0.5f));
    SB_CHECK(battle.State().Ship(second)->spoolTotal == sim::Ticks(seconds));
    SB_CHECK(battle.State().Ship(first)->spoolTotal >= 15 * kTicksPerSecond);
    const int fastTicks = battle.State().Ship(first)->spoolTotal;
    for (int tick = 0; tick < fastTicks - 1; ++tick) {
      sim::UpdateWithdrawal(Mutable(battle));
    }
    SB_CHECK(battle.State().InSpace(first));
    sim::UpdateWithdrawal(Mutable(battle));
    SB_CHECK(battle.State().units[first].fate == UnitFate::Withdrawn);
    SB_CHECK(battle.State().InSpace(second));
    for (int tick = 0; tick < fastTicks; ++tick) {
      sim::UpdateWithdrawal(Mutable(battle));
    }
    SB_CHECK(battle.State().units[second].fate == UnitFate::Withdrawn);
  }
}

void TractorStopRule()
{
  SnapshotBuilder builder;
  const UnitIndex target = builder.Add(Runner(Side::Alliance, 4));
  const UnitIndex first = builder.Add(Tug(Side::Empire, 3, "First"));
  const UnitIndex second = builder.Add(Tug(Side::Empire, 2, "Second"));
  Battle battle(builder.Snapshot(), Uncommanded());
  Place(battle, target, {0.0f, 0.0f}, 0.0f);
  Place(battle, first, {2000.0f, 0.0f}, kPi);
  Place(battle, second, {-2000.0f, 0.0f}, 0.0f);
  const float free = MaxSpeed(battle.State(), target);
  SB_CHECK(battle.Submit(MakeCommand(Side::Empire, OrderTractor{target}, {first})));
  battle.Step();
  SB_CHECK(battle.State().Ship(first)->tractorTarget == target);
  // One lock of power 3 against sub-light 4: three quarters of the speed is gone, and the ship can still jump.
  SB_CHECK_NEAR(MaxSpeed(battle.State(), target), 0.25f * free, 0.5f);
  SB_CHECK(CanJump(battle.State(), target));
  SB_CHECK(battle.State().Tags(target) & TagBit(TargetTag::Tractored));
  SB_CHECK(battle.Submit(MakeCommand(Side::Empire, OrderTractor{target}, {second})));
  battle.Step();
  SB_CHECK(battle.State().Ship(target)->tractorHeld >= 4.0f);
  SB_CHECK(MaxSpeed(battle.State(), target) == 0.0f);
  JumpBlocker blocker = JumpBlocker::None;
  SB_CHECK(!CanJump(battle.State(), target, &blocker) && blocker == JumpBlocker::Tractored);
  SB_CHECK(TurnRate(battle.State(), target) < 0.2f * TurnRate(battle.State(), first));
  // Ion disruption of a tractor ship breaks its lock.
  Mutable(battle).Ship(second)->shield = 0.0f;
  sim::DamageShip(Mutable(battle), second, 1000.0f, ShotKind::Ion, Subsystem::Hull);
  battle.Step();
  SB_CHECK(battle.State().Ship(second)->tractorTarget == kNoUnit);
  SB_CHECK(MaxSpeed(battle.State(), target) > 0.0f);
  // A lock breaks once the target is out of range.
  Place(battle, target, {0.0f, 9000.0f}, 0.0f);
  battle.Step();
  SB_CHECK(battle.State().Ship(first)->tractorTarget == kNoUnit);
  SB_CHECK(battle.State().Ship(target)->tractorHeld == 0.0f);
}

// Without an order, tractor ships hold an enemy that is spooling its hyperdrive.
void TractorsCatchWithdrawingShips()
{
  SnapshotBuilder builder;
  const UnitIndex target = builder.Add(Runner(Side::Alliance, 3));
  const UnitIndex tug = builder.Add(Tug(Side::Empire, 3));
  Battle battle(builder.Snapshot(), Uncommanded());
  Place(battle, target, {0.0f, 0.0f}, 0.0f);
  Place(battle, tug, {1500.0f, 0.0f}, kPi);
  battle.Step();
  SB_CHECK(battle.State().Ship(tug)->tractorTarget == kNoUnit);
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderWithdraw{}, {target})));
  battle.RunHeadless(static_cast<int64_t>(tuning::kSpoolMaxSeconds * 2.0f) * kTicksPerSecond);
  SB_CHECK(battle.State().Ship(tug)->tractorTarget == target);
  SB_CHECK(battle.State().units[target].fate == UnitFate::Active); // spooled long ago, but held
  SB_CHECK(battle.State().units[target].intent.kind == IntentKind::Trapped);
}

// A gravity well blocks jumps inside its circle, not outside (§6.10); ion disruption opens it for a while.
void GravityWellBlocksJumpsInside()
{
  SnapshotBuilder builder;
  const UnitIndex inside = builder.Add(Runner(Side::Alliance, 0, "Inside"));
  const UnitIndex outside = builder.Add(Runner(Side::Alliance, 0, "Outside"));
  CapitalShipEntry projector = InterdictorShip(Side::Empire);
  projector.sublight = 0;
  projector.weapons = {};
  const UnitIndex interdictor = builder.Add(projector);
  // Sub-light 0 means no engines: give them a crawl so only the well decides.
  BattleSnapshot snapshot = builder.Snapshot();
  std::get<CapitalShipEntry>(snapshot.units[inside]).sublight = 1;
  std::get<CapitalShipEntry>(snapshot.units[outside]).sublight = 1;
  Battle battle(snapshot, Uncommanded());
  Place(battle, interdictor, {0.0f, 0.0f}, 0.0f);
  Place(battle, inside, {tuning::kGravityWellRadius - 3000.0f, 0.0f}, 0.0f);
  Place(battle, outside, {-(tuning::kGravityWellRadius + 3000.0f), 0.0f}, kPi);
  SB_CHECK(InEnemyWell(battle.State(), inside) && !InEnemyWell(battle.State(), outside));
  JumpBlocker blocker = JumpBlocker::None;
  SB_CHECK(!CanJump(battle.State(), inside, &blocker) && blocker == JumpBlocker::GravityWell);
  SB_CHECK(CanJump(battle.State(), outside));
  ViewModel view;
  battle.BuildViewModel(0.0f, view);
  SB_CHECK(view.wells.size() == 1 && view.wells[0].active && view.wells[0].radius == tuning::kGravityWellRadius);
  // Disrupting the projector with ion fire opens the well.
  Mutable(battle).Ship(interdictor)->shield = 0.0f;
  sim::DamageShip(Mutable(battle), interdictor, 2000.0f, ShotKind::Ion, Subsystem::Hull);
  SB_CHECK(!WellActive(battle.State(), interdictor));
  SB_CHECK(CanJump(battle.State(), inside));
  for (int tick = 0; tick <= sim::Ticks(tuning::kIonSystemsSeconds); ++tick) {
    sim::UpdateShipSystems(Mutable(battle));
  }
  SB_CHECK(WellActive(battle.State(), interdictor));
}

// When a side withdraws, ships that cannot jump yet keep spooling and fighting until they can (§6.10).
void TrappedShipsKeepSpoolingAndFighting()
{
  SnapshotBuilder builder;
  const UnitIndex trapped = builder.Add(Broadside(Side::Alliance, "Trapped"));
  const UnitIndex free = builder.Add(Broadside(Side::Alliance, "Free"));
  CapitalShipEntry projector = InterdictorShip(Side::Empire);
  projector.sublight = 0;
  const UnitIndex interdictor = builder.Add(projector);
  const UnitIndex wedge = builder.Add(Wedge(Side::Empire));
  Battle battle(builder.Snapshot(), Uncommanded());
  Place(battle, interdictor, {0.0f, 0.0f}, 0.0f);
  Place(battle, wedge, {1500.0f, -2500.0f}, 0.5f * kPi);
  Place(battle, trapped, {2500.0f, 1500.0f}, 0.0f);
  Place(battle, free, {-(tuning::kGravityWellRadius + 6000.0f), 0.0f}, kPi);
  Command withdraw = MakeCommand(Side::Alliance, OrderWithdraw{});
  for (const GroupState& group : battle.State().groups) {
    if (group.side == Side::Alliance) {
      withdraw.target.groups.push_back(group.id);
    }
  }
  SB_CHECK(battle.Submit(withdraw));
  const int32_t spool = battle.State().Ship(free)->spoolTotal;
  bool fired = false;
  for (int tick = 0; tick < spool + 2 * kTicksPerSecond && !battle.Finished(); ++tick) {
    battle.Step();
    for (const PendingShot& shot : battle.State().shots) {
      fired = fired || (shot.shooter == trapped && battle.State().Ship(trapped)->withdrawing);
    }
  }
  SB_CHECK(battle.State().units[free].fate == UnitFate::Withdrawn);
  SB_CHECK(battle.State().units[trapped].fate == UnitFate::Active);
  SB_CHECK(battle.State().Ship(trapped)->withdrawing);
  SB_CHECK(battle.State().Ship(trapped)->spoolTicks == battle.State().Ship(trapped)->spoolTotal);
  SB_CHECK(battle.State().units[trapped].intent.kind == IntentKind::Trapped);
  SB_CHECK(battle.State().units[trapped].intent.blocker == JumpBlocker::GravityWell);
  // It runs for the edge of the well and fights on the way: it gets out, or it dies.
  for (int tick = 0; tick < 240 * kTicksPerSecond && !battle.Finished(); ++tick) {
    battle.Step();
    for (const PendingShot& shot : battle.State().shots) {
      fired = fired || (shot.shooter == trapped && battle.State().Ship(trapped)->withdrawing);
    }
  }
  const UnitFate fate = battle.State().units[trapped].fate;
  std::printf("trapped ship: %s after %.0f s\n", fate == UnitFate::Withdrawn ? "escaped" : "destroyed",
              battle.State().BattleSeconds());
  SB_CHECK(fate == UnitFate::Withdrawn || fate == UnitFate::Destroyed);
  SB_CHECK(fired);
  const BattleResultSet results = battle.Results();
  SB_CHECK(results.ships[1].fate == UnitFate::Withdrawn && results.ships[1].hull > 0);
}

struct DeathStarBattle {
  SnapshotBuilder builder;
  UnitIndex station = kNoUnit;
  std::vector<UnitIndex> rebels;
  std::unique_ptr<Battle> battle;
};

// The superlaser starts charged, recharges in five minutes of battle time and fires only at capital ships (§6.12).
void SuperlaserFiresOnSchedule()
{
  SnapshotBuilder builder;
  const UnitIndex station = builder.AddDeathStar();
  std::vector<UnitIndex> rebels;
  for (int i = 0; i < 3; ++i) {
    CapitalShipEntry ship = Broadside(Side::Alliance, "Rebel " + std::to_string(i + 1));
    ship.weapons = {};
    ship.weaponRecharge = 0;
    rebels.push_back(builder.Add(ship));
  }
  const UnitIndex fighters = builder.Add(XwingLike(Side::Alliance, kNoUnit));
  BattleOptions options;
  options.aiControlled = {false, true};
  Battle battle(builder.Snapshot(), options);
  SB_CHECK(SuperlaserReady(battle.State(), station));
  SB_CHECK(!battle.Submit(MakeCommand(Side::Empire, OrderFireSuperlaser{fighters})));
  ViewModel view;
  battle.BuildViewModel(0.0f, view);
  SB_CHECK(view.superlaserCharge == 1.0f);
  std::vector<int64_t> kills;
  while (battle.Tick() < 700 * kTicksPerSecond && kills.size() < 3) {
    battle.Step();
    int destroyed = 0;
    for (UnitIndex rebel : rebels) {
      destroyed += battle.State().units[rebel].fate == UnitFate::Destroyed ? 1 : 0;
    }
    if (destroyed > static_cast<int>(kills.size())) {
      kills.push_back(battle.Tick());
    }
  }
  std::printf("superlaser kills at %.1f %.1f %.1f s\n", kills.size() > 0 ? kills[0] / 20.0 : -1.0,
              kills.size() > 1 ? kills[1] / 20.0 : -1.0, kills.size() > 2 ? kills[2] / 20.0 : -1.0);
  SB_CHECK(kills.size() >= 2);
  SB_CHECK(kills[0] <= 3 * kTicksPerSecond); // charged at the start, about a second's delay
  const int64_t recharge = sim::Ticks(tuning::kSuperlaserRechargeSeconds);
  SB_CHECK(kills[1] - kills[0] >= recharge && kills[1] - kills[0] <= recharge + 2 * kTicksPerSecond);
  // The station itself cannot be targeted.
  SB_CHECK(ExpectedDps(battle.State(), fighters, station, DpsGeometry::BestArc) == 0.0f);
  SB_CHECK(!battle.Submit(MakeCommand(Side::Alliance, OrderAttack{{station}}, {fighters})));
}

// The run needs a complete station, no shield facility and more Alliance fighters in play (§6.12).
void TrenchRunConditions()
{
  for (int variant = 0; variant < 3; ++variant) {
    SnapshotBuilder builder;
    builder.Params().heldFirstBy = Side::Empire;
    builder.AddDeathStar(variant != 0, variant == 1);
    builder.Add(Broadside(Side::Alliance));
    builder.Add(XwingLike(Side::Alliance, kNoUnit));
    if (variant == 2) {
      builder.Add(TieLike(Side::Empire, kNoUnit));
      builder.Add(TieLike(Side::Empire, kNoUnit));
    }
    Battle battle(builder.Snapshot(), Uncommanded());
    BattleState& state = Mutable(battle);
    for (SquadronState& squadron : state.squadrons) {
      if (squadron.place == SquadronPlace::Docked) {
        sim::LaunchSquadron(state, squadron.unit);
      }
    }
    SB_CHECK(!TrenchRunPossible(battle.State(), 0));
    ViewModel view;
    battle.BuildViewModel(0.0f, view);
    SB_CHECK(!view.deathStarRunOpen);
  }
}

void TrenchRunDestroysTheStation()
{
  SnapshotBuilder builder(77);
  builder.Params().heldFirstBy = Side::Empire;
  const UnitIndex station = builder.AddDeathStar();
  const UnitIndex docked = builder.Add(TieLike(Side::Empire, station, "Docked Ties"));
  builder.Add(Broadside(Side::Alliance));
  // Agile craft with a huge torpedo load: every one survives, and the shot cannot miss.
  SquadronEntry aces = XwingLike(Side::Alliance, kNoUnit, "Aces");
  aces.maneuverability = 8;
  aces.torpedoesPerCraft = 200;
  const UnitIndex flight = builder.Add(aces);
  Battle battle(builder.Snapshot(), Uncommanded());
  Mutable(battle).Squadron(docked)->stayDocked = true;
  SB_CHECK(TrenchRunPossible(battle.State(), station));
  ViewModel view;
  battle.BuildViewModel(0.0f, view);
  SB_CHECK(view.deathStarRunOpen);
  const GroupId group = battle.State().units[flight].group;
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderDeathStarRun{}, {}, {group})));
  // One flight at a time.
  SB_CHECK(!battle.Submit(MakeCommand(Side::Alliance, OrderDeathStarRun{}, {}, {group})));
  bool entered = false;
  while (battle.Tick() < 200 * kTicksPerSecond && battle.State().units[station].fate == UnitFate::Active) {
    battle.Step();
    entered = entered || battle.State().Squadron(flight)->runTicks > 0;
  }
  SB_CHECK(entered);
  SB_CHECK(battle.State().units[station].fate == UnitFate::Destroyed);
  SB_CHECK(battle.State().Squadron(flight)->alive == 12); // agility 9 of 9: everyone survives
  SB_CHECK(battle.State().units[docked].fate == UnitFate::Destroyed);
  const BattleResultSet results = battle.Results();
  SB_CHECK(results.deathStars.size() == 1 && !results.deathStars[0].alive && results.deathStars[0].hull == 0);
}

// Clumsy craft lose most of their number in the trench, and a light torpedo load usually misses.
void TrenchRunCostsClumsyCraft()
{
  SnapshotBuilder builder(5);
  builder.Params().heldFirstBy = Side::Empire;
  const UnitIndex station = builder.AddDeathStar();
  builder.Add(Broadside(Side::Alliance));
  SquadronEntry heavy = BomberLike(Side::Alliance, kNoUnit, "Heavy");
  heavy.maneuverability = 1;
  heavy.torpedoesPerCraft = 0;
  const UnitIndex flight = builder.Add(heavy);
  Battle battle(builder.Snapshot(), Uncommanded());
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderDeathStarRun{}, {}, {battle.State().units[flight].group})));
  int64_t enteredAt = -1;
  while (battle.Tick() < 200 * kTicksPerSecond) {
    battle.Step();
    if (enteredAt < 0 && battle.State().Squadron(flight)->runTicks > 0) {
      enteredAt = battle.Tick();
    }
    if (enteredAt >= 0 && battle.State().Squadron(flight)->runTicks == 0) {
      break;
    }
  }
  SB_CHECK(enteredAt >= 0);
  const int survivors = battle.State().Squadron(flight)->alive;
  std::printf("clumsy flight: %d of 12 craft came out of the trench; station %s\n", survivors,
              battle.State().units[station].fate == UnitFate::Active ? "intact" : "destroyed");
  SB_CHECK(survivors <= 7);
  SB_CHECK(battle.State().stations[0].runFlight == kNoGroup); // the run is over either way
}

void CheckResultsEqual(const BattleResultSet& expected, const BattleResultSet& actual)
{
  SB_CHECK(actual.outcome.alliancePresent == expected.outcome.alliancePresent);
  SB_CHECK(actual.outcome.empirePresent == expected.outcome.empirePresent);
  SB_CHECK(actual.outcome.allianceWithdrew == expected.outcome.allianceWithdrew);
  SB_CHECK(actual.outcome.empireWithdrew == expected.outcome.empireWithdrew);
  SB_CHECK(actual.ships.size() == expected.ships.size());
  SB_CHECK(actual.squadrons.size() == expected.squadrons.size());
  SB_CHECK(actual.deathStars.size() == expected.deathStars.size());
  for (size_t i = 0; i < expected.ships.size(); ++i) {
    SB_CHECK(actual.ships[i].unit == expected.ships[i].unit);
    SB_CHECK(actual.ships[i].fate == expected.ships[i].fate);
    SB_CHECK(actual.ships[i].hull == expected.ships[i].hull);
    SB_CHECK(actual.ships[i].criticals == expected.ships[i].criticals);
  }
  for (size_t i = 0; i < expected.squadrons.size(); ++i) {
    SB_CHECK(actual.squadrons[i].unit == expected.squadrons[i].unit);
    SB_CHECK(actual.squadrons[i].fate == expected.squadrons[i].fate);
    SB_CHECK(actual.squadrons[i].size == expected.squadrons[i].size);
    SB_CHECK(actual.squadrons[i].carrier == expected.squadrons[i].carrier);
  }
  for (size_t i = 0; i < expected.deathStars.size(); ++i) {
    SB_CHECK(actual.deathStars[i].unit == expected.deathStars[i].unit);
    SB_CHECK(actual.deathStars[i].alive == expected.deathStars[i].alive);
    SB_CHECK(actual.deathStars[i].withdrew == expected.deathStars[i].withdrew);
    SB_CHECK(actual.deathStars[i].hull == expected.deathStars[i].hull);
  }
}

void OutroPreservesEveryEnding()
{
  for (int ending = 0; ending < 4; ++ending) {
    SnapshotBuilder builder;
    const UnitIndex survivor = builder.Add(Broadside(Side::Alliance));
    const UnitIndex enemy = builder.Add(Wedge(Side::Empire));
    BattleOptions options = Uncommanded();
    if (ending == 3) {
      options.tickLimit = 1;
    }
    Battle battle(builder.Snapshot(), options);
    battle.SetOutroSeconds(5.0f);
    BattleState& state = Mutable(battle);
    if (ending == 0) {
      state.Ship(enemy)->shield = 0.0f;
      sim::DamageShip(state, enemy, 100000.0f, ShotKind::Turbolaser, Subsystem::Hull);
    } else if (ending == 1) {
      SB_CHECK(battle.Submit(MakeCommand(Side::Empire, OrderWithdraw{}, {enemy})));
      state.Ship(enemy)->spoolTicks = state.Ship(enemy)->spoolTotal;
      sim::UpdateWithdrawal(state);
      SB_CHECK(state.units[enemy].fate == UnitFate::Withdrawn);
    } else if (ending == 2) {
      state.tick = sim::Ticks(tuning::kStalemateSeconds);
    }
    battle.Step();
    const EndReason expectedReason = ending < 2 ? EndReason::Elimination
                                               : ending == 2 ? EndReason::Stalemate : EndReason::TimeLimit;
    SB_CHECK(state.endReason == expectedReason && !battle.Finished());
    ViewModel view;
    battle.BuildViewModel(0.0f, view);
    SB_CHECK(view.decided && !view.finished);
    SB_CHECK(static_cast<int>(view.endReason) == static_cast<int>(expectedReason));
    const BattleResultSet decisive = battle.Results();
    const int64_t decisiveTick = battle.Tick();
    const size_t decisiveEvents = state.events.size();
    ShipState& ship = *state.Ship(survivor);
    ship.withdrawing = true;
    ship.spoolTotal = ship.spoolTicks = 1;
    ship.hull -= 0.25f;
    const float hull = ship.hull;
    for (int tick = 0; tick < 5 * kTicksPerSecond; ++tick) {
      SB_CHECK(!battle.Finished());
      battle.Step();
      CheckResultsEqual(decisive, battle.Results());
      SB_CHECK(ship.spoolTicks == 1 && state.units[survivor].fate == UnitFate::Active);
      SB_CHECK(ship.hull == hull);
      SB_CHECK(state.endReason == expectedReason && state.events.size() == decisiveEvents);
    }
    SB_CHECK(battle.Finished() && battle.Tick() == decisiveTick + 5 * kTicksPerSecond);
    battle.BuildViewModel(0.0f, view);
    SB_CHECK(view.decided && view.finished);
    const uint64_t hash = battle.StateHash();
    battle.Step();
    SB_CHECK(battle.StateHash() == hash);
  }
}

void OutroFreezesCombatAndHangarsButKeepsVisualsMoving()
{
  SnapshotBuilder builder;
  const UnitIndex carrier = builder.Add(Wedge(Side::Alliance));
  const UnitIndex docked = builder.Add(XwingLike(Side::Alliance, carrier, "Docked"));
  const UnitIndex returning = builder.Add(XwingLike(Side::Alliance, carrier, "Returning"));
  const UnitIndex attacking = builder.Add(XwingLike(Side::Alliance, carrier, "Attacking"));
  const UnitIndex enemy = builder.Add(Wedge(Side::Empire));
  BattleOptions options = Uncommanded();
  options.tickLimit = 1;
  Battle battle(builder.Snapshot(), options);
  battle.SetOutroSeconds(5.0f);
  sim::LaunchSquadron(Mutable(battle), returning);
  sim::LaunchSquadron(Mutable(battle), attacking);
  battle.Step();
  SB_CHECK(!battle.Finished() && battle.State().endReason == EndReason::TimeLimit);
  BattleState& state = Mutable(battle);
  Place(battle, carrier, {0.0f, 0.0f}, 0.0f);
  Place(battle, enemy, {1000.0f, 0.0f}, kPi);
  ShipState& ship = *state.Ship(carrier);
  ship.goal = {10000.0f, 0.0f};
  ship.desiredSpeed = ship.speed = 100.0f;
  ship.shield = 0.0f;
  ship.criticals.speed = 1;
  ship.damageControlTicks = 1;
  ship.primaryTarget = enemy;
  ship.batteryCooldown.fill(0);
  SquadronState& waiting = *state.Squadron(docked);
  waiting.stayDocked = false;
  waiting.launchOrdered = true;
  waiting.rearmTicks = 1;
  SquadronState& recovering = *state.Squadron(returning);
  recovering.task = SquadronTask::Return;
  recovering.centroid = state.units[carrier].position;
  for (int i = 0; i < recovering.craftCount; ++i) {
    state.craft[recovering.firstCraft + i].position = state.units[carrier].position;
  }
  SquadronState& flight = *state.Squadron(attacking);
  flight.task = SquadronTask::Strike;
  flight.target = enemy;
  flight.withdrawing = true;
  flight.spoolTotal = flight.spoolTicks = 1;
  for (int i = 0; i < flight.craftCount; ++i) {
    CraftState& craft = state.craft[flight.firstCraft + i];
    craft.position = {500.0f, static_cast<float>(i)};
    craft.heading = 0.0f;
    craft.phase = CraftPhase::Approach;
    craft.gunTicks = craft.torpedoTicks = 0;
  }
  SB_CHECK(!battle.Submit(MakeCommand(Side::Alliance, OrderLaunch{}, {docked})));
  SB_CHECK(!battle.Submit(MakeCommand(Side::Alliance, OrderWithdraw{}, {carrier})));
  PendingShot shot;
  shot.fireTick = battle.Tick() - 1;
  shot.impactTick = battle.Tick() + 3;
  shot.shooter = enemy;
  shot.target = carrier;
  shot.hit = true;
  shot.damage = 100000.0f;
  shot.from = state.units[enemy].position;
  shot.to = state.units[carrier].position;
  state.shots = {shot};
  shot.target = attacking;
  shot.targetCraft = flight.firstCraft;
  state.shots.push_back(shot);
  sim::AddEffect(state, ViewEffectKind::Explosion, {0.0f, 0.0f}, 100.0f, enemy);
  state.effects.back().tick = battle.Tick() - 1;
  const BattleResultSet decisive = battle.Results();
  const Vec2 shipPosition = state.units[carrier].position;
  const Vec2 craftPosition = state.craft[flight.firstCraft].position;
  const int torpedoes = state.craft[flight.firstCraft].torpedoes;
  const float hull = ship.hull;
  const float craftHull = state.craft[flight.firstCraft].hull;
  ViewModel before;
  battle.BuildViewModel(0.0f, before);
  SB_CHECK(before.shots.size() == 2 && !before.effects.empty());
  battle.Step();
  ViewModel after;
  battle.BuildViewModel(0.0f, after);
  SB_CHECK(after.shots.size() == 2 && after.shots[0].progress > before.shots[0].progress);
  SB_CHECK(after.effects.back().age > before.effects.back().age);
  SB_CHECK(state.units[carrier].position != shipPosition);
  SB_CHECK(state.craft[flight.firstCraft].position != craftPosition);
  for (int tick = 0; tick < 10; ++tick) {
    battle.Step();
    CheckResultsEqual(decisive, battle.Results());
    SB_CHECK(ship.hull == hull && ship.shield == 0.0f && ship.criticals.speed == 1);
    SB_CHECK(state.craft[flight.firstCraft].hull == craftHull);
    SB_CHECK(state.craft[flight.firstCraft].torpedoes == torpedoes);
    SB_CHECK(waiting.place == SquadronPlace::Docked && waiting.rearmTicks == 1);
    SB_CHECK(recovering.place == SquadronPlace::InSpace && recovering.carrier == carrier);
    SB_CHECK(flight.spoolTicks == 1 && flight.place == SquadronPlace::InSpace);
  }
  SB_CHECK(state.shots.empty());
}

void OutroFreezesSuperlaserAndTrenchRun()
{
  SnapshotBuilder builder;
  const UnitIndex ship = builder.Add(Broadside(Side::Alliance));
  const UnitIndex flight = builder.Add(XwingLike(Side::Alliance, kNoUnit));
  const UnitIndex station = builder.AddDeathStar();
  BattleOptions options = Uncommanded();
  options.tickLimit = 1;
  Battle battle(builder.Snapshot(), options);
  battle.SetOutroSeconds(5.0f);
  battle.Step();
  BattleState& state = Mutable(battle);
  StationState& deathStar = state.stations[0];
  deathStar.firingAt = ship;
  deathStar.fireDelayTicks = 1;
  deathStar.withdrawing = true;
  deathStar.spoolTotal = deathStar.spoolTicks = 1;
  SquadronState& squadron = *state.Squadron(flight);
  squadron.task = SquadronTask::TrenchRun;
  squadron.target = station;
  squadron.runTicks = 1;
  deathStar.runFlight = state.units[flight].group;
  const BattleResultSet decisive = battle.Results();
  for (int tick = 0; tick < 5 * kTicksPerSecond; ++tick) {
    battle.Step();
    CheckResultsEqual(decisive, battle.Results());
    SB_CHECK(deathStar.fireDelayTicks == 1 && deathStar.spoolTicks == 1);
    SB_CHECK(squadron.runTicks == 1);
  }
  SB_CHECK(battle.Finished());
}

void OutroSkipRestoreAndDefault()
{
  SnapshotBuilder builder;
  builder.Add(Broadside(Side::Alliance));
  builder.Add(Wedge(Side::Empire));
  BattleOptions options = Uncommanded();
  options.tickLimit = 1;
  Battle headless(builder.Snapshot(), options);
  const std::vector<uint8_t> initial = headless.SaveState();
  Battle live(builder.Snapshot(), options);
  live.SetOutroSeconds(5.0f);
  SB_CHECK(live.SaveState() == initial && live.StateHash() == headless.StateHash());
  live.SkipOutro();
  SB_CHECK(!live.Finished());
  headless.Step();
  live.Step();
  SB_CHECK(headless.Finished() && !live.Finished());
  const BattleResultSet decisive = live.Results();
  std::unique_ptr<Battle> restored = Battle::RestoreState(builder.Snapshot(), options, live.SaveState());
  SB_CHECK(restored != nullptr && !restored->Finished());
  SB_CHECK(restored->StateHash() == live.StateHash());
  restored->SetOutroSeconds(5.0f);
  restored->Step();
  SB_CHECK(restored->Finished());
  CheckResultsEqual(decisive, restored->Results());
  const int64_t decisiveTick = live.Tick();
  live.SkipOutro();
  SB_CHECK(live.Finished() && live.Tick() == decisiveTick);
  CheckResultsEqual(decisive, live.Results());
  live.SkipOutro();
  SB_CHECK(live.Finished() && live.Tick() == decisiveTick);
}

} // namespace

int main()
{
  SpoolTimeUsesRetreatDistanceAndDriveRating();
  TractorStopRule();
  TractorsCatchWithdrawingShips();
  GravityWellBlocksJumpsInside();
  TrappedShipsKeepSpoolingAndFighting();
  SuperlaserFiresOnSchedule();
  TrenchRunConditions();
  TrenchRunDestroysTheStation();
  TrenchRunCostsClumsyCraft();
  OutroPreservesEveryEnding();
  OutroFreezesCombatAndHangarsButKeepsVisualsMoving();
  OutroFreezesSuperlaserAndTrenchRun();
  OutroSkipRestoreAndDefault();
  std::puts("space battle special systems passed");
  return 0;
}
