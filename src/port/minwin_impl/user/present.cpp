// Presentation of the finished frame: see present.h.
#include "port/minwin_impl/user/present.h"
#include "port/minwin_impl/user/presentation_math.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <filesystem>
#include <string>
#include <vector>

#include "port/minwin_impl/config/config.h"
#include "port/minwin_impl/gdi_api.h"

namespace minwin
{
void RefreshCursor(); // misc.cpp: re-applies the current cursor (it is rebuilt at the new scale)
}

namespace minwin::present
{
namespace
{

constexpr int kWidth = minwin_gdi::kScreenWidth;
constexpr int kHeight = minwin_gdi::kScreenHeight;
constexpr int kMaxScale = 8;
constexpr int kDecorationReservePx = 48; // title bar and borders that the display's usable area does not account for

SDL_Window* g_window = nullptr;
SDL_Renderer* g_renderer = nullptr;
SDL_Texture* g_texture = nullptr;
SDL_Texture* g_intermediateTexture = nullptr;
int g_textureWidth = 0;
int g_textureHeight = 0;
int g_intermediateWidth = 0;
int g_intermediateHeight = 0;
int g_cursorScale = 1;
float g_cursorDensity = 0.0f;
bool g_mouseCaptured = false;
SDL_FRect g_outputRect{};
int g_outputWidth = 0;
int g_outputHeight = 0;
std::string g_requestedScreenshotPath;
std::vector<uint32_t> g_intermediatePixels;

bool VirtualClockRequested()
{
  const char* text = getenv("REB_DETERMINISTIC");
  return text && atoi(text) != 0;
}

int ClampScale(int scale)
{
  return scale < 1 ? 1 : (scale > kMaxScale ? kMaxScale : scale);
}

int LargestScaleThatFits()
{
  SDL_Rect usable;
  if (!SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usable) || usable.w <= 0 || usable.h <= 0) {
    return 1;
  }
  const int byWidth = usable.w / kWidth;
  const int byHeight = (usable.h - kDecorationReservePx) / kHeight;
  return ClampScale(byWidth < byHeight ? byWidth : byHeight);
}

int RequestedScale()
{
  if (const char* text = getenv("REB_MINWIN_SCALE")) {
    if (*text) {
      return ClampScale(atoi(text));
    }
  }
  const int configured = mw::config::Global().WindowScale();
  return configured > 0 ? ClampScale(configured) : LargestScaleThatFits();
}

// REB_MINWIN_HIDDEN=1 keeps the window off the desktop (offscreen review captures).
bool HiddenRequested()
{
  const char* text = getenv("REB_MINWIN_HIDDEN");
  return text && atoi(text) != 0;
}

bool FullscreenRequested()
{
  if (const char* text = getenv("REB_MINWIN_FULLSCREEN")) {
    if (*text) {
      return atoi(text) != 0;
    }
  }
  return atoi(mw::config::Global().Get("fullscreen", "0").c_str()) != 0;
}

SDL_Renderer* CreateRenderer(SDL_Window* window)
{
  const std::string wanted = mw::config::Global().Get("renderer");
  SDL_Renderer* renderer = SDL_CreateRenderer(window, wanted.empty() ? nullptr : wanted.c_str());
  return renderer ? renderer : SDL_CreateRenderer(window, "software");
}

void UpdateOutputRect()
{
  if (!g_renderer || !SDL_GetRenderOutputSize(g_renderer, &g_outputWidth, &g_outputHeight) || g_outputWidth <= 0
      || g_outputHeight <= 0) {
    return;
  }
  const minwin_gdi::ScreenSurface canvas = minwin_gdi::Screen();
  const float fitScale
      = fminf(static_cast<float>(g_outputWidth) / canvas.width, static_cast<float>(g_outputHeight) / canvas.height);
  const bool integerFilter = mw::config::Global().Get("upscale_filter", "sharp") == "integer";
  const float scale = integerFilter ? fmaxf(1.0f, floorf(fitScale)) : fitScale;
  g_outputRect = {(g_outputWidth - canvas.width * scale) * 0.5f, (g_outputHeight - canvas.height * scale) * 0.5f,
                  canvas.width * scale, canvas.height * scale};
}

int ComputeCursorScale()
{
  if (!g_renderer || !g_window) {
    return 1;
  }
  UpdateOutputRect();
  const float density = SDL_GetWindowPixelDensity(g_window);
  g_cursorDensity = density > 0 ? density : 1.0f;
  const float inWindowUnits = g_outputRect.w / static_cast<float>(minwin_gdi::Screen().width) / g_cursorDensity;
  const int scale = static_cast<int>(floorf(inWindowUnits + 0.5f));
  return scale < 1 ? 1 : scale;
}

int OutputScale()
{
  if (!g_renderer) {
    return 1;
  }
  const std::string configured = mw::config::Global().Get("render_scale", "0");
  char* end = nullptr;
  const long requested = strtol(configured.c_str(), &end, 10);
  if (end != configured.c_str() && *end == '\0' && requested >= 1 && requested <= 4) {
    return static_cast<int>(requested);
  }
  int width = 0, height = 0;
  if (!SDL_GetRenderOutputSize(g_renderer, &width, &height) || width <= 0 || height <= 0) {
    return 1;
  }
  const minwin_gdi::ScreenSurface canvas = minwin_gdi::Screen();
  return RenderScaleForOutput(width, height, canvas.width, canvas.height);
}

bool EnsureTexture(SDL_Texture** texture, int* oldWidth, int* oldHeight, int width, int height)
{
  if (*texture && *oldWidth == width && *oldHeight == height) {
    return true;
  }
  if (*texture) {
    SDL_DestroyTexture(*texture);
  }
  *texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
  *oldWidth = width;
  *oldHeight = height;
  return *texture != nullptr;
}

bool UpdateCursorScale()
{
  const int scale = ComputeCursorScale();
  const float density = g_cursorDensity;
  static float previousDensity = 0.0f;
  if (scale == g_cursorScale && density == previousDensity) {
    return false;
  }
  g_cursorScale = scale;
  previousDensity = density;
  RefreshCursor();
  return true;
}

std::vector<uint8_t> Enlarge(const uint8_t* rgba, int width, int height, int factor)
{
  std::vector<uint8_t> out(static_cast<size_t>(width) * factor * height * factor * 4);
  for (int y = 0; y < height * factor; ++y) {
    for (int x = 0; x < width * factor; ++x) {
      memcpy(&out[(static_cast<size_t>(y) * width * factor + x) * 4],
             rgba + (static_cast<size_t>(y / factor) * width + x / factor) * 4, 4);
    }
  }
  return out;
}

} // namespace

SDL_Window* Window()
{
  return g_window;
}

SDL_Renderer* Renderer()
{
  return g_renderer;
}

bool Create()
{
  const int scale = RequestedScale();
  SDL_PropertiesID props = SDL_CreateProperties();
  SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "Star Wars: Rebellion");
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, kWidth * scale);
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, kHeight * scale);
  SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
  SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);
  SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, FullscreenRequested() && !HiddenRequested());
  SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, HiddenRequested());
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED);
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED);
  g_window = SDL_CreateWindowWithProperties(props);
  SDL_DestroyProperties(props);
  if (g_window) {
    SDL_SetWindowMinimumSize(g_window, kWidth, kHeight);
    g_renderer = CreateRenderer(g_window);
  }
  if (g_renderer) {
    SDL_SetRenderVSync(g_renderer, atoi(mw::config::Global().Get("vsync", "0").c_str()) != 0 ? 1 : 0);
  }
  if (!g_window || !g_renderer) {
    return false;
  }
  UpdateOutputRect();
  g_cursorScale = ComputeCursorScale();
  if (getenv("REB_MINWIN_TRACE")) {
    fprintf(stderr, "minwin: window scale %d, cursor scale %d, renderer %s\n", scale, g_cursorScale,
            SDL_GetRendererName(g_renderer));
  }
  return true;
}

void Destroy()
{
  if (g_texture) {
    SDL_DestroyTexture(g_texture);
  }
  if (g_intermediateTexture) {
    SDL_DestroyTexture(g_intermediateTexture);
  }
  if (g_renderer) {
    SDL_DestroyRenderer(g_renderer);
  }
  if (g_window) {
    SDL_DestroyWindow(g_window);
  }
  g_texture = nullptr;
  g_intermediateTexture = nullptr;
  g_textureWidth = g_textureHeight = 0;
  g_intermediateWidth = g_intermediateHeight = 0;
  g_renderer = nullptr;
  g_window = nullptr;
  g_requestedScreenshotPath.clear();
  g_intermediatePixels.clear();
  g_mouseCaptured = false;
}

void Present(const uint32_t* pixels, int width, int height)
{
  const minwin_gdi::ScreenSurface canvas = minwin_gdi::Screen();
  if (!g_window || !g_renderer || !pixels || width < canvas.width || width % canvas.width != 0
      || height != (width / canvas.width) * canvas.height || (SDL_GetWindowFlags(g_window) & SDL_WINDOW_MINIMIZED)) {
    return;
  }
  UpdateOutputRect();
  if (!EnsureTexture(&g_texture, &g_textureWidth, &g_textureHeight, width, height)) {
    return;
  }
  const std::string filter = mw::config::Global().Get("upscale_filter", "sharp");
  SDL_SetTextureScaleMode(g_texture,
                          filter == "nearest" || filter == "integer" ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR);
  SDL_Texture* frameTexture = g_texture;
  const float targetScale = g_outputRect.w / canvas.width;
  const int sourceScale = width / canvas.width;
  if (filter == "sharp" && sourceScale == 1 && sourceScale < targetScale) {
    const int integerScale = ClampScale(static_cast<int>(ceilf(targetScale / sourceScale)));
    const int intermediateWidth = width * integerScale;
    const int intermediateHeight = height * integerScale;
    if (!EnsureTexture(&g_intermediateTexture, &g_intermediateWidth, &g_intermediateHeight, intermediateWidth,
                       intermediateHeight)) {
      return;
    }
    g_intermediatePixels.resize(static_cast<size_t>(intermediateWidth) * intermediateHeight);
    for (int y = 0; y < intermediateHeight; ++y) {
      for (int x = 0; x < intermediateWidth; ++x) {
        g_intermediatePixels[static_cast<size_t>(y) * intermediateWidth + x]
            = pixels[static_cast<size_t>(y / integerScale) * width + x / integerScale];
      }
    }
    SDL_SetTextureScaleMode(g_intermediateTexture, SDL_SCALEMODE_LINEAR);
    SDL_UpdateTexture(g_intermediateTexture, nullptr, g_intermediatePixels.data(),
                      intermediateWidth * static_cast<int>(sizeof(uint32_t)));
    frameTexture = g_intermediateTexture;
  } else {
    SDL_UpdateTexture(g_texture, nullptr, pixels, width * static_cast<int>(sizeof(uint32_t)));
  }
  SDL_RenderClear(g_renderer);
  SDL_RenderTexture(g_renderer, frameTexture, nullptr, &g_outputRect);
  if (!g_requestedScreenshotPath.empty()) {
    if (CaptureScreenshot(g_requestedScreenshotPath.c_str())) {
      fprintf(stderr, "minwin: screenshot saved to %s\n", g_requestedScreenshotPath.c_str());
    } else {
      fprintf(stderr, "minwin: screenshot failed: %s\n", SDL_GetError());
    }
    g_requestedScreenshotPath.clear();
  }
  SDL_RenderPresent(g_renderer);
}

int RenderScale()
{
  return OutputScale();
}

int GameplayWidth()
{
  int width = 0, height = 0;
  if (!g_renderer || !SDL_GetRenderOutputSize(g_renderer, &width, &height)) {
    return kWidth;
  }
  return GameplayWidthForOutput(width, height);
}

void LogicalCanvasChanged()
{
  UpdateOutputRect();
  UpdateCursorScale();
}

bool CaptureScreenshot(const char* path)
{
  if (!g_renderer || !path || !*path) {
    return false;
  }
  SDL_Surface* surface = SDL_RenderReadPixels(g_renderer, nullptr);
  if (!surface) {
    return false;
  }
  const bool saved = SDL_SaveBMP(surface, path);
  SDL_DestroySurface(surface);
  return saved;
}

bool CaptureScreenshotPixels(const uint32_t* xrgbPixels, int width, int height, const char* path)
{
  if (!xrgbPixels || width <= 0 || height <= 0 || !path || !*path) {
    return false;
  }
  SDL_Surface* surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_XRGB8888,
                                               const_cast<uint32_t*>(xrgbPixels), width * static_cast<int>(sizeof(uint32_t)));
  if (!surface) {
    return false;
  }
  const bool saved = SDL_SaveBMP(surface, path);
  SDL_DestroySurface(surface);
  return saved;
}

bool RequestScreenshot()
{
  if (!g_requestedScreenshotPath.empty()) {
    return false;
  }
  if (const char* path = getenv("REB_SCREENSHOT_PATH"); path && *path) {
    g_requestedScreenshotPath = path;
    return true;
  }

  char* preferences = SDL_GetPrefPath("Rebellion", "StarWarsRebellion");
  if (!preferences) {
    return false;
  }
  const std::filesystem::path directory = std::filesystem::path(preferences) / "screenshots";
  SDL_free(preferences);
  if (!SDL_CreateDirectory(directory.string().c_str())) {
    return false;
  }

  const time_t now = time(nullptr);
  struct tm localTime{};
#if defined(_WIN32)
  localtime_s(&localTime, &now);
#else
  localtime_r(&now, &localTime);
#endif
  char timestamp[32];
  if (!strftime(timestamp, sizeof(timestamp), "%Y%m%d-%H%M%S", &localTime)) {
    return false;
  }
  for (unsigned int suffix = 0;; ++suffix) {
    char name[64];
    snprintf(name, sizeof(name), "screenshot-%s-%02u.bmp", timestamp, suffix);
    const std::filesystem::path candidate = directory / name;
    if (!std::filesystem::exists(candidate)) {
      g_requestedScreenshotPath = candidate.string();
      return true;
    }
  }
}

void MapMouse(float windowX, float windowY, int* x, int* y)
{
  const minwin_gdi::ScreenSurface canvas = minwin_gdi::Screen();
  float logicalX = windowX, logicalY = windowY;
  if (g_renderer && g_window) {
    int windowWidth = 0, windowHeight = 0;
    SDL_GetWindowSize(g_window, &windowWidth, &windowHeight);
    if (windowWidth > 0 && windowHeight > 0 && g_outputWidth > 0 && g_outputHeight > 0 && g_outputRect.w > 0
        && g_outputRect.h > 0) {
      const float outputX = windowX * g_outputWidth / windowWidth;
      const float outputY = windowY * g_outputHeight / windowHeight;
      logicalX = (outputX - g_outputRect.x) * canvas.width / g_outputRect.w;
      logicalY = (outputY - g_outputRect.y) * canvas.height / g_outputRect.h;
    }
  }
  const int ix = static_cast<int>(floorf(logicalX)), iy = static_cast<int>(floorf(logicalY));
  *x = ix < 0 ? 0 : (ix >= canvas.width ? canvas.width - 1 : ix);
  *y = iy < 0 ? 0 : (iy >= canvas.height ? canvas.height - 1 : iy);
}

bool HandleWindowEvent(const SDL_Event& event)
{
  switch (event.type) {
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
    case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
    case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
    case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
      UpdateOutputRect();
      UpdateCursorScale();
      return true;
    case SDL_EVENT_WINDOW_EXPOSED:
    case SDL_EVENT_WINDOW_RESTORED:
      return true;
    default:
      return false;
  }
}

void ToggleFullscreen()
{
  if (!g_window) {
    return;
  }
  const bool fullscreen = (SDL_GetWindowFlags(g_window) & SDL_WINDOW_FULLSCREEN) != 0;
  SDL_SetWindowFullscreen(g_window, !fullscreen);
}

int CursorScale()
{
  return g_cursorScale;
}

SDL_Cursor* CreateScaledCursor(const uint8_t* rgba, int width, int height, int hotX, int hotY)
{
  const int scale = g_cursorScale;
  const int density = g_window ? static_cast<int>(floorf(SDL_GetWindowPixelDensity(g_window) + 0.5f)) : 1;
  std::vector<uint8_t> base = Enlarge(rgba, width, height, scale);
  SDL_Surface* surface
      = SDL_CreateSurfaceFrom(width * scale, height * scale, SDL_PIXELFORMAT_RGBA32, base.data(), width * scale * 4);
  if (!surface) {
    return nullptr;
  }
  std::vector<uint8_t> sharp;
  SDL_Surface* alternate = nullptr;
  if (density > 1) {
    sharp = Enlarge(rgba, width, height, scale * density);
    alternate = SDL_CreateSurfaceFrom(width * scale * density, height * scale * density, SDL_PIXELFORMAT_RGBA32,
                                      sharp.data(), width * scale * density * 4);
    if (alternate) {
      SDL_AddSurfaceAlternateImage(surface, alternate);
    }
  }
  SDL_Cursor* cursor = SDL_CreateColorCursor(surface, hotX * scale, hotY * scale);
  if (alternate) {
    SDL_DestroySurface(alternate);
  }
  SDL_DestroySurface(surface);
  return cursor;
}

void SyncMouseCapture(bool captured)
{
  if (captured == g_mouseCaptured) {
    return;
  }
  g_mouseCaptured = captured;
  SDL_CaptureMouse(captured);
}

void PaceIdleLoop()
{
  static const uint64_t periodNs = [] {
    if (VirtualClockRequested()) {
      return uint64_t{0};
    }
    const char* text = getenv("REB_MINWIN_PACING_US");
    return (text && *text ? static_cast<uint64_t>(atoll(text)) : uint64_t{250}) * 1000;
  }();
  static uint64_t last = 0;
  if (!periodNs) {
    return;
  }
  const uint64_t now = SDL_GetTicksNS();
  if (now < last + periodNs) {
    SDL_DelayNS(last + periodNs - now);
  }
  last = SDL_GetTicksNS();
}

} // namespace minwin::present
