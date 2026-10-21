#include "space_battle/snapshot.h"

#include <algorithm>
#include <charconv>
#include <locale>
#include <map>
#include <sstream>
#include <type_traits>

// The text form: a header line, a `battle` line, one line per side, one line per unit in registration order and an
// `end` line carrying the unit count (so a truncated file does not parse). Every line is a keyword followed by
// `key=value` fields; text values are quoted with backslash escapes. Each entry type's field list is written once
// (the *Fields functions below) and used for both directions, so writer and parser cannot drift apart.
namespace reb::battle
{
namespace
{

constexpr std::string_view kMagic = "rebellion-battle-snapshot";
constexpr std::array<std::string_view, kArcCount> kArcKeys{"fore", "aft", "port", "starboard"};

struct ParseError {};

std::string_view SideName(Side side)
{
  return side == Side::Alliance ? "alliance" : "empire";
}

constexpr std::array<std::string_view, kWeaponKinds> kWeaponNames{"turbolaser", "ion", "laser"};

// Floats keep the shortest decimal form that reads back to the same value, independent of the C locale (the game
// calls setlocale from the environment).
std::string FormatFloat(float value)
{
  for (int precision = 1;; ++precision) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out.precision(precision);
    out << value;
    std::istringstream back(out.str());
    back.imbue(std::locale::classic());
    float parsed = 0.0f;
    back >> parsed;
    if (parsed == value || precision >= 9) {
      return out.str();
    }
  }
}

float ParseFloat(std::string_view text)
{
  std::istringstream in{std::string(text)};
  in.imbue(std::locale::classic());
  float value = 0.0f;
  in >> value;
  if (in.fail() || !in.eof()) {
    throw ParseError{};
  }
  return value;
}

template <class Integer>
Integer ParseInteger(std::string_view text)
{
  int base = 10;
  if (text.starts_with("0x")) {
    text.remove_prefix(2);
    base = 16;
  }
  Integer value{};
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, base);
  if (error != std::errc{} || end != text.data() + text.size() || text.empty()) {
    throw ParseError{};
  }
  return value;
}

std::string QuoteText(std::string_view text)
{
  constexpr std::string_view kDigits = "0123456789abcdef";
  std::string out = "\"";
  for (const char c : text) {
    const auto byte = static_cast<unsigned char>(c);
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (byte >= 0x20 && byte < 0x7f) {
      out += c;
    } else {
      out += "\\x";
      out += kDigits[byte >> 4];
      out += kDigits[byte & 0xf];
    }
  }
  out += '"';
  return out;
}

// ---- one line: a keyword and its fields --------------------------------------------------------------------------

struct Line {
  std::string keyword;
  std::map<std::string, std::string, std::less<>> fields;
};

std::string ReadQuoted(std::string_view& rest)
{
  std::string text;
  rest.remove_prefix(1);
  while (!rest.empty() && rest.front() != '"') {
    char c = rest.front();
    rest.remove_prefix(1);
    if (c == '\\') {
      if (rest.empty()) {
        throw ParseError{};
      }
      c = rest.front();
      rest.remove_prefix(1);
      if (c == 'x') {
        if (rest.size() < 2) {
          throw ParseError{};
        }
        c = static_cast<char>(ParseInteger<uint8_t>("0x" + std::string(rest.substr(0, 2))));
        rest.remove_prefix(2);
      } else if (c != '"' && c != '\\') {
        throw ParseError{};
      }
    }
    text += c;
  }
  if (rest.empty()) {
    throw ParseError{};
  }
  rest.remove_prefix(1);
  return text;
}

Line SplitLine(std::string_view text)
{
  Line line;
  const size_t keywordEnd = text.find(' ');
  line.keyword = std::string(text.substr(0, keywordEnd));
  std::string_view rest = keywordEnd == std::string_view::npos ? std::string_view{} : text.substr(keywordEnd + 1);
  while (!rest.empty()) {
    const size_t equals = rest.find('=');
    if (equals == std::string_view::npos || equals == 0) {
      throw ParseError{};
    }
    std::string key(rest.substr(0, equals));
    rest.remove_prefix(equals + 1);
    std::string value;
    if (rest.starts_with('"')) {
      value = ReadQuoted(rest);
    } else {
      const size_t valueEnd = rest.find(' ');
      value = std::string(rest.substr(0, valueEnd));
      rest.remove_prefix(valueEnd == std::string_view::npos ? rest.size() : valueEnd);
    }
    if (!rest.empty()) {
      if (rest.front() != ' ') {
        throw ParseError{};
      }
      rest.remove_prefix(1);
    }
    if (!line.fields.emplace(std::move(key), std::move(value)).second) {
      throw ParseError{};
    }
  }
  return line;
}

// ---- the two directions of a field list --------------------------------------------------------------------------

class Writer
{
  public:
  Writer(std::string& out, std::string_view keyword)
      : m_out(out)
  {
    m_out += keyword;
  }
  ~Writer()
  {
    m_out += '\n';
  }
  Writer(const Writer&) = delete;
  Writer& operator=(const Writer&) = delete;

  int Version() const
  {
    return BattleSnapshot::kVersion;
  }

  void Int(std::string_view key, int32_t value)
  {
    Field(key, std::to_string(value));
  }
  void Unsigned(std::string_view key, uint32_t value)
  {
    Field(key, std::to_string(value));
  }
  void Hex(std::string_view key, uint32_t value)
  {
    std::array<char, 16> digits{};
    const auto end = std::to_chars(digits.data(), digits.data() + digits.size(), value, 16).ptr;
    Field(key, "0x" + std::string(digits.data(), end));
  }
  void Flag(std::string_view key, bool value)
  {
    Field(key, value ? "1" : "0");
  }
  void Real(std::string_view key, float value)
  {
    Field(key, FormatFloat(value));
  }
  void Text(std::string_view key, const std::string& value)
  {
    Field(key, QuoteText(value));
  }
  void SideOf(std::string_view key, Side value)
  {
    Field(key, std::string(SideName(value)));
  }
  void Weapon(std::string_view key, WeaponKind value)
  {
    Field(key, std::string(kWeaponNames[static_cast<size_t>(value)]));
  }
  void Triple(std::string_view key, const std::array<int32_t, kWeaponKinds>& values)
  {
    Field(key, std::to_string(values[0]) + "/" + std::to_string(values[1]) + "/" + std::to_string(values[2]));
  }

  private:
  void Field(std::string_view key, const std::string& value)
  {
    m_out += ' ';
    m_out += key;
    m_out += '=';
    m_out += value;
  }

  std::string& m_out;
};

class Reader
{
  public:
  explicit Reader(Line line, int version = BattleSnapshot::kVersion)
      : m_line(std::move(line))
      , m_version(version)
  {
  }

  int Version() const
  {
    return m_version;
  }

  void Int(std::string_view key, int32_t& value)
  {
    value = ParseInteger<int32_t>(Take(key));
  }
  void Unsigned(std::string_view key, uint32_t& value)
  {
    value = ParseInteger<uint32_t>(Take(key));
  }
  void Hex(std::string_view key, uint32_t& value)
  {
    const std::string text = Take(key);
    if (!text.starts_with("0x")) {
      throw ParseError{};
    }
    value = ParseInteger<uint32_t>(text);
  }
  void Flag(std::string_view key, bool& value)
  {
    const std::string text = Take(key);
    if (text != "0" && text != "1") {
      throw ParseError{};
    }
    value = text == "1";
  }
  void Real(std::string_view key, float& value)
  {
    value = ParseFloat(Take(key));
  }
  void Text(std::string_view key, std::string& value)
  {
    value = Take(key);
  }
  void SideOf(std::string_view key, Side& value)
  {
    const std::string text = Take(key);
    if (text == SideName(Side::Alliance)) {
      value = Side::Alliance;
    } else if (text == SideName(Side::Empire)) {
      value = Side::Empire;
    } else {
      throw ParseError{};
    }
  }
  void Weapon(std::string_view key, WeaponKind& value)
  {
    const std::string text = Take(key);
    const auto found = std::find(kWeaponNames.begin(), kWeaponNames.end(), text);
    if (found == kWeaponNames.end()) {
      throw ParseError{};
    }
    value = static_cast<WeaponKind>(found - kWeaponNames.begin());
  }
  void Triple(std::string_view key, std::array<int32_t, kWeaponKinds>& values)
  {
    const std::string value = Take(key);
    std::string_view text = value;
    for (int kind = 0; kind < kWeaponKinds; ++kind) {
      const size_t slash = text.find('/');
      if ((slash == std::string_view::npos) != (kind == kWeaponKinds - 1)) {
        throw ParseError{};
      }
      values[kind] = ParseInteger<int32_t>(text.substr(0, slash));
      text.remove_prefix(slash == std::string_view::npos ? text.size() : slash + 1);
    }
  }

  // Every field must have been consumed: an unknown key means a format this parser does not understand.
  void Finish() const
  {
    if (!m_line.fields.empty()) {
      throw ParseError{};
    }
  }

  private:
  std::string Take(std::string_view key)
  {
    const auto found = m_line.fields.find(key);
    if (found == m_line.fields.end()) {
      throw ParseError{};
    }
    std::string value = std::move(found->second);
    m_line.fields.erase(found);
    return value;
  }

  Line m_line;
  int m_version;
};

// ---- field lists (Entry is const when writing) -------------------------------------------------------------------

template <class Io, class Identity>
void IdentityFields(Io& io, Identity& identity)
{
  io.Hex("family", identity.family);
  io.Hex("id", identity.id);
  io.SideOf("side", identity.side);
  io.Hex("type-bits", identity.typeBits);
  io.Int("type-index", identity.typeIndex);
  io.Text("name", identity.name);
  if (io.Version() >= 2) {
    io.Text("class-name", identity.className);
  }
  if (io.Version() >= 3) {
    io.Flag("known-alliance", identity.briefingKnown[0]);
    io.Flag("known-empire", identity.briefingKnown[1]);
  }
}

template <class Io, class Grid>
void GridFields(Io& io, Grid& grid)
{
  for (int arc = 0; arc < kArcCount; ++arc) {
    io.Triple(kArcKeys[arc], grid.points[arc]);
  }
  io.Triple("range", grid.range);
}

template <class Io, class Criticals>
void CriticalFields(Io& io, Criticals& criticals)
{
  io.Int("crit-speed", criticals.speed);
  io.Int("crit-shield", criticals.shieldRecharge);
  io.Int("crit-weapon", criticals.weaponRecharge);
  io.Int("crit-tractor", criticals.tractor);
  io.Int("crit-hyperdrive", criticals.hyperdrivePrimary);
  io.Int("crit-hyperdrive-backup", criticals.hyperdriveBackup);
}

template <class Io, class Entry>
void ShipFields(Io& io, Entry& ship)
{
  IdentityFields(io, ship.identity);
  io.Int("hull-max", ship.hullMax);
  io.Int("hull", ship.hull);
  io.Int("shield-max", ship.shieldMax);
  io.Int("sublight", ship.sublight);
  io.Int("maneuverability", ship.maneuverability);
  io.Int("detection", ship.detection);
  io.Int("hyperdrive", ship.hyperdrive);
  io.Int("hyperdrive-backup", ship.hyperdriveBackup);
  io.Int("shield-recharge", ship.shieldRecharge);
  io.Int("weapon-recharge", ship.weaponRecharge);
  io.Int("damage-control", ship.damageControl);
  io.Int("tractor-power", ship.tractorPower);
  io.Int("tractor-range", ship.tractorRange);
  io.Int("gravity-well", ship.gravityWell);
  io.Int("interdiction", ship.interdiction);
  io.Int("fighter-capacity", ship.fighterCapacity);
  io.Int("hangar-free", ship.hangarFree);
  io.Int("troop-capacity", ship.troopCapacity);
  io.Int("grid-total", ship.gridTotal);
  GridFields(io, ship.weapons);
  CriticalFields(io, ship.criticals);
}

template <class Io, class Entry>
void SquadronFields(Io& io, Entry& squadron)
{
  IdentityFields(io, squadron.identity);
  io.Int("carrier", squadron.carrier);
  io.Int("squad-size", squadron.squadSize);
  io.Int("current-size", squadron.currentSize);
  io.Int("shield", squadron.shieldPerCraft);
  io.Int("sublight", squadron.sublight);
  io.Int("maneuverability", squadron.maneuverability);
  io.Int("detection", squadron.detection);
  io.Int("hyperdrive", squadron.hyperdrive);
  io.Int("hyperdrive-backup", squadron.hyperdriveBackup);
  io.Int("torpedoes", squadron.torpedoesPerCraft);
  io.Int("torpedo-range", squadron.torpedoRange);
  GridFields(io, squadron.weaponsPerCraft);
}

template <class Io, class Entry>
void CharacterFields(Io& io, Entry& character)
{
  IdentityFields(io, character.identity);
  io.Int("ship", character.ship);
  io.Flag("major", character.major);
  io.Flag("admiral", character.admiral);
  io.Flag("commander", character.commander);
  io.Flag("injured", character.injured);
  io.Int("leadership", character.leadership);
  io.Int("combat", character.combat);
}

template <class Io, class Entry>
void PassengerFields(Io& io, Entry& passenger)
{
  IdentityFields(io, passenger.identity);
  io.Int("ship", passenger.ship);
  io.Flag("troops", passenger.troopRegiment);
}

template <class Io, class Entry>
void DeathStarFields(Io& io, Entry& deathStar)
{
  IdentityFields(io, deathStar.identity);
  io.Flag("completed", deathStar.completed);
  io.Flag("shield-facility", deathStar.shieldFacility);
  io.Int("hull-max", deathStar.hullMax);
  io.Int("hull", deathStar.hull);
  io.Int("hangar-free", deathStar.hangarFree);
}

template <class Io, class Entry>
void DefenseFields(Io& io, Entry& defense)
{
  IdentityFields(io, defense.identity);
  io.Weapon("weapon", defense.weapon);
  io.Int("attack", defense.attackStrength);
}

template <class Io, class Params>
void BattleFields(Io& io, Params& params)
{
  io.Text("system", params.systemName);
  io.Unsigned("picture", params.systemPictureId);
  io.Flag("destroyed", params.systemDestroyed);
  io.SideOf("held-first", params.heldFirstBy);
  io.Int("control", params.controlSide);
  io.Flag("opponent-ai", params.opponentIsAi);
  io.Int("human-is-not-alliance", params.humanIsNotAlliance);
  io.Unsigned("seed", params.seed);
}

template <class Io, class Params>
void SideFields(Io& io, Params& side)
{
  io.Real("withdraw", side.withdrawThreshold);
  io.Flag("can-retreat", side.canRetreat);
  io.Real("retreat-days", side.retreatDays);
  io.Flag("human", side.human);
}

// The keyword of each UnitEntry alternative, in variant order.
constexpr std::array<std::string_view, std::variant_size_v<UnitEntry>> kUnitKeywords{"ship", "squadron", "character",
                                                                                     "passenger", "death-star"};
constexpr std::string_view kDefenseKeyword = "defense";

template <class Io, class Entry>
void UnitFields(Io& io, Entry& entry)
{
  using Plain = std::remove_const_t<Entry>;
  if constexpr (std::is_same_v<Plain, CapitalShipEntry>) {
    ShipFields(io, entry);
  } else if constexpr (std::is_same_v<Plain, SquadronEntry>) {
    SquadronFields(io, entry);
  } else if constexpr (std::is_same_v<Plain, CharacterEntry>) {
    CharacterFields(io, entry);
  } else if constexpr (std::is_same_v<Plain, PassengerEntry>) {
    PassengerFields(io, entry);
  } else {
    static_assert(std::is_same_v<Plain, DeathStarEntry>);
    DeathStarFields(io, entry);
  }
}

template <size_t Index = 0>
UnitEntry ReadUnit(Reader& reader, std::string_view keyword)
{
  if constexpr (Index == std::variant_size_v<UnitEntry>) {
    throw ParseError{};
  } else {
    if (keyword != kUnitKeywords[Index]) {
      return ReadUnit<Index + 1>(reader, keyword);
    }
    std::variant_alternative_t<Index, UnitEntry> entry;
    UnitFields(reader, entry);
    return entry;
  }
}

// Splits on '\n', accepting a final line with or without its newline.
std::vector<std::string_view> SplitLines(std::string_view text)
{
  std::vector<std::string_view> lines;
  while (!text.empty()) {
    const size_t end = text.find('\n');
    lines.push_back(text.substr(0, end));
    text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
  }
  return lines;
}

BattleSnapshot Parse(std::string_view text)
{
  const std::vector<std::string_view> lines = SplitLines(text);
  if (lines.size() < 5) {
    throw ParseError{};
  }
  int version = 0;
  for (int candidate = 1; candidate <= BattleSnapshot::kVersion; ++candidate) {
    if (lines[0] == std::string(kMagic) + " " + std::to_string(candidate)) {
      version = candidate;
    }
  }
  if (version == 0) {
    throw ParseError{};
  }
  BattleSnapshot snapshot;
  const auto readLine = [&](size_t index, std::string_view keyword, auto&& fields) {
    Line line = SplitLine(lines[index]);
    if (line.keyword != keyword) {
      throw ParseError{};
    }
    Reader reader(std::move(line), version);
    fields(reader);
    reader.Finish();
  };
  readLine(1, "battle", [&](Reader& reader) { BattleFields(reader, snapshot.params); });
  readLine(2, SideName(Side::Alliance), [&](Reader& reader) { SideFields(reader, snapshot.params.sides[0]); });
  readLine(3, SideName(Side::Empire), [&](Reader& reader) { SideFields(reader, snapshot.params.sides[1]); });
  const size_t unitEnd = lines.size() - 1;
  for (size_t index = 4; index < unitEnd; ++index) {
    Line line = SplitLine(lines[index]);
    const std::string keyword = line.keyword;
    Reader reader(std::move(line), version);
    if (keyword == kDefenseKeyword && version >= 4) {
      DefenseFields(reader, snapshot.defenses.emplace_back());
    } else if (!snapshot.defenses.empty()) {
      throw ParseError{}; // defenses follow every unit
    } else {
      snapshot.units.push_back(ReadUnit(reader, keyword));
    }
    reader.Finish();
  }
  int32_t count = -1;
  int32_t defenses = 0;
  readLine(unitEnd, "end", [&](Reader& reader) {
    reader.Int("units", count);
    if (version >= 4) {
      reader.Int("defenses", defenses);
    }
  });
  if (count != static_cast<int32_t>(snapshot.units.size()) || defenses != static_cast<int32_t>(snapshot.defenses.size())) {
    throw ParseError{};
  }
  return snapshot;
}

} // namespace

std::string SerializeSnapshot(const BattleSnapshot& snapshot)
{
  std::string out = std::string(kMagic) + " " + std::to_string(BattleSnapshot::kVersion) + "\n";
  {
    Writer writer(out, "battle");
    BattleFields(writer, snapshot.params);
  }
  for (int side = 0; side < 2; ++side) {
    Writer writer(out, SideName(static_cast<Side>(side)));
    SideFields(writer, snapshot.params.sides[side]);
  }
  for (const UnitEntry& unit : snapshot.units) {
    Writer writer(out, kUnitKeywords[unit.index()]);
    std::visit([&](const auto& entry) { UnitFields(writer, entry); }, unit);
  }
  for (const DefenseEntry& defense : snapshot.defenses) {
    Writer writer(out, kDefenseKeyword);
    DefenseFields(writer, defense);
  }
  Writer writer(out, "end");
  writer.Int("units", static_cast<int32_t>(snapshot.units.size()));
  writer.Int("defenses", static_cast<int32_t>(snapshot.defenses.size()));
  return out;
}

std::optional<BattleSnapshot> ParseSnapshot(std::string_view text)
{
  try {
    return Parse(text);
  } catch (const ParseError&) {
    return std::nullopt;
  }
}

} // namespace reb::battle
