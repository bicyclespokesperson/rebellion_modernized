#include "port/native/space_battle/presenter.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

#include "port/minwin_impl/user/takeover.h"
#include "port/minwin_impl/audio/sdl_audio.h"
#include "port/native/space_battle/audio_player.h"
#include "port/native/space_battle/draw_executor.h"
#include "port/native/space_battle/pack_sprites.h"
#include "port/native/space_battle/sdl_input.h"
#include "port/native/space_battle/sprite_source.h"
#include "port/native/space_battle/text_atlas.h"

namespace reb::native::space_battle
{
namespace
{

TextAtlas& Atlas()
{
  static TextAtlas atlas;
  return atlas;
}

SpriteSource& SpritesImpl()
{
  static const std::unique_ptr<SpriteSource> sprites = LoadConfiguredSprites();
  return *sprites;
}

SDL_Texture*& TimingTarget()
{
  static SDL_Texture* target = nullptr;
  return target;
}

DrawExecutor& Executor()
{
  static DrawExecutor executor(Atlas(), SpritesImpl());
  return executor;
}

float UiScaleOverride()
{
  const char* text = std::getenv("REB_BATTLE_UI_SCALE");
  const float value = text ? static_cast<float>(std::atof(text)) : 0.0f;
  return value > 0.0f ? value : 0.0f;
}

class Session : public minwin::takeover::Client
{
  public:
  void HandleEvent(const SDL_Event& event) override
  {
    if (auto translated
        = m_translator.Translate(event, minwin::takeover::Window(), minwin::takeover::Renderer(), Screen())) {
      m_input.push_back(std::move(*translated));
    }
  }

  bool Open()
  {
    if (m_open) {
      return true;
    }
    if (!minwin::takeover::Begin(*this)) {
      return false;
    }
    minwin_audio::Install();
    SDL_Renderer* renderer = minwin::takeover::Renderer();
    SDL_GetRenderVSync(renderer, &m_vsync);
    SDL_SetRenderVSync(renderer, 1);
    SDL_GetRenderDrawColor(renderer, &m_color[0], &m_color[1], &m_color[2], &m_color[3]);
    SDL_GetRenderDrawBlendMode(renderer, &m_blend);
    m_translator = {};
    m_input.clear();
    m_input.push_back(battle_ui::ScreenChanged{Screen()});
    m_lastFrame = -1.0;
    m_open = true;
    return true;
  }

  void Close()
  {
    if (!m_open) {
      return;
    }
    m_audioMixer.Stop(m_audioPlayer);
    SDL_Renderer* renderer = minwin::takeover::Renderer();
    SDL_SetRenderVSync(renderer, m_vsync);
    SDL_SetRenderDrawColor(renderer, m_color[0], m_color[1], m_color[2], m_color[3]);
    SDL_SetRenderDrawBlendMode(renderer, m_blend);
    SDL_SetRenderClipRect(renderer, nullptr);
    Atlas().ReleaseTexture();
    SpritesImpl().ReleaseTextures();
    if (SDL_Texture*& target = TimingTarget()) {
      SDL_DestroyTexture(target);
      target = nullptr;
    }
    minwin::takeover::End();
    m_input.clear();
    m_open = false;
  }

  bool IsOpen() const
  {
    return m_open;
  }

  std::vector<battle_ui::InputEvent> TakeInput()
  {
    return std::exchange(m_input, {});
  }

  // Real seconds since the previous call (0 on the first call after opening), capped.
  double Elapsed()
  {
    const double now = ClockSeconds();
    const double elapsed = m_lastFrame < 0.0 ? 0.0 : std::min(now - m_lastFrame, kMaxFrameSeconds);
    m_lastFrame = now;
    return elapsed;
  }

  battle_ui::DrawList& Frame()
  {
    m_frame.Clear();
    return m_frame;
  }

  void UpdateAudio(battle_ui::BattleScreen& screen)
  {
    m_audioPlayer.UpdateVolumes();
    const auto cues = screen.TakeAudioCues();
    m_audioMixer.Update(cues, screen.GetCamera(), screen.Seconds(), m_audioPlayer);
  }

  private:
  static constexpr double kMaxFrameSeconds = 0.25;
  InputTranslator m_translator;
  AudioPlayer m_audioPlayer;
  battle_ui::BattleAudioMixer m_audioMixer;
  battle_ui::DrawList m_frame;
  double m_lastFrame = -1.0;
  std::vector<battle_ui::InputEvent> m_input;
  bool m_open = false;
  int m_vsync = 0;
  uint8_t m_color[4] = {0, 0, 0, 255};
  SDL_BlendMode m_blend = SDL_BLENDMODE_NONE;
};

Session& Current()
{
  static Session session;
  return session;
}

void Render(SDL_Renderer* renderer, const battle_ui::DrawList& frame)
{
  SDL_SetRenderViewport(renderer, nullptr);
  SDL_SetRenderClipRect(renderer, nullptr);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);
  Executor().Execute(renderer, frame);
}

} // namespace

bool Available()
{
  const char* driver = SDL_WasInit(SDL_INIT_VIDEO) ? SDL_GetCurrentVideoDriver() : std::getenv("SDL_VIDEODRIVER");
  if (driver && (std::strcmp(driver, "dummy") == 0 || std::strcmp(driver, "offscreen") == 0)) {
    return false;
  }
  return !SDL_WasInit(SDL_INIT_VIDEO) || minwin::takeover::Renderer() != nullptr;
}

bool Open()
{
  return Current().Open();
}

void Close()
{
  Current().Close();
}

bool IsOpen()
{
  return Current().IsOpen();
}

battle_ui::ScreenInfo Screen()
{
  battle_ui::ScreenInfo info;
  SDL_Window* window = minwin::takeover::Window();
  SDL_Renderer* renderer = minwin::takeover::Renderer();
  if (!window || !renderer) {
    return info;
  }
  SDL_GetRenderOutputSize(renderer, &info.width, &info.height);
  const float density = SDL_GetWindowPixelDensity(window);
  const float displayScale = SDL_GetWindowDisplayScale(window);
  info.pixelDensity = density > 0.0f ? density : 1.0f;
  const float overrideScale = UiScaleOverride();
  info.uiScale = overrideScale > 0.0f ? overrideScale : std::max(1.0f, displayScale);
  return info;
}

std::vector<battle_ui::InputEvent> TakeInput()
{
  return Current().TakeInput();
}

void PumpEvents()
{
  minwin::takeover::Pump();
}

void Present(const battle_ui::DrawList& frame)
{
  SDL_Renderer* renderer = minwin::takeover::Renderer();
  if (!renderer || !Current().IsOpen()) {
    return;
  }
  SDL_SetRenderTarget(renderer, nullptr);
  Render(renderer, frame);
  SDL_RenderPresent(renderer);
}

bool DriveFrame(battle_ui::BattleScreen& screen)
{
  Session& session = Current();
  for (const battle_ui::InputEvent& event : session.TakeInput()) {
    screen.HandleInput(event);
  }
  if (screen.QuitRequested()) {
    return false;
  }
  screen.Update(session.Elapsed());
  session.UpdateAudio(screen);
  battle_ui::DrawList& frame = session.Frame();
  screen.Build(frame);
  Present(frame);
  return true;
}

void UpdateAudio(battle_ui::BattleScreen& screen)
{
  Current().UpdateAudio(screen);
}

bool Capture(const battle_ui::DrawList& frame, int width, int height, const std::string& pngPath)
{
  SDL_Renderer* renderer = minwin::takeover::Renderer();
  if (!renderer) {
    return false;
  }
  SDL_Texture* target = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, width, height);
  if (!target) {
    std::fprintf(stderr, "space battle: cannot create a %dx%d capture target: %s\n", width, height, SDL_GetError());
    return false;
  }
  SDL_SetRenderTarget(renderer, target);
  Render(renderer, frame);
  SDL_Surface* pixels = SDL_RenderReadPixels(renderer, nullptr);
  SDL_SetRenderTarget(renderer, nullptr);
  SDL_DestroyTexture(target);
  if (!pixels) {
    std::fprintf(stderr, "space battle: cannot read the capture back: %s\n", SDL_GetError());
    return false;
  }
  const bool saved = SDL_SavePNG(pixels, pngPath.c_str());
  if (!saved) {
    std::fprintf(stderr, "space battle: cannot save %s: %s\n", pngPath.c_str(), SDL_GetError());
  }
  SDL_DestroySurface(pixels);
  return saved;
}

void Render(const battle_ui::DrawList& frame, int width, int height)
{
  SDL_Renderer* renderer = minwin::takeover::Renderer();
  if (!renderer) {
    return;
  }
  SDL_Texture*& target = TimingTarget();
  float targetWidth = 0.0f;
  float targetHeight = 0.0f;
  if (target
      && (!SDL_GetTextureSize(target, &targetWidth, &targetHeight) || targetWidth != width || targetHeight != height)) {
    SDL_DestroyTexture(target);
    target = nullptr;
  }
  if (!target) {
    target = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, width, height);
  }
  SDL_SetRenderTarget(renderer, target);
  Render(renderer, frame);
  SDL_Rect corner{0, 0, 1, 1};
  if (SDL_Surface* pixel = SDL_RenderReadPixels(renderer, &corner)) { // waits for the GPU
    SDL_DestroySurface(pixel);
  }
  SDL_SetRenderTarget(renderer, nullptr);
}

const battle_ui::TextMeasurer& Text()
{
  return Atlas();
}

const battle_ui::SpriteCatalog& Sprites()
{
  return SpritesImpl();
}

double ClockSeconds()
{
  return static_cast<double>(SDL_GetTicksNS()) / 1e9;
}

} // namespace reb::native::space_battle
