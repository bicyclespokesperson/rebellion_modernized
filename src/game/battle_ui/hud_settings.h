#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "space_battle/commands.h"

// The battle HUD's persistent settings (tactical_combat.md §5.7, §5.2, §12 phase 4): auto-pause events, command
// phases, doctrine presets and accessibility. They live in a SettingsStore, which the campaign maps onto the player's
// profile; the demo and the tests keep them in memory.
namespace reb::battle_ui
{

// Short string values under dotted keys ("battle.hud.scale").
class SettingsStore
{
  public:
  virtual ~SettingsStore() = default;
  virtual std::optional<std::string> Get(std::string_view key) const = 0;
  virtual void Set(std::string_view key, std::string_view value) = 0;
  virtual void Erase(std::string_view key) = 0;
};

class MemorySettingsStore : public SettingsStore
{
  public:
  std::optional<std::string> Get(std::string_view key) const override;
  void Set(std::string_view key, std::string_view value) override;
  void Erase(std::string_view key) override;
  const std::map<std::string, std::string, std::less<>>& Values() const
  {
    return m_values;
  }

  private:
  std::map<std::string, std::string, std::less<>> m_values;
};

// The auto-pause events of §5.7, each toggled separately.
enum class AutoPauseEvent : uint8_t {
  Reinforcements,
  GravityWell,
  BelowWithdrawThreshold,
  CarrierLost,
  SuperlaserCharged,
};
inline constexpr int kAutoPauseEventCount = 5;

struct AutoPauseText {
  std::string_view key; // the settings key's last part
  std::string_view label;
  std::string_view help;
};
const AutoPauseText& Describe(AutoPauseEvent event);

// A named doctrine applied in one click. `fields` are the doctrine fields it sets; the others stay as they are.
struct DoctrinePreset {
  std::string name;
  bool flight = false;
  battle::ShipDoctrine ship;
  battle::FlightDoctrine flightDoctrine;
  battle::DoctrineOverrideMask fields = 0;
  bool builtIn = false;
};

// The named defaults of §5.2: Hammer, Screen, Strike package, Net, Hold the line.
const std::vector<DoctrinePreset>& BuiltInPresets();
// The fields a saved preset records: every setting on the card.
battle::DoctrineOverrideMask PresetFields(bool flight);

std::string SerializePreset(const DoctrinePreset& preset);
std::optional<DoctrinePreset> ParsePreset(std::string_view text);

struct HudSettings {
  static constexpr std::array<int, 4> kPhaseChoices{0, 10, 20, 30};
  static constexpr std::array<float, 5> kHudScales{0.85f, 1.0f, 1.15f, 1.3f, 1.5f};
  static constexpr int kMaxPresets = 24;

  std::array<bool, kAutoPauseEventCount> autoPause{true, true, true, true, true};
  int commandPhaseSeconds = 0; // 0 = off; otherwise pause every this many seconds of battle time
  bool colorBlindPalette = false;
  float hudScale = 1.0f;
  std::vector<DoctrinePreset> presets; // the player's own

  bool AutoPause(AutoPauseEvent event) const
  {
    return autoPause[static_cast<size_t>(event)];
  }

  static HudSettings Load(const SettingsStore& store);
  void Save(SettingsStore& store) const;
};

} // namespace reb::battle_ui
