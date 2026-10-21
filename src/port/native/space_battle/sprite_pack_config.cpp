#include "port/minwin_impl/config/config.h"
#include "port/native/space_battle/sprite_pack.h"

#include <cstdlib>

namespace reb::battle_sprites
{

std::filesystem::path ConfiguredSpritePackDirectory()
{
  if (const char* disabled = std::getenv("REB_NO_HD"); disabled && *disabled && *disabled != '0') {
    return {};
  }
  const char* environment = std::getenv("REB_TACTICAL_SPRITE_DIR");
  const auto configured = mw::config::Global().Get("tactical_sprite_dir", environment ? environment : "");
  if (configured.empty()) {
    return {};
  }
  auto directory = std::filesystem::u8path(configured);
  if (directory.is_relative()) {
    directory = std::filesystem::u8path(mw::config::GlobalPath()).parent_path() / directory;
  }
  return directory;
}

} // namespace reb::battle_sprites
