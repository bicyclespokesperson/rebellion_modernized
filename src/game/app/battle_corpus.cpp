#include "app/battle_corpus.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <utility>

#include "events/game_clock_hooks.h"
#include "gameobj/game_settings.h"

namespace
{

using reb::battle::UnitFate;

// The file stem of the battle being recorded, between its snapshot and its results.
std::filesystem::path g_currentStem;
int g_battleCount = 0;

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

void AppendLine(std::string& out, const char* keyword, std::initializer_list<std::pair<const char*, std::string>> fields)
{
  out += keyword;
  for (const auto& [key, value] : fields) {
    out += ' ';
    out += key;
    out += '=';
    out += value;
  }
  out += '\n';
}

std::string Number(int64_t value)
{
  return std::to_string(value);
}

std::string Flag(bool value)
{
  return value ? "1" : "0";
}

void WriteFile(const std::filesystem::path& path, const std::string& text)
{
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file << text;
  if (!file) {
    std::fprintf(stderr, "battle corpus: cannot write %s\n", path.string().c_str());
  }
}

} // namespace

void RecordBattleSnapshot(const reb::battle::BattleSnapshot& snapshot)
{
  g_currentStem.clear();
  const std::optional<std::string> directory = BattleCorpusDirectory();
  if (!directory) {
    return;
  }
  GameSettings settings;
  GetGameSettings(&settings);
  char number[16];
  std::snprintf(number, sizeof number, "%04d", ++g_battleCount);
  g_currentStem = std::filesystem::path(*directory) / (std::to_string(settings.m_field10) + "-" + number);
  WriteFile(g_currentStem.string() + ".snapshot", reb::battle::SerializeSnapshot(snapshot));
}

void RecordBattleResults(BattleEngine& engine)
{
  if (g_currentStem.empty()) {
    return;
  }
  WriteFile(g_currentStem.string() + ".space.result", FormatBattleResults(engine));
  g_currentStem.clear();
}

std::string FormatBattleResults(BattleEngine& engine)
{
  const reb::battle::BattleResultSet results = engine.Results();
  const BattleOutcome legacy = engine.Outcome();
  std::string out = "rebellion-battle-result 1\n";
  AppendLine(out, "engine",
             {{"name", "space"},
              {"steps", Number(engine.Steps())},
              {"stalemate", Flag(engine.EndedInStalemate())}});
  const reb::battle::BattleOutcomeSummary& outcome = results.outcome;
  AppendLine(out, "outcome",
             {{"alliance-present", Flag(outcome.alliancePresent)},
              {"empire-present", Flag(outcome.empirePresent)},
              {"alliance-withdrew", Flag(outcome.allianceWithdrew)},
              {"empire-withdrew", Flag(outcome.empireWithdrew)}});
  AppendLine(out, "legacy-outcome",
             {{"side-a-final", Number(legacy.m_sideAFinal)},
              {"side-a-alive", Number(legacy.m_sideAAlive)},
              {"side-b-final", Number(legacy.m_sideBFinal)},
              {"side-b-alive", Number(legacy.m_sideBAlive)},
              {"unfinished", Number(legacy.m_unfinished)}});
  for (const reb::battle::ShipResult& ship : results.ships) {
    const reb::battle::CriticalHits& crit = ship.criticals;
    AppendLine(out, "ship",
               {{"unit", Number(ship.unit)},
                {"fate", FateName(ship.fate)},
                {"hull", Number(ship.hull)},
                {"crit-speed", Number(crit.speed)},
                {"crit-shield", Number(crit.shieldRecharge)},
                {"crit-weapon", Number(crit.weaponRecharge)},
                {"crit-tractor", Number(crit.tractor)},
                {"crit-hyperdrive", Number(crit.hyperdrivePrimary)},
                {"crit-hyperdrive-backup", Number(crit.hyperdriveBackup)}});
  }
  for (const reb::battle::SquadronResult& squadron : results.squadrons) {
    AppendLine(out, "squadron",
               {{"unit", Number(squadron.unit)},
                {"fate", FateName(squadron.fate)},
                {"size", Number(squadron.size)},
                {"carrier", Number(squadron.carrier)}});
  }
  for (const reb::battle::DeathStarResult& deathStar : results.deathStars) {
    AppendLine(out, "death-star",
               {{"unit", Number(deathStar.unit)},
                {"alive", Flag(deathStar.alive)},
                {"withdrew", Flag(deathStar.withdrew)},
                {"hull", Number(deathStar.hull)}});
  }
  return out;
}
