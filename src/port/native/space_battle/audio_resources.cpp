#include "port/native/space_battle/audio_resources.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "port/minwin_impl/res/path.h"

namespace reb::native::space_battle
{
namespace
{

uint16_t Read16(const uint8_t* bytes)
{
  return static_cast<uint16_t>(bytes[0] | (bytes[1] << 8));
}

uint32_t Read32(const uint8_t* bytes)
{
  return bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
}

} // namespace

std::optional<PcmWave> ParsePcmWave(std::span<const uint8_t> image)
{
  if (image.size() < 12 || std::memcmp(image.data(), "RIFF", 4) || std::memcmp(image.data() + 8, "WAVE", 4)) {
    return std::nullopt;
  }
  const uint64_t end = uint64_t{Read32(image.data() + 4)} + 8;
  if (end < 12 || end > image.size()) {
    return std::nullopt;
  }
  std::optional<mwds::Format> format;
  std::span<const uint8_t> samples;
  for (size_t offset = 12; offset < end;) {
    if (end - offset < 8) {
      return std::nullopt;
    }
    const uint8_t* chunk = image.data() + offset;
    const uint32_t size = Read32(chunk + 4);
    offset += 8;
    if (size > end - offset) {
      return std::nullopt;
    }
    if (!std::memcmp(chunk, "fmt ", 4)) {
      if (size < 16 || Read16(image.data() + offset) != 1) {
        return std::nullopt;
      }
      const uint8_t* fields = image.data() + offset;
      mwds::Format parsed{Read16(fields + 2), Read16(fields + 14), static_cast<int>(Read32(fields + 4))};
      if ((parsed.channels != 1 && parsed.channels != 2) ||
          (parsed.bitsPerSample != 8 && parsed.bitsPerSample != 16) ||
          parsed.sampleRate < 100 || parsed.sampleRate > 200000 || Read16(fields + 12) != parsed.BlockAlign() ||
          Read32(fields + 8) != static_cast<uint32_t>(parsed.sampleRate * parsed.BlockAlign())) {
        return std::nullopt;
      }
      format = parsed;
    } else if (!std::memcmp(chunk, "data", 4)) {
      samples = image.subspan(offset, size);
    }
    offset += size;
    // RIFF chunks are word-aligned; tolerate a final unpadded odd data chunk used by some WAVE writers.
    if (offset < end) {
      offset += size & 1u;
    }
  }
  if (!format || samples.empty() || samples.size() % format->BlockAlign()) {
    return std::nullopt;
  }
  return PcmWave{*format, samples};
}

AudioResources::AudioResources(std::string directory) : m_directory(std::move(directory))
{
}

std::optional<PcmWave> AudioResources::Find(battle_ui::SoundResource resource)
{
  const auto moduleIndex = static_cast<size_t>(resource.module);
  if (moduleIndex >= m_modules.size() || !resource.waveId) {
    return std::nullopt;
  }
  const auto key = std::pair{resource.module, resource.waveId};
  if (const auto found = m_waves.find(key); found != m_waves.end()) {
    return found->second;
  }
  const std::string name(battle_ui::ModuleFileName(resource.module));
  if (!m_attempted[moduleIndex]) {
    m_attempted[moduleIndex] = true;
    if (!m_directory.empty()) {
      const auto path = mw::res::ResolvePath(mw::res::JoinPath(m_directory, name));
      m_modules[moduleIndex].reset(mw::res::Module::Open(path));
    }
  }
  std::optional<PcmWave> wave;
  mw::res::Entry entry;
  if (m_modules[moduleIndex] && m_modules[moduleIndex]->Find(mw::res::ResId::Str("WAVE"),
          mw::res::ResId::Int(resource.waveId), &entry)) {
    wave = ParsePcmWave({entry.data, entry.size});
  }
  if (!wave && std::getenv("REB_BATTLE_AUDIO_TRACE")) {
    std::fprintf(stderr, "battle audio: unavailable %s WAVE %u\n", name.c_str(), resource.waveId);
  }
  m_waves.emplace(key, wave);
  return wave;
}

} // namespace reb::native::space_battle
