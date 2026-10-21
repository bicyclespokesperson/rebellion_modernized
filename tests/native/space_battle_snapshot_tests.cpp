#include "space_battle/snapshot.h"

#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace
{

using namespace reb::battle;

void Require(bool condition, const char* what)
{
  if (!condition) {
    std::fprintf(stderr, "FAILED: %s\n", what);
    std::abort();
  }
}

// Every field gets a distinct non-default value, so a field the format drops or swaps fails the round trip.
int32_t g_next = 1;
int32_t Next()
{
  return g_next++;
}

UnitIdentity Identity(Side side, std::string name)
{
  return {static_cast<uint32_t>(Next()), static_cast<uint32_t>(Next()), side, static_cast<uint32_t>(Next()), Next(),
          std::move(name), "Class \"quoted\" \\ name\t\n\x01\x7f\xe9"};
}

WeaponGrid Grid()
{
  WeaponGrid grid;
  for (auto& arc : grid.points) {
    for (int32_t& points : arc) {
      points = Next();
    }
  }
  for (int32_t& range : grid.range) {
    range = Next();
  }
  return grid;
}

BattleSnapshot FullSnapshot()
{
  BattleSnapshot snapshot;
  BattleParams& params = snapshot.params;
  params.systemName = "Mon Calamari \"Dac\" \\ System";
  params.systemPictureId = 0xffffffffU;
  params.systemDestroyed = true;
  params.heldFirstBy = Side::Empire;
  params.controlSide = 2;
  params.opponentIsAi = false;
  params.humanIsNotAlliance = 1;
  params.seed = 0x89abcdefU;
  params.sides[0] = {0.35f, true, 3.0f, true};
  params.sides[1] = {10000.0f, false, 0.1f, false};

  CapitalShipEntry ship;
  ship.identity = Identity(Side::Alliance, "Home One");
  for (int32_t* field : {&ship.hullMax, &ship.hull, &ship.shieldMax, &ship.sublight, &ship.maneuverability,
                         &ship.detection, &ship.hyperdrive, &ship.hyperdriveBackup, &ship.shieldRecharge,
                         &ship.weaponRecharge, &ship.damageControl, &ship.tractorPower, &ship.tractorRange,
                         &ship.gravityWell, &ship.interdiction, &ship.fighterCapacity, &ship.hangarFree,
                         &ship.troopCapacity, &ship.gridTotal}) {
    *field = Next();
  }
  ship.weapons = Grid();
  ship.criticals = {Next(), Next(), Next(), Next(), Next(), Next()};
  ship.identity.briefingKnown = {false, true};
  snapshot.units.emplace_back(ship);

  SquadronEntry squadron;
  squadron.identity = Identity(Side::Alliance, "Rogue\tSquadron\n\x01\x7f\xe9");
  squadron.carrier = 0;
  for (int32_t* field : {&squadron.squadSize, &squadron.currentSize, &squadron.shieldPerCraft, &squadron.sublight,
                         &squadron.maneuverability, &squadron.detection, &squadron.hyperdrive,
                         &squadron.hyperdriveBackup, &squadron.torpedoesPerCraft, &squadron.torpedoRange}) {
    *field = Next();
  }
  squadron.weaponsPerCraft = Grid();
  snapshot.units.emplace_back(squadron);

  SquadronEntry based = squadron;
  based.identity = Identity(Side::Empire, "");
  based.carrier = kNoUnit;
  snapshot.units.emplace_back(based);

  CharacterEntry character;
  character.identity = Identity(Side::Alliance, "Admiral Ackbar");
  character.ship = 0;
  character.major = true;
  character.admiral = true;
  character.commander = false;
  character.injured = true;
  character.leadership = Next();
  character.combat = -Next();
  snapshot.units.emplace_back(character);

  PassengerEntry passenger{Identity(Side::Alliance, "Alliance Army Regiment"), 0, true};
  snapshot.units.emplace_back(passenger);
  PassengerEntry force{Identity(Side::Empire, "Noghri = Death Commandos"), kNoUnit, false};
  snapshot.units.emplace_back(force);

  DeathStarEntry deathStar{Identity(Side::Empire, "Death Star"), true, true, Next(), Next(), Next()};
  snapshot.units.emplace_back(deathStar);

  DefenseEntry ionCannon{Identity(Side::Empire, "KDY-150"), WeaponKind::Ion, Next()};
  ionCannon.identity.briefingKnown = {false, true};
  snapshot.defenses.push_back(ionCannon);
  snapshot.defenses.push_back({Identity(Side::Empire, "LNR Series II"), WeaponKind::Turbolaser, Next()});
  return snapshot;
}

// The full snapshot as an earlier version could hold it: no planetary batteries.
BattleSnapshot WithoutDefenses()
{
  BattleSnapshot snapshot = FullSnapshot();
  snapshot.defenses.clear();
  return snapshot;
}

void TestRoundTrip()
{
  const BattleSnapshot snapshot = FullSnapshot();
  const std::string text = SerializeSnapshot(snapshot);
  const std::optional<BattleSnapshot> parsed = ParseSnapshot(text);
  Require(parsed.has_value(), "the full snapshot parses");
  Require(*parsed == snapshot, "every field survives the round trip");
  Require(SerializeSnapshot(*parsed) == text, "serializing a parsed snapshot reproduces the text");
  Require(text.find("withdraw=0.35 ") != std::string::npos, "floats use their shortest exact form");
  Require(text.find("name=\"Rogue\\x09Squadron\\x0a\\x01\\x7f\\xe9\"") != std::string::npos,
          "control and non-ASCII bytes are escaped");
  Require(text.starts_with("rebellion-battle-snapshot 4\n"), "snapshots use version 4");
  Require(text.find("\ndefense family=") != std::string::npos && text.find("weapon=ion") != std::string::npos
              && text.find("weapon=turbolaser") != std::string::npos,
          "planetary batteries are written with their weapon kind");
  Require(text.find("class-name=\"Class \\\"quoted\\\" \\\\ name\\x09\\x0a\\x01\\x7f\\xe9\"")
              != std::string::npos,
          "class names escape quotes, backslashes, control and non-ASCII bytes");

  BattleSnapshot empty;
  Require(ParseSnapshot(SerializeSnapshot(empty)) == empty, "a snapshot without units round-trips");

  const std::string noFinalNewline = text.substr(0, text.size() - 1);
  Require(ParseSnapshot(noFinalNewline) == snapshot, "the final newline is optional");
}

void TestLocaleIndependence()
{
  const BattleSnapshot snapshot = FullSnapshot();
  const std::string reference = SerializeSnapshot(snapshot);
  for (const char* locale : {"de_DE.UTF-8", "de_DE", "fr_FR.UTF-8"}) {
    if (std::setlocale(LC_ALL, locale) != nullptr) {
      Require(SerializeSnapshot(snapshot) == reference, "a decimal-comma locale does not change the text");
      Require(ParseSnapshot(reference) == snapshot, "a decimal-comma locale does not change parsing");
      break;
    }
  }
  std::setlocale(LC_ALL, "C");
}

std::string Replace(std::string text, const std::string& from, const std::string& to)
{
  const size_t at = text.find(from);
  Require(at != std::string::npos, "the test edit applies");
  return text.replace(at, from.size(), to);
}

std::string RemoveField(std::string text, const std::string& key)
{
  const size_t at = text.find(" " + key + "=");
  Require(at != std::string::npos, "the test edit applies");
  size_t end = at + key.size() + 2;
  if (text[end] == '"') {
    ++end;
    while (text[end] != '"') {
      if (text[end] == '\\') {
        ++end;
      }
      ++end;
    }
    ++end;
  } else {
    end = text.find_first_of(" \n", end);
  }
  return text.erase(at, end - at);
}

// An earlier version's text for `snapshot`, which must have no defenses.
std::string OlderText(const BattleSnapshot& snapshot, int version)
{
  const std::string text = Replace(SerializeSnapshot(snapshot), " defenses=0", "");
  return Replace(text, "rebellion-battle-snapshot 4", "rebellion-battle-snapshot " + std::to_string(version));
}

void TestVersionOneCompatibility()
{
  BattleSnapshot expected = WithoutDefenses();
  std::string text = OlderText(expected, 1);
  Require(!ParseSnapshot(text), "version 1 rejects version 2 fields");
  for (UnitEntry& unit : expected.units) {
    std::visit([](auto& entry) {
      entry.identity.className.clear();
      entry.identity.briefingKnown = {true, true};
    }, unit);
    text = RemoveField(std::move(text), "class-name");
    text = RemoveField(std::move(text), "known-alliance");
    text = RemoveField(std::move(text), "known-empire");
  }
  Require(ParseSnapshot(text) == expected, "version 1 reads every unit kind with empty class names");
  Require(ParseSnapshot(SerializeSnapshot(*ParseSnapshot(text))) == expected,
          "version 1 snapshots can be saved and read as the current version");
  BattleSnapshot empty;
  Require(ParseSnapshot(OlderText(empty, 1)) == empty, "version 1 snapshots without units still parse");
}

void TestVersionTwoCompatibility()
{
  BattleSnapshot expected = WithoutDefenses();
  std::string text = OlderText(expected, 2);
  for (UnitEntry& unit : expected.units) {
    std::visit([](auto& entry) { entry.identity.briefingKnown = {true, true}; }, unit);
    text = RemoveField(std::move(text), "known-alliance");
    text = RemoveField(std::move(text), "known-empire");
  }
  Require(ParseSnapshot(text) == expected, "version 2 defaults briefing knowledge to full visibility");
}

void TestVersionThreeCompatibility()
{
  const BattleSnapshot expected = WithoutDefenses();
  Require(ParseSnapshot(OlderText(expected, 3)) == expected, "version 3 reads with no planetary batteries");
  const std::string withDefenses
      = Replace(SerializeSnapshot(FullSnapshot()), "rebellion-battle-snapshot 4", "rebellion-battle-snapshot 3");
  Require(!ParseSnapshot(withDefenses), "version 3 has no defense lines");
}

void TestRejectsMalformedText()
{
  const std::string text = SerializeSnapshot(FullSnapshot());
  const std::string lastLine = "end units=7 defenses=2\n";
  Require(text.ends_with(lastLine), "the unit and defense counts close the text");
  Require(!ParseSnapshot(text.substr(0, text.size() - lastLine.size())), "a truncated snapshot is rejected");
  Require(!ParseSnapshot(Replace(text, "end units=7", "end units=6")), "a wrong unit count is rejected");
  Require(!ParseSnapshot(Replace(text, "defenses=2", "defenses=1")), "a wrong defense count is rejected");
  Require(!ParseSnapshot(Replace(text, "rebellion-battle-snapshot 4", "rebellion-battle-snapshot 5")),
          "a future version is rejected");
  Require(!ParseSnapshot(Replace(text, "rebellion-battle-snapshot 4", "rebellion-battle-snapshot 0")),
          "an unsupported old version is rejected");
  Require(!ParseSnapshot(Replace(text, "weapon=ion", "weapon=proton")), "an unknown battery weapon is rejected");
  {
    // A unit after the first defense line: defenses follow every unit.
    const size_t defense = text.find("\ndefense ");
    const size_t deathStar = text.find("\ndeath-star ");
    std::string reordered = text;
    const std::string unitLine = text.substr(deathStar + 1, text.find('\n', deathStar + 1) - deathStar);
    reordered.erase(deathStar + 1, unitLine.size());
    reordered.insert(text.find('\n', defense + 1) - unitLine.size() + 1, unitLine);
    Require(!ParseSnapshot(reordered), "a unit after a defense line is rejected");
  }
  Require(!ParseSnapshot(Replace(text, " hull-max=", " hull-maximum=")), "an unknown key is rejected");
  Require(!ParseSnapshot(Replace(text, " detection=", " detection=x")), "a malformed number is rejected");
  Require(!ParseSnapshot(Replace(text, "major=1", "major=2")), "a malformed flag is rejected");
  Require(!ParseSnapshot(Replace(text, "side=alliance", "side=neutral")), "an unknown side is rejected");
  Require(!ParseSnapshot(Replace(text, "\nship ", "\nfrigate ")), "an unknown unit kind is rejected");
  Require(!ParseSnapshot(Replace(text, " crit-tractor=", " crit-tractor=1 crit-tractor=")),
          "a repeated key is rejected");
  Require(!ParseSnapshot(Replace(text, "name=\"Home One\"", "name=\"Home One")), "an unterminated name is rejected");
  Require(!ParseSnapshot(RemoveField(text, "grid-total")), "a missing key is rejected");
  Require(!ParseSnapshot(RemoveField(text, "seed")), "a missing battle key is rejected");
  Require(!ParseSnapshot(RemoveField(text, "class-name")), "version 3 requires class names");
  Require(!ParseSnapshot(""), "empty text is rejected");
}

} // namespace

int main()
{
  TestRoundTrip();
  TestLocaleIndependence();
  TestVersionOneCompatibility();
  TestVersionTwoCompatibility();
  TestVersionThreeCompatibility();
  TestRejectsMalformedText();
  std::puts("space battle snapshot tests passed");
  return 0;
}
