#include "ui/bitmap_status_bar.h"

#include "core/checked_new.h"
#include "gfx/dib_bitmap.h"

namespace
{

constexpr uint32_t kBarStyle = 0x54000000; // WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS

// The bar's length along an axis of `extent` pixels.
uint32_t BarLength(int extent, uint32_t position, uint32_t range)
{
  return range ? static_cast<uint32_t>(extent) * position / range : 0;
}

} // namespace

BitmapStatusBar::BitmapStatusBar(int x, int y, int width, int height, CoolWindow* parent, COLORREF barColor,
                                 COLORREF backgroundColor, DibBitmap* picture, int orientation, int direction)
    : CoolwinStatusBar(x, y, width, height, parent, barColor, backgroundColor, kBarStyle)
{
  m_canvas = nullptr;
  m_picture = nullptr;
  m_orientation = orientation;
  m_direction = direction;
  if (picture) {
    SetPicture(NewOrNull<DibBitmap>(picture->Width(), picture->Height(), picture));
  }
}

BitmapStatusBar::~BitmapStatusBar()
{
  if (m_picture) {
    delete m_picture;
  }
  if (m_canvas) {
    delete m_canvas;
  }
}

void BitmapStatusBar::SetPicture(DibBitmap* image)
{
  if (!image) {
    return;
  }
  HDC dc = reb::compat::GetDC(m_hwnd);
  delete m_picture;
  delete m_canvas;
  m_picture = nullptr;
  m_canvas = nullptr;
  if (image->IsLoaded()) {
    const int width = image->Width();
    const int height = image->Height();
    m_picture = NewOrNull<DibBitmap>(width, height, image);
    if (dc) {
      m_canvas = NewOrNull<DibBitmap>(static_cast<LONG>(width), static_cast<LONG>(height), image, dc);
    }
    if (m_picture && m_canvas) {
      image->Blit(m_picture, 0, 0, 0, 0, 0, 0);
      reb::compat::MoveWindow(m_hwnd, m_x, m_y, width, height - 1, FALSE);
    } else {
      delete m_picture;
      delete m_canvas;
      m_picture = nullptr;
      m_canvas = nullptr;
    }
  }
  delete image;
  if (dc) {
    reb::compat::ReleaseDC(m_hwnd, dc);
  }
}

LRESULT BitmapStatusBar::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
  HWND hwnd = m_hwnd;
  switch (message) {
    case WM_ERASEBKGND:
      return 1;
    case WM_CTLCOLORBTN:
      return reinterpret_cast<LRESULT>(reb::compat::GetStockObject(NULL_BRUSH));
    case WM_PALETTECHANGED:
      if (reinterpret_cast<HWND>(wParam) == hwnd) {
        return 0;
      }
      [[fallthrough]];
    case WM_QUERYNEWPALETTE:
      {
        HDC dc = reb::compat::GetDC(hwnd);
        HPALETTE previous = reb::compat::SelectPalette(dc, g_palette, FALSE);
        const UINT realized = reb::compat::RealizePalette(dc);
        reb::compat::SelectPalette(dc, previous, FALSE);
        reb::compat::InvalidateRect(hwnd, nullptr, FALSE);
        reb::compat::ReleaseDC(hwnd, dc);
        return realized;
      }
    case WM_PAINT:
      break;
    default:
      return CoolwinStatusBar::WindowProc(message, wParam, lParam);
  }

  PAINTSTRUCT paint;
  HDC dc = reb::compat::BeginPaint(hwnd, &paint);
  HPALETTE previousPalette = reb::compat::SelectPalette(dc, g_palette, FALSE);
  HDC memory = reb::compat::CreateCompatibleDC(dc);
  HBITMAP scratch = nullptr;
  HGDIOBJ previousBitmap;
  if (m_picture && m_canvas) {
    previousBitmap = reb::compat::SelectObject(memory, m_canvas->DibSectionHandle());
  } else {
    scratch = reb::compat::CreateCompatibleBitmap(dc, m_clientWidth, m_clientHeight);
    previousBitmap = reb::compat::SelectObject(memory, scratch);
  }
  if (m_backgroundBrush) {
    HGDIOBJ previousBrush = reb::compat::SelectObject(memory, m_backgroundBrush);
    HGDIOBJ previousPen = reb::compat::SelectObject(memory, reb::compat::GetStockObject(NULL_PEN));
    reb::compat::Rectangle(memory, 0, 0, m_clientWidth + 1, m_clientHeight + 1);
    reb::compat::SelectObject(memory, previousBrush);
    reb::compat::SelectObject(memory, previousPen);
  }
  if (m_barBrush && m_position) {
    HGDIOBJ previousBrush = reb::compat::SelectObject(memory, m_barBrush);
    HGDIOBJ previousPen = reb::compat::SelectObject(memory, reb::compat::GetStockObject(NULL_PEN));
    const int right = m_clientWidth + 1;
    const int bottom = m_clientHeight + 1;
    if (m_orientation == kBarHorizontal) {
      const uint32_t length = BarLength(m_clientWidth, m_position, m_range);
      if (m_direction == kBarFromStart) {
        reb::compat::Rectangle(memory, 0, 0, static_cast<int>(length + 1), bottom);
      } else if (m_direction == kBarFromEnd) {
        reb::compat::Rectangle(memory, static_cast<int>(m_clientWidth - length), 0, right, bottom);
      }
    } else if (m_orientation == kBarVertical) {
      const uint32_t length = BarLength(m_clientHeight, m_position, m_range);
      if (m_direction == kBarFromStart) {
        reb::compat::Rectangle(memory, 0, 0, right, static_cast<int>(length + 1));
      } else if (m_direction == kBarFromEnd) {
        reb::compat::Rectangle(memory, 0, static_cast<int>(m_clientHeight - length), right, bottom);
      }
    }
    reb::compat::SelectObject(memory, previousBrush);
    reb::compat::SelectObject(memory, previousPen);
  }
  if (m_tickStep) {
    HGDIOBJ pen = m_tickPen ? static_cast<HGDIOBJ>(m_tickPen) : reb::compat::GetStockObject(BLACK_PEN);
    HGDIOBJ previousPen = reb::compat::SelectObject(memory, pen);
    if (m_orientation == kBarHorizontal) {
      for (uint32_t tick = 0; tick < m_range; tick += m_tickStep) {
        const int x = static_cast<int>(tick * static_cast<uint32_t>(m_clientWidth) / m_range);
        reb::compat::MoveToEx(memory, x, 0, nullptr);
        reb::compat::LineTo(memory, x, m_clientHeight);
      }
    } else if (m_orientation == kBarVertical) {
      for (uint32_t tick = 0; tick < m_range; tick += m_tickStep) {
        const int y = static_cast<int>(tick * static_cast<uint32_t>(m_clientHeight) / m_range);
        reb::compat::MoveToEx(memory, 0, y, nullptr);
        reb::compat::LineTo(memory, m_clientWidth, y);
      }
    }
    reb::compat::SelectObject(memory, previousPen);
  }
  if (m_picture && m_canvas) {
    m_picture->DrawKeyed(m_canvas, 0, 0);
  }
  reb::compat::BitBlt(dc, 0, 0, m_clientWidth, m_clientHeight, memory, 0, 0, SRCCOPY);
  reb::compat::SelectObject(memory, previousBitmap);
  if (scratch) {
    reb::compat::DeleteObject(scratch);
  }
  reb::compat::DeleteDC(memory);
  reb::compat::SelectPalette(dc, previousPalette, FALSE);
  reb::compat::EndPaint(hwnd, &paint);
  return 0;
}
