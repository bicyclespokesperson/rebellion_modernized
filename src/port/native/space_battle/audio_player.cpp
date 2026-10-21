#include "port/native/space_battle/audio_player.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "port/minwin_impl/config/config.h"
#include "port/native/space_battle/audio_resources.h"

namespace reb::native::space_battle
{
namespace
{

std::string ResourceDirectory()
{
  auto& config = mw::config::Global();
  mw::config::RegistryValue location;
  if (config.GetRegistryValue("DllLocation", &location) && location.type == mw::config::ValueType::String) {
    return location.text;
  }
  return config.InstallDir();
}

int ConfiguredVolume()
{
  mw::config::RegistryValue volume;
  if (!mw::config::Global().GetRegistryValue("Volume", &volume) || volume.type != mw::config::ValueType::Dword) {
    return 0;
  }
  return -static_cast<int>(std::min(volume.dword, 10000u));
}

float Gain(float value)
{
  return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
}

int VoiceVolume(float gain, int setting)
{
  return gain > 0.0f ? std::clamp(setting + static_cast<int>(2000.0f * std::log10(gain)), -10000, 0) : -10000;
}

int MixerPan(float pan)
{
  const float quietChannelGain = 1.0f - std::abs(pan);
  const int attenuation = -VoiceVolume(quietChannelGain, 0);
  return pan < 0.0f ? -attenuation : attenuation;
}

} // namespace

struct AudioPlayer::Impl {
  struct Playback {
    uint64_t handle;
    float gain;
    float cueVolume;
    std::unique_ptr<mwds::Voice> voice;
  };
  AudioResources resources{ResourceDirectory()};
  std::vector<Playback> playing;
  uint64_t nextHandle = 1;
};

AudioPlayer::AudioPlayer() : m_impl(std::make_unique<Impl>())
{
  // The mixer's static lifetime must enclose a presenter's static AudioPlayer lifetime.
  (void)mwds::Mixer::Global();
}

AudioPlayer::~AudioPlayer() = default;

std::optional<uint64_t> AudioPlayer::Start(const battle_ui::AudioCue& cue, const battle_ui::AudioPlacement& placement)
{
  UpdateVolumes();
  const float gain = Gain(cue.volume) * Gain(placement.volume);
  const int volume = VoiceVolume(gain, ConfiguredVolume());
  if (volume <= -10000) {
    return std::nullopt;
  }
  const auto wave = m_impl->resources.Find(cue.resource);
  if (!wave) {
    return std::nullopt;
  }
  mwds::OpenDevice();
  auto voice = std::make_unique<mwds::Voice>(&mwds::Mixer::Global(), wave->format,
                                           static_cast<uint32_t>(wave->samples.size()));
  std::memcpy(voice->Data(), wave->samples.data(), wave->samples.size());
  voice->SetVolume(volume);
  const float pan = std::isfinite(placement.pan) ? std::clamp(placement.pan, -1.0f, 1.0f) : 0.0f;
  voice->SetPan(MixerPan(pan));
  voice->Play(false);
  const uint64_t handle = m_impl->nextHandle++;
  m_impl->playing.push_back({handle, gain, Gain(cue.volume), std::move(voice)});
  if (std::getenv("REB_BATTLE_AUDIO_TRACE")) {
    const std::string module(battle_ui::ModuleFileName(cue.resource.module));
    std::fprintf(stderr, "battle audio: start %s WAVE %u %s handle=%llu pan=%.3f gain=%.3f volume=%d\n",
        module.c_str(), cue.resource.waveId, cue.voice ? "voice" : "effect",
        static_cast<unsigned long long>(handle), pan, gain, volume);
  }
  return handle;
}

bool AudioPlayer::Playing(uint64_t handle) const
{
  const auto& playing = m_impl->playing;
  const auto found = std::find_if(playing.begin(), playing.end(),
      [handle](const Impl::Playback& playback) { return playback.handle == handle; });
  return found != playing.end() && found->voice->IsPlaying();
}

void AudioPlayer::Cancel(uint64_t handle)
{
  if (std::erase_if(m_impl->playing, [handle](const Impl::Playback& playback) { return playback.handle == handle; })
      && std::getenv("REB_BATTLE_AUDIO_TRACE")) {
    std::fprintf(stderr, "battle audio: cancel handle=%llu\n", static_cast<unsigned long long>(handle));
  }
}

void AudioPlayer::Stop()
{
  m_impl->playing.clear();
}

void AudioPlayer::Reposition(uint64_t handle, const battle_ui::AudioPlacement& placement)
{
  const auto found = std::ranges::find(m_impl->playing, handle, &Impl::Playback::handle);
  if (found == m_impl->playing.end()) {
    return;
  }
  found->gain = found->cueVolume * Gain(placement.volume);
  found->voice->SetVolume(VoiceVolume(found->gain, ConfiguredVolume()));
  const float pan = std::isfinite(placement.pan) ? std::clamp(placement.pan, -1.0f, 1.0f) : 0.0f;
  found->voice->SetPan(MixerPan(pan));
}

void AudioPlayer::UpdateVolumes()
{
  std::erase_if(m_impl->playing, [](const Impl::Playback& playback) { return !playback.voice->IsPlaying(); });
  const int setting = ConfiguredVolume();
  for (auto& playback : m_impl->playing) {
    playback.voice->SetVolume(VoiceVolume(playback.gain, setting));
  }
}

} // namespace reb::native::space_battle
