#include "ui/cool_scroll_bar.h"

#include "core/checked_new.h"
#include "gfx/dib_bitmap.h"
#include "ui/cool_button.h"
#include "ui/cool_scrollable.h"

namespace
{

constexpr uint32_t kScrollBarStyle = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
constexpr int kArrowButtonUnknown = 50;
constexpr char kBitBltMode = '@';
constexpr UINT kDragTimerPeriodMs = 50;
constexpr UINT kPageTimerPeriodMs = 150;
constexpr UINT kParentClickMessage = 0x40e;

enum ArtRecord
{
  kArtThickness = 0,
  kArtTrackTile = 1,
  kArtBackNormal = 2,
  kArtBackPressed = 3,
  kArtForwardNormal = 4,
  kArtForwardPressed = 5,
  kArtThumbTop = 6,
  kArtThumbMiddle = 7,
  kArtThumbBottom = 8,
};

DibBitmap* LoadArtBitmap(HMODULE module, uint16_t id)
{
  return NewOrNull<DibBitmap>(module, static_cast<uint32_t>(id), static_cast<uint32_t>(kDibResource));
}

void DeleteBitmap(DibBitmap* bitmap)
{
  if (bitmap)
    delete bitmap;
}

} // namespace

CoolScrollBar::CoolScrollBar(HINSTANCE instance,
                             int x,
                             int y,
                             int width,
                             int height,
                             CoolWindow* parent,
                             CoolScrollable* owner,
                             uint16_t artSlot,
                             uint16_t artId,
                             int total,
                             int first,
                             int last,
                             uint32_t buttonFlags)
: CoolWindow(instance,
             "Scroll Bar",
             kScrollBarStyle,
             x,
             y,
             width,
             height,
             parent,
             nullptr,
             static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)),
             nullptr,
             nullptr,
             nullptr,
             0,
             0)
{
  m_owner = owner;
  m_artSlot = artSlot;
  m_first = first;
  m_artId = artId;
  m_buttonFlags = buttonFlags;
  m_trackTile = nullptr;
  m_thumbTop = nullptr;
  m_thumbMiddle = nullptr;
  m_thumbBottom = nullptr;
  m_surface = nullptr;
  m_background = nullptr;
  m_track = nullptr;
  m_thumb = nullptr;
  m_backButton = nullptr;
  m_forwardButton = nullptr;
  m_dragging = 0;
  m_previousCapture = nullptr;
  m_timerId = 0;
  m_last = last;
  m_total = total;
}

CoolScrollBar::~CoolScrollBar()
{
  DeleteBitmap(m_trackTile);
  DeleteBitmap(m_track);
  DeleteBitmap(m_thumb);
  DeleteBitmap(m_thumbTop);
  DeleteBitmap(m_thumbMiddle);
  DeleteBitmap(m_thumbBottom);
  DeleteBitmap(m_surface);
  DeleteBitmap(m_background);
}

LRESULT CoolScrollBar::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
  {
    case WM_PAINT:
    {
      HWND hwnd = m_hwnd;
      PAINTSTRUCT paint;
      HDC dc = BeginPaint(hwnd, &paint);
      HPALETTE previous = SelectPalette(dc, g_palette, FALSE);
      Draw(dc);
      SelectPalette(dc, previous, FALSE);
      EndPaint(hwnd, &paint);
      return 0;
    }
    case WM_CREATE:
      if (LoadArt() == 0)
        Refresh();
      return 0;
    case WM_COMMAND:
      OnCommand(wParam);
      return 0;
    case WM_ERASEBKGND:
      return 1;
    case WM_TIMER:
      OnTimer(wParam);
      return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
      PostMessageA(m_parent->m_hwnd, kParentClickMessage, (message << 16) | (m_childId & 0xffff), lParam);
      OnLButtonDown(static_cast<short>(lParam), static_cast<short>(lParam >> 16));
      return 0;
    case WM_LBUTTONUP:
      EndDrag();
      return 0;
    default:
      return CoolWindow::WindowProc(message, wParam, lParam);
  }
}

void CoolScrollBar::CreateArrowButton(CoolWindow*& button,
                                      HMODULE module,
                                      uint16_t normalId,
                                      uint16_t pressedId,
                                      uint32_t childId)
{
  DibBitmap* image = LoadArtBitmap(module, normalId);
  if (image)
  {
    void* memory = ::operator new(sizeof(CoolStrobeButton));
    button = memory ? new (memory)
                        CoolStrobeButton(module, 0, 0, image->Width(), image->Height(), this, childId, normalId,
                                         pressedId, m_trackTile, kArrowButtonUnknown, m_buttonFlags) :
                      nullptr;
    delete image;
  }
  if (button)
  {
    button->Create(0);
    m_children.Insert(button);
  }
}

DWORD CoolScrollBar::LoadArt()
{
  HMODULE module = FindResourceModule(m_artSlot);
  DWORD error = 0;
  HGLOBAL resource = nullptr;
  if (module)
  {
    HRSRC info = FindResourceA(module, MAKEINTRESOURCEA(m_artId), RT_RCDATA);
    if (info)
      resource = LoadResource(module, info);
    else
      error = GetLastError();
  }
  if (resource)
  {
    const auto* art = static_cast<const uint16_t*>(LockResource(resource));
    m_surface = nullptr;
    m_track = nullptr;
    m_thumb = nullptr;
    m_thickness = art[kArtThickness];
    m_trackTile = LoadArtBitmap(module, art[kArtTrackTile]);
    m_thumbTop = LoadArtBitmap(module, art[kArtThumbTop]);
    m_thumbMiddle = LoadArtBitmap(module, art[kArtThumbMiddle]);
    m_thumbBottom = LoadArtBitmap(module, art[kArtThumbBottom]);
    CreateArrowButton(m_backButton, module, art[kArtBackNormal], art[kArtBackPressed], kScrollBackButtonId);
    CreateArrowButton(m_forwardButton, module, art[kArtForwardNormal], art[kArtForwardPressed], kScrollForwardButtonId);
  }
  FreeResource(resource);
  return error;
}

void CoolScrollBar::SetBackground(DibBitmap* parentImage)
{
  HMODULE module = FindResourceModule(m_artSlot);
  if (!parentImage || !module)
    return;
  DeleteBitmap(m_background);
  m_background = NewOrNull<DibBitmap>(m_width, m_height, parentImage);
  if (!m_background)
    return;
  POINT origin = {0, 0};
  if (!m_parent)
    return;
  MapWindowPoints(m_parent->m_hwnd, m_hwnd, &origin, 1);
  parentImage->Blit(m_background, 0, 0, origin.x, origin.y, m_width, m_height);
  Refresh();
}

int CoolScrollBar::Scroll(int delta)
{
  if (!ScrollBy(delta))
    return 0;
  RECT client;
  SetRect(&client, 0, 0, m_width, m_height);
  RECT track;
  GetTrackRect(&track);
  RECT thumb;
  GetThumbRect(&thumb);
  InvalidateRect(m_hwnd, nullptr, FALSE);
  if (!m_surface || !m_track || !m_thumb)
    return 0;
  if (m_background)
    m_background->Blit(m_surface, 0, 0, 0, 0, 0, 0);
  m_track->Blit(m_surface, track.left, track.top, 0, 0, 0, 0);
  m_thumb->Blit(m_surface, thumb.left, thumb.top, 0, 0, 0, 0);
  return 1;
}

int CoolScrollBar::Refresh()
{
  return Layout() ? BuildTrack() : 0;
}

void CoolScrollBar::EndDrag()
{
  HWND hwnd = m_hwnd;
  m_dragging = 0;
  if (GetCapture() == hwnd)
    ReleaseCapture();
  if (m_previousCapture)
  {
    SetCaptureWindow(m_previousCapture);
    m_previousCapture = nullptr;
  }
  StopTimer();
  m_owner->OnScrollEnd();
}

void CoolScrollBar::Draw(HDC dc)
{
  if (m_surface)
    m_surface->DrawToDc(dc, kBitBltMode, SRCCOPY, 0, 0, 0, 0, 0, 0);
}

int CoolScrollBar::SetRange(int total)
{
  if (total < 1)
    return 0;
  m_total = total;
  if (!SetVisibleRange(m_first, m_last))
    return 0;
  return Refresh();
}

int CoolScrollBar::SetVisibleRange(int first, int last)
{
  int result = 1;
  if (first < last)
  {
    const int span = last - first;
    if (span < m_total)
    {
      if (first < 0)
      {
        last = span;
        first = 0;
      }
      if (last > m_total)
      {
        m_last = m_total;
        m_first = first + (m_total - last);
      }
      else
      {
        m_first = first;
        m_last = last;
      }
    }
    else
    {
      m_first = 0;
      m_last = m_total;
    }
  }
  else
  {
    result = 0;
  }
  if (m_first == 0 && m_last == m_total)
    Hide();
  else
    Show(SW_SHOW);
  return result;
}

int CoolScrollBar::ScrollBy(int delta)
{
  const int first = m_first + delta;
  const int last = m_last + delta;
  m_first = first;
  m_last = last;
  if (first < 0)
  {
    m_first = 0;
    m_last = last - first;
    return 1;
  }
  if (last > m_total)
  {
    m_last = m_total;
    m_first = m_total - last + first;
  }
  return 1;
}

void CoolScrollBar::StartTimer(UINT id)
{
  m_timerId = id;
  if (id == kScrollTimerDrag)
    SetTimer(m_hwnd, id, kDragTimerPeriodMs, nullptr);
  else if (id > kScrollTimerDrag && id <= kScrollTimerPageRight)
    SetTimer(m_hwnd, id, kPageTimerPeriodMs, nullptr);
  else
    SetTimer(m_hwnd, id, id, nullptr);
}

void CoolScrollBar::StopTimer()
{
  KillTimer(m_hwnd, m_timerId);
  m_timerId = 0;
}

void CoolScrollBar::OnTimer(WPARAM id)
{
  if (!m_timerId)
    return;
  switch (id)
  {
    case kScrollTimerDrag:
      OnDragTimer();
      break;
    case kScrollTimerPageBack:
      PageBack();
      break;
    case kScrollTimerPageForward:
      PageForward();
      break;
    case kScrollTimerPageLeft:
      PageLeft();
      break;
    case kScrollTimerPageRight:
      PageRight();
      break;
    default:
      break;
  }
}

// Both rectangle getters are this one function in the original (0x60b4d0).
RECT* CoolScrollBar::GetTrackRect(RECT* out)
{
  out->left = 0;
  out->top = 0;
  out->right = 0;
  out->bottom = 0;
  return out;
}

RECT* CoolScrollBar::GetThumbRect(RECT* out)
{
  out->left = 0;
  out->top = 0;
  out->right = 0;
  out->bottom = 0;
  return out;
}

// The remaining defaults are linker-folded with do-nothing / return-zero functions of other classes.
void CoolScrollBar::OnCommand(WPARAM)
{
}

int CoolScrollBar::BuildTrack()
{
  return 0;
}

int CoolScrollBar::BuildThumb()
{
  return 0;
}

int CoolScrollBar::Layout()
{
  return 0;
}

void CoolScrollBar::OnLButtonDown(int, int)
{
}

void CoolScrollBar::OnDragTimer()
{
}

void CoolScrollBar::PageBack()
{
}

void CoolScrollBar::PageForward()
{
}

void CoolScrollBar::PageLeft()
{
}

void CoolScrollBar::PageRight()
{
}
