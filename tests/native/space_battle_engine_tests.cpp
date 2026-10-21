#include "app/battle_engine_space.h"
#include "space_battle_test_support.h"

StrategicBattleContext CurrentStrategicBattleContext()
{
  return {};
}

namespace
{
using namespace reb::battle;

int StrategicWinner(const BattleOutcome& outcome, int heldFirst)
{
  const auto winner = [heldFirst](int alliance, int empire) {
    return alliance && empire ? heldFirst : alliance ? 1 : empire ? 2 : 0;
  };
  const int present = winner(outcome.m_sideAAlive, outcome.m_sideBAlive);
  return present ? present : winner(outcome.m_sideAFinal, outcome.m_sideBFinal);
}

void CommanderMapping()
{
  BattleSnapshot snapshot;
  for (int column = 0; column < 8; ++column) {
    const auto options = BattleOptionsFor(snapshot, {{3, 5}, column});
    SB_CHECK(options.commanders[0].temperament == Temperament::Aggressive);
    SB_CHECK(options.commanders[1].temperament == Temperament::Cautious);
    SB_CHECK(options.commanders[0].difficulty == CommanderDifficulty::Normal);
    SB_CHECK(options.aiControlled[0] && options.aiControlled[1]);
  }
  const auto balanced = BattleOptionsFor(snapshot, {{4, 99}, 7});
  SB_CHECK(balanced.commanders[0].temperament == Temperament::Balanced);
  SB_CHECK(balanced.commanders[1].temperament == Temperament::Balanced);
  snapshot.params.sides[0].human = true;
  SB_CHECK(BattleOptionsFor(snapshot, {{3, 5}, 7}).commanders[0].temperament == Temperament::Balanced);
}

void OutcomeMapping()
{
  struct Case {
    BattleOutcomeSummary summary;
    int allianceHeldWinner;
    int empireHeldWinner;
  };
  const Case cases[] = {
      {{true, false, false, false}, 1, 1},  // Empire eliminated: Alliance keeps orbit.
      {{false, true, false, false}, 2, 2},  // Alliance eliminated: Empire keeps orbit (including a surviving Death Star).
      {{false, true, true, false}, 2, 2},   // Alliance escaped: strategic rules route its surviving ships away.
      {{true, false, false, true}, 1, 1},   // Empire escaped, including a withdrawing Death Star.
      {{true, true, false, false}, 1, 2},   // Stalemate/time limit: held-first wins; other fleet is routed, not killed.
      {{false, false, false, false}, 0, 0}, // Mutual destruction, including destruction of the last Death Star.
      {{false, false, true, false}, 1, 1},  // Only Alliance escapees survive; final flags preserve them.
      {{false, false, false, true}, 2, 2},  // Only Empire escapees survive, possibly aboard the Death Star.
      {{false, false, true, true}, 1, 2},   // Both fleets escaped: held-first breaks the tie.
      {{true, true, true, true}, 1, 2},     // Presence takes priority over escapees.
  };
  for (const auto& test : cases) {
    const BattleOutcome outcome = SpaceBattleOutcome(test.summary);
    SB_CHECK(outcome.m_sideAAlive == test.summary.alliancePresent);
    SB_CHECK(outcome.m_sideBAlive == test.summary.empirePresent);
    SB_CHECK(outcome.m_sideAFinal == test.summary.allianceWithdrew);
    SB_CHECK(outcome.m_sideBFinal == test.summary.empireWithdrew);
    SB_CHECK(outcome.m_unfinished == 0);
    SB_CHECK(outcome.m_unk10 == 0 && outcome.m_unk18 == 0);
    SB_CHECK(StrategicWinner(outcome, 1) == test.allianceHeldWinner);
    SB_CHECK(StrategicWinner(outcome, 2) == test.empireHeldWinner);
  }
}

void UnitRecords()
{
  sbtest::SnapshotBuilder builder;
  auto ship = sbtest::Wedge(Side::Alliance);
  const UnitIndex shipIndex = builder.Add(ship);
  auto squadron = sbtest::XwingLike(Side::Alliance, shipIndex);
  const UnitIndex squadronIndex = builder.Add(squadron);
  const UnitIndex stationIndex = builder.AddDeathStar();
  const BattleSnapshot& snapshot = builder.Snapshot();
  BattleResultSet results;
  CriticalHits hits{2, 3, 4, 5, 1, 1};
  results.ships.push_back({shipIndex, UnitFate::Withdrawn, 123, hits});
  results.squadrons.push_back({squadronIndex, UnitFate::Docked, 7, shipIndex});
  results.deathStars.push_back({stationIndex, true, false, 10000});

  const auto record = SpaceCapitalShipResult(snapshot, results, shipIndex + 1);
  SB_CHECK(record.m_unk00 == shipIndex + 1);
  SB_CHECK(record.m_unk08 == static_cast<int>(ship.identity.family));
  SB_CHECK(record.m_unk0c == static_cast<int>(ship.identity.id));
  SB_CHECK(record.m_hull == 123 && record.m_unk28 == 9);
  SB_CHECK(record.m_speedCH == 2 && record.m_shieldRechargeCH == 3);
  SB_CHECK(record.m_weaponRechargeCH == 4 && record.m_tractorBeamCH == 5);
  SB_CHECK(record.m_hyperdriveDamage == 2); // ApplyCapitalShip assigns one to primary, then one to backup.
  results.ships.front().criticals.hyperdriveBackup = 0;
  SB_CHECK(SpaceCapitalShipResult(snapshot, results, 1).m_hyperdriveDamage == 1);
  results.ships.front().criticals.hyperdrivePrimary = 0;
  SB_CHECK(SpaceCapitalShipResult(snapshot, results, 1).m_hyperdriveDamage == 0);

  auto fighter = SpaceFightSquadResult(snapshot, results, squadronIndex + 1);
  SB_CHECK(fighter.m_squadSize == 7 && fighter.m_unk1c == 1);
  SB_CHECK(fighter.m_unk08 == static_cast<int>(squadron.identity.family));
  SB_CHECK(fighter.m_unk0c == static_cast<int>(squadron.identity.id));
  SB_CHECK(fighter.m_unk14 == 0); // Its carrier jumped before this squadron recovered.
  results.squadrons.front().fate = UnitFate::Withdrawn;
  fighter = SpaceFightSquadResult(snapshot, results, squadronIndex + 1);
  SB_CHECK(fighter.m_unk14 == 1 && fighter.m_unk10 == shipIndex + 1);
  results.ships.front().fate = UnitFate::Destroyed;
  results.ships.front().hull = 0;
  fighter = SpaceFightSquadResult(snapshot, results, squadronIndex + 1);
  SB_CHECK(fighter.m_unk14 == 0 && fighter.m_unk10 == 0); // Orphan goes to the system before carrier destruction.
  SB_CHECK(SpaceCapitalShipResult(snapshot, results, 1).m_unk28 == 0xb);
  results.squadrons.front().carrier = kNoUnit;
  SB_CHECK(SpaceFightSquadResult(snapshot, results, squadronIndex + 1).m_unk14 == 0);

  results.squadrons.front().carrier = stationIndex;
  results.squadrons.front().fate = UnitFate::Docked;
  fighter = SpaceFightSquadResult(snapshot, results, squadronIndex + 1);
  SB_CHECK(fighter.m_unk14 == 1 && fighter.m_unk10 == stationIndex + 1);
  const auto carrier = SpaceCapitalShipResult(snapshot, results, fighter.m_unk10);
  const auto& station = std::get<DeathStarEntry>(snapshot.units[stationIndex]);
  SB_CHECK(carrier.m_unk08 == static_cast<int>(station.identity.family));
  SB_CHECK(carrier.m_unk0c == static_cast<int>(station.identity.id));
  SB_CHECK(carrier.m_hull == 10000 && carrier.m_unk28 == 2);
  results.deathStars.front().withdrew = true;
  SB_CHECK(SpaceDeathStarResult(results, stationIndex + 1).m_unk10 == 1);
  SB_CHECK(SpaceCapitalShipResult(snapshot, results, stationIndex + 1).m_unk28 == 9);
  results.deathStars.front() = {stationIndex, false, false, 0};
  SB_CHECK(SpaceFightSquadResult(snapshot, results, squadronIndex + 1).m_unk14 == 0);
  SB_CHECK(SpaceDeathStarResult(results, stationIndex + 1).m_alive == 0);
  SB_CHECK(SpaceDeathStarResult(results, stationIndex + 1).m_hull == 0);
}

void EngineLifecycle()
{
  sbtest::SnapshotBuilder builder;
  builder.Add(sbtest::Wedge(Side::Alliance));
  builder.Add(sbtest::TransportShip(Side::Empire));
  builder.Side(Side::Empire).withdrawThreshold = 10000.0f;
  auto engine = CreateSpaceBattleEngine();
  SB_CHECK((engine->Load(builder.Snapshot()) == std::vector<int>{1, 2}));
  engine->Start();
  int frames = 0;
  do {
    ++frames;
    SB_CHECK(frames <= 900);
  } while (!engine->Advance());
  SB_CHECK(engine->Steps() > 0);
  SB_CHECK(engine->Steps() <= static_cast<int64_t>(frames) * kTicksPerSecond);
  SB_CHECK(engine->Outcome().m_sideAAlive == 1);
  SB_CHECK(engine->Outcome().m_sideBFinal == 1); // Snapshot retreat route and threshold reached the AI.
  SB_CHECK(engine->CapitalShipResultFor(2).m_unk28 == 9);
  engine->Finish();
  engine->Load(builder.Snapshot());
  SB_CHECK(engine->Steps() == 0);
  engine->Finish();
}

void StalemateKeepsBothFleets()
{
  sbtest::SnapshotBuilder builder;
  builder.Add(sbtest::TransportShip(Side::Alliance));
  builder.Add(sbtest::TransportShip(Side::Empire));
  auto engine = CreateSpaceBattleEngine();
  engine->Load(builder.Snapshot());
  engine->Start();
  while (!engine->Advance()) {
    SB_CHECK(engine->Steps() < 18000);
  }
  SB_CHECK(engine->EndedInStalemate());
  SB_CHECK(engine->Outcome().m_sideAAlive && engine->Outcome().m_sideBAlive);
  SB_CHECK(engine->CapitalShipResultFor(1).m_hull == 400);
  SB_CHECK(engine->CapitalShipResultFor(2).m_hull == 400);
  engine->Finish();
}

void DeathStarCarrierSurvivesHeadless()
{
  sbtest::SnapshotBuilder builder;
  builder.Add(sbtest::TransportShip(Side::Alliance));
  const UnitIndex station = builder.AddDeathStar();
  const UnitIndex squadron = builder.Add(sbtest::TieLike(Side::Empire, station));
  auto engine = CreateSpaceBattleEngine();
  engine->Load(builder.Snapshot());
  engine->Start();
  while (!engine->Advance()) {
    SB_CHECK(engine->Steps() < 18000);
  }
  SB_CHECK(engine->Outcome().m_sideAAlive == 0 && engine->Outcome().m_sideBAlive == 1);
  SB_CHECK(engine->CapitalShipResultFor(1).m_hull == 0);
  SB_CHECK(engine->DeathStarResultFor(station + 1).m_alive == 1);
  const auto fighter = engine->FightSquadResultFor(squadron + 1);
  SB_CHECK(fighter.m_squadSize == 12 && fighter.m_unk10 == station + 1 && fighter.m_unk14 == 1);
  SB_CHECK(engine->CapitalShipResultFor(fighter.m_unk10).m_unk08 == 0x18);
  engine->Finish();
}

} // namespace

int main()
{
  CommanderMapping();
  OutcomeMapping();
  UnitRecords();
  EngineLifecycle();
  StalemateKeepsBothFleets();
  DeathStarCarrierSurvivesHeadless();
}
