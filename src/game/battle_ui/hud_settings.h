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

// The battle HUD's persistent settings: auto-pause events and accessibility. They live in a SettingsStore, which the
// campaign maps onto the player's profile; the demo and the tests keep them in memory.
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

// The auto-pause events, each toggled separately.
enum class AutoPauseEvent : uint8_t {
  GravityWell,
  BelowWithdrawThreshold,
  CarrierLost,
  SuperlaserCharged,
};
inline constexpr int kAutoPauseEventCount = 4;

struct AutoPauseText {
  std::string_view key; // the settings key's last part
  std::string_view label;
  std::string_view help;
};
const AutoPauseText& Describe(AutoPauseEvent event);

struct HudSettings {
  static constexpr std::array<float, 5> kHudScales{0.85f, 1.0f, 1.15f, 1.3f, 1.5f};

  std::array<bool, kAutoPauseEventCount> autoPause{false, false, false, true};
  bool colorBlindPalette = false;
  float hudScale = 1.0f;

  bool AutoPause(AutoPauseEvent event) const
  {
    return autoPause[static_cast<size_t>(event)];
  }

  static HudSettings Load(const SettingsStore& store);
  void Save(SettingsStore& store) const;
};

} // namespace reb::battle_ui
