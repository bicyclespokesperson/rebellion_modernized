#include "battle_ui/battle_audio.h"

#include <algorithm>
#include <cstdlib>

#include "battle_ui_test_support.h"

using namespace reb::battle_ui;
using namespace reb::battle_ui::test;
using namespace reb::battle;

namespace
{

struct MockPlayback final : AudioPlayback {
  std::optional<uint64_t> Start(const AudioCue& cue, const AudioPlacement& placement) override
  {
    starts.push_back({cue, placement});
    if (failStarts) {
      return std::nullopt;
    }
    const uint64_t handle = nextHandle++;
    playingHandles.push_back(handle);
    return handle;
  }

  bool Playing(uint64_t handle) const override
  {
    return std::ranges::find(playingHandles, handle) != playingHandles.end();
  }

  void Reposition(uint64_t handle, const AudioPlacement& placement) override
  {
    repositioned.emplace_back(handle, placement);
  }

  void Cancel(uint64_t handle) override
  {
    cancelled.push_back(handle);
    std::erase(playingHandles, handle);
  }

  void Stop() override
  {
    ++stopCalls;
    playingHandles.clear();
  }

  struct StartRecord {
    AudioCue cue;
    AudioPlacement placement;
  };
  std::vector<StartRecord> starts;
  std::vector<std::pair<uint64_t, AudioPlacement>> repositioned;
  std::vector<uint64_t> playingHandles;
  std::vector<uint64_t> cancelled;
  uint64_t nextHandle = 1;
  int stopCalls = 0;
  bool failStarts = false;
};

AudioCue Effect(BattleSound sound, uint32_t variant = 0, Vec2 position = {})
{
  return {EffectResource(sound, variant), position, false, 1.0f};
}

AudioCue Voice(uint16_t wave, Vec2 position = {})
{
  return {{SoundModule::VoiceAlliance, wave}, position, true, 1.0f};
}

AudioCue InterruptingVoice(uint16_t wave)
{
  AudioCue cue = Voice(wave);
  cue.interrupts = true;
  return cue;
}

Camera CameraAtCenter()
{
  Camera camera;
  camera.SetViewport({0.0f, 0.0f, 1000.0f, 600.0f});
  camera.SetCenter({0.0f, 0.0f});
  camera.SetZoom(0.1f);
  return camera;
}

void TestEffectPolyphonyAndVariantFamilyCap()
{
  BattleAudioMixer mixer;
  MockPlayback playback;
  const auto camera = CameraAtCenter();
  std::vector<AudioCue> cues{
      Effect(BattleSound::ImpactLaserCapital, 0), Effect(BattleSound::ImpactLaserCapital, 1),
      Effect(BattleSound::ImpactLaserCapital, 2), Effect(BattleSound::ExplosionSmall),
      Effect(BattleSound::ExplosionMedium), Effect(BattleSound::ExplosionLarge),
      Effect(BattleSound::TorpedoLaunch), Effect(BattleSound::TractorBeamStart)};
  mixer.Update(cues, camera, 0.0, playback);
  REQUIRE(playback.starts.size() == kEffectPolyphony);
  int laserFamily = 0;
  for (const auto& start : playback.starts) {
    if (start.cue.resource.waveId >= EffectSoundFor(BattleSound::ImpactLaserCapital).firstWave
        && start.cue.resource.waveId < EffectSoundFor(BattleSound::ImpactLaserCapital).firstWave
                                                    + EffectSoundFor(BattleSound::ImpactLaserCapital).variants) {
      ++laserFamily;
    }
  }
  REQUIRE(laserFamily == BattleAudioMixer::kIdenticalEffects);
  REQUIRE(mixer.PendingVoices() == 0);
}

void TestVoicesQueueIndependentlyInFifoOrder()
{
  BattleAudioMixer mixer;
  MockPlayback playback;
  const auto camera = CameraAtCenter();
  const std::vector<AudioCue> first{Voice(14001), Voice(14002), Voice(14003), Voice(14004),
                                    Effect(BattleSound::ExplosionSmall)};
  mixer.Update(first, camera, 1.0, playback);
  REQUIRE(playback.starts.size() == 2);
  REQUIRE(playback.starts[0].cue.resource.waveId == 14001);
  REQUIRE(playback.starts[1].cue.resource.waveId == EffectResource(BattleSound::ExplosionSmall, 0).waveId);
  REQUIRE(mixer.PendingVoices() == BattleAudioMixer::kPendingVoices);

  playback.playingHandles.erase(playback.playingHandles.begin());
  mixer.Update({}, camera, 2.0, playback);
  REQUIRE(playback.starts.size() == 3);
  REQUIRE(playback.starts.back().cue.resource.waveId == 14002);
  REQUIRE(mixer.PendingVoices() == 1);

  playback.playingHandles.erase(std::remove_if(playback.playingHandles.begin(), playback.playingHandles.end(),
                                               [&](uint64_t handle) { return handle == 3; }),
                                playback.playingHandles.end());
  mixer.Update({}, camera, 3.0, playback);
  REQUIRE(playback.starts.size() == 4);
  REQUIRE(playback.starts.back().cue.resource.waveId == 14003);
  REQUIRE(mixer.PendingVoices() == 0);
}

void TestStaleVoicesAndFailedStarts()
{
  BattleAudioMixer mixer;
  MockPlayback playback;
  const auto camera = CameraAtCenter();
  mixer.Update(std::vector<AudioCue>{Voice(14001), Voice(14002)}, camera, 0.0, playback);
  REQUIRE(playback.starts.size() == 1 && mixer.PendingVoices() == 1);
  playback.playingHandles.clear();
  mixer.Update({}, camera, 6.01, playback);
  REQUIRE(playback.starts.size() == 1);
  REQUIRE(mixer.PendingVoices() == 0);

  playback.failStarts = true;
  mixer.Update(std::vector<AudioCue>{Voice(14003)}, camera, 7.0, playback);
  REQUIRE(playback.starts.size() == 2);
  REQUIRE(mixer.PendingVoices() == 0);
  playback.failStarts = false;
  mixer.Update(std::vector<AudioCue>{Voice(14004)}, camera, 8.0, playback);
  REQUIRE(playback.starts.size() == 3);
  REQUIRE(playback.starts.back().cue.resource.waveId == 14004);
}

void TestStopClearsActiveAndQueuedAudio()
{
  BattleAudioMixer mixer;
  MockPlayback playback;
  const auto camera = CameraAtCenter();
  mixer.Update(std::vector<AudioCue>{Voice(14001), Voice(14002), Voice(14003)}, camera, 0.0, playback);
  REQUIRE(mixer.PendingVoices() == 2);
  mixer.Stop(playback);
  REQUIRE(playback.stopCalls == 1);
  REQUIRE(playback.playingHandles.empty());
  REQUIRE(mixer.PendingVoices() == 0);
  mixer.Update({}, camera, 1.0, playback);
  REQUIRE(playback.starts.size() == 1);
}

void TestInterruptingVoiceSilencesOtherVoices()
{
  BattleAudioMixer mixer;
  MockPlayback playback;
  const auto camera = CameraAtCenter();
  mixer.Update(std::vector<AudioCue>{Voice(14054), Voice(14055), Voice(14056), Effect(BattleSound::ExplosionSmall)},
               camera, 0.0, playback);
  REQUIRE(playback.starts.size() == 2 && mixer.PendingVoices() == 2);

  mixer.Update(std::vector<AudioCue>{Voice(14057), InterruptingVoice(14121), Voice(14058)}, camera, 0.5, playback);
  REQUIRE(playback.cancelled == std::vector<uint64_t>{1});
  REQUIRE(playback.Playing(2));
  REQUIRE(playback.starts.size() == 3);
  REQUIRE(playback.starts.back().cue.resource.waveId == 14121);
  REQUIRE(mixer.PendingVoices() == 1);

  std::erase(playback.playingHandles, 3);
  mixer.Update({}, camera, 1.0, playback);
  REQUIRE(playback.starts.back().cue.resource.waveId == 14058);
}

void TestCameraPanAttenuationAndRotation()
{
  auto camera = CameraAtCenter();
  const AudioPlacement center = PlaceAudio({}, camera);
  REQUIRE(Near(center.pan, 0.0f));
  REQUIRE(Near(center.volume, 1.0f));
  const AudioPlacement right = PlaceAudio({5000.0f, 0.0f}, camera);
  REQUIRE(Near(right.pan, 1.0f));
  REQUIRE(right.volume < center.volume);
  const AudioPlacement far = PlaceAudio({10000.0f, 0.0f}, camera);
  REQUIRE(Near(far.pan, 1.0f));
  REQUIRE(far.volume < right.volume);

  camera.Rotate(4);
  const AudioPlacement rotated = PlaceAudio({5000.0f, 0.0f}, camera);
  REQUIRE(rotated.pan < -0.99f);
  REQUIRE(rotated.volume < center.volume);
}

void TestActiveAudioRepositionsWithCamera()
{
  BattleAudioMixer mixer;
  MockPlayback playback;
  auto camera = CameraAtCenter();
  const std::vector<AudioCue> cues{Effect(BattleSound::ExplosionSmall, 0, {5000.0f, 0.0f})};
  mixer.Update(cues, camera, 0.0, playback);
  REQUIRE(playback.starts.size() == 1);
  REQUIRE(Near(playback.starts[0].placement.pan, 1.0f));

  camera.SetCenter({-1000.0f, 0.0f});
  camera.Rotate(4);
  mixer.Update({}, camera, 0.1, playback);
  REQUIRE(playback.repositioned.size() == 1);
  REQUIRE(playback.repositioned[0].first == 1);
  REQUIRE(Near(playback.repositioned[0].second.pan, -1.0f));
  REQUIRE(playback.repositioned[0].second.volume < playback.starts[0].placement.volume);
}

} // namespace

int main()
{
  TestEffectPolyphonyAndVariantFamilyCap();
  TestVoicesQueueIndependentlyInFifoOrder();
  TestStaleVoicesAndFailedStarts();
  TestStopClearsActiveAndQueuedAudio();
  TestInterruptingVoiceSilencesOtherVoices();
  TestCameraPanAttenuationAndRotation();
  TestActiveAudioRepositionsWithCamera();
  return 0;
}
