#include "battle_ui/hud_settings.h"

#include <algorithm>
#include <charconv>

#include "battle_ui/doctrine_text.h"

namespace reb::battle_ui
{
namespace
{

using battle::TargetTag;

constexpr std::string_view kPrefix = "battle.hud.";

constexpr std::array<AutoPauseText, kAutoPauseEventCount> kAutoPauseTexts{{
    {"reinforcements", "Enemy reinforcements", "Pause when enemy reinforcements arrive."},
    {"gravity_well", "Gravity well active", "Pause when an enemy gravity well comes on, trapping ships inside it."},
    {"withdraw_threshold", "Ship below withdraw threshold",
     "Pause when one of your ships falls below its doctrine's withdrawal threshold."},
    {"carrier_lost", "Carrier lost", "Pause when one of your carriers is destroyed."},
    {"superlaser", "Superlaser charged", "Pause when a Death Star superlaser finishes charging."},
}};

std::string Key(std::string_view name)
{
  return std::string(kPrefix) + std::string(name);
}

battle::TargetPriorities Priorities(std::initializer_list<TargetTag> tags, bool finishDamaged = false)
{
  battle::TargetPriorities priorities;
  priorities.count = 0;
  for (TargetTag tag : tags) {
    priorities.tags[priorities.count++] = tag;
  }
  priorities.finishDamaged = finishDamaged;
  return priorities;
}

std::vector<DoctrinePreset> MakeBuiltInPresets()
{
  std::vector<DoctrinePreset> presets;
  {
    DoctrinePreset hammer{"Hammer", false};
    hammer.ship.role = battle::Role::Line;
    hammer.ship.range = battle::EngagementRange::Close;
    hammer.ship.approach = battle::Approach::Direct;
    hammer.ship.priorities = Priorities({TargetTag::Heavy, TargetTag::Medium, TargetTag::Damaged}, true);
    hammer.ship.allocation = battle::FireAllocation::Concentrate;
    hammer.ship.power = battle::PowerRouting::Weapons;
    hammer.ship.formation = battle::Formation::Wedge;
    hammer.fields = battle::kShipRole | battle::kShipRange | battle::kShipApproach | battle::kShipPriorities
                    | battle::kShipAllocation | battle::kShipPower | battle::kShipFormation;
    presets.push_back(hammer);
  }
  {
    DoctrinePreset screen{"Screen", false};
    screen.ship.role = battle::Role::Screen;
    screen.ship.range = battle::EngagementRange::Close;
    screen.ship.priorities = Priorities({TargetTag::Bomber, TargetTag::Fighter, TargetTag::Threatening});
    screen.ship.allocation = battle::FireAllocation::Spread;
    screen.ship.formation = battle::Formation::Ring;
    screen.fields = battle::kShipRole | battle::kShipRange | battle::kShipPriorities | battle::kShipAllocation
                    | battle::kShipFormation;
    presets.push_back(screen);
  }
  {
    DoctrinePreset strike{"Strike package", true};
    strike.flightDoctrine.mission = battle::FlightMission::Strike;
    strike.flightDoctrine.priorities = Priorities({TargetTag::Carrier, TargetTag::Transport, TargetTag::ShieldsDown});
    strike.flightDoctrine.dogfight = battle::DogfightRule::AvoidUnlessAttacked;
    strike.flightDoctrine.rearm = battle::RearmRule::WhenTorpedoesSpent;
    strike.flightDoctrine.launch = battle::LaunchRule::Immediately;
    strike.fields = PresetFields(true);
    presets.push_back(strike);
  }
  {
    DoctrinePreset net{"Net", false};
    net.ship.role = battle::Role::Flank;
    net.ship.range = battle::EngagementRange::Optimal;
    net.ship.approach = battle::Approach::Pincer;
    net.ship.priorities = Priorities({TargetTag::Withdrawing, TargetTag::Damaged, TargetTag::Nearest});
    net.ship.subsystem = battle::Subsystem::Hyperdrive;
    net.ship.formation = battle::Formation::Loose;
    net.fields = battle::kShipRole | battle::kShipRange | battle::kShipApproach | battle::kShipPriorities
                 | battle::kShipSubsystem | battle::kShipFormation;
    presets.push_back(net);
  }
  {
    DoctrinePreset hold{"Hold the line", false};
    hold.ship.role = battle::Role::Line;
    hold.ship.range = battle::EngagementRange::Optimal;
    hold.ship.approach = battle::Approach::Hold;
    hold.ship.facing = battle::Facing::StrongestToThreat;
    hold.ship.power = battle::PowerRouting::Shields;
    hold.ship.withdraw = battle::WithdrawRule::Never;
    hold.ship.formation = battle::Formation::LineAbreast;
    hold.fields = battle::kShipRole | battle::kShipRange | battle::kShipApproach | battle::kShipFacing
                  | battle::kShipPower | battle::kShipWithdraw | battle::kShipFormation;
    presets.push_back(hold);
  }
  for (DoctrinePreset& preset : presets) {
    preset.builtIn = true;
  }
  return presets;
}

std::vector<std::string_view> Split(std::string_view text, char separator)
{
  std::vector<std::string_view> parts;
  size_t start = 0;
  while (start <= text.size()) {
    const size_t end = text.find(separator, start);
    parts.push_back(text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
    if (end == std::string_view::npos) {
      break;
    }
    start = end + 1;
  }
  return parts;
}

std::optional<int64_t> ParseInt(std::string_view text)
{
  int64_t value = 0;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size()) {
    return std::nullopt;
  }
  return value;
}

std::span<const DoctrineField> ChoiceFields(bool flight)
{
  static constexpr std::array kShip{DoctrineField::Role,   DoctrineField::Range,      DoctrineField::Approach,
                                    DoctrineField::Facing, DoctrineField::Allocation, DoctrineField::Subsystem,
                                    DoctrineField::Power,  DoctrineField::Withdraw,   DoctrineField::Formation};
  static constexpr std::array kFlight{DoctrineField::Mission, DoctrineField::Dogfight, DoctrineField::Rearm,
                                      DoctrineField::Launch};
  if (flight) {
    return kFlight;
  }
  return kShip;
}

} // namespace

std::optional<std::string> MemorySettingsStore::Get(std::string_view key) const
{
  const auto it = m_values.find(key);
  return it == m_values.end() ? std::nullopt : std::optional<std::string>(it->second);
}

void MemorySettingsStore::Set(std::string_view key, std::string_view value)
{
  m_values.insert_or_assign(std::string(key), std::string(value));
}

void MemorySettingsStore::Erase(std::string_view key)
{
  if (const auto it = m_values.find(key); it != m_values.end()) {
    m_values.erase(it);
  }
}

const AutoPauseText& Describe(AutoPauseEvent event)
{
  return kAutoPauseTexts[static_cast<size_t>(event)];
}

const std::vector<DoctrinePreset>& BuiltInPresets()
{
  static const std::vector<DoctrinePreset> presets = MakeBuiltInPresets();
  return presets;
}

battle::DoctrineOverrideMask PresetFields(bool flight)
{
  battle::DoctrineOverrideMask fields = 0;
  for (DoctrineField field : flight ? FlightFields() : ShipFields()) {
    fields |= FieldMask(field, flight);
  }
  return fields;
}

// "flight=0;fields=1023;options=0,1,2,0,1,0,1,1,0;tags=6,8;finish=1;name=Hammer": the name is last and may contain
// any character.
std::string SerializePreset(const DoctrinePreset& preset)
{
  std::string options;
  for (DoctrineField field : ChoiceFields(preset.flight)) {
    options += (options.empty() ? "" : ",") + std::to_string(GetOption(field, preset.ship, preset.flightDoctrine));
  }
  const battle::TargetPriorities& priorities
      = preset.flight ? preset.flightDoctrine.priorities : preset.ship.priorities;
  std::string tags;
  for (int i = 0; i < priorities.count; ++i) {
    tags += (i ? "," : "") + std::to_string(static_cast<int>(priorities.tags[i]));
  }
  return "flight=" + std::to_string(preset.flight ? 1 : 0) + ";fields=" + std::to_string(preset.fields) + ";options="
         + options + ";tags=" + tags + ";finish=" + (priorities.finishDamaged ? "1" : "0") + ";name=" + preset.name;
}

std::optional<DoctrinePreset> ParsePreset(std::string_view text)
{
  DoctrinePreset preset;
  std::optional<std::string_view> options;
  std::optional<std::string_view> tags;
  bool finish = false;
  bool named = false;
  while (!text.empty() && !named) {
    const size_t separator = text.find(';');
    std::string_view part = text.substr(0, separator);
    const size_t equals = part.find('=');
    if (equals == std::string_view::npos) {
      return std::nullopt;
    }
    const std::string_view key = part.substr(0, equals);
    std::string_view value = part.substr(equals + 1);
    if (key == "name") {
      value = text.substr(equals + 1);
      preset.name = std::string(value);
      named = true;
      break;
    }
    if (key == "flight") {
      preset.flight = value == "1";
    } else if (key == "fields") {
      const std::optional<int64_t> fields = ParseInt(value);
      if (!fields || *fields < 0) {
        return std::nullopt;
      }
      preset.fields = static_cast<battle::DoctrineOverrideMask>(*fields);
    } else if (key == "options") {
      options = value;
    } else if (key == "tags") {
      tags = value;
    } else if (key == "finish") {
      finish = value == "1";
    }
    if (separator == std::string_view::npos) {
      break;
    }
    text.remove_prefix(separator + 1);
  }
  if (!named || preset.name.empty() || !options) {
    return std::nullopt;
  }
  const std::span<const DoctrineField> fields = ChoiceFields(preset.flight);
  const std::vector<std::string_view> values = Split(*options, ',');
  if (values.size() != fields.size()) {
    return std::nullopt;
  }
  for (size_t i = 0; i < fields.size(); ++i) {
    const std::optional<int64_t> value = ParseInt(values[i]);
    if (!value || *value < 0 || *value >= OptionCount(fields[i])) {
      return std::nullopt;
    }
    SetOption(fields[i], static_cast<int>(*value), preset.ship, preset.flightDoctrine);
  }
  battle::TargetPriorities priorities;
  priorities.count = 0;
  if (tags && !tags->empty()) {
    for (std::string_view tag : Split(*tags, ',')) {
      const std::optional<int64_t> value = ParseInt(tag);
      if (!value || *value < 0 || *value >= kTargetTagCount || priorities.count >= battle::TargetPriorities::kMaxTags) {
        return std::nullopt;
      }
      priorities.tags[priorities.count++] = static_cast<TargetTag>(*value);
    }
  }
  priorities.finishDamaged = finish;
  (preset.flight ? preset.flightDoctrine.priorities : preset.ship.priorities) = priorities;
  return preset;
}

HudSettings HudSettings::Load(const SettingsStore& store)
{
  HudSettings settings;
  for (int i = 0; i < kAutoPauseEventCount; ++i) {
    if (const auto value = store.Get(Key("autopause.") + std::string(kAutoPauseTexts[i].key))) {
      settings.autoPause[i] = *value == "1";
    }
  }
  if (const auto value = store.Get(Key("command_phase"))) {
    const std::optional<int64_t> seconds = ParseInt(*value);
    if (seconds && std::ranges::contains(kPhaseChoices, static_cast<int>(*seconds))) {
      settings.commandPhaseSeconds = static_cast<int>(*seconds);
    }
  }
  if (const auto value = store.Get(Key("palette"))) {
    settings.colorBlindPalette = *value == "colorblind";
  }
  if (const auto value = store.Get(Key("scale_percent"))) {
    if (const std::optional<int64_t> percent = ParseInt(*value)) {
      const auto match = std::ranges::find_if(kHudScales, [&](float scale) {
        return static_cast<int>(scale * 100.0f + 0.5f) == *percent;
      });
      if (match != kHudScales.end()) {
        settings.hudScale = *match;
      }
    }
  }
  if (const auto value = store.Get(Key("presets"))) {
    const std::optional<int64_t> count = ParseInt(*value);
    for (int i = 0; count && i < std::min<int64_t>(*count, kMaxPresets); ++i) {
      if (const auto text = store.Get(Key("preset.") + std::to_string(i))) {
        if (std::optional<DoctrinePreset> preset = ParsePreset(*text)) {
          settings.presets.push_back(std::move(*preset));
        }
      }
    }
  }
  return settings;
}

void HudSettings::Save(SettingsStore& store) const
{
  for (int i = 0; i < kAutoPauseEventCount; ++i) {
    store.Set(Key("autopause.") + std::string(kAutoPauseTexts[i].key), autoPause[i] ? "1" : "0");
  }
  store.Set(Key("command_phase"), std::to_string(commandPhaseSeconds));
  store.Set(Key("palette"), colorBlindPalette ? "colorblind" : "standard");
  store.Set(Key("scale_percent"), std::to_string(static_cast<int>(hudScale * 100.0f + 0.5f)));
  int previous = 0;
  if (const auto value = store.Get(Key("presets"))) {
    previous = static_cast<int>(ParseInt(*value).value_or(0));
  }
  const int count = std::min(static_cast<int>(presets.size()), kMaxPresets);
  for (int i = 0; i < count; ++i) {
    store.Set(Key("preset.") + std::to_string(i), SerializePreset(presets[i]));
  }
  for (int i = count; i < previous; ++i) {
    store.Erase(Key("preset.") + std::to_string(i));
  }
  store.Set(Key("presets"), std::to_string(count));
}

} // namespace reb::battle_ui
