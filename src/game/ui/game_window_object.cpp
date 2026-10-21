#include "ui/game_window_object.h"

#include <cstdlib>
#include <cstring>

#include "core/checked_new.h"
#include "gfx/dib_bitmap.h"
#include "ui/resource_dll.h"

namespace
{

constexpr uint32_t kGameWindowStyle = WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
constexpr char kBitBltMode = '@';

} // namespace

GameWindowObject::GameWindowObject(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                                   DibBitmap* background, uint32_t extraStyle)
    : CoolWindow(instance, "GAME_WINDOW_OBJECT", extraStyle | kGameWindowStyle, x, y, width, height, parent, nullptr,
                 static_cast<HBRUSH>(reb::compat::GetStockObject(BLACK_BRUSH)), nullptr, nullptr, nullptr, 0, 0)
{
  m_background = background;
  m_source = nullptr;
}

GameWindowObject::~GameWindowObject()
{
  if (m_background) {
    delete m_background;
  }
  if (m_source) {
    delete m_source;
  }
}

void GameWindowObject::SetBackground(DibBitmap* image, int srcX, int srcY)
{
  HWND hwnd = m_hwnd;
  HDC dc = reb::compat::GetDC(hwnd);
  if (!m_background && m_source) {
    m_background
        = NewOrNull<DibBitmap>(static_cast<LONG>(m_clientWidth), static_cast<LONG>(m_clientHeight), m_source, dc);
  } else if (image && !m_background) {
    m_background = NewOrNull<DibBitmap>(static_cast<LONG>(m_clientWidth), static_cast<LONG>(m_clientHeight), image, dc);
  }

  if (image) {
    if (!m_source) {
      image->Blit(m_background, 0, 0, 0, 0, 0, 0);
    } else if (m_background) {
      m_source->Blit(m_background, 0, 0, srcX, srcY, 0, 0);
      image->DrawKeyed(m_background, 0, 0);
    } else {
      // The image is consumed even when allocation of the background fails.
    }
    delete image;
  } else {
    if (m_background) {
      delete m_background;
    }
    m_background = nullptr;
  }
  reb::compat::ReleaseDC(hwnd, dc);
}

void GameWindowObject::SetBackgroundPreservingAddress(DibBitmap* image, int srcX, int srcY)
{
  if (image && m_background
      && (m_background->Width() != m_clientWidth || std::abs(m_background->Height()) != m_clientHeight)) {
    HDC dc = reb::compat::GetDC(m_hwnd);
    DibBitmap* const format = m_source && m_source != m_background ? m_source : image;
    auto* replacement
        = NewOrNull<DibBitmap>(static_cast<LONG>(m_clientWidth), static_cast<LONG>(m_clientHeight), format, dc);
    if (replacement && replacement->IsLoaded()) {
      m_background->Swap(*replacement);
      delete replacement;
    } else {
      delete replacement;
    }
    reb::compat::ReleaseDC(m_hwnd, dc);
  }
  SetBackground(image, srcX, srcY);
}

DibBitmap* GameWindowObject::GetBackground()
{
  return m_background;
}

void GameWindowObject::Draw(HDC dc)
{
  if (m_background && m_background->IsLoaded() && g_palette) {
    HPALETTE previous = reb::compat::SelectPalette(dc, g_palette, FALSE);
    m_background->DrawToDc(dc, kBitBltMode, SRCCOPY, 0, 0, 0, 0, 0, 0);
    reb::compat::SelectPalette(dc, previous, FALSE);
  }
}

void GameWindowObject::DrawRect(HDC dc, const RECT* area)
{
  if (m_background && m_background->IsLoaded() && g_palette) {
    HPALETTE previous = reb::compat::SelectPalette(dc, g_palette, FALSE);
    const int top = area->top;
    const int left = area->left;
    m_background->DrawToDc(dc, kBitBltMode, SRCCOPY, left, top, left, top, area->right - left, area->bottom - top);
    reb::compat::SelectPalette(dc, previous, FALSE);
  }
}

int GameWindowObject::HandleWindowRequest(int, int, int, intptr_t, int)
{
  return 0;
}

// Linker-folded with other do-nothing virtuals.
void GameWindowObject::TickIdleAnimations() {}

void GameWindowObject::OnIdle() {}

void GameWindowObject::DrawOverlay(HDC) {}

LRESULT GameWindowObject::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
  HWND hwnd = m_hwnd;
  LRESULT result = 0;
  if (message == WM_PAINT) {
    PAINTSTRUCT paint;
    HDC dc = reb::compat::BeginPaint(hwnd, &paint);
    Draw(dc);
    DrawOverlay(dc);
    reb::compat::EndPaint(hwnd, &paint);
    return result;
  }
  if (message == WM_DESTROY) {
    return result;
  }
  if (message == WM_ERASEBKGND) {
    return 1;
  }
  if (message == WM_CTLCOLORBTN) {
    return reinterpret_cast<LRESULT>(reb::compat::GetStockObject(NULL_BRUSH));
  }
  if (message == WM_QUERYNEWPALETTE || message == WM_PALETTECHANGED) {
    if (message == WM_PALETTECHANGED) {
      reb::compat::InvalidateRect(hwnd, nullptr, FALSE);
      for (HWND child = reb::compat::GetWindow(hwnd, GW_CHILD); child; child = reb::compat::GetWindow(child, GW_HWNDNEXT)) {
        reb::compat::SendMessageA(child, WM_PALETTECHANGED, wParam, lParam);
      }
      if (hwnd == reinterpret_cast<HWND>(wParam)) {
        return result;
      }
    }
    HDC dc = reb::compat::GetDC(hwnd);
    HPALETTE previous = reb::compat::SelectPalette(dc, g_palette, FALSE);
    result = reb::compat::RealizePalette(dc);
    reb::compat::SelectPalette(dc, previous, FALSE);
    if (result) {
      reb::compat::InvalidateRect(hwnd, nullptr, FALSE);
    }
    reb::compat::ReleaseDC(hwnd, dc);
    return result;
  }
  return CoolWindow::WindowProc(message, wParam, lParam);
}

int GameWindowObject::DrawStringResource(UINT stringId, RECT area, int fontSlot, COLORREF color, UINT format)
{
  char text[0x100];
  std::memset(text, 0, 0xff);
  bool drawn = false;
  LoadResourceString(stringId, text, 0xff);
  if (text[0] == '\0') {
    return drawn;
  }
  DibBitmap* target = m_source ? m_source : m_background;
  if (!target) {
    return drawn;
  }

  HWND hwnd = m_hwnd;
  HDC screen = reb::compat::GetDC(hwnd);
  HDC memory = reb::compat::CreateCompatibleDC(screen);
  HFONT font = CoolGetFont(fontSlot);
  drawn = memory != nullptr;
  if (drawn) {
    HPALETTE palette = g_palette;
    HGDIOBJ previousBitmap = reb::compat::SelectObject(memory, target->DibSectionHandle());
    HPALETTE previousPalette = reb::compat::SelectPalette(memory, palette, FALSE);
    HGDIOBJ previousFont = reb::compat::SelectObject(memory, font);
    reb::compat::SetTextColor(memory, color);
    reb::compat::SetBkMode(memory, TRANSPARENT);
    reb::compat::DrawTextA(memory, text, -1, &area, format);
    reb::compat::SelectPalette(memory, previousPalette, FALSE);
    reb::compat::SelectObject(memory, previousFont);
    reb::compat::SelectObject(memory, previousBitmap);
    reb::compat::DeleteDC(memory);
  }
  reb::compat::ReleaseDC(hwnd, screen);
  return drawn;
}
