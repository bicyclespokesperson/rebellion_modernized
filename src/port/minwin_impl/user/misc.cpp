// The small USER32 pieces: system metrics, the caret, cursors, resource-backed loaders (strings, accelerators,
// bitmaps), rectangle helpers, wsprintf and MessageBox.
#include <SDL3/SDL.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include <map>
#include <utility>

#include "port/minwin_impl/res/modules.h"
#include "port/minwin_impl/user/input.h"
#include "port/minwin_impl/user/present.h"
#include "port/minwin_impl/user/user_internal.h"

namespace minwin
{

int ShowMessageBox(const char* text, const char* caption, unsigned type);
void SetSdlTitle(const char* text);

namespace
{

HCURSOR g_cursor = nullptr; // an mw::res::CursorObject: LoadCursorA belongs to the resource library
int g_showCursorCounter = 0;

struct Caret {
  Window* window;
  int width, height;
  int x, y;
  int hidden;
  unsigned created;
} g_caret = {nullptr, 0, 0, 0, 0, 0, 0};

SDL_SystemCursor SystemCursorFor(unsigned id)
{
  switch (id) {
    case 32513:
      return SDL_SYSTEM_CURSOR_TEXT;
    case 32514:
      return SDL_SYSTEM_CURSOR_WAIT;
    case 32515:
      return SDL_SYSTEM_CURSOR_CROSSHAIR;
    case 32642:
      return SDL_SYSTEM_CURSOR_NWSE_RESIZE;
    case 32643:
      return SDL_SYSTEM_CURSOR_NESW_RESIZE;
    case 32644:
      return SDL_SYSTEM_CURSOR_EW_RESIZE;
    case 32645:
      return SDL_SYSTEM_CURSOR_NS_RESIZE;
    case 32646:
      return SDL_SYSTEM_CURSOR_MOVE;
    case 32648:
      return SDL_SYSTEM_CURSOR_NOT_ALLOWED;
    case 32649:
      return SDL_SYSTEM_CURSOR_POINTER;
    default:
      return SDL_SYSTEM_CURSOR_DEFAULT;
  }
}

SDL_Cursor* CreateSdlCursor(const mw::res::CursorObject& cursor)
{
  if (cursor.systemId) {
    return SDL_CreateSystemCursor(SystemCursorFor(static_cast<unsigned>(cursor.systemId)));
  }
  const mw::res::Image& image = cursor.image;
  if (image.width <= 0 || image.height <= 0) {
    return SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
  }
  SDL_Cursor* sdl = present::CreateScaledCursor(image.rgba.data(), image.width, image.height, image.hotX, image.hotY);
  return sdl ? sdl : SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
}

SDL_Cursor* SdlCursorFor(HCURSOR handle)
{
  static std::map<std::pair<const mw::res::CursorObject*, int>, SDL_Cursor*> cache;
  const mw::res::CursorObject* cursor = mw::res::CursorFromHandle(handle);
  if (!cursor) {
    return nullptr;
  }
  const std::pair<const mw::res::CursorObject*, int> key(cursor, present::CursorScale());
  auto found = cache.find(key);
  if (found != cache.end()) {
    return found->second;
  }
  EnsurePlatform();
  return cache[key] = CreateSdlCursor(*cursor);
}

void ApplyCursor()
{
  const bool visible = g_showCursorCounter >= 0 && g_cursor != nullptr;
  if (visible) {
    if (SDL_Cursor* sdl = SdlCursorFor(g_cursor)) {
      SDL_SetCursor(sdl);
    }
    SDL_ShowCursor();
  } else {
    SDL_HideCursor();
  }
}

} // namespace

int g_showCursorCount = 0;

void RefreshCursor()
{
  ApplyCursor();
}

void DrawCaretOverlay(uint32_t* pixels, int pitch, int scale)
{
  if (!g_caret.window || g_caret.hidden > 0 || !IsLive(g_caret.window) || !IsWindowVisibleTree(g_caret.window)) {
    return;
  }
  const unsigned phase = (PlatformTicksMs() - g_caret.created) / 530;
  if (phase & 1) {
    return;
  }
  Region visible;
  ComputeVisibleRegion(g_caret.window, true, (g_caret.window->style & WS_CLIPCHILDREN) != 0, true, &visible);
  const POINT o = ClientOriginOnScreen(g_caret.window);
  const RECT caret
      = MakeRect(o.x + g_caret.x, o.y + g_caret.y, o.x + g_caret.x + g_caret.width, o.y + g_caret.y + g_caret.height);
  for (int i = 0; i < visible.Count(); ++i) {
    RECT r;
    if (!Intersect(&r, caret, visible.Rects()[i])) {
      continue;
    }
    for (int y = r.top; y < r.bottom; ++y) {
      for (int x = r.left; x < r.right; ++x) {
        if (x >= 0 && y >= 0 && x < minwin_gdi::Screen().width && y < minwin_gdi::Screen().height) {
          for (int sy = 0; sy < scale; ++sy) {
            for (int sx = 0; sx < scale; ++sx) {
              pixels[(y * scale + sy) * pitch + x * scale + sx] ^= 0x00ffffffu;
            }
          }
        }
      }
    }
  }
}

} // namespace minwin

using namespace minwin;

namespace
{

HRSRC FindResourceFor(HINSTANCE instance, LPCSTR name, int type)
{
  return reb::compat::FindResourceA(instance ? instance : reb::compat::GetModuleHandleA(nullptr), name, MAKEINTRESOURCE(type));
}

} // namespace

// ---- metrics, rectangles, small stubs
// ---------------------------------------------------------------------------------------

int REB_STDCALL reb::compat::GetSystemMetrics(int index)
{
  switch (index) {
    case SM_CXSCREEN:
    case SM_CXFULLSCREEN:
      return minwin_gdi::Screen().width;
    case SM_CYSCREEN:
    case SM_CYFULLSCREEN:
      return minwin_gdi::Screen().height;
    case SM_CYCAPTION:
      return 19;
    case SM_CXBORDER:
    case SM_CYBORDER:
      return 1;
    case SM_CXDLGFRAME:
    case SM_CYDLGFRAME:
      return 3;
    case SM_MOUSEPRESENT:
      return 1;
    default:
      return 0;
  }
}

BOOL REB_STDCALL reb::compat::SystemParametersInfoA(UINT action, UINT, PVOID param, UINT)
{
  if (action == SPI_GETWORKAREA && param) {
    const minwin_gdi::ScreenSurface screen = minwin_gdi::Screen();
    *static_cast<RECT*>(param) = MakeRect(0, 0, screen.width, screen.height);
    return TRUE;
  }
  return FALSE;
}

HMENU REB_STDCALL reb::compat::GetMenu(HWND)
{
  return nullptr;
}
BOOL REB_STDCALL reb::compat::MessageBeep(UINT)
{
  return TRUE;
}
BOOL REB_STDCALL reb::compat::WinHelpA(HWND, LPCSTR, UINT, ULONG_PTR)
{
  return TRUE;
}
void REB_STDCALL reb::compat::InitCommonControls(void) {}

int REB_STDCALL reb::compat::MessageBoxA(HWND, LPCSTR text, LPCSTR caption, UINT type)
{
  return ShowMessageBox(text, caption, type);
}
int REB_STDCALL reb::compat::MessageBoxExA(HWND, LPCSTR text, LPCSTR caption, UINT type, WORD)
{
  return ShowMessageBox(text, caption, type);
}

BOOL REB_STDCALL reb::compat::SetRect(LPRECT r, int l, int t, int rr, int b)
{
  *r = MakeRect(l, t, rr, b);
  return TRUE;
}
BOOL REB_STDCALL reb::compat::SetRectEmpty(LPRECT r)
{
  *r = MakeRect(0, 0, 0, 0);
  return TRUE;
}
BOOL REB_STDCALL reb::compat::CopyRect(LPRECT dst, const RECT* src)
{
  *dst = *src;
  return TRUE;
}
BOOL REB_STDCALL reb::compat::IntersectRect(LPRECT dst, const RECT* a, const RECT* b)
{
  return Intersect(dst, *a, *b);
}
BOOL REB_STDCALL reb::compat::UnionRect(LPRECT dst, const RECT* a, const RECT* b)
{
  *dst = Union(*a, *b);
  return !IsEmpty(*dst);
}
BOOL REB_STDCALL reb::compat::IsRectEmpty(const RECT* r)
{
  return IsEmpty(*r);
}
BOOL REB_STDCALL reb::compat::PtInRect(const RECT* r, POINT p)
{
  return p.x >= r->left && p.x < r->right && p.y >= r->top && p.y < r->bottom;
}

int REB_CDECL reb::compat::wsprintfA(LPSTR out, LPCSTR format, ...)
{
  va_list args;
  va_start(args, format);
  const int n = vsnprintf(out, 1024, format, args);
  va_end(args);
  return n < 0 ? 0 : (n > 1023 ? 1023 : n);
}

int REB_STDCALL wvsprintfA(LPSTR out, LPCSTR format, va_list args)
{
  const int n = vsnprintf(out, 1024, format, args);
  return n < 0 ? 0 : (n > 1023 ? 1023 : n);
}

// ---- cursor
// ---------------------------------------------------------------------------------------------------------------

HCURSOR REB_STDCALL reb::compat::SetCursor(HCURSOR handle)
{
  HCURSOR old = g_cursor;
  g_cursor = handle;
  ApplyCursor();
  return old;
}

int REB_STDCALL reb::compat::ShowCursor(BOOL show)
{
  g_showCursorCounter += show ? 1 : -1;
  ApplyCursor();
  return g_showCursorCounter;
}

// ---- caret
// ----------------------------------------------------------------------------------------------------------------

BOOL REB_STDCALL reb::compat::CreateCaret(HWND hwnd, HBITMAP, int width, int height)
{
  Window* w = FromHandle(hwnd);
  if (!w) {
    return FALSE;
  }
  g_caret.window = w;
  g_caret.width = width > 0 ? width : 1;
  g_caret.height = height > 0 ? height : 16;
  g_caret.x = g_caret.y = 0;
  g_caret.hidden = 1;
  g_caret.created = PlatformTicksMs();
  return TRUE;
}

BOOL REB_STDCALL reb::compat::DestroyCaret(void)
{
  g_caret.window = nullptr;
  return TRUE;
}

BOOL REB_STDCALL reb::compat::HideCaret(HWND hwnd)
{
  if (hwnd && FromHandle(hwnd) != g_caret.window) {
    return FALSE;
  }
  if (!g_caret.window) {
    return FALSE;
  }
  ++g_caret.hidden;
  return TRUE;
}

BOOL REB_STDCALL reb::compat::ShowCaret(HWND hwnd)
{
  if (hwnd && FromHandle(hwnd) != g_caret.window) {
    return FALSE;
  }
  if (!g_caret.window) {
    return FALSE;
  }
  if (g_caret.hidden > 0) {
    --g_caret.hidden;
  }
  g_caret.created = PlatformTicksMs();
  return TRUE;
}

BOOL REB_STDCALL reb::compat::SetCaretPos(int x, int y)
{
  if (!g_caret.window) {
    return FALSE;
  }
  g_caret.x = x;
  g_caret.y = y;
  g_caret.created = PlatformTicksMs();
  return TRUE;
}

BOOL REB_STDCALL reb::compat::GetCaretPos(LPPOINT p)
{
  if (!g_caret.window) {
    return FALSE;
  }
  p->x = g_caret.x;
  p->y = g_caret.y;
  return TRUE;
}

// ---- bitmap resources (strings, cursors, icons and accelerators are in the res library)
// ------------------------------------------------------------------------------------------------------------

HBITMAP REB_STDCALL reb::compat::LoadBitmapA(HINSTANCE instance, LPCSTR name)
{
  HRSRC block = FindResourceFor(instance, name, 2);
  if (!block) {
    return nullptr;
  }
  const auto* data
      = static_cast<const uint8_t*>(reb::compat::LockResource(reb::compat::LoadResource(instance ? instance : reb::compat::GetModuleHandleA(nullptr), block)));
  if (!data) {
    return nullptr;
  }
  const auto* header = reinterpret_cast<const BITMAPINFOHEADER*>(data);
  unsigned colors = header->biClrUsed;
  if (!colors && header->biBitCount <= 8) {
    colors = 1u << header->biBitCount;
  }
  const uint8_t* bits = data + header->biSize + colors * sizeof(RGBQUAD);
  HDC dc = reb::compat::GetDC(nullptr);
  HBITMAP bitmap
      = reb::compat::CreateDIBitmap(dc, header, CBM_INIT, bits, reinterpret_cast<const BITMAPINFO*>(data), DIB_RGB_COLORS);
  reb::compat::ReleaseDC(nullptr, dc);
  return bitmap;
}

namespace minwin_user
{

// The notification is delivered by the system after the window procedure that realized the palette has returned.
HWND g_pendingRealizer = nullptr;
bool g_paletteNotificationPending = false;

// The first realization is announced with WM_PALETTEISCHANGING after the realizing handler returns; every later one
// that changes the palette is broadcast at once as WM_PALETTECHANGED (what Wine's user32 does, observed with the real
// window messages).
void NotifyPaletteChanged(HWND realizer)
{
  static bool first = true;
  if (first) {
    first = false;
    g_pendingRealizer = realizer;
    g_paletteNotificationPending = true;
    return;
  }
  BroadcastPaletteChanged(realizer);
}

void BroadcastPaletteChanged(HWND realizer)
{
  for (minwin::Window* t = minwin::FirstTopLevel(); t;) {
    minwin::Window* next = t->next;
    minwin::Send(t, WM_PALETTECHANGED, reinterpret_cast<WPARAM>(realizer), 0);
    t = next;
  }
}

} // namespace minwin_user

namespace minwin
{

void FlushPaletteNotification()
{
  if (!minwin_user::g_paletteNotificationPending) {
    return;
  }
  minwin_user::g_paletteNotificationPending = false;
  for (Window* t = FirstTopLevel(); t;) {
    Window* next = t->next;
    Send(t, kWmPaletteIsChanging, reinterpret_cast<WPARAM>(minwin_user::g_pendingRealizer), 0);
    t = next;
  }
}

} // namespace minwin
