#pragma once

#include <deque>
#include <memory>

#include "audio/music.h"
#include "audio/sound_manager.h"
#include "audio/sound_source.h"

namespace reb::native
{

class AudioContext
{
public:
  AudioContext() = default;
  ~AudioContext();

  AudioContext(const AudioContext&) = delete;
  AudioContext& operator=(const AudioContext&) = delete;

  void Shutdown() noexcept;

  CdAudio cdAudio;
  SmackSound smackSound;
  SoundSourceList soundSources;
  SoundCatalogList soundCatalog;
  std::deque<std::unique_ptr<SoundEvent>> soundEventQueue;
  SoundEventCallback soundEventCallback = nullptr;

  HWND soundWindow = nullptr;
  IDirectSound* directSound = nullptr;
  IDirectSoundBuffer* primaryBuffer = nullptr;
  IDirectSound3DListener* listener = nullptr;
  int soundUseCount = 0;
  int soundVolume = 0;
  WAVEFORMATEX primaryFormat{};

  CRITICAL_SECTION soundQueueLock{};
  HANDLE soundEvents[2]{};
  HANDLE soundThread = nullptr;
  LONG soundDispatchBusy = 0;
  LONG sweepBusy = 0;
  int soundShuttingDown = 0;
  int soundThreadReady = 0;

  SoundRequest musicRequest{1, 0, 0, 0, 0, 0};
  PersistentInt musicFlags{0};
  std::string musicDirectory;
  int musicSwitchOn = 1;
  int musicVolume = 0;
};

} // namespace reb::native

namespace reb::native
{
AudioContext* CurrentAudioContext() noexcept;
}
