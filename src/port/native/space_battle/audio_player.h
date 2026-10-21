#pragma once

#include <memory>

#include "battle_ui/battle_audio.h"

namespace reb::native::space_battle
{

class AudioPlayer final : public battle_ui::AudioPlayback
{
  public:
  AudioPlayer();
  ~AudioPlayer() override;
  AudioPlayer(const AudioPlayer&) = delete;
  AudioPlayer& operator=(const AudioPlayer&) = delete;

  std::optional<uint64_t> Start(const battle_ui::AudioCue& cue, const battle_ui::AudioPlacement& placement) override;
  bool Playing(uint64_t handle) const override;
  void Reposition(uint64_t handle, const battle_ui::AudioPlacement& placement) override;
  void Cancel(uint64_t handle) override;
  void Stop() override;
  void UpdateVolumes();

  private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

} // namespace reb::native::space_battle
