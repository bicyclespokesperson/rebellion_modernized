#include <filesystem>
#include <source_location>

#include "port/minwin_impl/audio/sdl_audio.cpp"

namespace reb::platform
{
std::string UserConfigDir()
{
  return std::filesystem::temp_directory_path().string();
}
}

namespace
{
void Require(bool condition, std::source_location location = std::source_location::current())
{
  if (!condition) {
    fprintf(stderr, "audio recovery requirement failed at line %u: %s\n", location.line(), SDL_GetError());
    std::abort();
  }
}

void WaitForCallback(Uint64 after)
{
  for (int attempt = 0; attempt < 100 && g_lastCallback.load() <= after; ++attempt) {
    SDL_Delay(10);
  }
  Require(g_lastCallback.load() > after);
}
}

int main()
{
  SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
  minwin_audio::Install();
  mwds::OpenDevice();
  Require(g_stream != nullptr);
  WaitForCallback(g_lastCallback.load());
  Require(SDL_PauseAudioStreamDevice(g_stream));
  const Uint64 pausedAt = g_lastCallback.load();
  SDL_Delay(3100);
  minwin_audio::ServiceDevice();
  Require(g_stream != nullptr && !PacingWorker().joinable());
  WaitForCallback(pausedAt);

  SDL_DestroyAudioStream(g_stream);
  g_stream = nullptr;
  SDL_QuitSubSystem(SDL_INIT_AUDIO);
  SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "unavailable-test-driver");
  g_lastDeviceCheck = 0;
  minwin_audio::ServiceDevice();
  Require(g_stream == nullptr && PacingWorker().joinable());

  SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
  g_lastDeviceCheck = 0;
  minwin_audio::ServiceDevice();
  Require(g_stream != nullptr && !PacingWorker().joinable());
  WaitForCallback(g_lastCallback.load());
  SDL_DestroyAudioStream(g_stream);
  g_stream = nullptr;
  SDL_Quit();
  puts("audio recovery checks passed");
}
