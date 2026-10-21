// The seam between the window manager (src/port/minwin_impl/user: "which window is where and what needs painting") and
// the GDI implementation (src/port/minwin_impl/gdi: "pixels"). The window manager calls these; the GDI side implements
// them.
//
// The authoritative screen surface is 8 bits per pixel, top-down, with palette indices. Window DCs draw into it
// using logical coordinates. DeviceScreen exposes a second indexed surface for presentation at an integer scale;
// game-visible reads and all DIB dimensions remain logical.
//
// The Win32 entry points that take an HDC (BitBlt, SelectObject, FillRect, DrawTextA, ...) are implemented by the GDI
// side completely; user32.h's FillRect and DrawTextA belong to it too. The window manager only creates and destroys the
// DCs of windows (GetDC, GetWindowDC, BeginPaint/EndPaint, ReleaseDC) and of the screen (GetDC(NULL)).
#pragma once
#include <span>
#include <stdint.h>
#include <vector>

#include "port/minwin/minwin.h"

namespace minwin_gdi
{

constexpr int kScreenWidth = 640;
constexpr int kScreenHeight = 480;
constexpr int kMaxScreenWidth = 1280;

struct ScreenSurface {
  uint8_t* bits; // height rows of `pitch` bytes, one palette index per pixel
  int width;
  int height;
  int pitch;
};

// The desktop. Width is 640 in classic mode and may expand during gameplay.
ScreenSurface Screen();
bool SetScreenSize(int width, int height); // preserves the overlapping logical pixels

// Indexed device-resolution presentation surface. Screen() remains the authoritative
// logical surface for all game-visible reads and DIB operations.
ScreenSurface DeviceScreen(int scale);

// Call after software writes through CreateDIBSection's bits pointer. The next
// device blit rebuilds this bitmap from its logical indices.
void InvalidateDeviceBitmap(HBITMAP bitmap);

struct DeviceBitmapPixels {
  std::vector<uint8_t> bits; // top-down, tightly packed
  int scale = 1;
  uint64_t revision = 0;
};

int DeviceScale();
uint64_t DeviceBitmapRevision(HBITMAP bitmap);
bool CopyDeviceBitmapPixels(HBITMAP bitmap, DeviceBitmapPixels& out);
uint64_t SetDeviceBitmapPixels(HBITMAP bitmap, int scale, std::span<const uint8_t> pixels, int x = 0, int y = 0,
                               int width = 0, int height = 0);

// The system palette the surface is displayed through: 256 entries 0x00RRGGBB. `generation` changes whenever any entry
// does.
void CurrentPalette(uint32_t rgb[256], uint32_t* generation);

// What the window manager knows about a window DC when it creates one.
struct WindowDcDesc {
  HWND window; // null for the screen DC
  int originX; // screen coordinates of the DC's (0,0): the client origin (GetDC, BeginPaint) or window origin
               // (GetWindowDC)
  int originY;
  const RECT* clip; // visible area in screen coordinates: non-overlapping rectangles
  int clipCount;    // the GDI side copies them; the pointer is not kept
};

HDC CreateWindowDc(const WindowDcDesc& desc); // GetDC, GetWindowDC, BeginPaint
void DestroyWindowDc(HDC dc);                 // ReleaseDC, EndPaint

} // namespace minwin_gdi

// Window manager services the GDI side may call (src/port/minwin_impl/user/user_api.h declares the rest).
namespace minwin_user
{

// RealizePalette changed the system palette: sends WM_PALETTEISCHANGING (0x310, wParam = `realizer`) to the top-level
// windows, which is all the real user32 does under Wine for the game's first realization (call it only when the palette
// really changed).
void NotifyPaletteChanged(HWND realizer);

// Sends WM_PALETTECHANGED (wParam = `realizer`) to every top-level window, as Wine's user32 does when a palette
// realization or a display mode switch changes the display palette.
void BroadcastPaletteChanged(HWND realizer);

} // namespace minwin_user
