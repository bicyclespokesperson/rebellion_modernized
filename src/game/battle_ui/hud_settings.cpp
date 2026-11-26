#include "battle_ui/hud_settings.h"

#include <algorithm>
#include <charconv>

namespace reb::battle_ui
{
namespace
{

constexpr std::string_view kPrefix = "battle.hud.";

constexpr std::array<AutoPauseText, kAutoPauseEventCount> kAutoPauseTexts{{
    {"gravity_well", "Gravity well active", "Pause when an enemy gravity-well projector enters the battle, stopping all your jumps."},
    {"withdraw_threshold", "Ship below withdraw threshold",
     "Pause when one of your ships falls below its doctrine's withdrawal threshold."},
    {"carrier_lost", "Carrier lost", "Pause when one of your carriers is destroyed."},
    {"superlaser", "Superlaser charged", "Pause when a Death Star superlaser finishes charging."},
}};

std::string Key(std::string_view name)
{
  return std::string(kPrefix) + std::string(name);
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

HudSettings HudSettings::Load(const SettingsStore& store)
{
  HudSettings settings;
  for (int i = 0; i < kAutoPauseEventCount; ++i) {
    if (const auto value = store.Get(Key("autopause.") + std::string(kAutoPauseTexts[i].key))) {
      settings.autoPause[i] = *value == "1";
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
  return settings;
}

void HudSettings::Save(SettingsStore& store) const
{
  for (int i = 0; i < kAutoPauseEventCount; ++i) {
    store.Set(Key("autopause.") + std::string(kAutoPauseTexts[i].key), autoPause[i] ? "1" : "0");
  }
  store.Set(Key("palette"), colorBlindPalette ? "colorblind" : "standard");
  store.Set(Key("scale_percent"), std::to_string(static_cast<int>(hudScale * 100.0f + 0.5f)));
}

} // namespace reb::battle_ui
