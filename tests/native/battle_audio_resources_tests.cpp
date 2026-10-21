#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include "battle_ui/battle_audio.h"
#include "port/minwin_impl/config/config.h"
#include "port/native/space_battle/audio_player.h"
#include "port/native/space_battle/audio_resources.h"

#define REQUIRE(condition) do { if (!(condition)) { \
  std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); std::abort(); \
} } while (false)

using namespace reb::native::space_battle;
using namespace reb::battle_ui;

namespace
{

void Write16(std::vector<uint8_t>& bytes, size_t offset, uint16_t value)
{
  bytes[offset] = static_cast<uint8_t>(value);
  bytes[offset + 1] = static_cast<uint8_t>(value >> 8);
}

void Write32(std::vector<uint8_t>& bytes, size_t offset, uint32_t value)
{
  Write16(bytes, offset, static_cast<uint16_t>(value));
  Write16(bytes, offset + 2, static_cast<uint16_t>(value >> 16));
}

std::vector<uint8_t> Wave(int channels = 1, int bits = 8, bool oddChunk = false)
{
  const uint32_t align = channels * bits / 8;
  const size_t formatOffset = oddChunk ? 22 : 12;
  const size_t dataOffset = formatOffset + 24;
  std::vector<uint8_t> bytes(dataOffset + 8 + 8 * align, 0);
  std::memcpy(bytes.data(), "RIFF", 4);
  Write32(bytes, 4, static_cast<uint32_t>(bytes.size() - 8));
  std::memcpy(bytes.data() + 8, "WAVE", 4);
  if (oddChunk) {
    std::memcpy(bytes.data() + 12, "JUNK", 4);
    Write32(bytes, 16, 1);
    bytes[20] = 42;
  }
  std::memcpy(bytes.data() + formatOffset, "fmt ", 4);
  Write32(bytes, formatOffset + 4, 16);
  Write16(bytes, formatOffset + 8, 1);
  Write16(bytes, formatOffset + 10, channels);
  Write32(bytes, formatOffset + 12, 22050);
  Write32(bytes, formatOffset + 16, 22050 * align);
  Write16(bytes, formatOffset + 20, align);
  Write16(bytes, formatOffset + 22, bits);
  std::memcpy(bytes.data() + dataOffset, "data", 4);
  Write32(bytes, dataOffset + 4, 8 * align);
  std::fill(bytes.begin() + dataOffset + 8, bytes.end(), 192);
  return bytes;
}

std::vector<uint8_t> ResourceModule(uint16_t id, const std::vector<uint8_t>& wave)
{
  constexpr size_t resourceOffset = 0x400;
  constexpr uint32_t payloadOffset = 100;
  const uint32_t sectionSize = payloadOffset + static_cast<uint32_t>(wave.size());
  std::vector<uint8_t> bytes(resourceOffset + sectionSize, 0);
  std::memcpy(bytes.data(), "MZ", 2);
  Write32(bytes, 0x3c, 0x80);
  std::memcpy(bytes.data() + 0x80, "PE\0\0", 4);
  Write16(bytes, 0x84, 0x14c);
  Write16(bytes, 0x86, 1);
  Write16(bytes, 0x94, 224);
  constexpr size_t optional = 0x98;
  Write16(bytes, optional, 0x10b);
  Write32(bytes, optional + 92, 16);
  Write32(bytes, optional + 112, 0x1000);
  Write32(bytes, optional + 116, sectionSize);
  constexpr size_t section = optional + 224;
  std::memcpy(bytes.data() + section, ".rsrc", 5);
  Write32(bytes, section + 8, sectionSize);
  Write32(bytes, section + 12, 0x1000);
  Write32(bytes, section + 16, sectionSize);
  Write32(bytes, section + 20, resourceOffset);
  Write16(bytes, resourceOffset + 12, 1);
  Write32(bytes, resourceOffset + 16, 0x80000058);
  Write32(bytes, resourceOffset + 20, 0x80000018);
  Write16(bytes, resourceOffset + 24 + 14, 1);
  Write32(bytes, resourceOffset + 40, id);
  Write32(bytes, resourceOffset + 44, 0x80000030);
  Write16(bytes, resourceOffset + 48 + 14, 1);
  Write32(bytes, resourceOffset + 64, 1033);
  Write32(bytes, resourceOffset + 68, 72);
  Write32(bytes, resourceOffset + 72, 0x1000 + payloadOffset);
  Write32(bytes, resourceOffset + 76, static_cast<uint32_t>(wave.size()));
  Write16(bytes, resourceOffset + 88, 4);
  for (size_t i = 0; i < 4; ++i) {
    Write16(bytes, resourceOffset + 90 + 2 * i, "WAVE"[i]);
  }
  std::copy(wave.begin(), wave.end(), bytes.begin() + resourceOffset + payloadOffset);
  return bytes;
}

void Save(const std::filesystem::path& path, const std::vector<uint8_t>& bytes)
{
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  REQUIRE(output.good());
}

void PcmFormatsAndBounds()
{
  for (int channels : {1, 2}) {
    for (int bits : {8, 16}) {
      auto bytes = Wave(channels, bits, true);
      const auto wave = ParsePcmWave(bytes);
      REQUIRE(wave);
      REQUIRE(wave->format.channels == channels && wave->format.bitsPerSample == bits);
      REQUIRE(wave->format.sampleRate == 22050 && wave->samples.size() == 8u * channels * bits / 8);
      for (size_t length = 0; length < bytes.size(); ++length) {
        REQUIRE(!ParsePcmWave(std::span(bytes).first(length)));
      }
    }
  }
  auto bytes = Wave();
  Write16(bytes, 20, 3);
  REQUIRE(!ParsePcmWave(bytes));
  bytes = Wave();
  Write16(bytes, 22, 3);
  REQUIRE(!ParsePcmWave(bytes));
  bytes = Wave();
  Write16(bytes, 32, 2);
  REQUIRE(!ParsePcmWave(bytes));
  bytes = Wave();
  Write32(bytes, 40, 0xffffffff);
  REQUIRE(!ParsePcmWave(bytes));
  bytes = Wave(2, 16);
  bytes.pop_back();
  Write32(bytes, 4, static_cast<uint32_t>(bytes.size() - 8));
  Write32(bytes, 40, 31);
  REQUIRE(!ParsePcmWave(bytes));
}

int g_openCount = 0;

void ResourceLookupAndPlayback(const std::filesystem::path& directory)
{
  AudioResources missing((directory / "missing").string());
  REQUIRE(!missing.Find({SoundModule::Tactical, 13000}));
  REQUIRE(!missing.Find({SoundModule::VoiceAlliance, 15133}));
  Save(directory / "tactical.dll", ResourceModule(13000, Wave()));
  Save(directory / "voicefxa.dll", ResourceModule(15133, Wave(2, 16)));
  AudioResources resources(directory.string());
  const auto effect = resources.Find({SoundModule::Tactical, 13000});
  REQUIRE(effect && effect->format.channels == 1 && effect->format.bitsPerSample == 8);
  const auto voice = resources.Find({SoundModule::VoiceAlliance, 15133});
  REQUIRE(voice && voice->format.channels == 2);
  REQUIRE(!resources.Find({SoundModule::VoiceEmpire, 15133}));
  REQUIRE(!resources.Find({SoundModule::Tactical, 13001}));

  mw::config::Global().Parse("install_dir=" + directory.string() + "\n");
  mwds::SetDeviceOpener([] { ++g_openCount; });
  AudioPlayer player;
  const AudioCue cue{{SoundModule::Tactical, 13000}, {}, false, 1.0f};
  const auto handle = player.Start(cue, {1.0f, 1.0f});
  REQUIRE(handle && player.Playing(*handle) && g_openCount == 1);
  std::vector<int16_t> output(8);
  mwds::Mixer::Global().Render(output.data(), 4);
  REQUIRE(output[0] == 0 && output[1] > 0);
  REQUIRE(player.Playing(*handle));
  const int16_t fullGain = output[1];
  player.Reposition(*handle, {-1.0f, 0.5f});
  mwds::Mixer::Global().Render(output.data(), 4);
  REQUIRE(output[0] > 0 && output[0] < fullGain && output[1] == 0);
  mw::config::Global().SetRegistryValue("Volume", {mw::config::ValueType::Dword, {}, 10000});
  player.UpdateVolumes();
  mwds::Mixer::Global().Render(output.data(), 4);
  REQUIRE(std::ranges::all_of(output, [](int16_t value) { return value == 0; }));
  REQUIRE(!player.Start(cue, {}));
  mw::config::Global().SetRegistryValue("Volume", {mw::config::ValueType::Dword, {}, 0});
  output.resize(32);
  mwds::Mixer::Global().Render(output.data(), 16);
  REQUIRE(!player.Playing(*handle));
  const auto next = player.Start(cue, {});
  REQUIRE(next && *next != *handle && player.Playing(*next));
  player.Stop();
  REQUIRE(!player.Playing(*next));
  REQUIRE(!player.Start({{SoundModule::VoiceEmpire, 15133}}, {}));
  mwds::SetDeviceOpener(nullptr);
}

} // namespace

int main(int argc, char** argv)
{
  PcmFormatsAndBounds();
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto directory = std::filesystem::temp_directory_path() / ("reb-audio-" + std::to_string(stamp));
  REQUIRE(std::filesystem::create_directory(directory));
  ResourceLookupAndPlayback(directory);
  std::filesystem::remove_all(directory);
  if (argc == 3 && std::string(argv[1]) == "--install-dir") {
    AudioResources installed(argv[2]);
    const auto check = [&](SoundModule module, int first, int last) {
      for (int id = first; id <= last; ++id) {
        REQUIRE(installed.Find({module, static_cast<uint16_t>(id)}));
      }
    };
    check(SoundModule::Tactical, 13000, 13065);
    check(SoundModule::VoiceAlliance, 14001, 14122);
    check(SoundModule::VoiceAlliance, 15133, 15163);
    check(SoundModule::VoiceEmpire, 15001, 15132);
    std::puts("all 351 installed WAVE resources parsed");
  } else {
    REQUIRE(argc == 1);
  }
  std::puts("battle audio resources passed");
}
