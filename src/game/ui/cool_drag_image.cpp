#include "ui/cool_drag_image.h"

#include "core/checked_new.h"
#include "gfx/dib_bitmap.h"
#include "ui/cool_app.h"
#include "ui/cool_window.h"

namespace
{

constexpr char kBitBltMode = '@';

} // namespace

CoolDragImage::CoolDragImage(CoolWindow* window)
    : m_image(nullptr)
    , m_window(window)
    , m_saved(nullptr)
    , m_composite(nullptr)
{
  m_window = g_app;
  m_flags = 0;
}

CoolDragImage::~CoolDragImage()
{
  if (IsDragging()) {
    EndDrag();
  }
  ClearImage();
}

int CoolDragImage::IsDragging()
{
  return m_flags & kDragImageDragging;
}

int CoolDragImage::SetImage(DibBitmap* source)
{
  if (IsDragging() || !source) {
    return 0;
  }
  ClearImage();
  auto* copy = NewOrNull<DibBitmap>(source->Width(), source->Height(), source);
  if (!copy) {
    return 0;
  }
  source->Blit(copy, 0, 0, 0, 0, 0, 0);
  m_image = copy;
  return 1;
}

int CoolDragImage::ClearImage()
{
  if (m_image) {
    delete m_image;
  }
  m_image = nullptr;
  return 1;
}

int CoolDragImage::SetHotspot(int x, int y)
{
  if (IsDragging()) {
    return 0;
  }
  m_hotspotX = x;
  m_hotspotY = y;
  return 1;
}

int CoolDragImage::BeginDrag(POINT cursor)
{
  if (IsDragging() || !m_image || !m_window || !m_window->m_hwnd) {
    return 0;
  }
  DibBitmap* image = m_image;
  const int width = m_window->m_width;
  const int height = m_window->m_height;
  m_flags |= kDragImageDragging;
  m_savedStyle = reb::compat::GetWindowLongA(m_window->m_hwnd, GWL_STYLE);
  reb::compat::SetWindowLongA(m_window->m_hwnd, GWL_STYLE, m_savedStyle & ~WS_CLIPCHILDREN);
  HDC dc = reb::compat::GetDC(m_window->m_hwnd);
  if (!dc) {
    reb::compat::SetWindowLongA(m_window->m_hwnd, GWL_STYLE, m_savedStyle);
    m_flags &= ~kDragImageDragging;
    return 0;
  }
  HPALETTE previous = reb::compat::SelectPalette(dc, g_palette, FALSE);
  m_saved = NewOrNull<DibBitmap>(static_cast<LONG>(width), static_cast<LONG>(height), image, dc);
  if (m_saved) {
    m_composite
        = NewOrNull<DibBitmap>(static_cast<LONG>(m_image->Width()), static_cast<LONG>(m_image->Height()), image, dc);
  }
  bool ready = m_saved && m_composite;
  if (ready) {
    POINT topLeft;
    TopLeftFor(&topLeft, &cursor);
    ready = SaveBackground(dc, &topLeft) != 0;
  }
  reb::compat::SelectPalette(dc, previous, FALSE);
  reb::compat::ReleaseDC(m_window->m_hwnd, dc);
  if (!ready) {
    delete m_saved;
    m_saved = nullptr;
    delete m_composite;
    m_composite = nullptr;
    reb::compat::SetWindowLongA(m_window->m_hwnd, GWL_STYLE, m_savedStyle);
    m_flags &= ~kDragImageDragging;
    return 0;
  }
  return 1;
}

int CoolDragImage::EndDrag()
{
  if (!IsDragging()) {
    return 0;
  }
  HDC dc = reb::compat::GetDC(m_window->m_hwnd);
  if (dc) {
    HPALETTE previous = reb::compat::SelectPalette(dc, g_palette, FALSE);
    RestoreBackground(dc);
    reb::compat::SelectPalette(dc, previous, FALSE);
    reb::compat::ReleaseDC(m_window->m_hwnd, dc);
  }
  reb::compat::SetWindowLongA(m_window->m_hwnd, GWL_STYLE, m_savedStyle);
  if (m_saved) {
    delete m_saved;
  }
  m_saved = nullptr;
  if (m_composite) {
    delete m_composite;
  }
  m_composite = nullptr;
  m_flags &= ~kDragImageDragging;
  return 1;
}

int CoolDragImage::MoveTo(const POINT* cursor)
{
  if (!IsDragging()) {
    return 0;
  }
  HDC dc = reb::compat::GetDC(m_window->m_hwnd);
  if (!dc) {
    return 0;
  }
  HPALETTE previous = reb::compat::SelectPalette(dc, g_palette, FALSE);
  RestoreBackground(dc);
  POINT topLeft;
  TopLeftFor(&topLeft, cursor);
  DrawAt(dc, &topLeft);
  reb::compat::SelectPalette(dc, previous, FALSE);
  reb::compat::ReleaseDC(m_window->m_hwnd, dc);
  return 1;
}

int CoolDragImage::SaveBackground(HDC dc, const POINT* topLeft)
{
  if (!m_saved) {
    return 0;
  }
  HDC memory = reb::compat::CreateCompatibleDC(dc);
  if (!memory) {
    return 0;
  }
  HGDIOBJ previousBitmap = reb::compat::SelectObject(memory, m_saved->DibSectionHandle());
  HPALETTE palette = m_saved->CreatePaletteFromColorTable();
  HPALETTE previousPalette = reb::compat::SelectPalette(memory, palette, FALSE);
  reb::compat::BitBlt(memory, 0, 0, m_saved->Width(), m_saved->Height(), dc, 0, 0, SRCCOPY);
  reb::compat::SelectPalette(memory, previousPalette, FALSE);
  reb::compat::SelectObject(memory, previousBitmap);
  reb::compat::DeleteDC(memory);
  reb::compat::DeleteObject(palette);
  m_position = *topLeft;
  return 1;
}

int CoolDragImage::RestoreBackground(HDC dc)
{
  if (!m_saved) {
    return 0;
  }
  const int x = m_position.x;
  const int y = m_position.y;
  m_saved->DrawToDc(dc, kBitBltMode, SRCCOPY, x, y, x, y, m_image->Width(), m_image->Height());
  return 1;
}

int CoolDragImage::DrawAt(HDC dc, const POINT* topLeft)
{
  if (!m_saved) {
    return 0;
  }
  m_saved->Blit(m_composite, 0, 0, topLeft->x, topLeft->y, 0, 0);
  m_image->DrawKeyed(m_composite, 0, 0);
  m_composite->DrawToDc(dc, kBitBltMode, SRCCOPY, topLeft->x, topLeft->y, 0, 0, 0, 0);
  m_position = *topLeft;
  return 1;
}

void CoolDragImage::TopLeftFor(POINT* topLeft, const POINT* cursor)
{
  topLeft->x = cursor->x - m_hotspotX;
  topLeft->y = cursor->y - m_hotspotY;
}
