#include "ui/cool_scroll_bars.h"

#include "core/checked_new.h"
#include "gfx/dib_bitmap.h"

namespace
{

// WM_COMMAND codes the bars understand besides the arrow buttons' ids: SB_LINE*/SB_PAGE*.
constexpr uint16_t kCommandLineBack = 0;
constexpr uint16_t kCommandLineForward = 1;
constexpr uint16_t kCommandPageBack = 2;
constexpr uint16_t kCommandPageForward = 3;

constexpr int kMinimumThumbLength = 12;

void TileDown(DibBitmap* tile, DibBitmap* dest, int length)
{
  const int step = tile->Height();
  int y = 0;
  for (int count = length / step + 1; count > 0; --count)
  {
    tile->Blit(dest, 0, y, 0, 0, 0, 0);
    y += step;
  }
}

void TileRight(DibBitmap* tile, DibBitmap* dest, int length)
{
  const int step = tile->Width();
  int x = 0;
  for (int count = length / step + 1; count > 0; --count)
  {
    tile->Blit(dest, x, 0, 0, 0, 0, 0);
    x += step;
  }
}

void DeleteBitmap(DibBitmap*& bitmap)
{
  if (bitmap)
  {
    delete bitmap;
    bitmap = nullptr;
  }
}

POINT CursorIn(HWND hwnd)
{
  POINT cursor;
  GetCursorPos(&cursor);
  MapWindowPoints(nullptr, hwnd, &cursor, 1);
  return cursor;
}

RECT ThumbOf(CoolScrollBar* bar)
{
  RECT thumb;
  bar->GetThumbRect(&thumb);
  return thumb;
}

RECT TrackOf(CoolScrollBar* bar)
{
  RECT track;
  bar->GetTrackRect(&track);
  return track;
}

int CenteredOn(uint16_t thickness, int size)
{
  return (thickness >> 1) - size / 2;
}

} // namespace

// ---- CoolVerticalScrollBar ------------------------------------------------------------------------------------------

CoolVerticalScrollBar::CoolVerticalScrollBar(HINSTANCE instance,
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
: CoolScrollBar(instance, x, y, width, height, parent, owner, artSlot, artId, total, first, last, buttonFlags)
{
}

CoolVerticalScrollBar::~CoolVerticalScrollBar() = default;

void CoolVerticalScrollBar::OnCommand(WPARAM command)
{
  if (!m_owner)
    return;
  int scrolled;
  switch (static_cast<uint16_t>(command))
  {
    case kCommandLineBack:
    case kScrollBackButtonId:
      scrolled = m_owner->LineUp();
      break;
    case kCommandLineForward:
    case kScrollForwardButtonId:
      scrolled = m_owner->LineDown();
      break;
    case kCommandPageBack:
      PageBack();
      return;
    case kCommandPageForward:
      PageForward();
      return;
    default:
      return;
  }
  if (scrolled)
    Scroll(scrolled);
}

int CoolVerticalScrollBar::Layout()
{
  CoolWindow* parent = m_parent;
  if (!parent || !m_backButton || !m_forwardButton)
    return 0;
  const int arrowHeight = m_forwardButton->m_height;
  const int x = parent->m_clientWidth - m_thickness;
  int height = parent->m_clientHeight;
  int downArrowTop = height - arrowHeight;
  if (m_owner->m_horizontalBar)
  {
    height = downArrowTop;
    downArrowTop -= arrowHeight;
  }
  if (!MoveWindow(m_hwnd, x, 0, m_thickness, height, TRUE))
    return 0;
  m_height = height;
  m_width = m_thickness;
  CoolWindow* up = m_backButton;
  if (!MoveWindow(up->m_hwnd, CenteredOn(m_thickness, up->m_width), 0, up->m_width, up->m_height, TRUE))
    return 0;
  CoolWindow* down = m_forwardButton;
  return MoveWindow(down->m_hwnd, CenteredOn(m_thickness, down->m_width), downArrowTop, down->m_width, down->m_height,
                    TRUE);
}

RECT* CoolVerticalScrollBar::GetThumbRect(RECT* out)
{
  const RECT track = TrackOf(this);
  const int trackLength = track.bottom - track.top;
  int width = 0;
  if (m_thumbMiddle)
    width = m_thumbMiddle->Width();
  const int scrollable = m_first - m_last + m_total;
  const int total = m_total > 0 ? m_total : 1;
  int length = (m_last - m_first) * trackLength / total;
  if (length <= kMinimumThumbLength)
    length = kMinimumThumbLength;
  int top = track.top;
  int bottom = track.bottom;
  if (scrollable > 0)
  {
    top = (trackLength - length) * m_first / scrollable + track.top;
    bottom = top + length;
  }
  if (top < track.top)
  {
    bottom += top - track.top;
    top = track.top;
  }
  if (bottom > track.bottom)
  {
    top += track.bottom - bottom;
    bottom = track.bottom;
  }
  if (top < track.top)
    top = track.top;
  const int left = CenteredOn(m_thickness, width);
  RECT thumb;
  SetRect(&thumb, left, top, left + width, bottom);
  *out = thumb;
  return out;
}

RECT* CoolVerticalScrollBar::GetTrackRect(RECT* out)
{
  RECT track = {0, 0, 0, 0};
  if (m_trackTile && m_backButton && m_forwardButton)
  {
    const int left = CenteredOn(m_thickness, m_trackTile->Width());
    SetRect(&track, left, m_backButton->m_height, m_trackTile->Width() + left, m_height - m_forwardButton->m_height);
  }
  *out = track;
  return out;
}

int CoolVerticalScrollBar::BuildThumb()
{
  int drawn = 0;
  DeleteBitmap(m_thumb);
  if (m_thumbTop && m_thumbMiddle && m_thumbBottom)
  {
    const RECT thumb = ThumbOf(this);
    const int height = thumb.bottom - thumb.top;
    m_thumb = NewOrNull<DibBitmap>(static_cast<int>(thumb.right - thumb.left), height, m_thumbTop);
    TileDown(m_thumbMiddle, m_thumb, height);
    m_thumbTop->Blit(m_thumb, 0, 0, 0, 0, 0, 0);
    m_thumbBottom->Blit(m_thumb, 0, height - m_thumbBottom->Height(), 0, 0, 0, 0);
    drawn = Scroll(0);
  }
  return drawn;
}

int CoolVerticalScrollBar::BuildTrack()
{
  int drawn = 0;
  DeleteBitmap(m_surface);
  DeleteBitmap(m_track);
  if (m_trackTile)
  {
    const RECT track = TrackOf(this);
    const int height = track.bottom - track.top;
    m_surface = NewOrNull<DibBitmap>(m_width, m_height, m_trackTile);
    m_track = NewOrNull<DibBitmap>(static_cast<int>(track.right - track.left), height, m_trackTile);
    TileDown(m_trackTile, m_track, height);
    drawn = BuildThumb();
  }
  return drawn;
}

void CoolVerticalScrollBar::OnLButtonDown(int x, int y)
{
  const POINT point = {x, y};
  RECT track = TrackOf(this);
  if (!PtInRect(&track, point))
    return;
  RECT thumb = ThumbOf(this);
  if (PtInRect(&thumb, point))
  {
    m_dragging = 1;
    m_previousCapture = SetCaptureWindow(m_hwnd);
    m_dragX = x;
    m_dragY = y;
    m_dragOffset = y - thumb.top;
    StartTimer(kScrollTimerDrag);
    return;
  }
  if (!m_owner)
    return;
  if (y < thumb.top)
  {
    PageBack();
    if (y < ThumbOf(this).top)
      StartTimer(kScrollTimerPageBack);
  }
  else if (y > thumb.bottom)
  {
    PageForward();
    if (y > ThumbOf(this).bottom)
      StartTimer(kScrollTimerPageForward);
  }
}

void CoolVerticalScrollBar::OnDragTimer()
{
  if (!m_dragging)
    return;
  const POINT cursor = CursorIn(m_hwnd);
  if (cursor.y == m_dragY)
    return;
  const RECT track = TrackOf(this);
  RECT bar = {0, 0, m_width, m_height};
  const int trackLength = track.bottom - track.top;
  const int scrollable = m_total - m_last + m_first;
  if (m_total <= 0)
    return;
  int length = (m_last - m_first) * trackLength / m_total;
  if (length < kMinimumThumbLength && trackLength > kMinimumThumbLength)
    length = kMinimumThumbLength;
  int travel = trackLength - length;
  if (travel < 0)
    travel = 0;
  int thumbTop;
  if (PtInRect(&bar, cursor))
  {
    thumbTop = cursor.y - m_dragOffset;
    if (thumbTop < track.top)
      thumbTop = track.top;
    else if (thumbTop > track.bottom - length)
      thumbTop = track.bottom - length;
  }
  else
  {
    thumbTop = m_dragY - m_dragOffset;
  }
  const int first = travel > 0 ? (thumbTop - track.top) * scrollable / travel : m_first;
  const int delta = first - m_first;
  if (m_owner && delta != 0 && m_owner->ScrollVertical(delta))
    Scroll(delta);
}

void CoolVerticalScrollBar::PageBack()
{
  if (m_owner->PageUp())
    Scroll(m_first - m_last);
  const POINT cursor = CursorIn(m_hwnd);
  if (cursor.y >= ThumbOf(this).top)
    StopTimer();
}

void CoolVerticalScrollBar::PageForward()
{
  if (m_owner->PageDown())
    Scroll(m_last - m_first);
  const POINT cursor = CursorIn(m_hwnd);
  if (cursor.y <= ThumbOf(this).bottom)
    StopTimer();
}

void CoolVerticalScrollBar::PageLeft()
{
}

void CoolVerticalScrollBar::PageRight()
{
}

// ---- CoolHorizontalScrollBar ----------------------------------------------------------------------------------------

CoolHorizontalScrollBar::CoolHorizontalScrollBar(HINSTANCE instance,
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
: CoolScrollBar(instance, x, y, width, height, parent, owner, artSlot, artId, total, first, last, buttonFlags)
{
}

CoolHorizontalScrollBar::~CoolHorizontalScrollBar() = default;

void CoolHorizontalScrollBar::OnCommand(WPARAM command)
{
  if (!m_owner)
    return;
  int scrolled;
  switch (static_cast<uint16_t>(command))
  {
    case kCommandLineBack:
    case kScrollBackButtonId:
      scrolled = m_owner->LineLeft();
      break;
    case kCommandLineForward:
    case kScrollForwardButtonId:
      scrolled = m_owner->LineRight();
      break;
    case kCommandPageBack:
      PageLeft();
      return;
    case kCommandPageForward:
      PageRight();
      return;
    default:
      return;
  }
  if (scrolled)
    Scroll(scrolled);
}

int CoolHorizontalScrollBar::Layout()
{
  int moved = 0;
  CoolWindow* parent = m_parent;
  if (!parent || !m_owner || !m_backButton || !m_forwardButton)
    return 0;
  const int arrowWidth = m_forwardButton->m_width;
  const int y = parent->m_clientHeight - m_thickness;
  int width = parent->m_clientWidth;
  int rightArrowLeft = width - arrowWidth;
  if (m_owner->m_verticalBar)
  {
    width = rightArrowLeft;
    rightArrowLeft -= arrowWidth;
  }
  if (!MoveWindow(m_hwnd, 0, y, width, m_thickness, TRUE))
    return 0;
  m_width = width;
  m_height = m_thickness;
  CoolStrobeButton* left = static_cast<CoolStrobeButton*>(m_backButton);
  const int leftTop = CenteredOn(m_thickness, left->m_height);
  if (!MoveWindow(left->m_hwnd, 0, leftTop, left->m_width, left->m_height, TRUE))
    return 0;
  if (m_background)
  {
    left->m_backdrop = m_background;
    left->m_backdropX = 0;
    left->m_backdropY = leftTop;
  }
  CoolStrobeButton* right = static_cast<CoolStrobeButton*>(m_forwardButton);
  const int rightTop = CenteredOn(m_thickness, right->m_height);
  moved = MoveWindow(right->m_hwnd, rightArrowLeft, rightTop, right->m_width, right->m_height, TRUE);
  if (moved && m_background)
  {
    right->m_backdrop = m_background;
    right->m_backdropX = rightArrowLeft;
    right->m_backdropY = rightTop;
  }
  return moved;
}

RECT* CoolHorizontalScrollBar::GetThumbRect(RECT* out)
{
  const RECT track = TrackOf(this);
  const int trackLength = track.right - track.left;
  int height = 0;
  if (m_thumbMiddle)
    height = m_thumbMiddle->Height();
  int left = m_first * trackLength / m_total + track.left;
  const int top = CenteredOn(m_thickness, height);
  int right = m_last * trackLength / m_total + track.left;
  if (right - left < kMinimumThumbLength)
  {
    if (right - left > trackLength)
    {
      left = 0;
      right = trackLength;
    }
    else
    {
      right += kMinimumThumbLength - (right - left);
      if (right > track.right)
      {
        left = track.right - kMinimumThumbLength;
        right = track.right;
      }
    }
  }
  RECT thumb;
  SetRect(&thumb, left, top, right, top + height);
  *out = thumb;
  return out;
}

RECT* CoolHorizontalScrollBar::GetTrackRect(RECT* out)
{
  RECT track = {0, 0, 0, 0};
  if (m_trackTile && m_backButton && m_forwardButton)
  {
    const int left = m_backButton->m_width;
    const int top = CenteredOn(m_thickness, m_trackTile->Height());
    const int right = m_width - m_forwardButton->m_width;
    SetRect(&track, left, top, right, m_trackTile->Height() + top);
  }
  *out = track;
  return out;
}

int CoolHorizontalScrollBar::BuildThumb()
{
  int drawn = 0;
  DeleteBitmap(m_thumb);
  if (m_thumbTop && m_thumbMiddle && m_thumbBottom)
  {
    const RECT thumb = ThumbOf(this);
    const int width = thumb.right - thumb.left;
    m_thumb = NewOrNull<DibBitmap>(width, static_cast<int>(thumb.bottom - thumb.top), m_thumbTop);
    TileRight(m_thumbMiddle, m_thumb, width);
    m_thumbTop->Blit(m_thumb, 0, 0, 0, 0, 0, 0);
    m_thumbBottom->Blit(m_thumb, width - m_thumbBottom->Width(), 0, 0, 0, 0, 0);
    drawn = Scroll(0);
  }
  return drawn;
}

int CoolHorizontalScrollBar::BuildTrack()
{
  int drawn = 0;
  DeleteBitmap(m_surface);
  DeleteBitmap(m_track);
  if (m_trackTile)
  {
    const RECT track = TrackOf(this);
    const int width = track.right - track.left;
    m_surface = NewOrNull<DibBitmap>(m_width, m_height, m_trackTile);
    m_track = NewOrNull<DibBitmap>(width, static_cast<int>(track.bottom - track.top), m_trackTile);
    TileRight(m_trackTile, m_track, width);
    drawn = BuildThumb();
  }
  return drawn;
}

void CoolHorizontalScrollBar::OnLButtonDown(int x, int y)
{
  const POINT point = {x, y};
  RECT track = TrackOf(this);
  if (!PtInRect(&track, point))
    return;
  RECT thumb = ThumbOf(this);
  if (PtInRect(&thumb, point))
  {
    m_dragging = 1;
    m_previousCapture = SetCaptureWindow(m_hwnd);
    m_dragX = x;
    m_dragY = y;
    m_dragOffset = x - thumb.left;
    StartTimer(kScrollTimerDrag);
    return;
  }
  if (!m_owner)
    return;
  if (x < thumb.left)
  {
    PageLeft();
    StartTimer(kScrollTimerPageLeft);
  }
  else if (x > thumb.right)
  {
    PageRight();
    StartTimer(kScrollTimerPageRight);
  }
}

void CoolHorizontalScrollBar::OnDragTimer()
{
  if (!m_dragging)
    return;
  const POINT cursor = CursorIn(m_hwnd);
  // The thumb follows only cursor moves that change both coordinates.
  if (cursor.x == m_dragX || cursor.y == m_dragY)
    return;
  const RECT track = TrackOf(this);
  RECT bar = {0, 0, m_width, m_height};
  int thumbLeft;
  if (PtInRect(&bar, cursor))
  {
    thumbLeft = cursor.x;
    if (thumbLeft < track.left)
      thumbLeft = track.left;
    else if (thumbLeft > track.right)
      thumbLeft = track.right;
  }
  else
  {
    thumbLeft = m_dragX;
  }
  const int first = (thumbLeft - m_backButton->m_width - m_dragOffset) * m_total / (track.right - track.left);
  const int delta = first - m_first;
  if (m_owner && m_owner->ScrollHorizontal(delta))
    Scroll(delta);
}

void CoolHorizontalScrollBar::PageBack()
{
}

void CoolHorizontalScrollBar::PageForward()
{
}

void CoolHorizontalScrollBar::PageLeft()
{
  if (m_owner->PageLeft())
    Scroll(m_first - m_last);
  const POINT cursor = CursorIn(m_hwnd);
  const RECT thumb = ThumbOf(this);
  if (cursor.x > thumb.left && cursor.x < thumb.right)
    StopTimer();
}

void CoolHorizontalScrollBar::PageRight()
{
  if (m_owner->PageRight())
    Scroll(m_last - m_first);
  const POINT cursor = CursorIn(m_hwnd);
  const RECT thumb = ThumbOf(this);
  if (cursor.x > thumb.left && cursor.x < thumb.right)
    StopTimer();
}
