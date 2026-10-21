#pragma once
#include <string>

#include "battle_ui/hud_settings.h"
#include "port/minwin_impl/config/config.h"

namespace reb::native::space_battle
{

class ConfigSettingsStore : public battle_ui::SettingsStore
{
  public:
  ConfigSettingsStore();
  ConfigSettingsStore(mw::config::Config& config, std::string path);

  std::optional<std::string> Get(std::string_view key) const override;
  void Set(std::string_view key, std::string_view value) override;
  void Erase(std::string_view key) override;

  private:
  mw::config::Config& m_config;
  std::string m_path;
};

} // namespace reb::native::space_battle
