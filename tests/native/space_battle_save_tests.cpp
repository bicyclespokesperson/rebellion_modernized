#include "space_battle_test_support.h"

#include <cstring>

#include "space_battle/battle_save.h"

using namespace sbtest;

namespace
{
// Everything at once: line ships, pickets, carriers and fighters, an interdictor, tractors, an admiral and a commander.
BattleSnapshot BusyBattle(uint32_t seed)
{
  SnapshotBuilder builder(seed);
  const UnitIndex flagship = builder.Add(Broadside(Side::Alliance, "Home One"));
  builder.Add(Broadside(Side::Alliance, "Defiance"));
  builder.Add(Picket(Side::Alliance, "Screen"));
  const UnitIndex rebelCarrier = builder.Add(CarrierShip(Side::Alliance, "Rebel Carrier"));
  builder.Add(XwingLike(Side::Alliance, rebelCarrier, "Red"));
  builder.Add(BomberLike(Side::Alliance, rebelCarrier, "Gold"));
  builder.Add(XwingLike(Side::Alliance, kNoUnit, "Rogue"));
  builder.AddCharacter(Side::Alliance, flagship, true, false, 70, 30);
  builder.AddCharacter(Side::Alliance, flagship, false, true, 30, 80);
  const UnitIndex wedge = builder.Add(Wedge(Side::Empire, "Wedge"));
  builder.Add(Wedge(Side::Empire, "Second Wedge"));
  builder.Add(InterdictorShip(Side::Empire));
  const UnitIndex empireCarrier = builder.Add(CarrierShip(Side::Empire, "Imperial Carrier"));
  builder.Add(TieLike(Side::Empire, wedge, "Black"));
  builder.Add(TieLike(Side::Empire, empireCarrier, "Obsidian"));
  builder.Add(BomberLike(Side::Empire, empireCarrier, "Scimitar"));
  builder.AddCharacter(Side::Empire, wedge, true, false, 50, 40);
  builder.Add(Picket(Side::Empire, "Lancer")); // the AI assigns it a ship to screen
  return builder.Snapshot();
}

// Save and restore mid-battle, then continue both: the restored battle must stay identical to the uninterrupted one.
void RestoreContinuesExactly()
{
  const BattleSnapshot snapshot = BusyBattle(41);
  const BattleOptions options;
  Battle reference(snapshot, options);
  reference.RunHeadless(75 * kTicksPerSecond);
  SB_CHECK(!reference.Finished());
  SB_CHECK(!reference.State().shots.empty());
  const std::vector<uint8_t> saved = reference.SaveState();
  std::unique_ptr<Battle> restored = Battle::RestoreState(snapshot, options, saved);
  SB_CHECK(restored != nullptr);
  SB_CHECK(restored->StateHash() == reference.StateHash());
  SB_CHECK(restored->Tick() == reference.Tick());
  SB_CHECK(restored->SaveState() == saved);
  for (int checkpoint = 0; checkpoint < 6; ++checkpoint) {
    reference.RunHeadless(30 * kTicksPerSecond);
    restored->RunHeadless(30 * kTicksPerSecond);
    SB_CHECK(restored->StateHash() == reference.StateHash());
  }
  reference.RunHeadless(reference.Options().tickLimit);
  restored->RunHeadless(reference.Options().tickLimit);
  SB_CHECK(reference.Finished() && restored->Finished());
  SB_CHECK(restored->StateHash() == reference.StateHash());
  const BattleResultSet a = reference.Results();
  const BattleResultSet b = restored->Results();
  SB_CHECK(a.ships.size() == b.ships.size() && a.squadrons.size() == b.squadrons.size());
  for (size_t i = 0; i < a.ships.size(); ++i) {
    SB_CHECK(a.ships[i].fate == b.ships[i].fate && a.ships[i].hull == b.ships[i].hull);
  }
  std::printf("restored battle matched to the end (%.0f s, %zu save bytes at 75 s)\n", reference.State().BattleSeconds(),
              saved.size());
}

// The view model after a restore matches too (positions, shots in flight, intents).
void RestoredViewModelMatches()
{
  const BattleSnapshot snapshot = BusyBattle(9);
  Battle reference(snapshot, BattleOptions{});
  reference.RunHeadless(60 * kTicksPerSecond);
  std::unique_ptr<Battle> restored = Battle::RestoreState(snapshot, BattleOptions{}, reference.SaveState());
  ViewModel a;
  ViewModel b;
  reference.BuildViewModel(0.5f, a);
  restored->BuildViewModel(0.5f, b);
  SB_CHECK(a.units.size() == b.units.size() && a.shots.size() == b.shots.size() && a.craft.size() == b.craft.size());
  for (size_t i = 0; i < a.units.size(); ++i) {
    SB_CHECK(a.units[i].position == b.units[i].position && a.units[i].intent == b.units[i].intent);
  }
  SB_CHECK(a.recentEvents.size() == b.recentEvents.size());
}

// A replay: the snapshot plus every logged command re-simulates the battle, with nobody thinking (§11.4).
void CommandLogReplaysTheBattle()
{
  BattleSnapshot snapshot = BusyBattle(23);
  snapshot.params.sides[1].withdrawThreshold = 0.8f; // the AI withdraws once it falls behind
  BattleOptions live;
  live.aiControlled = {false, true}; // the Empire under the AI, the Alliance under "the player"
  Battle battle(snapshot, live);
  // The player's briefing and a few live orders.
  OrderSetShipDoctrine doctrine;
  doctrine.doctrine.range = EngagementRange::Close;
  doctrine.fields = kShipRange;
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, doctrine, {}, {battle.State().units[0].group})));
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderSignal{SignalKind::Scramble, kNoUnit})));
  battle.RunHeadless(40 * kTicksPerSecond);
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderSetPower{PowerRouting::Weapons}, {}, {battle.State().units[0].group})));
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderAttack{{9}}, {0, 1})));
  battle.RunHeadless(40 * kTicksPerSecond);
  SB_CHECK(battle.Submit(MakeCommand(Side::Alliance, OrderSignal{SignalKind::ConcentrateFire, 10})));
  battle.RunHeadless(battle.Options().tickLimit);
  SB_CHECK(battle.Finished());

  const auto aiCommands = std::count_if(battle.CommandLog().begin(), battle.CommandLog().end(),
                                       [](const Command& command) { return command.side == Side::Empire; });
  SB_CHECK(aiCommands >= 2);
  const std::vector<uint8_t> log = EncodeCommands(battle.CommandLog());
  const std::optional<std::vector<Command>> decoded = DecodeCommands(log);
  SB_CHECK(decoded.has_value() && decoded->size() == battle.CommandLog().size());
  std::printf("replay: %zu commands (%zu bytes) for %.0f s of battle\n", decoded->size(), log.size(),
              battle.State().BattleSeconds());

  BattleOptions replayOptions;
  replayOptions.aiControlled = {false, false};
  Battle replay(snapshot, replayOptions);
  for (const Command& command : *decoded) {
    SB_CHECK(replay.Submit(command));
  }
  replay.RunHeadless(battle.Options().tickLimit);
  SB_CHECK(replay.Finished());
  SB_CHECK(replay.Tick() == battle.Tick());
  std::vector<uint8_t> replayState;
  std::vector<uint8_t> battleState;
  WriteState(replay.State(), replayState);
  WriteState(battle.State(), battleState);
  SB_CHECK(replayState == battleState);
}

void ForeignOrDamagedStatesAreRejected()
{
  const BattleSnapshot snapshot = BusyBattle(3);
  Battle battle(snapshot, BattleOptions{});
  battle.RunHeadless(20 * kTicksPerSecond);
  std::vector<uint8_t> saved = battle.SaveState();
  SB_CHECK(Battle::RestoreState(BusyBattle(4), BattleOptions{}, saved) == nullptr);
  for (size_t length : {size_t{0}, size_t{4}, saved.size() / 2, saved.size() - 1}) {
    SB_CHECK(Battle::RestoreState(snapshot, BattleOptions{}, std::span(saved).first(length)) == nullptr);
  }
  std::vector<uint8_t> longer = saved;
  longer.push_back(0);
  SB_CHECK(Battle::RestoreState(snapshot, BattleOptions{}, longer) == nullptr);
  saved[4] ^= 0xff; // the version
  SB_CHECK(Battle::RestoreState(snapshot, BattleOptions{}, saved) == nullptr);
  SB_CHECK(!DecodeCommands(std::vector<uint8_t>{1, 2, 3}).has_value());
}

// A commander's private state rides along in saves.
class CountingCommander final : public Commander
{
  public:
  void Update(const Battle&, Side, std::vector<Command>&) override
  {
    ++m_updates;
  }
  void Save(std::vector<uint8_t>& out) const override
  {
    const auto* bytes = reinterpret_cast<const uint8_t*>(&m_updates);
    out.assign(bytes, bytes + sizeof(m_updates));
  }
  bool Restore(std::span<const uint8_t> data) override
  {
    if (data.size() != sizeof(m_updates)) {
      return false;
    }
    std::memcpy(&m_updates, data.data(), sizeof(m_updates));
    return true;
  }
  int64_t m_updates = 0;
};

void CommanderStateIsSaved()
{
  BattleOptions options;
  options.makeCommander = [](Side) { return std::make_unique<CountingCommander>(); };
  const BattleSnapshot snapshot = BusyBattle(5);
  Battle battle(snapshot, options);
  battle.RunHeadless(100);
  const uint64_t hash = battle.StateHash();
  std::unique_ptr<Battle> restored = Battle::RestoreState(snapshot, options, battle.SaveState());
  SB_CHECK(restored != nullptr && restored->StateHash() == hash);
  battle.Step();
  restored->Step();
  SB_CHECK(restored->StateHash() == battle.StateHash());
}

// A version-2 state block has no launched flags: a squadron flying, rearming or kept aboard has launched, one waiting
// in its hangar has not.
void VersionTwoStateBlocksRestoreLaunchedSquadrons()
{
  const BattleSnapshot snapshot = BusyBattle(41);
  Battle reference(snapshot, BattleOptions{});
  reference.RunHeadless(75 * kTicksPerSecond);
  const std::vector<uint8_t> full = reference.SaveState();
  std::vector<uint8_t> current;
  WriteState(reference.State(), current);
  std::vector<uint8_t> versionTwo;
  WriteState(reference.State(), versionTwo, 2);
  SB_CHECK(versionTwo.size() < current.size());
  versionTwo.insert(versionTwo.end(), full.begin() + static_cast<std::ptrdiff_t>(current.size()), full.end());
  std::unique_ptr<Battle> restored = Battle::RestoreState(snapshot, BattleOptions{}, versionTwo);
  SB_CHECK(restored != nullptr);
  int flying = 0;
  for (size_t i = 0; i < reference.State().squadrons.size(); ++i) {
    const SquadronState& before = reference.State().squadrons[i];
    const SquadronState& after = restored->State().squadrons[i];
    if (before.place == SquadronPlace::InSpace) {
      SB_CHECK(after.launched);
      ++flying;
    } else if (before.place == SquadronPlace::Docked && !before.launched) {
      SB_CHECK(!after.launched);
    }
  }
  SB_CHECK(flying > 0);
}

} // namespace

int main()
{
  RestoreContinuesExactly();
  RestoredViewModelMatches();
  CommandLogReplaysTheBattle();
  ForeignOrDamagedStatesAreRejected();
  CommanderStateIsSaved();
  VersionTwoStateBlocksRestoreLaunchedSquadrons();
  std::puts("space battle save passed");
  return 0;
}
