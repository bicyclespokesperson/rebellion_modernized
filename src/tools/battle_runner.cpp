// reb_battle_runner: runs battle snapshots (the REB_BATTLE_CORPUS files) headless through a battle engine and writes
// one JSON line per battle and seed, for tools/battle_calibration.py. See docs/ai_matches.md.
//
//   reb_battle_runner [--seeds N] [--out results.jsonl] [--write-results DIR]
//                     --in a.snapshot [b.snapshot ...]
//
// Seed i (0-based) runs the snapshot with BattleParams::seed + i. --write-results
// also writes seed 0's results in the corpus result form (<DIR>/<stem>.space.result), so a replay can be compared
// byte for byte with the result the campaign recorded.
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "app/battle_corpus.h"
#include "app/battle_engine.h"
#include "app/battle_engine_space.h"

namespace
{

using reb::battle::BattleResultSet;
using reb::battle::BattleSnapshot;
using reb::battle::Side;
using reb::battle::UnitFate;

// Beyond the simulator time limit.
constexpr int64_t kStepLimit = 1'000'000;

struct Options {
  int seeds = 1;
  std::optional<std::filesystem::path> out;
  std::optional<std::filesystem::path> resultDirectory;
  std::vector<std::filesystem::path> inputs;
};

[[noreturn]] void Usage(const std::string& problem)
{
  std::cerr << "reb_battle_runner: " << problem << "\n"
            << "usage: reb_battle_runner [--seeds N] [--out results.jsonl] "
               "[--write-results DIR] --in a.snapshot [b.snapshot ...]\n";
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
    if (arg == "--seeds") {
      options.seeds = std::atoi(value().c_str());
      if (options.seeds < 1) {
        Usage("--seeds must be at least 1");
      }
    } else if (arg == "--out") {
      options.out = value();
    } else if (arg == "--write-results") {
      options.resultDirectory = value();
    } else if (arg == "--in") {
      while (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0) {
        options.inputs.emplace_back(argv[++i]);
      }
    } else {
      Usage("unknown option " + arg);
    }
  }
  if (options.inputs.empty()) {
    Usage("no --in snapshots");
  }
  return options;
}

std::optional<BattleSnapshot> ReadSnapshot(const std::filesystem::path& path)
{
  std::ifstream file(path, std::ios::binary);
  std::ostringstream text;
  text << file.rdbuf();
  return file ? reb::battle::ParseSnapshot(text.str()) : std::nullopt;
}

std::string JsonString(const std::string& text)
{
  std::string out = "\"";
  for (const char c : text) {
    const auto byte = static_cast<unsigned char>(c);
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (byte < 0x20 || byte >= 0x7f) {
      char escaped[8];
      std::snprintf(escaped, sizeof escaped, "\\u%04x", byte);
      out += escaped;
    } else {
      out += c;
    }
  }
  return out + "\"";
}

const char* Bool(bool value)
{
  return value ? "true" : "false";
}

struct SideTally {
  int capitalShips = 0;
  int destroyed = 0;
  int withdrawn = 0;
  int damaged = 0; // survived with less hull than it started with
  int64_t hullStart = 0;
  int64_t hullLost = 0;
  int squadrons = 0;
  int fighters = 0;
  int fightersLost = 0;
  int squadronsDestroyed = 0;
};

std::array<SideTally, 2> Tally(const BattleSnapshot& snapshot, const BattleResultSet& results)
{
  std::array<SideTally, 2> sides{};
  for (const reb::battle::ShipResult& ship : results.ships) {
    const auto& entry = std::get<reb::battle::CapitalShipEntry>(snapshot.units[ship.unit]);
    SideTally& side = sides[static_cast<int>(entry.identity.side)];
    ++side.capitalShips;
    side.destroyed += ship.fate == UnitFate::Destroyed;
    side.withdrawn += ship.fate == UnitFate::Withdrawn;
    side.damaged += ship.fate != UnitFate::Destroyed && ship.hull < entry.hull;
    side.hullStart += entry.hull;
    side.hullLost += entry.hull - ship.hull;
  }
  for (const reb::battle::SquadronResult& squadron : results.squadrons) {
    const auto& entry = std::get<reb::battle::SquadronEntry>(snapshot.units[squadron.unit]);
    SideTally& side = sides[static_cast<int>(entry.identity.side)];
    ++side.squadrons;
    side.fighters += entry.currentSize;
    side.fightersLost += entry.currentSize - squadron.size;
    side.squadronsDestroyed += squadron.fate == UnitFate::Destroyed;
  }
  return sides;
}

// The winner as BattleResults::Apply decides it: 1 Alliance, 2 Empire, 0 none. A contested system goes to whoever
// held it first (the snapshot cannot tell the rare "both" control state apart).
int Winner(const BattleOutcome& outcome, const BattleSnapshot& snapshot)
{
  const auto fromSurvivors = [&](int alliance, int empire) {
    if (alliance != 0 && empire != 0) {
      return snapshot.params.heldFirstBy == Side::Alliance ? 1 : 2;
    }
    return alliance != 0 ? 1 : (empire != 0 ? 2 : 0);
  };
  const int winner = fromSurvivors(outcome.m_sideAAlive, outcome.m_sideBAlive);
  return winner != 0 ? winner : fromSurvivors(outcome.m_sideAFinal, outcome.m_sideBFinal);
}

std::string SideJson(const SideTally& side)
{
  std::ostringstream out;
  out << "{\"capital_ships\":" << side.capitalShips << ",\"destroyed\":" << side.destroyed
      << ",\"withdrawn\":" << side.withdrawn << ",\"damaged\":" << side.damaged << ",\"hull_start\":" << side.hullStart
      << ",\"hull_lost\":" << side.hullLost << ",\"squadrons\":" << side.squadrons << ",\"fighters\":" << side.fighters
      << ",\"fighters_lost\":" << side.fightersLost << ",\"squadrons_destroyed\":" << side.squadronsDestroyed << "}";
  return out.str();
}

std::string BattleJson(const std::filesystem::path& input, const BattleSnapshot& snapshot, int seedIndex,
                       BattleEngine& engine, bool finished, double elapsedMilliseconds)
{
  const BattleResultSet results = engine.Results();
  const BattleOutcome outcome = engine.Outcome();
  const std::array<SideTally, 2> sides = Tally(snapshot, results);
  const bool deathStar = std::ranges::any_of(
      snapshot.units, [](const auto& unit) { return std::holds_alternative<reb::battle::DeathStarEntry>(unit); });
  const reb::battle::BattleOutcomeSummary& summary = results.outcome;
  std::ostringstream out;
  out << "{\"snapshot\":" << JsonString(input.string()) << ",\"engine\":\"" << "space"
      << "\",\"seed\":" << snapshot.params.seed << ",\"seed_index\":" << seedIndex
      << ",\"finished\":" << Bool(finished) << ",\"steps\":" << engine.Steps()
      << ",\"elapsed_ms\":" << elapsedMilliseconds
      << ",\"stalemate\":" << Bool(engine.EndedInStalemate()) << ",\"death_star\":" << Bool(deathStar)
      << ",\"winner\":" << Winner(outcome, snapshot) << ",\"outcome\":{\"alliance_present\":"
      << Bool(summary.alliancePresent) << ",\"empire_present\":" << Bool(summary.empirePresent)
      << ",\"alliance_withdrew\":" << Bool(summary.allianceWithdrew)
      << ",\"empire_withdrew\":" << Bool(summary.empireWithdrew) << ",\"side_a_final\":" << outcome.m_sideAFinal
      << ",\"side_a_alive\":" << outcome.m_sideAAlive << ",\"side_b_final\":" << outcome.m_sideBFinal
      << ",\"side_b_alive\":" << outcome.m_sideBAlive << "},\"alliance\":" << SideJson(sides[0])
      << ",\"empire\":" << SideJson(sides[1]) << "}";
  return out.str();
}

void WriteText(const std::filesystem::path& path, const std::string& text)
{
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file << text;
  if (!file) {
    std::cerr << "reb_battle_runner: cannot write " << path << "\n";
    std::exit(1);
  }
}

} // namespace

int main(int argc, char** argv)
{
  const Options options = ParseOptions(argc, argv);
  std::ofstream outFile;
  if (options.out) {
    if (options.out->has_parent_path()) {
      std::filesystem::create_directories(options.out->parent_path());
    }
    outFile.open(*options.out, std::ios::binary | std::ios::trunc);
  }
  std::ostream& out = options.out ? static_cast<std::ostream&>(outFile) : std::cout;

  int failures = 0;
  for (const std::filesystem::path& input : options.inputs) {
    std::optional<BattleSnapshot> snapshot = ReadSnapshot(input);
    if (!snapshot) {
      std::cerr << "reb_battle_runner: cannot read snapshot " << input << "\n";
      ++failures;
      continue;
    }
    const uint32_t baseSeed = snapshot->params.seed;
    for (int seedIndex = 0; seedIndex < options.seeds; ++seedIndex) {
      snapshot->params.seed = baseSeed + static_cast<uint32_t>(seedIndex);
      const auto start = std::chrono::steady_clock::now();
      std::unique_ptr<BattleEngine> engine = CreateSpaceBattleEngine();
      engine->Load(*snapshot);
      engine->Start();
      bool finished = false;
      while (!finished && engine->Steps() < kStepLimit) {
        finished = engine->Advance();
      }
      const double elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
      out << BattleJson(input, *snapshot, seedIndex, *engine, finished, elapsed) << "\n";
      if (seedIndex == 0 && options.resultDirectory) {
        const std::string name = input.stem().string() + "." + "space" + ".result";
        WriteText(*options.resultDirectory / name, FormatBattleResults(*engine));
      }
      engine->Finish();
      failures += !finished;
    }
  }
  out.flush();
  return failures == 0 ? 0 : 1;
}
