#pragma once
#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <vector>

#include "battle_ui/battle_sounds.h"
#include "battle_ui/camera.h"
#include "space_battle/view_model.h"

namespace reb::battle_ui
{

struct AudioCue {
  SoundResource resource;
  battle::Vec2 position;
  bool voice = false;
  float volume = 1.0f;
  bool interrupts = false; // a voice that silences every other voice, playing or queued
};

struct AudioPlacement {
  float pan = 0.0f; // -1 left, +1 right
  float volume = 1.0f;
};

AudioPlacement PlaceAudio(battle::Vec2 position, const Camera& camera);

class AudioPlayback
{
  public:
  virtual ~AudioPlayback() = default;
  virtual std::optional<uint64_t> Start(const AudioCue& cue, const AudioPlacement& placement) = 0;
  virtual bool Playing(uint64_t handle) const = 0;
  virtual void Reposition(uint64_t handle, const AudioPlacement& placement) {}
  virtual void Cancel(uint64_t handle) = 0;
  virtual void Stop() = 0;
};

class BattleAudioCues
{
  public:
  explicit BattleAudioCues(battle::Side humanSide = battle::Side::Alliance) : m_humanSide(humanSide) {}
  void Observe(const battle::ViewModel& view);
  void Submitted(const battle::Command& command, const battle::ViewModel& view);
  std::vector<AudioCue> Take();

  private:
  void Effect(BattleSound sound, battle::Vec2 position);
  void Voice(VoiceCue cue, battle::Vec2 position, VoiceAddress address = {}, bool interrupts = false);
  void SpeakResult(const battle::ViewModel& view);
  void RunVoice(DeathStarRunReport report, const battle::ViewUnit& unit, const battle::ViewModel& view);
  void Impact(const battle::ViewShot& shot, const battle::ViewModel& view);
  battle::Side m_humanSide;
  std::optional<battle::ViewModel> m_previous;
  std::vector<battle::ViewShot> m_pendingShots;
  std::vector<AudioCue> m_cues;
  uint32_t m_variant = 0;
  bool m_decided = false;
};

class BattleAudioMixer
{
  public:
  static constexpr size_t kPendingVoices = 2;
  static constexpr double kVoicePatienceSeconds = 6.0;
  static constexpr int kIdenticalEffects = 2;
  void Update(std::span<const AudioCue> cues, const Camera& camera, double seconds, AudioPlayback& playback);
  void Stop(AudioPlayback& playback);
  size_t PendingVoices() const { return m_voices.size(); }

  private:
  void SilenceVoices(AudioPlayback& playback);
  struct Active {
    AudioCue cue;
    uint64_t handle;
  };
  struct Waiting {
    AudioCue cue;
    double time;
  };
  std::vector<Active> m_active;
  std::deque<Waiting> m_voices;
};

} // namespace reb::battle_ui
