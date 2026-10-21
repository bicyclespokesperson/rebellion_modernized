#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <source_location>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "port/minwin_impl/config/config.h"
#include "port/minwin_impl/gdi_api.h"
#include "port/minwin_impl/user/present.h"

namespace minwin_gdi
{
int testCanvasWidth = 640;
ScreenSurface Screen()
{
  static uint8_t pixels[1280 * 480] = {};
  return {pixels, testCanvasWidth, 480, testCanvasWidth};
}
} // namespace minwin_gdi

namespace minwin
{
void RefreshCursor() {}
} // namespace minwin

namespace reb::platform
{
std::string UserConfigDir()
{
  return std::filesystem::temp_directory_path().string();
}
} // namespace reb::platform

namespace
{

void Require(bool condition, std::source_location location = std::source_location::current())
{
  if (!condition) {
    fprintf(stderr, "presentation integration requirement failed at line %u\n", location.line());
    std::abort();
  }
}

uint64_t SurfaceHash(SDL_Surface* surface)
{
  uint64_t hash = 1469598103934665603ull;
  const auto* pixels = static_cast<const uint8_t*>(surface->pixels);
  const size_t bytes = static_cast<size_t>(surface->pitch) * surface->h;
  for (size_t i = 0; i < bytes; ++i) {
    hash = (hash ^ pixels[i]) * 1099511628211ull;
  }
  return hash;
}

uint64_t SaveRenderAndHash(const std::filesystem::path& path, const std::vector<uint32_t>& pixels, int width,
                           int height)
{
  Require(minwin::present::RequestScreenshot());
  minwin::present::Present(pixels.data(), width, height);
  SDL_Surface* image = SDL_LoadBMP(path.string().c_str());
  Require(image != nullptr);
  const uint64_t hash = SurfaceHash(image);
  SDL_DestroySurface(image);
  return hash;
}

} // namespace

int main()
{
  SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
  Require(SDL_Init(SDL_INIT_VIDEO));
  mw::config::Global().Set("renderer", "software");
  mw::config::Global().Set("window_scale", "1");
  mw::config::Global().Set("fullscreen", "0");
  Require(minwin::present::Create());
  mw::config::Global().Set("render_scale", "2");
  Require(minwin::present::RenderScale() == 2);
  mw::config::Global().Set("render_scale", "4");
  Require(minwin::present::RenderScale() == 4);
  mw::config::Global().Set("render_scale", "invalid");

  constexpr int sourceScale = 3;
  constexpr int width = 640 * sourceScale;
  constexpr int height = 480 * sourceScale;
  std::vector<uint32_t> pixels(static_cast<size_t>(width) * height);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const uint8_t value = x % sourceScale == 0 ? 0 : (x % sourceScale == 1 ? 128 : 255);
      pixels[static_cast<size_t>(y) * width + x]
          = 0xff000000u | (static_cast<uint32_t>(value) << 16) | (static_cast<uint32_t>(value) << 8) | value;
    }
  }

  SDL_SetWindowSize(minwin::present::Window(), 800, 600);
  const std::filesystem::path path = PRESENT_SCREENSHOT_PATH;
  mw::config::Global().Set("upscale_filter", "sharp");
  const uint64_t sharpHash = SaveRenderAndHash(path, pixels, width, height);
  mw::config::Global().Set("upscale_filter", "nearest");
  const uint64_t nearestHash = SaveRenderAndHash(path, pixels, width, height);
  Require(sharpHash != nearestHash);

  int x = -1, y = -1;
  minwin::present::MapMouse(400.0f, 300.0f, &x, &y);
  Require(x == 320 && y == 240);
  mw::config::Global().Set("upscale_filter", "integer");
  minwin::present::Present(pixels.data(), width, height);
  minwin::present::MapMouse(400.0f, 300.0f, &x, &y);
  Require(x == 320 && y == 240);

  minwin::present::MapMouse(80.0f, 60.0f, &x, &y);
  Require(x == 0 && y == 0);
  minwin::present::MapMouse(719.0f, 539.0f, &x, &y);
  Require(x == 639 && y == 479);
  minwin::present::MapMouse(79.0f, 59.0f, &x, &y);
  Require(x == 0 && y == 0);

  SDL_SetWindowSize(minwin::present::Window(), 1000, 600);
  mw::config::Global().Set("upscale_filter", "sharp");
  minwin::present::Present(pixels.data(), width, height);
  minwin::present::MapMouse(100.0f, 0.0f, &x, &y);
  Require(x == 0 && y == 0);
  minwin::present::MapMouse(899.0f, 599.0f, &x, &y);
  Require(x == 639 && y == 479);
  minwin::present::MapMouse(900.0f, 600.0f, &x, &y);
  Require(x == 639 && y == 479);

  SDL_SetWindowSize(minwin::present::Window(), width, height);
  SaveRenderAndHash(path, pixels, width, height);
  SDL_Surface* exact = SDL_LoadBMP(path.string().c_str());
  Require(exact != nullptr);
  Uint8 red = 0, green = 0, blue = 0, alpha = 0;
  Require(SDL_ReadSurfacePixel(exact, 1, 1, &red, &green, &blue, &alpha));
  Require(red == 128 && green == 128 && blue == 128);
  Require(SDL_ReadSurfacePixel(exact, 2, 1, &red, &green, &blue, &alpha));
  Require(red == 255 && green == 255 && blue == 255);
  SDL_DestroySurface(exact);

  SDL_SetWindowSize(minwin::present::Window(), 960, 540);
  minwin_gdi::testCanvasWidth = 853;
  minwin::present::LogicalCanvasChanged();
  std::vector<uint32_t> widePixels(853 * 480, 0xff123456u);
  minwin::present::Present(widePixels.data(), 853, 480);
  minwin::present::MapMouse(959.0f, 539.0f, &x, &y);
  Require(x == 852 && y == 479);
  minwin::present::MapMouse(480.0f, 270.0f, &x, &y);
  Require(x == 426 && y == 240);

  minwin::present::Destroy();
  SDL_Quit();
  std::filesystem::remove(path);
}
