// The tooltips_class32 control, as comctl32 v5 (the one Wine ships) behaves for the messages the game sends: tools are windows
// (TTF_IDISHWND) or rectangles of an owner window; the pointer resting on a tool for the initial delay shows the tool's text in a
// small yellow popup under the pointer, a button message, leaving the tool or the auto-pop time hides it again.
#define MINWIN_IMPLEMENTATION 1
#include <string.h>

#include <string>
#include <vector>

#include "port/minwin/comctl32.h"
#include "port/minwin/gdi32.h"
#include "port/minwin_impl/user/user_internal.h"

namespace minwin
{
namespace
{

constexpr UINT_PTR kTimerShow = 1, kTimerPop = 2, kTimerLeave = 3;
constexpr int kNoTool = -1;
constexpr unsigned kLeaveCheckMs = 100;
constexpr unsigned kDefaultInitialMs = 500;
constexpr unsigned kDelayAutomatic = 0, kDelayReshow = 1, kDelayAutopop = 2, kDelayInitial = 3;
constexpr unsigned kDtExternalLeading = 0x200;
constexpr int kTextPadding = 2, kBorder = 1, kPointerHeight = 20;
constexpr int kSysColorInfoText = 23, kSysColorInfoBk = 24, kSysColorWindowFrame = 6;

struct Tool
{
  HWND owner;
  UINT flags;
  UINT_PTR id;
  RECT rect;
  std::string text;
};

struct Tip
{
  HWND hwnd = nullptr;
  bool active = true;
  unsigned initialMs = kDefaultInitialMs, reshowMs = kDefaultInitialMs / 5, autopopMs = kDefaultInitialMs * 10;
  std::vector<Tool> tools;
  int hovered = kNoTool;
  int shownTool = kNoTool;
  std::string shownText;
  HFONT font = nullptr;
};

std::vector<Tip*>& AllTips()
{
  static std::vector<Tip*> tips;
  return tips;
}

Tip* TipOf(HWND hwnd)
{
  for (Tip* t : AllTips())
    if (t->hwnd == hwnd)
      return t;
  return nullptr;
}

COLORREF SystemColor(int index)
{
  switch (index)
  {
    case kSysColorInfoBk:
      return RGB(255, 255, 225);
    case kSysColorWindowFrame:
      return RGB(0, 0, 0);
    default:
      return RGB(0, 0, 0);
  }
}

HFONT StatusFont()
{
  LOGFONTA lf;
  memset(&lf, 0, sizeof lf);
  lf.lfHeight = -11;
  lf.lfWeight = 400;
  lf.lfCharSet = 1;
  strcpy(lf.lfFaceName, "Tahoma");
  return CreateFontIndirectA(&lf);
}

int FindTool(const Tip& tip, const TTTOOLINFOA* info)
{
  for (size_t i = 0; i < tip.tools.size(); ++i)
  {
    const Tool& t = tip.tools[i];
    if (t.owner == info->hwnd && t.id == info->uId)
      return static_cast<int>(i);
  }
  return kNoTool;
}

void StoreText(Tool* tool, const char* text)
{
  tool->text = (text && reinterpret_cast<intptr_t>(text) != -1) ? text : "";
}

int ToolUnderPointer(const Tip& tip)
{
  POINT pt;
  GetCursorPos(&pt);
  HWND under = WindowFromPoint(pt);
  for (size_t i = 0; i < tip.tools.size(); ++i)
  {
    const Tool& t = tip.tools[i];
    if (t.flags & TTF_IDISHWND)
    {
      if (reinterpret_cast<HWND>(t.id) == under)
        return static_cast<int>(i);
      continue;
    }
    if (t.owner != under)
      continue;
    POINT client = pt;
    ScreenToClient(t.owner, &client);
    if (PtInRect(&t.rect, client))
      return static_cast<int>(i);
  }
  return kNoTool;
}

void Hide(Tip* tip)
{
  if (tip->shownTool == kNoTool)
    return;
  KillTimer(tip->hwnd, kTimerPop);
  KillTimer(tip->hwnd, kTimerLeave);
  tip->shownTool = kNoTool;
  SetWindowPos(tip->hwnd, nullptr, 0, 0, 0, 0,
               kSwpNoMove | kSwpNoSize | kSwpNoZOrder | kSwpNoActivate | kSwpHideWindow);
}

SIZE MeasureText(Tip* tip, const std::string& text)
{
  HDC dc = GetDC(tip->hwnd);
  HGDIOBJ old = SelectObject(dc, tip->font);
  RECT rc = MakeRect(0, 0, 0x7fff, 0);
  DrawTextA(dc, text.c_str(), -1, &rc, DT_CALCRECT | DT_NOPREFIX | kDtExternalLeading);
  SelectObject(dc, old);
  ReleaseDC(tip->hwnd, dc);
  SIZE size;
  size.cx = Width(rc);
  size.cy = Height(rc);
  return size;
}

void Show(Tip* tip)
{
  if (tip->hovered == kNoTool || !tip->active)
    return;
  const std::string text = tip->tools[static_cast<size_t>(tip->hovered)].text;
  if (text.empty())
    return;
  if (!tip->font)
    tip->font = StatusFont();
  const SIZE textSize = MeasureText(tip, text);
  const int width = textSize.cx + 2 * (kTextPadding + kBorder);
  const int height = textSize.cy + 2 * (kTextPadding + kBorder);
  POINT pt;
  GetCursorPos(&pt);
  int x = pt.x - kBorder, y = pt.y + kPointerHeight - kBorder;
  const int screenW = GetSystemMetrics(SM_CXSCREEN), screenH = GetSystemMetrics(SM_CYSCREEN);
  if (x + width > screenW)
    x = screenW - width;
  if (x < 0)
    x = 0;
  if (y + height > screenH)
    y = pt.y - height;
  tip->shownTool = tip->hovered;
  tip->shownText = text;
  SetWindowPos(tip->hwnd, reinterpret_cast<HWND>(static_cast<intptr_t>(-1)), x, y, width, height,
               kSwpNoActivate | kSwpShowWindow);
  InvalidateRect(tip->hwnd, nullptr, TRUE);
  SetTimer(tip->hwnd, kTimerPop, tip->autopopMs, nullptr);
  SetTimer(tip->hwnd, kTimerLeave, kLeaveCheckMs, nullptr);
}

void StartShowTimer(Tip* tip, unsigned delayMs)
{
  if (tip->active)
    SetTimer(tip->hwnd, kTimerShow, delayMs, nullptr);
}

void RelayMouseMove(Tip* tip)
{
  const int previous = tip->hovered;
  tip->hovered = ToolUnderPointer(*tip);
  if (tip->hovered == previous)
  {
    if (tip->hovered != kNoTool && tip->shownTool == kNoTool)
      StartShowTimer(tip, tip->initialMs);
    return;
  }
  if (tip->hovered == kNoTool)
  {
    KillTimer(tip->hwnd, kTimerShow);
    Hide(tip);
  }
  else if (previous == kNoTool)
  {
    StartShowTimer(tip, tip->initialMs);
  }
  else
  {
    Hide(tip);
    StartShowTimer(tip, tip->reshowMs);
  }
}

void RelayEvent(Tip* tip, const MSG* msg)
{
  if (!msg)
    return;
  if (msg->message >= WM_LBUTTONDOWN && msg->message <= WM_RBUTTONUP)
  {
    Hide(tip);
    if (tip->hovered != kNoTool)
      StartShowTimer(tip, tip->initialMs);
  }
  else if (msg->message == WM_MOUSEMOVE)
  {
    RelayMouseMove(tip);
  }
}

void SetDelay(Tip* tip, WPARAM which, LPARAM value)
{
  const unsigned ms = static_cast<unsigned>(value & 0xffff);
  switch (static_cast<unsigned>(which))
  {
    case kDelayAutomatic:
      tip->initialMs = ms ? ms : kDefaultInitialMs;
      tip->reshowMs = tip->initialMs / 5;
      tip->autopopMs = tip->initialMs * 10;
      break;
    case kDelayReshow:
      tip->reshowMs = ms;
      break;
    case kDelayAutopop:
      tip->autopopMs = ms;
      break;
    case kDelayInitial:
      tip->initialMs = ms;
      break;
    default:
      break;
  }
}

void OnTimer(Tip* tip, UINT_PTR id)
{
  switch (id)
  {
    case kTimerShow:
      KillTimer(tip->hwnd, kTimerShow);
      if (tip->shownTool == kNoTool)
      {
        tip->hovered = ToolUnderPointer(*tip);
        Show(tip);
      }
      break;
    case kTimerPop:
      Hide(tip);
      break;
    case kTimerLeave:
      if (ToolUnderPointer(*tip) != tip->shownTool)
      {
        tip->hovered = ToolUnderPointer(*tip);
        Hide(tip);
      }
      break;
    default:
      break;
  }
}

// Wine draws the tooltip's WS_BORDER as it comes out on an 8-bit display: the top and right edges in the static grey, the left and
// bottom edges dithered with white (read off the real control's pixels, not derived from the SDK).
void DrawBorder(HDC dc, const RECT& box)
{
  HBRUSH grey = CreateSolidBrush(RGB(128, 128, 128));
  HBRUSH white = CreateSolidBrush(RGB(255, 255, 255));
  RECT top = MakeRect(box.left, box.top, box.right, box.top + kBorder);
  RECT right = MakeRect(box.right - kBorder, box.top, box.right, box.bottom);
  FillRect(dc, &top, grey);
  FillRect(dc, &right, grey);
  for (int y = box.top + kBorder; y < box.bottom; ++y)
  {
    RECT left = MakeRect(box.left, y, box.left + kBorder, y + 1);
    FillRect(dc, &left, ((y - box.top) & 1) ? white : grey);
  }
  for (int x = box.left; x < box.right - kBorder; ++x)
  {
    RECT bottom = MakeRect(x, box.bottom - kBorder, x + 1, box.bottom);
    FillRect(dc, &bottom, ((x - box.left) & 1) ? white : grey);
  }
  DeleteObject(grey);
  DeleteObject(white);
}

void Paint(Tip* tip)
{
  PAINTSTRUCT ps;
  HDC dc = BeginPaint(tip->hwnd, &ps);
  RECT client;
  GetClientRect(tip->hwnd, &client);
  DrawBorder(dc, client);
  RECT inner = MakeRect(client.left + kBorder, client.top + kBorder, client.right - kBorder, client.bottom - kBorder);
  HBRUSH background = CreateSolidBrush(SystemColor(kSysColorInfoBk));
  FillRect(dc, &inner, background);
  DeleteObject(background);
  RECT textRect = MakeRect(inner.left + kTextPadding, inner.top + kTextPadding, inner.right - kTextPadding,
                           inner.bottom - kTextPadding);
  HGDIOBJ old = SelectObject(dc, tip->font);
  SetBkMode(dc, 1);
  SetTextColor(dc, SystemColor(kSysColorInfoText));
  DrawTextA(dc, tip->shownText.c_str(), -1, &textRect, DT_NOPREFIX | kDtExternalLeading);
  SelectObject(dc, old);
  EndPaint(tip->hwnd, &ps);
}

} // namespace

LRESULT REB_STDCALL TooltipProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  Tip* tip = TipOf(hwnd);
  switch (message)
  {
    case WM_NCCREATE:
    {
      Tip* created = new Tip;
      created->hwnd = hwnd;
      AllTips().push_back(created);
      return TRUE;
    }
    case WM_NCDESTROY:
    {
      std::vector<Tip*>& tips = AllTips();
      for (size_t i = 0; i < tips.size(); ++i)
      {
        if (tips[i] != tip)
          continue;
        if (tip->font)
          DeleteObject(tip->font);
        delete tip;
        tips.erase(tips.begin() + static_cast<long>(i));
        break;
      }
      return DefWindowProcA(hwnd, message, wParam, lParam);
    }
    default:
      break;
  }
  if (!tip)
    return DefWindowProcA(hwnd, message, wParam, lParam);
  switch (message)
  {
    case TTM_ACTIVATE:
      tip->active = wParam != 0;
      if (!tip->active)
        Hide(tip);
      return 0;
    case TTM_SETDELAYTIME:
      SetDelay(tip, wParam, lParam);
      return 0;
    case TTM_ADDTOOLA:
    {
      const TTTOOLINFOA* info = reinterpret_cast<const TTTOOLINFOA*>(lParam);
      if (!info)
        return FALSE;
      Tool tool;
      tool.owner = info->hwnd;
      tool.flags = info->uFlags;
      tool.id = info->uId;
      tool.rect = info->rect;
      StoreText(&tool, info->lpszText);
      tip->tools.push_back(tool);
      if (Window* owner = FromHandle(info->hwnd))
        Send(owner, kWmNotifyFormat, reinterpret_cast<WPARAM>(hwnd), 3);
      return TRUE;
    }
    case TTM_DELTOOLA:
    {
      const int index = FindTool(*tip, reinterpret_cast<const TTTOOLINFOA*>(lParam));
      if (index == kNoTool)
        return 0;
      if (tip->shownTool == index)
        Hide(tip);
      tip->tools.erase(tip->tools.begin() + index);
      tip->hovered = tip->hovered == index ? kNoTool : (tip->hovered > index ? tip->hovered - 1 : tip->hovered);
      if (tip->shownTool > index)
        --tip->shownTool;
      return 0;
    }
    case TTM_UPDATETIPTEXTA:
    {
      const TTTOOLINFOA* info = reinterpret_cast<const TTTOOLINFOA*>(lParam);
      const int index = info ? FindTool(*tip, info) : kNoTool;
      if (index != kNoTool)
        StoreText(&tip->tools[static_cast<size_t>(index)], info->lpszText);
      return 0;
    }
    case TTM_RELAYEVENT:
      RelayEvent(tip, reinterpret_cast<const MSG*>(lParam));
      return 0;
    case WM_TIMER:
      OnTimer(tip, static_cast<UINT_PTR>(wParam));
      return 0;
    case kWmNcHitTest:
      return kHtTransparent;
    case WM_ERASEBKGND:
      return TRUE;
    case kWmNcPaint:
      return 0;
    case WM_PAINT:
      Paint(tip);
      return 0;
    default:
      return DefWindowProcA(hwnd, message, wParam, lParam);
  }
}

} // namespace minwin
