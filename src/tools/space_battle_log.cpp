// reb_space_battle_log: runs one battle snapshot through the space battle simulator headless, with the default
// commander on both sides, and prints a readable battle log: the deployment, every event as it happens, a periodic
// status line and the results. For looking at what a snapshot dumped from a campaign does.
//
//   reb_space_battle_log [--seed N] [--bearing RADIANS] [--status SECONDS] [--save-commands FILE] snapshot
//   reb_space_battle_log --batch snapshot-list.txt [--seeds N] [--engine-label NAME] [--default-commander | --aggressive]
// Batch paths are read literally, one per line; seed i is the snapshot seed (or --seed) + i.
// --default-commander selects the frozen original baseline; --ai-side uses the current AI only for that side.
// --expert selects the Cautious / High commander for preservation benchmarking.
// --aggressive matches the strategic adapter's fallback Classic / Aggressive commanders on both sides.
// --expert-side changes only that production commander, preserving its production opponent.
// Capital value is an asset-preservation proxy: original maximum hull of each surviving or withdrawn capital ship.
// capital_survival exposes class identities for external refined-material cost measurements.
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>

#include "space_battle/battle.h"
#include "space_battle/battle_queries.h"
#include "space_battle/battle_save.h"
#include "space_battle/battle_state.h"

namespace
{
using namespace reb::battle;

// Preserve the pre-composition AI for repeatable calibration after the production commander changes.
namespace baseline
{
class BaselineCommander final : public Commander
{
  public:
  void Update(const Battle& battle, Side side, std::vector<Command>& out) override;
};

namespace
{
Command MakeCommand(const Battle& battle, Side side, OrderPayload order)
{
  Command command;
  command.tick = battle.Tick();
  command.side = side;
  command.order = std::move(order);
  return command;
}

UnitIndex HeaviestLineShip(const BattleState& state, Side side)
{
  UnitIndex best = kNoUnit;
  float bestHull = 0.0f;
  for (const ShipState& ship : state.ships) {
    const UnitProfile& profile = state.Profile(ship.unit);
    const bool line = profile.archetype == Archetype::BowLine || profile.archetype == Archetype::BroadsideLine;
    if (state.units[ship.unit].side == side && state.InSpace(ship.unit) && line && profile.hullMax > bestHull) {
      bestHull = profile.hullMax;
      best = ship.unit;
    }
  }
  return best;
}

void AssignEscorts(const Battle& battle, Side side, std::vector<Command>& out)
{
  const BattleState& state = battle.State();
  const UnitIndex protectedShip = HeaviestLineShip(state, side);
  UnitIndex bomberSquadron = kNoUnit;
  for (const GroupState& group : state.groups) {
    if (group.side == side && group.flight && !group.members.empty()
        && state.Profile(group.members.front()).archetype == Archetype::Bomber) {
      bomberSquadron = group.members.front();
      break;
    }
  }
  for (const GroupState& group : state.groups) {
    if (group.side != side || group.members.empty()) {
      continue;
    }
    if (!group.flight && group.shipDoctrine.role == Role::Screen && protectedShip != kNoUnit) {
      OrderSetShipDoctrine order;
      order.doctrine.screenTarget = protectedShip;
      order.fields = kShipScreenTarget;
      Command command = MakeCommand(battle, side, order);
      command.target.groups = {group.id};
      out.push_back(command);
    }
    if (group.flight && bomberSquadron != kNoUnit && state.Profile(group.members.front()).archetype == Archetype::Multirole) {
      OrderSetFlightDoctrine order;
      order.doctrine.mission = FlightMission::Escort;
      order.doctrine.escortTarget = bomberSquadron;
      order.fields = kFlightMission | kFlightEscortTarget;
      Command command = MakeCommand(battle, side, order);
      command.target.groups = {group.id};
      out.push_back(command);
    }
  }
}

void FireSuperlaser(const Battle& battle, Side side, std::vector<Command>& out)
{
  const BattleState& state = battle.State();
  for (const StationState& station : state.stations) {
    if (state.units[station.unit].side != side || !SuperlaserReady(state, station.unit)) {
      continue;
    }
    UnitIndex best = kNoUnit;
    float bestValue = 0.0f;
    for (const ShipState& ship : state.ships) {
      if (state.units[ship.unit].side == side || !state.InSpace(ship.unit)) {
        continue;
      }
      const float value = (1.0f + state.Profile(ship.unit).firepower * WeaponOutput(state, ship.unit))
                          * EffectiveHealth(state, ship.unit);
      if (value > bestValue) {
        bestValue = value;
        best = ship.unit;
      }
    }
    if (best != kNoUnit) {
      out.push_back(MakeCommand(battle, side, OrderFireSuperlaser{best}));
    }
  }
}

void SendTrenchRun(const Battle& battle, Side side, std::vector<Command>& out)
{
  const BattleState& state = battle.State();
  for (const StationState& station : state.stations) {
    if (state.units[station.unit].side == side || station.runFlight != kNoGroup
        || !TrenchRunPossible(state, station.unit)) {
      continue;
    }
    GroupId best = kNoGroup;
    int bestTorpedoes = -1;
    for (const GroupState& group : state.groups) {
      if (group.side != side || !group.flight) {
        continue;
      }
      int torpedoes = 0;
      bool flying = false;
      for (UnitIndex member : group.members) {
        const SquadronState* squadron = state.Squadron(member);
        if (squadron == nullptr || squadron->place != SquadronPlace::InSpace) {
          continue;
        }
        flying = true;
        for (int i = 0; i < squadron->craftCount; ++i) {
          const CraftState& craft = state.craft[squadron->firstCraft + i];
          torpedoes += craft.alive ? craft.torpedoes : 0;
        }
      }
      if (flying && torpedoes > bestTorpedoes) {
        bestTorpedoes = torpedoes;
        best = group.id;
      }
    }
    if (best != kNoGroup) {
      Command command = MakeCommand(battle, side, OrderDeathStarRun{});
      command.target.groups = {best};
      out.push_back(command);
    }
  }
}

void WithdrawByThreshold(const Battle& battle, Side side, std::vector<Command>& out)
{
  const BattleState& state = battle.State();
  const SideParams& params = state.snapshot.params.sides[SideIndex(side)];
  const float threshold = params.human ? battle.Options().humanSideAiWithdrawThreshold : params.withdrawThreshold;
  const float enemy = state.sides[SideIndex(Opponent(side))].strength;
  const float own = state.sides[SideIndex(side)].strength;
  if (threshold <= 0.0f || !params.canRetreat || enemy <= 0.0f || own >= threshold * enemy) {
    return;
  }
  bool anyStaying = false;
  for (UnitIndex i = 0; i < static_cast<UnitIndex>(state.units.size()); ++i) {
    anyStaying = anyStaying
                 || (state.units[i].side == side && state.InSpace(i) && !state.Withdrawing(i)
                     && state.units[i].kind != UnitKind::Passive);
  }
  if (!anyStaying) {
    return;
  }
  Command command = MakeCommand(battle, side, OrderWithdraw{});
  for (const GroupState& group : state.groups) {
    if (group.side == side && !group.members.empty()) {
      command.target.groups.push_back(group.id);
    }
  }
  for (const StationState& station : state.stations) {
    if (state.units[station.unit].side == side && state.InSpace(station.unit) && !station.withdrawing) {
      command.target.units.push_back(station.unit);
    }
  }
  out.push_back(command);
}

} // namespace

void BaselineCommander::Update(const Battle& battle, Side side, std::vector<Command>& out)
{
  const int64_t tick = battle.Tick();
  if (tick == 0) {
    AssignEscorts(battle, side, out);
  }
  // Once a second, the two sides half a second apart.
  if ((tick + SideIndex(side) * (kTicksPerSecond / 2)) % kTicksPerSecond != 0) {
    return;
  }
  FireSuperlaser(battle, side, out);
  SendTrenchRun(battle, side, out);
  WithdrawByThreshold(battle, side, out);
}

} // namespace baseline

struct Options {
  std::optional<uint32_t> seed;
  float bearing = 0.0f;
  float statusSeconds = 30.0f;
  std::string saveCommands;
  std::string input;
  std::string batch;
  std::string engineLabel = "space";
  int seeds = 1;
  bool defaultCommander = false;
  bool expert = false;
  bool aggressive = false;
  std::optional<Side> aiSide;
  std::optional<Side> expertSide;
};

[[noreturn]] void Usage(const std::string& problem)
{
  std::cerr << "reb_space_battle_log: " << problem << "\n"
            << "usage: reb_space_battle_log [--seed N] [--bearing RADIANS] [--status SECONDS] [--save-commands FILE] "
               "snapshot\n"
            << "       reb_space_battle_log --batch snapshot-list.txt [--seeds N] [--seed N] "
               "[--engine-label NAME] [--expert] [--expert-side alliance|empire] "
               "[--default-commander | --ai-side alliance|empire | --aggressive]\n";
  std::exit(2);
}

Options ParseOptions(int argc, char** argv)
{
  Options options;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    const auto value = [&]() -> std::string {
      if (i + 1 >= argc) {
        Usage(arg + " needs a value");
      }
      return argv[++i];
    };
    if (arg == "--seed") {
      options.seed = static_cast<uint32_t>(std::strtoul(value().c_str(), nullptr, 10));
    } else if (arg == "--batch") {
      options.batch = value();
    } else if (arg == "--seeds") {
      options.seeds = std::atoi(value().c_str());
      if (options.seeds < 1) {
        Usage("--seeds must be positive");
      }
    } else if (arg == "--engine-label") {
      options.engineLabel = value();
    } else if (arg == "--aggressive") {
      options.aggressive = true;
    } else if (arg == "--expert") {
      options.expert = true;
    } else if (arg == "--expert-side") {
      const std::string name = value();
      if (name != "alliance" && name != "empire") {
        Usage("--expert-side must be alliance or empire");
      }
      options.expertSide = name == "alliance" ? Side::Alliance : Side::Empire;
    } else if (arg == "--ai-side") {
      const std::string name = value();
      if (name != "alliance" && name != "empire") {
        Usage("--ai-side must be alliance or empire");
      }
      options.aiSide = name == "alliance" ? Side::Alliance : Side::Empire;
    } else if (arg == "--default-commander") {
      options.defaultCommander = true;
    } else if (arg == "--bearing") {
      options.bearing = std::strtof(value().c_str(), nullptr);
    } else if (arg == "--status") {
      options.statusSeconds = std::strtof(value().c_str(), nullptr);
    } else if (arg == "--save-commands") {
      options.saveCommands = value();
    } else if (!arg.empty() && arg[0] == '-') {
      Usage("unknown option " + arg);
    } else if (options.input.empty()) {
      options.input = arg;
    } else {
      Usage("one snapshot at a time");
    }
  }
  if (options.input.empty() == options.batch.empty()) {
    Usage("give one snapshot or --batch snapshot-list.txt");
  }
  if (options.aggressive && (options.expert || options.defaultCommander || options.aiSide)) {
    Usage("--aggressive cannot be combined with --expert, --default-commander or --ai-side");
  }
  if (options.expert && options.defaultCommander) {
    Usage("--expert and --default-commander are mutually exclusive");
  }
  if (options.aiSide && options.defaultCommander) {
    Usage("--ai-side and --default-commander are mutually exclusive");
  }
  if (options.expertSide && (options.expert || options.aiSide || options.defaultCommander)) {
    Usage("--expert-side requires the production commander on both sides");
  }
  if (!options.batch.empty() && !options.saveCommands.empty()) {
    Usage("--save-commands is only available for one snapshot");
  }
  return options;
}

const char* SideName(Side side)
{
  return side == Side::Alliance ? "Alliance" : "Empire";
}

const char* FateName(UnitFate fate)
{
  switch (fate) {
    case UnitFate::Active:
      return "active";
    case UnitFate::Withdrawn:
      return "withdrawn";
    case UnitFate::Destroyed:
      return "destroyed";
    case UnitFate::Docked:
      return "docked";
  }
  return "?";
}

const std::string& NameOf(const BattleSnapshot& snapshot, UnitIndex unit)
{
  return std::visit([](const auto& entry) -> const std::string& { return entry.identity.name; }, snapshot.units[unit]);
}

void PrintDeployment(const Battle& battle)
{
  const BattleState& state = battle.State();
  std::printf("%s: %s attacks, seed %u\n", state.snapshot.params.systemName.c_str(), SideName(state.attacker),
              state.snapshot.params.seed);
  for (const GroupState& group : state.groups) {
    std::printf("  %-8s %-14s", SideName(group.side), group.name.c_str());
    for (UnitIndex member : group.members) {
      std::printf(" %s (%s)", NameOf(state.snapshot, member).c_str(), ArchetypeName(state.Profile(member).archetype));
    }
    std::printf("\n");
  }
  for (size_t i = 0; i < state.defenseProfiles.size(); ++i) {
    const DefenseProfile& defense = state.defenseProfiles[i];
    std::printf("  %-8s planetary battery %s (%s, %.1f damage/s)\n", SideName(defense.side),
                state.snapshot.defenses[i].identity.className.c_str(),
                defense.weapon == WeaponKind::Ion ? "ion" : "turbolaser", defense.dps);
  }
}

// What each planetary battery fired and landed, counted as its salvos leave the surface.
struct BatteryTally {
  int salvos = 0;
  int hits = 0;
  float damage = 0.0f;
};

void CountBatterySalvos(const BattleState& state, std::vector<BatteryTally>& tallies)
{
  tallies.resize(state.defenses.size());
  for (const PendingShot& shot : state.shots) {
    if (shot.battery >= 0 && shot.fireTick == state.tick - 1) {
      BatteryTally& tally = tallies[static_cast<size_t>(shot.battery)];
      ++tally.salvos;
      tally.hits += shot.hit ? 1 : 0;
      tally.damage += shot.hit ? shot.damage : 0.0f;
    }
  }
}

void PrintBatteries(const BattleState& state, const std::vector<BatteryTally>& tallies)
{
  for (size_t i = 0; i < tallies.size(); ++i) {
    std::printf("  battery  %-28s %-9s salvos %d  hits %d  damage %.0f\n",
                state.snapshot.defenses[i].identity.className.c_str(), SideName(state.defenseProfiles[i].side),
                tallies[i].salvos, tallies[i].hits, tallies[i].damage);
  }
}

void PrintStatus(const BattleState& state)
{
  std::printf("%7.1f s  strength %.0f / %.0f", state.BattleSeconds(), state.sides[0].strength, state.sides[1].strength);
  for (Side side : {Side::Alliance, Side::Empire}) {
    int ships = 0;
    int craft = 0;
    for (const ShipState& ship : state.ships) {
      ships += state.units[ship.unit].side == side && state.InSpace(ship.unit) ? 1 : 0;
    }
    for (const SquadronState& squadron : state.squadrons) {
      craft += state.units[squadron.unit].side == side && state.Present(squadron.unit) ? squadron.alive : 0;
    }
    std::printf("  %s %d ships %d craft", SideName(side), ships, craft);
  }
  std::printf("\n");
}

void PrintResults(const Battle& battle)
{
  const BattleState& state = battle.State();
  static constexpr const char* kReasons[] = {"none", "elimination", "stalemate", "time limit"};
  const BattleResultSet results = battle.Results();
  std::printf("ended after %.1f s by %s: alliance %s%s, empire %s%s\n", state.BattleSeconds(),
              kReasons[static_cast<int>(state.endReason)], results.outcome.alliancePresent ? "present" : "gone",
              results.outcome.allianceWithdrew ? " (withdrew)" : "", results.outcome.empirePresent ? "present" : "gone",
              results.outcome.empireWithdrew ? " (withdrew)" : "");
  for (const ShipResult& ship : results.ships) {
    const CriticalHits& hits = ship.criticals;
    std::printf("  ship     %-28s %-9s hull %5d  hits s%d w%d t%d e%d h%d/%d\n", NameOf(state.snapshot, ship.unit).c_str(),
                FateName(ship.fate), ship.hull, hits.shieldRecharge, hits.weaponRecharge, hits.tractor, hits.speed,
                hits.hyperdrivePrimary, hits.hyperdriveBackup);
  }
  for (const SquadronResult& squadron : results.squadrons) {
    std::printf("  squadron %-28s %-9s size %2d  carrier %s\n", NameOf(state.snapshot, squadron.unit).c_str(),
                FateName(squadron.fate), squadron.size,
                squadron.carrier == kNoUnit ? "(system)" : NameOf(state.snapshot, squadron.carrier).c_str());
  }
  for (const DeathStarResult& station : results.deathStars) {
    std::printf("  station  %-28s %s hull %d\n", NameOf(state.snapshot, station.unit).c_str(),
                station.alive ? (station.withdrew ? "withdrew" : "alive") : "destroyed", station.hull);
  }
}

std::optional<BattleSnapshot> ReadSnapshot(const std::string& path)
{
  std::ifstream file(path, std::ios::binary);
  const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  return file ? ParseSnapshot(text) : std::nullopt;
}

BattleOptions MakeBattleOptions(const Options& options)
{
  BattleOptions result;
  result.attackerBearing = options.bearing;
  const CommanderParams params = options.expert ? CommanderParams{Temperament::Cautious, CommanderDifficulty::High}
                                                  : CommanderParams{};
  result.commanders.fill(options.aggressive ? CommanderParams{Temperament::Aggressive, CommanderDifficulty::Normal}
                                            : params);
  if (options.expertSide) {
    result.commanders[SideIndex(*options.expertSide)] = {Temperament::Cautious, CommanderDifficulty::High};
  }
  if (options.defaultCommander || options.aiSide) {
    result.makeCommander = [aiSide = options.aiSide, params](Side side) -> std::unique_ptr<Commander> {
      if (aiSide && *aiSide == side) {
        return std::make_unique<DefaultCommander>(params);
      }
      return std::make_unique<baseline::BaselineCommander>();
    };
  }
  return result;
}

std::string JsonString(const std::string& text)
{
  std::string result = "\"";
  for (unsigned char value : text) {
    if (value == '"' || value == '\\') {
      result += '\\';
      result += static_cast<char>(value);
    } else if (value < 0x20) {
      char escaped[8];
      std::snprintf(escaped, sizeof escaped, "\\u%04x", value);
      result += escaped;
    } else {
      result += static_cast<char>(value);
    }
  }
  return result + "\"";
}

struct SideTally {
  int capitalShips = 0;
  int destroyed = 0;
  int withdrawn = 0;
  int damaged = 0;
  int64_t hullStart = 0;
  int64_t hullLost = 0;
  int64_t capitalValueStart = 0;
  int64_t capitalValueRetained = 0;
  int squadrons = 0;
  int fighters = 0;
  int fightersLost = 0;
  int squadronsDestroyed = 0;
  std::vector<std::pair<uint32_t, bool>> capitalSurvival;
};

std::array<SideTally, 2> Tally(const BattleSnapshot& snapshot, const BattleResultSet& results)
{
  std::array<SideTally, 2> sides{};
  for (const ShipResult& ship : results.ships) {
    const auto& entry = std::get<CapitalShipEntry>(snapshot.units[ship.unit]);
    SideTally& side = sides[static_cast<int>(entry.identity.side)];
    ++side.capitalShips;
    side.destroyed += ship.fate == UnitFate::Destroyed;
    side.withdrawn += ship.fate == UnitFate::Withdrawn;
    side.damaged += ship.fate != UnitFate::Destroyed && ship.hull < entry.hull;
    side.hullStart += entry.hull;
    side.hullLost += entry.hull - ship.hull;
    side.capitalValueStart += entry.hullMax;
    side.capitalValueRetained += ship.fate != UnitFate::Destroyed ? entry.hullMax : 0;
    side.capitalSurvival.emplace_back(entry.identity.typeBits, ship.fate != UnitFate::Destroyed);
  }
  for (const SquadronResult& squadron : results.squadrons) {
    const auto& entry = std::get<SquadronEntry>(snapshot.units[squadron.unit]);
    SideTally& side = sides[static_cast<int>(entry.identity.side)];
    ++side.squadrons;
    side.fighters += entry.currentSize;
    side.fightersLost += entry.currentSize - squadron.size;
    side.squadronsDestroyed += squadron.fate == UnitFate::Destroyed;
  }
  return sides;
}

std::string SideJson(const SideTally& side)
{
  std::ostringstream out;
  out << "{\"capital_ships\":" << side.capitalShips << ",\"destroyed\":" << side.destroyed
      << ",\"withdrawn\":" << side.withdrawn << ",\"damaged\":" << side.damaged << ",\"hull_start\":" << side.hullStart
      << ",\"hull_lost\":" << side.hullLost << ",\"capital_value_start\":" << side.capitalValueStart
      << ",\"capital_value_retained\":" << side.capitalValueRetained << ",\"squadrons\":" << side.squadrons << ",\"fighters\":" << side.fighters
      << ",\"fighters_lost\":" << side.fightersLost << ",\"squadrons_destroyed\":" << side.squadronsDestroyed
      << ",\"capital_survival\":[";
  for (size_t i = 0; i < side.capitalSurvival.size(); ++i) {
    const auto [typeBits, survived] = side.capitalSurvival[i];
    out << (i == 0 ? "" : ",") << "{\"type_bits\":" << typeBits << ",\"survived\":"
        << (survived ? "true" : "false") << '}';
  }
  out << "]}";
  return out.str();
}

int Winner(const BattleSnapshot& snapshot, const BattleOutcomeSummary& outcome)
{
  const auto fromSurvivors = [&](bool alliance, bool empire) {
    if (alliance && empire) {
      return snapshot.params.heldFirstBy == Side::Alliance ? 1 : 2;
    }
    return alliance ? 1 : (empire ? 2 : 0);
  };
  const int winner = fromSurvivors(outcome.alliancePresent, outcome.empirePresent);
  return winner != 0 ? winner : fromSurvivors(outcome.allianceWithdrew, outcome.empireWithdrew);
}

int FightersAlive(const BattleState& state)
{
  int alive = 0;
  for (const SquadronState& squadron : state.squadrons) {
    alive += squadron.alive;
  }
  return alive;
}

struct WithdrawalTelemetry {
  UnitIndex unit = kNoUnit;
  int64_t start = -1;
  int64_t end = -1;
  float hullStart = 0.0f;
  float hullEnd = 0.0f;
  float strengthRatio = 0.0f;
  int targetedTicks = 0;
  int tractoredTicks = 0;
  int spoolTotal = 0;
};

struct RunTelemetry {
  int fightersLost60 = 0;
  int fightersLost120 = 0;
  int ionSpoolResets = 0;
  std::vector<WithdrawalTelemetry> withdrawals;
};

RunTelemetry RunMeasured(Battle& battle)
{
  RunTelemetry result;
  const int initialFighters = FightersAlive(battle.State());
  std::vector<int32_t> previousSpool(battle.State().ships.size());
  result.withdrawals.resize(battle.State().ships.size());
  while (!battle.Finished()) {
    for (size_t index = 0; index < previousSpool.size(); ++index) {
      previousSpool[index] = battle.State().ships[index].spoolTicks;
    }
    battle.Step();
    for (size_t index = 0; index < previousSpool.size(); ++index) {
      const ShipState& ship = battle.State().ships[index];
      result.ionSpoolResets += previousSpool[index] > 0 && ship.spoolTicks < previousSpool[index]
                               && ship.withdrawing && battle.State().InSpace(ship.unit) && ship.engineOffTicks > 0;
      auto& sample = result.withdrawals[index];
      const BattleState& state = battle.State();
      if (ship.withdrawing && sample.start < 0) {
        sample.unit = ship.unit;
        sample.start = battle.Tick() - 1;
        sample.hullStart = ship.hull;
        sample.spoolTotal = ship.spoolTotal;
        const Side side = state.units[ship.unit].side;
        sample.strengthRatio = SideStrength(state, side) / std::max(1.0f, SideStrength(state, Opponent(side)));
      }
      if (sample.start >= 0 && sample.end < 0) {
        sample.hullEnd = ship.hull;
        sample.tractoredTicks += ship.tractorHeld > 0.0f;
        sample.targetedTicks += std::any_of(state.ships.begin(), state.ships.end(), [&](const ShipState& enemy) {
          return state.units[enemy.unit].side != state.units[ship.unit].side && state.InSpace(enemy.unit)
                 && enemy.primaryTarget == ship.unit;
        });
        if (!state.InSpace(ship.unit)) {
          sample.end = battle.Tick();
        }
      }
    }
    if (battle.Tick() <= 60 * kTicksPerSecond) {
      result.fightersLost60 = initialFighters - FightersAlive(battle.State());
    }
    if (battle.Tick() <= 120 * kTicksPerSecond) {
      result.fightersLost120 = initialFighters - FightersAlive(battle.State());
    }
  }
  return result;
}

int RunBatch(const Options& options)
{
  std::ifstream list(options.batch);
  if (!list) {
    Usage("cannot read " + options.batch);
  }
  std::cout << std::boolalpha;
  for (std::string input; std::getline(list, input);) {
    if (input.empty()) {
      continue;
    }
    const std::optional<BattleSnapshot> original = ReadSnapshot(input);
    if (!original) {
      std::cerr << "reb_space_battle_log: invalid snapshot " << input << "\n";
      return 1;
    }
    for (int seedIndex = 0; seedIndex < options.seeds; ++seedIndex) {
      BattleSnapshot snapshot = *original;
      snapshot.params.seed = options.seed.value_or(snapshot.params.seed) + static_cast<uint32_t>(seedIndex);
      const auto started = std::chrono::steady_clock::now();
      Battle battle(snapshot, MakeBattleOptions(options));
      const std::array<float, 2> strength{SideStrength(battle.State(), Side::Alliance),
                                         SideStrength(battle.State(), Side::Empire)};
      const RunTelemetry telemetry = RunMeasured(battle);
      const double elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
      const BattleResultSet results = battle.Results();
      const auto sides = Tally(snapshot, results);
      const auto& outcome = results.outcome;
      const bool deathStar = std::ranges::any_of(snapshot.units, [](const auto& unit) {
        return std::holds_alternative<DeathStarEntry>(unit);
      });
      std::cout << "{\"snapshot\":" << JsonString(input) << ",\"engine\":" << JsonString(options.engineLabel)
                << ",\"seed\":" << snapshot.params.seed << ",\"seed_index\":" << seedIndex
                << ",\"finished\":" << battle.Finished() << ",\"steps\":" << battle.Tick()
                << ",\"elapsed_ms\":" << elapsed << ",\"stalemate\":" << (battle.State().endReason == EndReason::Stalemate)
                << ",\"death_star\":" << deathStar << ",\"winner\":" << Winner(snapshot, outcome)
                << ",\"initial_strength\":[" << strength[0] << ',' << strength[1] << ']'
                << ",\"end_reason\":" << static_cast<int>(battle.State().endReason)
                << ",\"fighters_lost_60\":" << telemetry.fightersLost60
                << ",\"fighters_lost_120\":" << telemetry.fightersLost120
                << ",\"ion_spool_resets\":" << telemetry.ionSpoolResets
                << ",\"withdrawals\":[";
      bool first = true;
      for (const auto& sample : telemetry.withdrawals) {
        if (sample.start < 0) {
          continue;
        }
        const BattleState& state = battle.State();
        std::cout << (first ? "" : ",") << "{\"unit\":" << sample.unit
                  << ",\"side\":" << SideIndex(state.units[sample.unit].side)
                  << ",\"start\":" << sample.start << ",\"end\":" << sample.end
                  << ",\"hull_start\":" << sample.hullStart << ",\"hull_end\":" << sample.hullEnd
                  << ",\"hull_max\":" << state.Profile(sample.unit).hullMax
                  << ",\"strength_ratio\":" << sample.strengthRatio
                  << ",\"spool_total\":" << sample.spoolTotal
                  << ",\"targeted_ticks\":" << sample.targetedTicks
                  << ",\"tractored_ticks\":" << sample.tractoredTicks
                  << ",\"escaped\":" << (state.units[sample.unit].fate == UnitFate::Withdrawn) << '}';
        first = false;
      }
      std::cout << "],\"outcome\":{\"alliance_present\":" << outcome.alliancePresent
                << ",\"empire_present\":" << outcome.empirePresent
                << ",\"alliance_withdrew\":" << outcome.allianceWithdrew
                << ",\"empire_withdrew\":" << outcome.empireWithdrew << "},\"alliance\":" << SideJson(sides[0])
                << ",\"empire\":" << SideJson(sides[1]) << "}\n";
      std::cout.flush();
    }
  }
  return 0;
}

} // namespace

int main(int argc, char** argv)
{
  const Options options = ParseOptions(argc, argv);
  if (!options.batch.empty()) {
    return RunBatch(options);
  }
  std::optional<BattleSnapshot> snapshot = ReadSnapshot(options.input);
  if (!snapshot) {
    std::cerr << "reb_space_battle_log: " << options.input << " is not a battle snapshot\n";
    return 1;
  }
  if (options.seed) {
    snapshot->params.seed = *options.seed;
  }
  Battle battle(*snapshot, MakeBattleOptions(options));
  PrintDeployment(battle);
  const auto statusTicks = static_cast<int64_t>(std::max(1.0f, options.statusSeconds * kTicksPerSecond));
  std::vector<BatteryTally> batteries;
  while (!battle.Finished()) {
    if (battle.Tick() % statusTicks == 0) {
      PrintStatus(battle.State());
    }
    battle.Step();
    CountBatterySalvos(battle.State(), batteries);
    // Everything logged during a step carries that step's tick.
    for (const BattleEvent& event : battle.State().events) {
      if (event.tick == battle.Tick() - 1) {
        std::printf("%7.1f s  %-8s %s\n", static_cast<double>(event.tick) / kTicksPerSecond, SideName(event.side),
                    event.text.c_str());
      }
    }
  }
  PrintStatus(battle.State());
  PrintResults(battle);
  PrintBatteries(battle.State(), batteries);
  if (!options.saveCommands.empty()) {
    const std::vector<uint8_t> bytes = EncodeCommands(battle.CommandLog());
    std::ofstream out(options.saveCommands, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  }
  return 0;
}
