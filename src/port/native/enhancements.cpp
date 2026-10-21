#include "app/enhancements.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <string>

#include "port/minwin_impl/config/config.h"

namespace
{

constexpr const char* kEnhancementsKey = "enhancements";

std::optional<bool> g_processOverride;

} // namespace

bool ParseEnhancementsSetting(std::string_view value)
{
  std::string lowered(value);
  std::ranges::transform(lowered, lowered.begin(), [](unsigned char c) { return std::tolower(c); });
  constexpr std::array<std::string_view, 4> kOff = {"0", "false", "off", "no"};
  return std::ranges::find(kOff, lowered) == kOff.end();
}

bool EnhancementsEnabled()
{
  if (g_processOverride) {
    return *g_processOverride;
  }
  return ParseEnhancementsSetting(mw::config::Global().Get(kEnhancementsKey, "1"));
}

bool SetEnhancementsEnabled(bool enabled)
{
  if (g_processOverride) {
    g_processOverride = enabled;
  }
  mw::config::Config& config = mw::config::Global();
  config.Set(kEnhancementsKey, enabled ? "1" : "0");
  return config.Save(mw::config::GlobalPath());
}

void OverrideEnhancementsForProcess(bool enabled)
{
  g_processOverride = enabled;
}
