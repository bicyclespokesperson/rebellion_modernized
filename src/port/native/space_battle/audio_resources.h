#pragma once

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "battle_ui/battle_sounds.h"
#include "port/minwin_impl/dsound/mixer.h"
#include "port/minwin_impl/res/pe_module.h"

namespace reb::native::space_battle
{

struct PcmWave {
  mwds::Format format;
  std::span<const uint8_t> samples;
};

std::optional<PcmWave> ParsePcmWave(std::span<const uint8_t> image);

class AudioResources
{
  public:
  explicit AudioResources(std::string directory);
  std::optional<PcmWave> Find(battle_ui::SoundResource resource);

  private:
  std::string m_directory;
  std::array<std::unique_ptr<mw::res::Module>, 3> m_modules;
  std::array<bool, 3> m_attempted{};
  std::map<std::pair<battle_ui::SoundModule, uint16_t>, std::optional<PcmWave>> m_waves;
};

} // namespace reb::native::space_battle
