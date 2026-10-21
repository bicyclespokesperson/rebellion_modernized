// The SDL executor on a software renderer: fills (either winding), discs, clipping, dashes, additive blending, sprites
// from a sprite source, and text from the glyph atlas.
#include <SDL3/SDL.h>

#include "battle_ui/draw_list.h"
#include "battle_ui_test_support.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "port/native/space_battle/draw_executor.h"
#include "port/native/space_battle/pack_sprites.h"
#include "port/native/space_battle/sprite_source.h"
#include "port/native/space_battle/text_atlas.h"
#include "port/native/space_battle/settings_store.h"

using namespace reb::battle_ui;
using namespace reb::native::space_battle;
using reb::battle_ui::test::Near;

namespace
{

constexpr int kWidth = 240;
constexpr int kHeight = 160;

// A one-texel white sprite for typeIndex 7, so a DrawSprite covers a known area in its tint.
class SolidSprite : public SpriteSource
{
  public:
  bool Has(int32_t typeIndex, SpriteView) const override
  {
    return typeIndex == 7;
  }
  bool Frame(SDL_Renderer* renderer, int32_t typeIndex, SpriteView, SpriteLayer, float, float,
             SpriteFrame& out) override
  {
    if (typeIndex != 7) {
      return false;
    }
    if (!m_texture) {
      m_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 4, 2);
      const uint32_t white[8] = {~0U, ~0U, ~0U, ~0U, ~0U, ~0U, ~0U, ~0U};
      SDL_UpdateTexture(m_texture, nullptr, white, 16);
      SDL_SetTextureScaleMode(m_texture, SDL_SCALEMODE_NEAREST);
    }
    out = {m_texture, {0.0f, 0.0f, 4.0f, 2.0f}, 4.0f, 0.0f, {0.5f, 0.5f}};
    return true;
  }
  ~SolidSprite() override
  {
    if (m_texture) {
      SDL_DestroyTexture(m_texture);
    }
  }

  private:
  SDL_Texture* m_texture = nullptr;
};

struct Canvas {
  Canvas()
  {
    surface = SDL_CreateSurface(kWidth, kHeight, SDL_PIXELFORMAT_ARGB8888);
    renderer = SDL_CreateSoftwareRenderer(surface);
    REQUIRE(surface && renderer);
  }
  ~Canvas()
  {
    atlas.ReleaseTexture();
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(surface);
  }
  void Draw(const DrawList& list)
  {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    DrawExecutor executor(atlas, sprites);
    executor.Execute(renderer, list);
    SDL_FlushRenderer(renderer);
  }
  Color At(int x, int y) const
  {
    uint8_t r = 0, g = 0, b = 0, a = 0;
    SDL_ReadSurfacePixel(surface, x, y, &r, &g, &b, &a);
    return {r, g, b, a};
  }

  SDL_Surface* surface = nullptr;
  SDL_Renderer* renderer = nullptr;
  TextAtlas atlas;
  SolidSprite sprites;
};

bool Close(Color a, Color b, int tolerance = 8)
{
  return std::abs(a.r - b.r) <= tolerance && std::abs(a.g - b.g) <= tolerance && std::abs(a.b - b.b) <= tolerance;
}

const Color kBlack{0, 0, 0, 255};

void Fills(Canvas& canvas)
{
  DrawList list;
  list.FillRect({10.0f, 10.0f, 20.0f, 20.0f}, Rgb(0xff0000));
  list.FillConvex({{50.0f, 10.0f}, {80.0f, 10.0f}, {65.0f, 40.0f}}, Rgb(0x00ff00));   // clockwise on screen
  list.FillConvex({{90.0f, 10.0f}, {105.0f, 40.0f}, {120.0f, 10.0f}}, Rgb(0x00ff00)); // counter-clockwise
  list.FillCircle({160.0f, 30.0f}, 15.0f, Rgb(0x0000ff));
  list.Ring({210.0f, 30.0f}, 10.0f, 18.0f, Rgb(0xffffff));
  list.StrokeRect({10.0f, 60.0f, 40.0f, 30.0f}, Rgb(0xffff00), 2.0f);
  canvas.Draw(list);
  REQUIRE(Close(canvas.At(20, 20), Rgb(0xff0000)));
  REQUIRE(Close(canvas.At(9, 20), kBlack) && Close(canvas.At(31, 20), kBlack));
  REQUIRE(Close(canvas.At(65, 20), Rgb(0x00ff00)));
  REQUIRE(Close(canvas.At(105, 20), Rgb(0x00ff00)));
  REQUIRE(Close(canvas.At(160, 30), Rgb(0x0000ff)));
  REQUIRE(Close(canvas.At(160, 10), kBlack));
  REQUIRE(Close(canvas.At(210, 30), kBlack)); // the ring's hole
  REQUIRE(Close(canvas.At(210 + 14, 30), Rgb(0xffffff)));
  REQUIRE(Close(canvas.At(11, 75), Rgb(0xffff00))); // the outline
  REQUIRE(Close(canvas.At(30, 75), kBlack));        // its inside
}

void ClipsDashesAndBlending(Canvas& canvas)
{
  DrawList list;
  list.PushClip({100.0f, 0.0f, 60.0f, 50.0f});
  list.FillRect({0.0f, 0.0f, 240.0f, 50.0f}, Rgb(0xffffff));
  list.PopClip();
  list.Line({0.0f, 100.0f}, {240.0f, 100.0f}, Rgb(0xffffff), Stroke{3.0f, 10.0f, 10.0f});
  list.FillRect({20.0f, 120.0f, 40.0f, 20.0f}, Rgb(0x800000), Blend::Additive);
  list.FillRect({40.0f, 120.0f, 40.0f, 20.0f}, Rgb(0x800000), Blend::Additive);
  canvas.Draw(list);
  REQUIRE(Close(canvas.At(130, 25), Rgb(0xffffff)));
  REQUIRE(Close(canvas.At(90, 25), kBlack) && Close(canvas.At(170, 25), kBlack));
  int lit = 0;
  for (int x = 0; x < kWidth; ++x) {
    lit += canvas.At(x, 100).r > 128 ? 1 : 0;
  }
  REQUIRE(lit > kWidth / 3 && lit < kWidth * 2 / 3); // about half: 10 on, 10 off
  REQUIRE(Close(canvas.At(30, 130), Rgb(0x800000)));
  REQUIRE(Close(canvas.At(50, 130), Rgb(0xff0000), 4)); // the overlap adds up
}

void SpritesAndText(Canvas& canvas)
{
  DrawList list;
  DrawSprite sprite;
  sprite.typeIndex = 7;
  sprite.center = {60.0f, 40.0f};
  sprite.length = 40.0f;       // the 4x2 source becomes 40x20
  sprite.heading = kPi * 0.5f; // nose up: 20 wide, 40 tall
  sprite.tint = Rgb(0x00ff00);
  list.Sprite(sprite);
  sprite.typeIndex = 8; // missing from the source: nothing drawn
  sprite.center = {150.0f, 40.0f};
  list.Sprite(sprite);
  list.Text("Rebellion", {120.0f, 120.0f}, 24.0f, Rgb(0xffffff), Align::Center, VAlign::Middle);
  canvas.Draw(list);
  REQUIRE(Close(canvas.At(60, 40), Rgb(0x00ff00)));
  REQUIRE(Close(canvas.At(60, 25), Rgb(0x00ff00))); // inside the rotated length
  REQUIRE(Close(canvas.At(75, 40), kBlack));        // outside the rotated width
  REQUIRE(Close(canvas.At(150, 40), kBlack));
  const float width = canvas.atlas.Width("Rebellion", 24.0f, FontWeight::Regular);
  if (width <= 0.0f) {
    std::puts("no font found: text not checked");
    return;
  }
  int lit = 0;
  int left = kWidth;
  int right = 0;
  for (int y = 100; y < 140; ++y) {
    for (int x = 0; x < kWidth; ++x) {
      if (canvas.At(x, y).r > 128) {
        ++lit;
        left = std::min(left, x);
        right = std::max(right, x);
      }
    }
  }
  REQUIRE(lit > 100);
  REQUIRE(std::abs((left + right) / 2 - 120) <= 3); // centred
  REQUIRE(std::abs((right - left) - static_cast<int>(width)) <= 4);

  // Truncation keeps the text within its maximum width.
  DrawList truncated;
  DrawText text{"A long ship name that does not fit", {10.0f, 120.0f}, 16.0f, Rgb(0xffffff)};
  text.maxWidth = 100.0f;
  truncated.Text(text);
  canvas.Draw(truncated);
  for (int y = 100; y < 150; ++y) {
    for (int x = 112; x < kWidth; ++x) {
      REQUIRE(canvas.At(x, y).r < 64);
    }
  }
}

void SetEnvironment(const char* name, const std::string& value)
{
#if defined(_WIN32)
  _putenv_s(name, value.c_str());
#else
  if (value.empty()) {
    unsetenv(name);
  } else {
    setenv(name, value.c_str(), 1);
  }
#endif
}

// A QOI file of literal RGBA pixels: valid, and independent of the pack loader's decoder.
void WriteQoi(const std::filesystem::path& path, int width, int height, const std::vector<uint32_t>& rgba)
{
  std::vector<uint8_t> bytes{'q', 'o', 'i', 'f'};
  for (const int value : {width, height}) {
    for (int shift = 24; shift >= 0; shift -= 8) {
      bytes.push_back(static_cast<uint8_t>(value >> shift));
    }
  }
  bytes.insert(bytes.end(), {4, 0});
  for (const uint32_t pixel : rgba) {
    bytes.insert(bytes.end(), {0xff, static_cast<uint8_t>(pixel >> 24), static_cast<uint8_t>(pixel >> 16),
                               static_cast<uint8_t>(pixel >> 8), static_cast<uint8_t>(pixel)});
  }
  bytes.insert(bytes.end(), {0, 0, 0, 0, 0, 0, 0, 1});
  std::ofstream(path, std::ios::binary)
      .write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

// A pack with one 400 m class: a red 4x2 large frame at 100 m/px and a blue 2x1 small frame at 200 m/px.
std::filesystem::path WriteTinyPack()
{
  const std::filesystem::path directory = std::filesystem::temp_directory_path() / "reb_battle_ui_executor_pack";
  std::filesystem::create_directories(directory);
  const uint32_t red = 0xff0000ffU;
  const uint32_t blue = 0x0000ffffU;
  const uint32_t clear = 0;
  WriteQoi(directory / "atlas.qoi", 6, 2, {red, red, red, red, blue, blue, red, red, red, red, clear, clear});
  std::ofstream(directory / "manifest.txt") << "rebellion-tactical-sprites 1\n"
                                               "setting pitch_degrees 50\n"
                                               "atlas 0 atlas.qoi 6 2\n"
                                               "class 7 test original 400 Test Ship\n"
                                               "view 7 top large 1 100\n"
                                               "frame 7 top large 0 0 0 0 4 2 2 1\n"
                                               "view 7 top small 1 200\n"
                                               "frame 7 top small 0 0 4 0 2 1 1 0.5\n";
  return directory;
}

void DrawsFromTheSpritePack(Canvas& canvas)
{
  const std::filesystem::path directory = WriteTinyPack();
  std::string error;
  std::optional<reb::battle_sprites::SpritePack> pack = reb::battle_sprites::SpritePack::Load(directory, &error);
  REQUIRE(pack.has_value());
  PackSpriteSource source(std::move(*pack));
  REQUIRE(source.Has(7, SpriteView::TopDown) && !source.Has(7, SpriteView::Angled)
          && !source.Has(8, SpriteView::TopDown));
  REQUIRE(Near(source.LengthMeters(7), 400.0f) && source.LengthMeters(8) == 0.0f);

  SpriteFrame frame;
  REQUIRE(source.Frame(canvas.renderer, 7, SpriteView::TopDown, SpriteLayer::Color, 0.0f, 2.0f, frame));
  REQUIRE(frame.source.x == 4.0f && frame.source.w == 2.0f && Near(frame.length, 2.0f)); // small: 1:1 on screen
  REQUIRE(source.Frame(canvas.renderer, 7, SpriteView::TopDown, SpriteLayer::Color, 0.0f, 40.0f, frame));
  REQUIRE(frame.source.x == 0.0f && frame.source.w == 4.0f && Near(frame.length, 4.0f)); // large when enlarged
  REQUIRE(Near(frame.pivot.x, 0.5f) && Near(frame.pivot.y, 0.5f));
  REQUIRE(!source.Frame(canvas.renderer, 7, SpriteView::TopDown, SpriteLayer::Emissive, 0.0f, 40.0f, frame));

  DrawList list;
  list.Sprite({7, SpriteView::TopDown, SpriteLayer::Color, {120.0f, 80.0f}, 0.0f, 80.0f, Rgb(0xffffff)});
  DrawExecutor executor(canvas.atlas, source);
  SDL_SetRenderDrawColor(canvas.renderer, 0, 0, 0, 255);
  SDL_RenderClear(canvas.renderer);
  executor.Execute(canvas.renderer, list);
  SDL_FlushRenderer(canvas.renderer);
  REQUIRE(Close(canvas.At(120, 80), Rgb(0xff0000), 24));
  REQUIRE(Close(canvas.At(120, 125), kBlack)); // 80 x 40 on screen
  source.ReleaseTextures();

  SetEnvironment("REB_TACTICAL_SPRITE_DIR", directory.string());
  REQUIRE(LoadConfiguredSprites()->Has(7, SpriteView::TopDown));
  SetEnvironment("REB_TACTICAL_SPRITE_DIR", (directory / "missing").string());
  REQUIRE(!LoadConfiguredSprites()->Has(7, SpriteView::TopDown)); // falls back to silhouettes
  SetEnvironment("REB_TACTICAL_SPRITE_DIR", "");
  std::filesystem::remove_all(directory);
}

void HudSettingsPersistInNativeConfig()
{
  const auto path = std::filesystem::temp_directory_path() / "reb-battle-hud-settings-test.ini";
  mw::config::Config config;
  config.Set("install_dir", "/preserved-install");
  config.SetRegistryValue("Volume", {mw::config::ValueType::Dword, {}, 17});
  ConfigSettingsStore store(config, path.string());
  HudSettings settings;
  settings.autoPause[0] = false;
  settings.commandPhaseSeconds = 20;
  settings.colorBlindPalette = true;
  settings.hudScale = 1.3f;
  DoctrinePreset preset;
  preset.name = " Leading name\n[registry]\nVolume=bad ";
  settings.presets.push_back(preset);
  settings.Save(store);
  mw::config::Config loaded;
  REQUIRE(loaded.Load(path.string()));
  ConfigSettingsStore restored(loaded, path.string());
  const auto actual = HudSettings::Load(restored);
  REQUIRE(!actual.autoPause[0] && actual.commandPhaseSeconds == 20 && actual.colorBlindPalette);
  REQUIRE(Near(actual.hudScale, 1.3f));
  REQUIRE(actual.presets.size() == 1 && actual.presets[0].name == preset.name);
  REQUIRE(loaded.InstallDir() == "/preserved-install");
  mw::config::RegistryValue volume;
  REQUIRE(loaded.GetRegistryValue("Volume", &volume) && volume.dword == 17);
  settings.presets.clear();
  settings.Save(restored);
  REQUIRE(!restored.Get("battle.hud.preset.0"));
  REQUIRE(loaded.Load(path.string()) && loaded.Get("battle.hud.preset.0").empty());
  std::filesystem::remove(path);
}

void DecodesNamesFromTheStrategicGame()
{
  REQUIRE((DecodeText("Mon Cal") == std::vector<uint32_t>{'M', 'o', 'n', ' ', 'C', 'a', 'l'}));
  REQUIRE((DecodeText("\xc3\xa9t\xc3\xa9") == std::vector<uint32_t>{0xe9, 't', 0xe9})); // UTF-8
  REQUIRE((DecodeText("\xe9t\x93") == std::vector<uint32_t>{0xe9, 't', 0x201c}));       // Windows-1252
}

} // namespace

int main()
{
  {
    Canvas canvas;
    Fills(canvas);
    ClipsDashesAndBlending(canvas);
    SpritesAndText(canvas);
    DrawsFromTheSpritePack(canvas);
  }
  DecodesNamesFromTheStrategicGame();
  HudSettingsPersistInNativeConfig();
  std::puts("battle_ui executor tests passed");
}
