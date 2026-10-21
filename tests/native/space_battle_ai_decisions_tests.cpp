#include "space_battle_test_support.h"

#include "space_battle/battle_save.h"
#include "space_battle/doctrine.h"
#include "space_battle/tuning.h"

using namespace sbtest;

namespace
{
template <typename Order>
bool Contains(const std::vector<Command>& commands)
{
  return std::any_of(commands.begin(), commands.end(), [](const Command& command) {
    return std::holds_alternative<Order>(command.order);
  });
}

std::vector<Command> Think(Commander& commander, Battle& battle, int64_t tick)
{
  Mutable(battle).tick = tick;
  std::vector<Command> commands;
  commander.Update(battle, Side::Alliance, commands);
  for (const Command& command : commands) {
    SB_CHECK(command.side == Side::Alliance && command.tick == tick);
  }
  return commands;
}

bool Targets(const Command& command, const Battle& battle, UnitIndex unit)
{
  return std::find(command.target.units.begin(), command.target.units.end(), unit) != command.target.units.end()
         || std::find(command.target.groups.begin(), command.target.groups.end(), battle.State().units[unit].group)
                != command.target.groups.end();
}

void ReassessesOnDifficultyInterval()
{
  for (CommanderDifficulty difficulty : {CommanderDifficulty::Low, CommanderDifficulty::Normal,
                                        CommanderDifficulty::High}) {
    SnapshotBuilder builder;
    builder.Side(Side::Alliance).withdrawThreshold = 1.0f;
    const UnitIndex ship = builder.Add(Wedge(Side::Alliance));
    builder.Add(Wedge(Side::Empire));
    Battle battle(builder.Snapshot(), Uncommanded());
    auto commander = MakeBattleCommander({Temperament::Balanced, difficulty});
    Think(*commander, battle, 0);
    Mutable(battle).Ship(ship)->hull = 0.5f * battle.State().Profile(ship).hullMax;
    const int interval = (difficulty == CommanderDifficulty::Low ? 5 : difficulty == CommanderDifficulty::Normal ? 2 : 1)
                         * kTicksPerSecond;
    SB_CHECK(!Contains<OrderWithdraw>(Think(*commander, battle, interval - 1)));
    const std::vector<Command> commands = Think(*commander, battle, interval);
    SB_CHECK(Contains<OrderWithdraw>(commands) || Contains<OrderRecover>(commands));
  }
}

void FleetThresholdHonorsHumanSettingAndRetreatPermission()
{
  for (bool human : {false, true}) {
    for (bool canRetreat : {false, true}) {
      SnapshotBuilder builder;
      builder.Side(Side::Alliance).human = human;
      builder.Side(Side::Alliance).withdrawThreshold = human ? 0.0f : 1.0f;
      builder.Side(Side::Alliance).canRetreat = canRetreat;
      const UnitIndex ship = builder.Add(Wedge(Side::Alliance));
      builder.Add(Wedge(Side::Empire));
      BattleOptions options = Uncommanded();
      options.humanSideAiWithdrawThreshold = 1.0f;
      Battle battle(builder.Snapshot(), options);
      auto commander = MakeBattleCommander();
      SB_CHECK(!Contains<OrderWithdraw>(Think(*commander, battle, 0)));
      Mutable(battle).Ship(ship)->hull -= 1.0f;
      const auto commands = Think(*commander, battle, 2 * kTicksPerSecond);
      SB_CHECK(Contains<OrderWithdraw>(commands) == canRetreat);
    }
  }
}

void FightToEndThresholdDoesNotWithdrawFleet()
{
  SnapshotBuilder builder;
  builder.Side(Side::Alliance).withdrawThreshold = 0.0f;
  builder.Add(Wedge(Side::Alliance));
  for (int i = 0; i < 8; ++i) {
    builder.Add(Wedge(Side::Empire));
  }
  Battle battle(builder.Snapshot(), Uncommanded());
  auto commander = MakeBattleCommander();
  const auto commands = Think(*commander, battle, 0);
  SB_CHECK(!Contains<OrderWithdraw>(commands));
  SB_CHECK(!Contains<OrderRecover>(commands));
}

void RecoverBeforeFleetJumpButDoNotWaitForever()
{
  SnapshotBuilder builder;
  builder.Side(Side::Alliance).withdrawThreshold = 10000.0f;
  const UnitIndex carrier = builder.Add(CarrierShip(Side::Alliance));
  const UnitIndex fighters = builder.Add(TieLike(Side::Alliance, carrier));
  builder.Add(Wedge(Side::Empire));
  Battle battle(builder.Snapshot(), Uncommanded());
  sim::LaunchSquadron(Mutable(battle), fighters);
  auto commander = MakeBattleCommander();
  const auto initial = Think(*commander, battle, 0);
  SB_CHECK(Contains<OrderRecover>(initial));
  SB_CHECK(!Contains<OrderWithdraw>(initial));
  SB_CHECK(!Contains<OrderWithdraw>(Think(*commander, battle, 14 * kTicksPerSecond)));
  const auto deadline = Think(*commander, battle, 16 * kTicksPerSecond);
  SB_CHECK(Contains<OrderWithdraw>(deadline));
  SB_CHECK(std::any_of(deadline.begin(), deadline.end(), [&](const Command& command) {
    return std::holds_alternative<OrderWithdraw>(command.order) && Targets(command, battle, carrier);
  }));
}

void RecoveredFightersAllowEarlierJump()
{
  SnapshotBuilder builder;
  builder.Side(Side::Alliance).withdrawThreshold = 10000.0f;
  const UnitIndex carrier = builder.Add(CarrierShip(Side::Alliance));
  const UnitIndex fighters = builder.Add(TieLike(Side::Alliance, carrier));
  builder.Add(Wedge(Side::Empire));
  Battle battle(builder.Snapshot(), Uncommanded());
  sim::LaunchSquadron(Mutable(battle), fighters);
  auto commander = MakeBattleCommander();
  SB_CHECK(Contains<OrderRecover>(Think(*commander, battle, 0)));
  Mutable(battle).Squadron(fighters)->place = SquadronPlace::Docked;
  Mutable(battle).units[fighters].inSpace = false;
  SB_CHECK(Contains<OrderWithdraw>(Think(*commander, battle, 2 * kTicksPerSecond)));
}

void PersonalitiesPreserveShipsAtTheirOwnThreshold()
{
  for (Temperament temperament : {Temperament::Aggressive, Temperament::Balanced, Temperament::Cautious}) {
    const float threshold = temperament == Temperament::Cautious ? 0.5f
                            : temperament == Temperament::Balanced ? 0.25f : 0.15f;
    SnapshotBuilder builder;
    const UnitIndex damaged = builder.Add(Wedge(Side::Alliance));
    const UnitIndex healthy = builder.Add(Wedge(Side::Alliance));
    builder.Add(Wedge(Side::Empire));
    Battle battle(builder.Snapshot(), Uncommanded());
    auto commander = MakeBattleCommander({temperament, CommanderDifficulty::High});
    Mutable(battle).Ship(damaged)->hull = (threshold + 0.01f) * battle.State().Profile(damaged).hullMax;
    SB_CHECK(!Contains<OrderWithdraw>(Think(*commander, battle, 0)));
    Mutable(battle).Ship(damaged)->hull = (threshold - 0.01f) * battle.State().Profile(damaged).hullMax;
    const auto commands = Think(*commander, battle, kTicksPerSecond);
    SB_CHECK(std::any_of(commands.begin(), commands.end(), [&](const Command& command) {
      return std::holds_alternative<OrderWithdraw>(command.order) && Targets(command, battle, damaged)
             && !Targets(command, battle, healthy);
    }));
  }
}

void IndividualPreservationReactsBetweenFleetReassessments()
{
  for (CommanderDifficulty difficulty : {CommanderDifficulty::Low, CommanderDifficulty::Normal,
                                        CommanderDifficulty::High}) {
    SnapshotBuilder builder;
    const UnitIndex damaged = builder.Add(Wedge(Side::Alliance));
    const UnitIndex healthy = builder.Add(Wedge(Side::Alliance));
    builder.Add(Wedge(Side::Empire));
    Battle battle(builder.Snapshot(), Uncommanded());
    auto commander = MakeBattleCommander({Temperament::Balanced, difficulty});
    SB_CHECK(!Contains<OrderWithdraw>(Think(*commander, battle, 0)));
    Mutable(battle).Ship(damaged)->hull = 0.24f * battle.State().Profile(damaged).hullMax;
    const auto commands = Think(*commander, battle, 1);
    SB_CHECK(std::any_of(commands.begin(), commands.end(), [&](const Command& command) {
      return std::holds_alternative<OrderWithdraw>(command.order) && Targets(command, battle, damaged)
             && !Targets(command, battle, healthy);
    }));
  }
}

void AggressiveCommanderStillPreservesTransportsAtHalfHull()
{
  SnapshotBuilder builder;
  const UnitIndex transport = builder.Add(TransportShip(Side::Alliance));
  builder.Add(Wedge(Side::Alliance));
  builder.Add(Wedge(Side::Empire));
  Battle battle(builder.Snapshot(), Uncommanded());
  auto commander = MakeBattleCommander({Temperament::Aggressive, CommanderDifficulty::High});
  Mutable(battle).Ship(transport)->hull = 0.51f * battle.State().Profile(transport).hullMax;
  SB_CHECK(!Contains<OrderWithdraw>(Think(*commander, battle, 0)));
  Mutable(battle).Ship(transport)->hull = 0.49f * battle.State().Profile(transport).hullMax;
  const auto commands = Think(*commander, battle, kTicksPerSecond);
  SB_CHECK(std::any_of(commands.begin(), commands.end(), [&](const Command& command) {
    return std::holds_alternative<OrderWithdraw>(command.order) && Targets(command, battle, transport);
  }));
}

void HigherDifficultiesUseSignals()
{
  for (CommanderDifficulty difficulty : {CommanderDifficulty::Low, CommanderDifficulty::Normal,
                                        CommanderDifficulty::High}) {
    SnapshotBuilder builder;
    const UnitIndex carrier = builder.Add(CarrierShip(Side::Alliance));
    builder.Add(TieLike(Side::Alliance, carrier));
    builder.AddCharacter(Side::Alliance, carrier, true, false, 90, 10);
    builder.Add(Wedge(Side::Empire));
    Battle battle(builder.Snapshot(), Uncommanded());
    auto commander = MakeBattleCommander({Temperament::Balanced, difficulty});
    const auto commands = Think(*commander, battle, 0);
    SB_CHECK(Contains<OrderSignal>(commands) == (difficulty != CommanderDifficulty::Low));
  }
}

void ReassessmentChangesRolesAndDropsDeadThreats()
{
  SnapshotBuilder builder;
  const UnitIndex damaged = builder.Add(Wedge(Side::Alliance));
  for (int i = 0; i < 4; ++i) {
    builder.Add(Wedge(Side::Alliance));
  }
  const UnitIndex picket = builder.Add(Picket(Side::Alliance));
  const UnitIndex carrier = builder.Add(CarrierShip(Side::Empire));
  const UnitIndex bomber = builder.Add(BomberLike(Side::Empire, carrier));
  BattleOptions options = Uncommanded();
  options.aiControlled[0] = true;
  Battle battle(builder.Snapshot(), options);
  battle.Step();
  SB_CHECK(battle.State().EffectiveShipDoctrine(picket).role == Role::Screen);
  Mutable(battle).Ship(damaged)->hull = 0.45f * battle.State().Profile(damaged).hullMax;
  Mutable(battle).Squadron(bomber)->place = SquadronPlace::Destroyed;
  Mutable(battle).Squadron(bomber)->alive = 0;
  Mutable(battle).units[bomber].fate = UnitFate::Destroyed;
  Mutable(battle).units[bomber].inSpace = false;
  battle.RunHeadless(2 * kTicksPerSecond);
  SB_CHECK(battle.State().EffectiveShipDoctrine(damaged).role == Role::Reserve);
  SB_CHECK(battle.State().EffectiveShipDoctrine(picket).role == Role::Line);
  Mutable(battle).Ship(damaged)->hull = battle.State().Profile(damaged).hullMax;
  battle.RunHeadless(2 * kTicksPerSecond);
  SB_CHECK(battle.State().EffectiveShipDoctrine(damaged).role == Role::Line);
}

void ReassessmentStopsFocusingDisabledInterdictor()
{
  SnapshotBuilder builder;
  const UnitIndex line = builder.Add(Broadside(Side::Alliance));
  const UnitIndex interdictor = builder.Add(InterdictorShip(Side::Empire));
  for (int i = 0; i < 5; ++i) {
    builder.Add(Wedge(Side::Empire));
  }
  BattleOptions options = Uncommanded();
  options.aiControlled[0] = true;
  Battle battle(builder.Snapshot(), options);
  battle.Step();
  SB_CHECK(battle.State().EffectiveShipDoctrine(line).priorities.tags[0] == TargetTag::Interdictor);
  Mutable(battle).Ship(interdictor)->systemsOffTicks = 1000;
  battle.RunHeadless(2 * kTicksPerSecond);
  SB_CHECK(battle.State().EffectiveShipDoctrine(line).priorities.tags[0] != TargetTag::Interdictor);
}

void ObserveStartsWithCurrentDoctrinesAndCanTakeOverAgain()
{
  SnapshotBuilder builder;
  const UnitIndex line = builder.Add(Wedge(Side::Alliance));
  const UnitIndex flight = builder.Add(XwingLike(Side::Alliance, line));
  builder.Add(Wedge(Side::Empire));
  Battle battle(builder.Snapshot(), Uncommanded());
  OrderSetShipDoctrine custom;
  custom.doctrine.range = EngagementRange::StandOff;
  custom.doctrine.facing = Facing::StrongestToThreat;
  custom.doctrine.formation = Formation::Loose;
  custom.doctrine.power = PowerRouting::Engines;
  custom.fields = kShipRange | kShipFacing | kShipFormation | kShipPower;
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, custom, {}, {battle.State().units[line].group})));
  OrderSetFlightDoctrine customFlight;
  customFlight.doctrine.mission = FlightMission::HoldInHangar;
  customFlight.doctrine.launch = LaunchRule::OnOrder;
  customFlight.fields = kFlightMission | kFlightLaunch;
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, customFlight, {}, {battle.State().units[flight].group})));
  battle.RunHeadless(3);
  battle.SetAiControlled(Side::Alliance, true);
  battle.Step();
  ShipDoctrine doctrine = battle.State().EffectiveShipDoctrine(line);
  SB_CHECK(doctrine.range == EngagementRange::StandOff && doctrine.facing == Facing::StrongestToThreat);
  SB_CHECK(doctrine.formation == Formation::Loose && doctrine.power == PowerRouting::Engines);
  SB_CHECK(battle.State().EffectiveFlightDoctrine(flight).mission == FlightMission::HoldInHangar);
  SB_CHECK(battle.State().EffectiveFlightDoctrine(flight).launch == LaunchRule::OnOrder);
  battle.SetAiControlled(Side::Alliance, false);
  custom.doctrine.range = EngagementRange::Close;
  Command command = MakeCommand(Side::Alliance, custom, {}, {battle.State().units[line].group});
  command.tick = battle.Tick();
  SB_CHECK(battle.Submit(command));
  battle.Step();
  battle.SetAiControlled(Side::Alliance, true);
  battle.Step();
  doctrine = battle.State().EffectiveShipDoctrine(line);
  SB_CHECK(doctrine.range == EngagementRange::Close && doctrine.facing == Facing::StrongestToThreat);
}

void HumanDeploymentUsesMatchupsUnlessBriefingDoctrineWasChanged()
{
  for (bool edited : {false, true}) {
    SnapshotBuilder builder;
    builder.Side(Side::Alliance).human = true;
    const UnitIndex ship = builder.Add(Wedge(Side::Alliance));
    builder.Add(Wedge(Side::Empire));
    BattleOptions options = Uncommanded();
    options.commanders[0] = {Temperament::Cautious, CommanderDifficulty::High};
    Battle battle(builder.Snapshot(), options);
    if (edited) {
      OrderSetShipDoctrine custom;
      custom.doctrine.range = EngagementRange::Close;
      custom.doctrine.withdraw = WithdrawRule::HullBelow75;
      custom.fields = kShipRange | kShipWithdraw;
      SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, custom, {}, {battle.State().units[ship].group})));
    }
    battle.SetAiControlled(Side::Alliance, true);
    battle.Step();
    const ShipDoctrine doctrine = battle.State().EffectiveShipDoctrine(ship);
    SB_CHECK(doctrine.range == (edited ? EngagementRange::Close : EngagementRange::StandOff));
    SB_CHECK(doctrine.withdraw == (edited ? WithdrawRule::HullBelow75 : WithdrawRule::OnOrderOnly));
  }
}

void ObservedDepartingCarrierRecoversDependentFighters()
{
  SnapshotBuilder builder;
  const UnitIndex carrier = builder.Add(CarrierShip(Side::Alliance));
  const UnitIndex flight = builder.Add(TieLike(Side::Alliance, carrier));
  builder.Add(Wedge(Side::Empire));
  Battle battle(builder.Snapshot(), Uncommanded());
  sim::LaunchSquadron(Mutable(battle), flight);
  battle.Step();
  Command withdraw = MakeCommand(Side::Alliance, OrderWithdraw{}, {carrier});
  withdraw.tick = battle.Tick();
  SB_CHECK(battle.Submit(withdraw));
  battle.Step();
  SB_CHECK(battle.State().Ship(carrier)->withdrawing);
  battle.SetAiControlled(Side::Alliance, true);
  battle.Step();
  SB_CHECK(std::any_of(battle.CommandLog().begin(), battle.CommandLog().end(), [&](const Command& command) {
    return std::holds_alternative<OrderRecover>(command.order) && Targets(command, battle, flight);
  }));
}

void AiAutoAllocationSpillsPastDoomedTargets()
{
  SnapshotBuilder builder;
  std::vector<UnitIndex> shooters;
  for (int i = 0; i < 4; ++i) {
    shooters.push_back(builder.Add(Wedge(Side::Alliance)));
  }
  const UnitIndex weak = builder.Add(Wedge(Side::Empire, "Almost dead"));
  const UnitIndex other = builder.Add(Wedge(Side::Empire, "Healthy alternative"));
  BattleOptions options = Uncommanded();
  options.aiControlled[0] = true;
  Battle battle(builder.Snapshot(), options);
  battle.Step();
  BattleState& state = Mutable(battle);
  state.Ship(weak)->hull = 1.0f;
  state.Ship(weak)->shield = 0.0f;
  state.Ship(other)->shield = 0.0f;
  Place(battle, weak, {0.0f, 0.0f}, kPi);
  Place(battle, other, {0.0f, 1000.0f}, kPi);
  for (UnitIndex shooter : shooters) {
    Place(battle, shooter, {-2500.0f, 0.0f}, 0.0f);
    state.Ship(shooter)->primaryTarget = kNoUnit;
  }
  GroupState& group = *state.Group(state.units[shooters.front()].group);
  SB_CHECK(group.shipDoctrine.allocation == FireAllocation::Auto);
  sim::ScoreGroupTargets(state, group);
  sim::AllocateFire(state, group);
  const int weakAssignments = static_cast<int>(std::count_if(shooters.begin(), shooters.end(), [&](UnitIndex shooter) {
    return state.Ship(shooter)->primaryTarget == weak;
  }));
  SB_CHECK(weakAssignments == 1);
  SB_CHECK(std::any_of(shooters.begin(), shooters.end(), [&](UnitIndex shooter) {
    return state.Ship(shooter)->primaryTarget == other;
  }));
}

void CommanderPrivateStateRestoresDeterministically()
{
  for (bool fleetWithdrawal : {false, true}) {
    SnapshotBuilder builder;
    builder.Side(Side::Alliance).withdrawThreshold = fleetWithdrawal ? 10000.0f : 0.0f;
    const UnitIndex carrier = builder.Add(CarrierShip(Side::Alliance));
    const UnitIndex squadron = builder.Add(TieLike(Side::Alliance, carrier));
    builder.Add(Wedge(Side::Empire));
    Battle battle(builder.Snapshot(), Uncommanded());
    sim::LaunchSquadron(Mutable(battle), squadron);
    if (!fleetWithdrawal) {
      Mutable(battle).Ship(carrier)->hull = 0.4f * battle.State().Profile(carrier).hullMax;
    }
    auto original = MakeBattleCommander({Temperament::Cautious, CommanderDifficulty::High});
    Think(*original, battle, 0);
    Think(*original, battle, 7 * kTicksPerSecond);
    std::vector<uint8_t> saved;
    original->Save(saved);
    SB_CHECK(!saved.empty());
    auto restored = MakeBattleCommander();
    SB_CHECK(restored->Restore(saved));
    std::vector<uint8_t> resaved;
    restored->Save(resaved);
    SB_CHECK(resaved == saved);
    for (int seconds : {8, 13, 16, 20}) {
      const auto expected = Think(*original, battle, seconds * kTicksPerSecond);
      const auto actual = Think(*restored, battle, seconds * kTicksPerSecond);
      SB_CHECK(EncodeCommands(expected) == EncodeCommands(actual));
    }
    SB_CHECK(restored->Restore({}));
    SB_CHECK(!restored->Restore(std::span(saved).first(saved.size() - 1)));
    saved.front() ^= 0xff;
    SB_CHECK(!restored->Restore(saved));
  }
}

void TractorPursuitClosesOnStableNearestTarget()
{
  SnapshotBuilder builder;
  const UnitIndex hunter = builder.Add(Wedge(Side::Alliance));
  builder.Add(Wedge(Side::Alliance));
  const UnitIndex first = builder.Add(Wedge(Side::Empire));
  const UnitIndex second = builder.Add(Wedge(Side::Empire));
  Battle battle(builder.Snapshot(), Uncommanded());
  Mutable(battle).Ship(first)->hull *= 0.5f;
  Mutable(battle).Ship(second)->hull *= 0.5f;
  sim::BeginWithdrawal(Mutable(battle), first);
  sim::BeginWithdrawal(Mutable(battle), second);
  Place(battle, hunter, {0.0f, 0.0f}, 0.0f);
  Place(battle, first, {8000.0f, 0.0f}, kPi);
  Place(battle, second, {-8000.0f, 0.0f}, 0.0f);
  auto commander = MakeBattleCommander();
  const auto commands = Think(*commander, battle, 0);
  const auto tractor = std::find_if(commands.begin(), commands.end(), [&](const Command& command) {
    return std::holds_alternative<OrderTractor>(command.order) && Targets(command, battle, hunter);
  });
  SB_CHECK(tractor != commands.end());
  SB_CHECK(std::get<OrderTractor>(tractor->order).target == first);
  for (const Command& command : commands) {
    SB_CHECK(battle.Submit(command));
  }
  SB_CHECK(!Contains<OrderTractor>(Think(*commander, battle, 2 * kTicksPerSecond)));
  battle.Step();
  SB_CHECK(battle.State().Ship(hunter)->goal.x > 0.0f);
  SB_CHECK(battle.State().InSpace(first) && battle.State().InSpace(second));
}

void WinnersPursueWithdrawingShipsWithoutAbandoningStations()
{
  for (bool station : {false, true}) {
    SnapshotBuilder builder;
    builder.Side(Side::Alliance).withdrawThreshold = 0.0f;
    builder.Add(Wedge(Side::Alliance));
    builder.Add(Wedge(Side::Alliance));
    const UnitIndex runner = builder.Add(Wedge(Side::Empire));
    if (station) {
      // A station defender keeps its screen rather than chasing the departing fleet.
      builder.AddDeathStar();
    }
    BattleSnapshot snapshot = builder.Snapshot();
    if (station) {
      std::get<DeathStarEntry>(snapshot.units.back()).identity.side = Side::Alliance;
    }
    Battle battle(snapshot, Uncommanded());
    auto commander = MakeBattleCommander();
    for (const Command& command : Think(*commander, battle, 0)) {
      SB_CHECK(battle.Submit(command));
    }
    Mutable(battle).Ship(runner)->withdrawing = true;
    const auto commands = Think(*commander, battle, 2 * kTicksPerSecond);
    const bool pursuit = std::any_of(commands.begin(), commands.end(), [](const Command& command) {
      const auto* order = std::get_if<OrderSetShipDoctrine>(&command.order);
      return order != nullptr && order->doctrine.priorities.tags[0] == TargetTag::Withdrawing
             && order->doctrine.role == Role::Line && order->doctrine.range == EngagementRange::Close
             && order->doctrine.approach == Approach::Direct;
    });
    SB_CHECK(pursuit == !station);
  }
}

// A stalemate leaves the system with the side that held it, so a stronger attacker whose fight has stalled commits its
// stand-off line and its interdictor picket to the attack.
void StrongerAttackerPressesAfterALull()
{
  SnapshotBuilder builder;
  builder.Params().heldFirstBy = Side::Empire; // the Alliance attacks
  const UnitIndex wedge = builder.Add(Wedge(Side::Alliance));
  const UnitIndex interdictor = builder.Add(InterdictorShip(Side::Alliance));
  builder.Add(TransportShip(Side::Empire));
  Battle battle(builder.Snapshot(), Uncommanded());
  auto commander = MakeBattleCommander({Temperament::Cautious, CommanderDifficulty::Normal});
  const auto think = [&](int64_t tick) {
    for (const Command& command : Think(*commander, battle, tick)) {
      battle.Submit(command);
    }
  };
  think(0);
  SB_CHECK(battle.State().EffectiveShipDoctrine(wedge).range == EngagementRange::StandOff);
  SB_CHECK(battle.State().EffectiveShipDoctrine(interdictor).role == Role::Picket);
  think(static_cast<int64_t>(tuning::kPressAttackSeconds) * kTicksPerSecond - 2 * kTicksPerSecond);
  SB_CHECK(battle.State().EffectiveShipDoctrine(wedge).range == EngagementRange::StandOff);
  think(static_cast<int64_t>(tuning::kPressAttackSeconds) * kTicksPerSecond);
  SB_CHECK(battle.State().EffectiveShipDoctrine(wedge).range == EngagementRange::Optimal);
  SB_CHECK(battle.State().EffectiveShipDoctrine(interdictor).role == Role::Line);
}

void FocusSignalsAvoidOverkill()
{
  SnapshotBuilder builder;
  const UnitIndex flagship = builder.Add(Wedge(Side::Alliance));
  builder.AddCharacter(Side::Alliance, flagship, true, false, 90, 10);
  const UnitIndex weak = builder.Add(Wedge(Side::Empire));
  CapitalShipEntry durable = Wedge(Side::Empire, "Durable alternative");
  durable.hull = durable.hullMax = 100000;
  const UnitIndex other = builder.Add(durable);
  Battle battle(builder.Snapshot(), Uncommanded());
  Place(battle, flagship, {-2500.0f, 0.0f}, 0.0f);
  Place(battle, weak, {0.0f, 0.0f}, kPi);
  Place(battle, other, {0.0f, 1000.0f}, kPi);
  Mutable(battle).Ship(weak)->hull = 1.0f;
  Mutable(battle).Ship(weak)->shield = 0.0f;
  auto commander = MakeBattleCommander({Temperament::Balanced, CommanderDifficulty::High});
  const auto commands = Think(*commander, battle, 0);
  const auto signal = std::find_if(commands.begin(), commands.end(), [](const Command& command) {
    const auto* order = std::get_if<OrderSignal>(&command.order);
    return order != nullptr && order->signal == SignalKind::ConcentrateFire;
  });
  SB_CHECK(signal != commands.end());
  SB_CHECK(std::get<OrderSignal>(signal->order).target == other);
}
} // namespace

int main()
{
  StrongerAttackerPressesAfterALull();
  ReassessesOnDifficultyInterval();
  FleetThresholdHonorsHumanSettingAndRetreatPermission();
  FightToEndThresholdDoesNotWithdrawFleet();
  RecoverBeforeFleetJumpButDoNotWaitForever();
  RecoveredFightersAllowEarlierJump();
  PersonalitiesPreserveShipsAtTheirOwnThreshold();
  IndividualPreservationReactsBetweenFleetReassessments();
  AggressiveCommanderStillPreservesTransportsAtHalfHull();
  HigherDifficultiesUseSignals();
  ReassessmentChangesRolesAndDropsDeadThreats();
  ReassessmentStopsFocusingDisabledInterdictor();
  ObserveStartsWithCurrentDoctrinesAndCanTakeOverAgain();
  HumanDeploymentUsesMatchupsUnlessBriefingDoctrineWasChanged();
  ObservedDepartingCarrierRecoversDependentFighters();
  AiAutoAllocationSpillsPastDoomedTargets();
  CommanderPrivateStateRestoresDeterministically();
  TractorPursuitClosesOnStableNearestTarget();
  WinnersPursueWithdrawingShipsWithoutAbandoningStations();
  FocusSignalsAvoidOverkill();
  std::puts("space battle AI decisions passed");
  return 0;
}
