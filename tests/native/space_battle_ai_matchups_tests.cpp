#include "space_battle_test_support.h"

#include "space_battle/doctrine.h"

using namespace sbtest;

namespace
{
BattleOptions AllianceAi(CommanderParams params = {})
{
  BattleOptions options = Uncommanded();
  options.aiControlled[0] = true;
  options.commanders[0] = params;
  return options;
}

void BombersStrikeCarriersAndMultiroleEscort()
{
  SnapshotBuilder builder;
  builder.Add(Broadside(Side::Alliance));
  const UnitIndex home = builder.Add(CarrierShip(Side::Alliance));
  const UnitIndex bomber = builder.Add(BomberLike(Side::Alliance, home));
  const UnitIndex escort = builder.Add(XwingLike(Side::Alliance, home));
  builder.Add(Wedge(Side::Empire));
  const UnitIndex enemyCarrier = builder.Add(CarrierShip(Side::Empire));
  builder.Add(TieLike(Side::Empire, enemyCarrier));
  Battle battle(builder.Snapshot(), AllianceAi());
  battle.Step();
  const FlightDoctrine strike = battle.State().EffectiveFlightDoctrine(bomber);
  SB_CHECK(strike.mission == FlightMission::Strike);
  SB_CHECK(strike.priorities.tags[0] == TargetTag::Carrier);
  const FlightDoctrine screen = battle.State().EffectiveFlightDoctrine(escort);
  SB_CHECK(screen.mission == FlightMission::Escort);
  SB_CHECK(screen.escortTarget == bomber);
}

void EnemyBombersGetLaserScreens()
{
  SnapshotBuilder builder;
  const UnitIndex line = builder.Add(Broadside(Side::Alliance));
  const UnitIndex picket = builder.Add(Picket(Side::Alliance));
  CapitalShipEntry frigate = Picket(Side::Alliance, "Frigate");
  SetArc(frigate, Arc::Fore, 600, 0, 140);
  frigate.weapons.range[0] = 35;
  frigate.fighterCapacity = 2;
  const UnitIndex escort = builder.Add(frigate);
  const UnitIndex carrier = builder.Add(CarrierShip(Side::Empire));
  builder.Add(BomberLike(Side::Empire, carrier));
  Battle battle(builder.Snapshot(), AllianceAi());
  battle.Step();
  SB_CHECK(battle.State().Profile(picket).archetype == Archetype::Picket);
  SB_CHECK(battle.State().Profile(escort).archetype == Archetype::EscortFrigate);
  for (UnitIndex ship : {picket, escort}) {
    const ShipDoctrine doctrine = battle.State().EffectiveShipDoctrine(ship);
    SB_CHECK(doctrine.role == Role::Screen);
    SB_CHECK(doctrine.screenTarget == line);
    SB_CHECK(doctrine.priorities.tags[0] == TargetTag::Bomber);
    SB_CHECK(doctrine.formation == Formation::Ring);
  }
}

void DefendersPrioritizeInvasionTransports()
{
  for (bool defending : {false, true}) {
    SnapshotBuilder builder;
    builder.Params().heldFirstBy = defending ? Side::Alliance : Side::Empire;
    const UnitIndex line = builder.Add(Broadside(Side::Alliance));
    const UnitIndex bomber = builder.Add(BomberLike(Side::Alliance, kNoUnit));
    builder.Add(Wedge(Side::Empire));
    builder.Add(TransportShip(Side::Empire));
    Battle battle(builder.Snapshot(), AllianceAi());
    battle.Step();
    const ShipDoctrine ship = battle.State().EffectiveShipDoctrine(line);
    const FlightDoctrine flight = battle.State().EffectiveFlightDoctrine(bomber);
    SB_CHECK((ship.priorities.tags[0] == TargetTag::Transport) == defending);
    SB_CHECK((flight.priorities.tags[0] == TargetTag::Transport) == defending);
  }
}

void OutmatchedFleetPrioritizesActiveInterdictor()
{
  for (bool active : {false, true}) {
    SnapshotBuilder builder;
    const UnitIndex line = builder.Add(Broadside(Side::Alliance));
    const UnitIndex interdictor = builder.Add(InterdictorShip(Side::Empire));
    for (int i = 0; i < 5; ++i) {
      builder.Add(Wedge(Side::Empire));
    }
    Battle battle(builder.Snapshot(), AllianceAi());
    if (!active) {
      Mutable(battle).Ship(interdictor)->systemsOffTicks = 100;
    }
    SB_CHECK(SideStrength(battle.State(), Side::Alliance) < SideStrength(battle.State(), Side::Empire));
    battle.Step();
    const ShipDoctrine doctrine = battle.State().EffectiveShipDoctrine(line);
    SB_CHECK((doctrine.priorities.tags[0] == TargetTag::Interdictor) == active);
  }
}

void DeathStarDefendersScreenStation()
{
  SnapshotBuilder builder;
  const UnitIndex station = builder.AddDeathStar();
  const UnitIndex ship = builder.Add(Wedge(Side::Empire));
  const UnitIndex fighter = builder.Add(TieLike(Side::Empire, station));
  const UnitIndex attacker = builder.Add(Broadside(Side::Alliance));
  builder.Add(BomberLike(Side::Alliance, attacker));
  BattleOptions options = Uncommanded();
  options.aiControlled[1] = true;
  Battle battle(builder.Snapshot(), options);
  battle.Step();
  const ShipDoctrine doctrine = battle.State().EffectiveShipDoctrine(ship);
  SB_CHECK(doctrine.role == Role::Screen);
  SB_CHECK(doctrine.screenTarget == station);
  SB_CHECK(battle.State().EffectiveFlightDoctrine(fighter).mission == FlightMission::Intercept);
}

void DeathStarAttackersSeekSuperiorityAndSendBestTorpedoFlight()
{
  SnapshotBuilder builder;
  const UnitIndex carrier = builder.Add(CarrierShip(Side::Alliance));
  const UnitIndex multirole = builder.Add(XwingLike(Side::Alliance, carrier));
  const UnitIndex bomber = builder.Add(BomberLike(Side::Alliance, carrier));
  const UnitIndex station = builder.AddDeathStar();
  Battle battle(builder.Snapshot(), AllianceAi());
  sim::LaunchSquadron(Mutable(battle), multirole);
  sim::LaunchSquadron(Mutable(battle), bomber);
  sim::UpdateSquadronBounds(Mutable(battle));
  SB_CHECK(TrenchRunPossible(battle.State(), station));
  battle.Step();
  SB_CHECK(battle.State().EffectiveFlightDoctrine(multirole).mission == FlightMission::Intercept);
  const auto run = std::find_if(battle.CommandLog().begin(), battle.CommandLog().end(), [](const Command& command) {
    return std::holds_alternative<OrderDeathStarRun>(command.order);
  });
  SB_CHECK(run != battle.CommandLog().end());
  SB_CHECK(run->target.groups == std::vector<GroupId>{battle.State().units[bomber].group});
}

void TemperamentAndDifficultyChooseDoctrineWithoutBonuses()
{
  SnapshotBuilder builder;
  const UnitIndex line = builder.Add(Wedge(Side::Alliance));
  const UnitIndex broadside = builder.Add(Broadside(Side::Alliance));
  builder.Add(Wedge(Side::Empire));
  builder.Add(Broadside(Side::Empire));
  const BattleSnapshot snapshot = builder.Snapshot();
  for (Temperament temperament : {Temperament::Aggressive, Temperament::Balanced, Temperament::Cautious}) {
    for (CommanderDifficulty difficulty : {CommanderDifficulty::Low, CommanderDifficulty::Normal,
                                           CommanderDifficulty::High}) {
      Battle battle(snapshot, AllianceAi({temperament, difficulty}));
      const float hull = battle.State().Profile(line).hullMax;
      const float firepower = battle.State().Profile(line).firepower;
      battle.Step();
      const ShipDoctrine doctrine = battle.State().EffectiveShipDoctrine(line);
      SB_CHECK(doctrine.allocation == (difficulty == CommanderDifficulty::Low ? FireAllocation::Spread
                                                                            : FireAllocation::Auto));
      SB_CHECK(doctrine.withdraw == WithdrawRule::OnOrderOnly);
      SB_CHECK(doctrine.facing == Facing::BowOn);
      SB_CHECK(battle.State().EffectiveShipDoctrine(broadside).facing == Facing::Broadside);
      if (temperament == Temperament::Aggressive) {
        SB_CHECK(doctrine.range == EngagementRange::Close);
        SB_CHECK(doctrine.power == PowerRouting::Weapons);
      } else if (temperament == Temperament::Cautious) {
        SB_CHECK(doctrine.range == EngagementRange::StandOff);
      } else {
        SB_CHECK(doctrine.range == EngagementRange::Optimal);
      }
      SB_CHECK(battle.State().Profile(line).hullMax == hull);
      SB_CHECK(battle.State().Profile(line).firepower == firepower);
    }
  }
}
} // namespace

int main()
{
  BombersStrikeCarriersAndMultiroleEscort();
  EnemyBombersGetLaserScreens();
  DefendersPrioritizeInvasionTransports();
  OutmatchedFleetPrioritizesActiveInterdictor();
  DeathStarDefendersScreenStation();
  DeathStarAttackersSeekSuperiorityAndSendBestTorpedoFlight();
  TemperamentAndDifficultyChooseDoctrineWithoutBonuses();
  std::puts("space battle AI matchups passed");
  return 0;
}
