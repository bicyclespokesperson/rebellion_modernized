#include "port/native/audio_context.h"

#include "app/rebapp_globals.h"
#include "app/rebellion_application.h"
#include "port/native/game_context.h"

namespace reb::native
{

AudioContext* CurrentAudioContext() noexcept
{
  if (!g_app) {
    return nullptr;
  }
  return &static_cast<RebellionApplication*>(g_app)->m_context.GetAudioContext();
}

AudioContext::~AudioContext()
{
  Shutdown();
}

void AudioContext::Shutdown() noexcept
{
  if (CurrentAudioContext() != this) {
    return;
  }

  SoundManager::StopEventThread();
  SoundManager::SetEventCallback(nullptr);
  StopMusic();
  smackSound.Stop(1);
  soundCatalog.Clear();
  soundEventQueue.clear();
  SoundManager::Shutdown(1);
  soundSources.Clear();
  cdAudio.Close();
}

} // namespace reb::native
