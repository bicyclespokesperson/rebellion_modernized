// Activation, keyboard focus and mouse capture.
#define MINWIN_IMPLEMENTATION 1
#include "port/minwin_impl/user/input.h"
#include "port/minwin_impl/user/user_internal.h"

namespace minwin
{
namespace
{

Window* g_active = nullptr;
Window* g_focus = nullptr;
Window* g_capture = nullptr;
bool g_appActive = false;

constexpr WPARAM kWaInactive = 0, kWaActive = 1, kWaClickActive = 2;
constexpr LPARAM kImeShowAll = static_cast<LPARAM>(0xc000000f);

Window* NextActivatable(const Window* excluding)
{
  for (Window* t = FirstTopLevel(); t; t = t->next)
    if (t != excluding && (t->style & WS_VISIBLE) && !(t->style & kWsDisabled) && !(t->exStyle & kWsExNoActivate))
      return t;
  return nullptr;
}

} // namespace

Window* ActiveWindow()
{
  return g_appActive ? g_active : nullptr;
}
Window* FocusWindow()
{
  return g_focus;
}
Window* CaptureWindow()
{
  return g_capture;
}
bool AppActive()
{
  return g_appActive;
}

void SetFocusWindow(Window* w, bool)
{
  Window* old = g_focus;
  if (w == old)
    return;
  g_focus = w;
  if (old)
  {
    Send(old, WM_KILLFOCUS, reinterpret_cast<WPARAM>(ToHandle(w)), 0);
    Send(old, kWmImeSetContext, 0, kImeShowAll);
  }
  if (w && g_focus == w)
  {
    Send(w, kWmImeSetContext, 1, kImeShowAll);
    if (g_focus == w)
      Send(w, WM_SETFOCUS, reinterpret_cast<WPARAM>(ToHandle(old)), 0);
  }
}

void ActivateApp(bool active)
{
  if (active == g_appActive)
    return;
  g_appActive = active;
  for (Window* t = FirstTopLevel(); t;)
  {
    Window* next = t->next;
    Send(t, WM_ACTIVATEAPP, active ? TRUE : FALSE, 0);
    t = next;
  }
}

void ActivateTopLevel(Window* w, bool mouse)
{
  if (!w || !IsLive(w))
    return;
  if (w == g_active && g_appActive)
    return;
  Window* old = g_active;
  if (!g_appActive)
  {
    g_active = w;
    Send(w, WM_QUERYNEWPALETTE, 0, 0);
    ActivateApp(true);
    if (!IsLive(w))
      return;
    old = nullptr;
  }
  else if (old && IsLive(old))
  {
    Send(old, kWmNcActivate, FALSE, reinterpret_cast<LPARAM>(ToHandle(w)));
    if (IsLive(old))
      Send(old, WM_ACTIVATE, kWaInactive, reinterpret_cast<LPARAM>(ToHandle(w)));
    if (!IsLive(w))
      return;
  }
  g_active = w;
  Send(w, kWmNcActivate, TRUE, 0);
  if (!IsLive(w))
    return;
  Send(w, WM_ACTIVATE, mouse ? kWaClickActive : kWaActive, reinterpret_cast<LPARAM>(ToHandle(old)));
}

// The SDL window lost or regained the system focus: the application is (de)activated like on a task switch.
void InputFocusChanged(bool gained)
{
  Window* active = g_active && IsLive(g_active) ? g_active : nullptr;
  if (!gained)
  {
    if (!g_appActive || !active)
      return;
    Send(active, kWmNcActivate, FALSE, 0);
    if (IsLive(active))
      Send(active, WM_ACTIVATE, kWaInactive, 0);
    ActivateApp(false);
  }
  else if (!g_appActive && active)
  {
    ActivateApp(true);
    if (!IsLive(active))
      return;
    Send(active, kWmNcActivate, TRUE, 0);
    if (IsLive(active))
      Send(active, WM_ACTIVATE, kWaActive, 0);
  }
}

void ReleaseCaptureSilently()
{
  g_capture = nullptr;
}
void SetCaptureWindow(Window* w)
{
  g_capture = w;
}

void OnWindowHidden(Window* w)
{
  if (g_focus && (g_focus == w || IsDescendant(w, g_focus)))
  {
    Window* fallback =
      (g_active && g_active != w && !IsDescendant(w, g_active) && IsWindowVisibleTree(g_active)) ? g_active : nullptr;
    SetFocusWindow(fallback);
  }
  if (g_capture && (g_capture == w || IsDescendant(w, g_capture)))
  {
    Window* old = g_capture;
    g_capture = nullptr;
    Send(old, WM_CAPTURECHANGED, 0, 0);
  }
  if (g_active == w && !w->parent)
  {
    Window* next = NextActivatable(w);
    g_active = nullptr;
    if (next)
      ActivateTopLevel(next, false);
  }
}

void OnWindowDestroyed(Window* w)
{
  if (g_focus == w || (g_focus && IsDescendant(w, g_focus)))
    g_focus = nullptr;
  if (g_capture == w || (g_capture && IsDescendant(w, g_capture)))
    g_capture = nullptr;
  if (g_active == w)
    g_active = nullptr;
}

} // namespace minwin

using namespace minwin;

extern "C" {

HWND REB_STDCALL SetFocus(HWND h)
{
  Window* w = FromHandle(h);
  Window* old = FocusWindow();
  if (!w)
  {
    if (h)
      return nullptr;
    SetFocusWindow(nullptr);
    return ToHandle(old);
  }
  if (!IsEnabledTree(w))
    return nullptr; // like Windows, visibility is not required: only disabled (or minimized) windows refuse
  if (w == old)
    return ToHandle(old);
  Window* top = TopLevelOf(w);
  if (top != g_active && !(top->exStyle & kWsExNoActivate) && top->style & WS_VISIBLE)
  {
    ActivateTopLevel(top, false);
    if (!IsLive(w))
      return ToHandle(old);
    if (FocusWindow() == w)
      return ToHandle(old);
  }
  SetFocusWindow(w);
  return ToHandle(old);
}

HWND REB_STDCALL GetFocus(void)
{
  return ToHandle(FocusWindow());
}
HWND REB_STDCALL GetActiveWindow(void)
{
  return ToHandle(ActiveWindow());
}
HWND REB_STDCALL GetCapture(void)
{
  return ToHandle(CaptureWindow());
}

HWND REB_STDCALL SetCapture(HWND h)
{
  Window* w = FromHandle(h);
  Window* old = CaptureWindow();
  if (!w)
    return nullptr;
  SetCaptureWindow(w);
  if (old && old != w && IsLive(old))
    Send(old, WM_CAPTURECHANGED, 0, reinterpret_cast<LPARAM>(ToHandle(w)));
  return ToHandle(old);
}

BOOL REB_STDCALL ReleaseCapture(void)
{
  Window* old = CaptureWindow();
  ReleaseCaptureSilently();
  if (old && IsLive(old))
    Send(old, WM_CAPTURECHANGED, 0, 0);
  return TRUE;
}

} // extern "C"
