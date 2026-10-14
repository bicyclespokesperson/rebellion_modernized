// SDL3: the one window, the event pump, presentation of the 8-bit screen surface, MessageBox.
#include <SDL3/SDL.h>

#define MINWIN_IMPLEMENTATION 1
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <vector>

#include "port/minwin_impl/config/config.h"
#include "port/minwin_impl/user/canvas.h"
#include "port/minwin_impl/user/input.h"
#include "port/minwin_impl/user/present.h"
#include "port/minwin_impl/user/user_internal.h"

namespace minwin
{

void ServiceMmTimers();

namespace
{

SDL_Window* g_window = nullptr;
bool g_failed = false;
bool g_everFocused = false;
bool g_outlines = false;
bool g_traceMouse = false;
bool g_pumping = false;
std::vector<uint32_t> g_pixels;
uint32_t g_palette[256];
uint32_t g_paletteGeneration = 0xffffffffu;
int g_lastDeviceScale = 1;
unsigned g_lastPresent = 0;
bool g_gameplayCanvasEnabled = false;
bool g_canvasResizePending = false;
bool g_presentPending = false;

bool OptionEnabled(const char* name, const char* value)
{
  const char* text = getenv(name);
  return text && strstr(text, value);
}

void UpdateLogicalCanvas()
{
  const bool widescreen = mw::config::Global().Get("widescreen", "1") != "0";
  const int width = g_gameplayCanvasEnabled && widescreen ? present::GameplayWidth() : minwin_gdi::kScreenWidth;
  if (!minwin_gdi::SetScreenSize(width, minwin_gdi::kScreenHeight))
    return;
  present::LogicalCanvasChanged();
  g_canvasResizePending = true;
  g_presentPending = true;
  ForEachVisibleWindow([](Window* w, void*) { InvalidateWindowRect(w, nullptr, false); }, nullptr);
}

void ResizeRootForCanvas()
{
  if (!g_canvasResizePending)
    return;
  g_canvasResizePending = false;
  Window* root = FirstTopLevel();
  while (root && root->owner)
    root = root->next;
  if (!root)
    return;
  const minwin_gdi::ScreenSurface screen = minwin_gdi::Screen();
  const int outerWidth = root->width + screen.width - root->clientWidth;
  const int outerHeight = root->height + screen.height - root->clientHeight;
  if (outerWidth != root->width || outerHeight != root->height)
    SetWindowPos(ToHandle(root), nullptr, 0, 0, outerWidth, outerHeight,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void PostCloseRequest()
{
  Window* target = ActiveWindow();
  if (!target)
    target = FirstTopLevel();
  if (target)
    Post(target, WM_SYSCOMMAND, 0xf060, 0);
}

constexpr int kMouseButtons = 4;
bool g_keysDown[SDL_SCANCODE_COUNT];
bool g_buttonsDown[kMouseButtons];
bool g_swallowReturnRelease = false;
int g_mouseX = 0, g_mouseY = 0;
bool g_mousePositionKnown = false;

// Keys and buttons still held when the window loses the keyboard focus are released toward the game: it would otherwise never see
// the release (the keyboard goes to another window) and treat them as held on return.
void ReleaseHeldInput()
{
  for (int sc = 0; sc < SDL_SCANCODE_COUNT; ++sc)
    if (g_keysDown[sc])
    {
      g_keysDown[sc] = false;
      InputKey(sc, false, false, 0);
    }
  for (int button = 1; button < kMouseButtons; ++button)
    if (g_buttonsDown[button])
    {
      g_buttonsDown[button] = false;
      InputMouseButton(button, false, PlatformTicksMs(), g_mouseX, g_mouseY);
    }
}

bool IsAltChord(const SDL_KeyboardEvent& key, SDL_Scancode scancode)
{
  return key.scancode == scancode && (key.mod & SDL_KMOD_ALT);
}

// Alt+Enter and Alt+F4 belong to the window, not to the game. True when the event was consumed.
bool HandleWindowShortcut(const SDL_KeyboardEvent& key)
{
  const bool isReturn = key.scancode == SDL_SCANCODE_RETURN || key.scancode == SDL_SCANCODE_KP_ENTER;
  if (isReturn && !key.down && g_swallowReturnRelease)
  {
    g_swallowReturnRelease = false;
    return true;
  }
  if (key.scancode == SDL_SCANCODE_F12)
  {
    if (key.down && !key.repeat)
      present::RequestScreenshot();
    return true;
  }
  if (!key.down)
    return false;
  if (IsAltChord(key, SDL_SCANCODE_RETURN) || IsAltChord(key, SDL_SCANCODE_KP_ENTER))
  {
    if (!key.repeat)
      present::ToggleFullscreen();
    g_swallowReturnRelease = true;
    return true;
  }
  if (IsAltChord(key, SDL_SCANCODE_F4))
  {
    if (!key.repeat)
      PostCloseRequest();
    return true;
  }
  return false;
}

void ForwardFocusLost()
{
  if (!g_everFocused)
    return;
  ReleaseHeldInput();
  InputFocusChanged(false);
}

void HandleEvent(const SDL_Event& original)
{
  SDL_Event e = original;
  if (e.type >= SDL_EVENT_WINDOW_FIRST && e.type <= SDL_EVENT_WINDOW_LAST && present::HandleWindowEvent(e))
  {
    UpdateLogicalCanvas();
    g_presentPending = true;
  }
  switch (e.type)
  {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
      PostCloseRequest();
      break;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
      g_everFocused = true;
      InputFocusChanged(true);
      break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
    case SDL_EVENT_WINDOW_MINIMIZED:
      ForwardFocusLost();
      break;
    case SDL_EVENT_WINDOW_RESTORED:
      if (g_window && (SDL_GetWindowFlags(g_window) & SDL_WINDOW_INPUT_FOCUS))
        InputFocusChanged(true);
      break;
    case SDL_EVENT_MOUSE_MOTION:
      present::MapMouse(e.motion.x, e.motion.y, &g_mouseX, &g_mouseY);
      g_mousePositionKnown = true;
      if (g_traceMouse)
        fprintf(stderr, "minwin: mouse window (%g,%g) -> game (%d,%d)\n", e.motion.x, e.motion.y, g_mouseX, g_mouseY);
      InputMouseMotion(g_mouseX, g_mouseY);
      break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    {
      const int previousX = g_mouseX;
      const int previousY = g_mouseY;
      present::MapMouse(e.button.x, e.button.y, &g_mouseX, &g_mouseY);
      if (!g_mousePositionKnown || g_mouseX != previousX || g_mouseY != previousY)
        InputMouseMotion(g_mouseX, g_mouseY);
      g_mousePositionKnown = true;
      if (g_traceMouse)
        fprintf(stderr, "minwin: mouse button %u %s window (%g,%g) -> game (%d,%d)\n",
                e.button.button, e.button.down ? "down" : "up", e.button.x, e.button.y, g_mouseX, g_mouseY);
      const int button = e.button.button == SDL_BUTTON_LEFT ? 1 : e.button.button == SDL_BUTTON_RIGHT ? 2 : 3;
      g_buttonsDown[button] = e.button.down;
      InputMouseButton(button, e.button.down, static_cast<unsigned>(e.button.timestamp / 1000000u), g_mouseX, g_mouseY);
      break;
    }
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
      if (HandleWindowShortcut(e.key))
        break;
      if (e.key.scancode < SDL_SCANCODE_COUNT)
        g_keysDown[e.key.scancode] = e.key.down;
      InputKey(static_cast<int>(e.key.scancode), e.key.down, e.key.repeat, 0);
      break;
    case SDL_EVENT_TEXT_INPUT:
      InputText(e.text.text);
      break;
    default:
      break;
  }
}

struct OutlineTarget
{
  uint32_t* pixels;
  int pitch;
  int scale;
};

void DrawOutline(const OutlineTarget& target, RECT r, uint32_t color)
{
  const minwin_gdi::ScreenSurface screen = minwin_gdi::Screen();
  r.left = r.left < 0 ? 0 : r.left;
  r.top = r.top < 0 ? 0 : r.top;
  r.right = r.right > screen.width ? screen.width : r.right;
  r.bottom = r.bottom > screen.height ? screen.height : r.bottom;
  if (IsEmpty(r))
    return;
  for (int x = r.left; x < r.right; ++x)
  {
    for (int sx = 0; sx < target.scale; ++sx)
      for (int sy = 0; sy < target.scale; ++sy)
      {
        target.pixels[(r.top * target.scale + sy) * target.pitch + x * target.scale + sx] = color;
        target.pixels[((r.bottom - 1) * target.scale + sy) * target.pitch + x * target.scale + sx] = color;
      }
  }
  for (int y = r.top; y < r.bottom; ++y)
  {
    for (int sx = 0; sx < target.scale; ++sx)
      for (int sy = 0; sy < target.scale; ++sy)
      {
        target.pixels[(y * target.scale + sy) * target.pitch + r.left * target.scale + sx] = color;
        target.pixels[(y * target.scale + sy) * target.pitch + (r.right - 1) * target.scale + sx] = color;
      }
  }
}

void OutlineVisitor(Window* w, void* raw)
{
  static const uint32_t kColors[] = {0xffff0000, 0xff00ff00, 0xff0000ff, 0xffffff00, 0xffff00ff, 0xff00ffff};
  int depth = 0;
  for (const Window* p = w->parent; p; p = p->parent)
    ++depth;
  DrawOutline(*static_cast<OutlineTarget*>(raw), WindowRectOnScreen(w), kColors[depth % 6]);
}

void InvalidateForDeviceScale(Window* w, void*)
{
  InvalidateWindowRect(w, nullptr, false);
}

} // namespace

bool OutlinesEnabled()
{
  return g_outlines;
}

unsigned PlatformTicksMs()
{
  return static_cast<unsigned>(SDL_GetTicks());
}

bool EnsurePlatform()
{
  if (g_window)
    return true;
  if (g_failed)
    return false;
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
  {
    fprintf(stderr, "minwin: SDL_Init failed: %s\n", SDL_GetError());
    g_failed = true;
    return false;
  }
  if (!present::Create())
  {
    fprintf(stderr, "minwin: cannot create the window: %s\n", SDL_GetError());
    present::Destroy();
    g_failed = true;
    return false;
  }
  g_window = present::Window();
  SDL_StartTextInput(g_window);
  g_outlines = OptionEnabled("REB_MINWIN_TRACE", "outlines");
  g_traceMouse = OptionEnabled("REB_MINWIN_TRACE", "mouse");
  InitInput();
  return true;
}

void ShutdownPlatform()
{
  present::Destroy();
  g_window = nullptr;
}

void SetGameplayCanvasEnabled(bool enabled)
{
  if (g_gameplayCanvasEnabled == enabled)
    return;
  g_gameplayCanvasEnabled = enabled;
  UpdateLogicalCanvas();
}

bool GameplayCanvasEnabled()
{
  return g_gameplayCanvasEnabled;
}

// REB_MINWIN_DUMP=<directory>: when the scenario driver leaves "dump_request" (a name) there, write <name>.idx: the 8-bit frame
// (640x480 palette indices) followed by the 256-entry palette (RGBA bytes), for tools/pixel_compare.py.
void DumpFrameOnRequest(const minwin_gdi::ScreenSurface& screen, const uint32_t* rgb)
{
  static const char* directory = getenv("REB_MINWIN_DUMP");
  if (!directory)
    return;
  char path[512];
  snprintf(path, sizeof path, "%s/dump_request", directory);
  FILE* request = fopen(path, "rb");
  if (!request)
    return;
  char name[128] = {};
  size_t length = fread(name, 1, sizeof name - 1, request);
  fclose(request);
  remove(path);
  while (length && (name[length - 1] == '\n' || name[length - 1] == '\r' || name[length - 1] == ' '))
    name[--length] = 0;
  snprintf(path, sizeof path, "%s/%s.idx", directory, length ? name : "frame");
  FILE* out = fopen(path, "wb");
  if (!out)
    return;
  for (int y = 0; y < screen.height; ++y)
    fwrite(screen.bits + static_cast<size_t>(y) * static_cast<size_t>(screen.pitch), 1, screen.width, out);
  for (int i = 0; i < 256; ++i)
  {
    const uint8_t entry[4] = {static_cast<uint8_t>(rgb[i] >> 16), static_cast<uint8_t>(rgb[i] >> 8),
                              static_cast<uint8_t>(rgb[i]), 255};
    fwrite(entry, 1, 4, out);
  }
  fclose(out);
}

void PresentScreen()
{
  if (!g_window)
    return;
  uint32_t generation = 0;
  uint32_t rgb[256];
  minwin_gdi::CurrentPalette(rgb, &generation);
  if (generation != g_paletteGeneration)
  {
    memcpy(g_palette, rgb, sizeof g_palette);
    g_paletteGeneration = generation;
  }
  DumpFrameOnRequest(minwin_gdi::Screen(), g_palette);
  const minwin_gdi::ScreenSurface screen = minwin_gdi::DeviceScreen(present::RenderScale());
  const int scale = screen.width / minwin_gdi::Screen().width;
  if (scale != g_lastDeviceScale)
  {
    g_lastDeviceScale = scale;
    ForEachVisibleWindow(InvalidateForDeviceScale, nullptr);
  }
  g_pixels.resize(static_cast<size_t>(screen.width) * screen.height);
  for (int y = 0; y < screen.height; ++y)
  {
    const uint8_t* row = screen.bits + static_cast<size_t>(y) * static_cast<size_t>(screen.pitch);
    uint32_t* out = g_pixels.data() + static_cast<size_t>(y) * screen.width;
    for (int x = 0; x < screen.width; ++x)
      out[x] = 0xff000000u | g_palette[row[x]];
  }
  DrawCaretOverlay(g_pixels.data(), screen.width, scale);
  if (g_outlines)
  {
    OutlineTarget target = {g_pixels.data(), screen.width, scale};
    ForEachVisibleWindow(OutlineVisitor, &target);
  }
  present::Present(g_pixels.data(), screen.width, screen.height);
}

void PumpPlatform()
{
  if (g_window)
  {
    SDL_Event e;
    while (SDL_PollEvent(&e))
      HandleEvent(e);
  }
  ResizeRootForCanvas();
  if (g_presentPending)
  {
    g_presentPending = false;
    PresentScreen();
  }
  present::SyncMouseCapture(CaptureWindow() != nullptr);
  if (!g_pumping)
  {
    g_pumping = true;
    ServiceMmTimers();
    g_pumping = false;
  }
  const unsigned now = PlatformTicksMs();
  if (g_window && now - g_lastPresent >= 15)
  {
    g_lastPresent = now;
    PresentScreen();
  }
}

void WaitForPlatformEvent(unsigned maxMs)
{
  if (!maxMs)
    return;
  if (g_window)
    SDL_WaitEventTimeout(nullptr, static_cast<Sint32>(maxMs));
  else
    SDL_Delay(maxMs);
}

void SetSdlTitle(const char* text)
{
  if (g_window && text)
    SDL_SetWindowTitle(g_window, text);
}

int ShowMessageBox(const char* text, const char* caption, unsigned type)
{
  SDL_MessageBoxFlags flags = SDL_MESSAGEBOX_INFORMATION;
  if (type & 0x10)
    flags = SDL_MESSAGEBOX_ERROR;
  else if (type & 0x30)
    flags = SDL_MESSAGEBOX_WARNING;
  if (!g_window)
    SDL_Init(SDL_INIT_VIDEO);
  SDL_ShowSimpleMessageBox(flags, caption ? caption : "", text ? text : "", g_window);
  return 1;
}

} // namespace minwin
