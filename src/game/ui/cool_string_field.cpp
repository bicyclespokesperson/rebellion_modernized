#include "ui/cool_string_field.h"
#include "core/game_string.h"

#include <cctype>
#include <cstring>

#include "core/checked_new.h"
#include "gfx/dib_bitmap.h"
#include "ui/cool_app.h"
#include "ui/resource_dll.h"


namespace
{

constexpr uint32_t kFieldStyle = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
constexpr UINT kMeasureFormat = DT_CALCRECT | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER;
constexpr UINT kShadowedTextFormat = DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER;
constexpr int kMaxFieldLength = 0x103;
constexpr size_t kTruncationBufferSize = 260;

bool IsShiftHeld()
{
  return (static_cast<uint16_t>(reb::compat::GetKeyState(VK_SHIFT)) & 0xfffe) != 0;
}

} // namespace

CoolStringField::CoolStringField(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                                 DibBitmap* backdrop, uint32_t extraStyle)
    : CoolWindow(instance, "CoolStringField", extraStyle | kFieldStyle, x, y, width, height, parent, nullptr,
                 static_cast<HBRUSH>(reb::compat::GetStockObject(NULL_BRUSH)), nullptr, nullptr, nullptr, 0, 0)
{
  m_flags = kFieldOverBackdrop;
  m_backdrop = backdrop;
  m_maxLength = 0;
  m_backdropX = x;
  m_backdropY = y;
  HDC dc = reb::compat::GetDC(parent->m_hwnd);
  m_shadowOffset = 1;
  m_viewWidth = width;
  m_viewHeight = height;
  m_buffer = NewOrNull<DibBitmap>(static_cast<LONG>((m_shadowOffset + width) * 2),
                                  static_cast<LONG>((height + m_shadowOffset) * 2), backdrop, dc);
  m_fillBitmap = nullptr;
  reb::compat::ReleaseDC(parent->m_hwnd, dc);
  m_shadowColor = 0;
  m_selLeftPixel = 0;
  m_selRightPixel = width;
  m_length = 0;
  m_scroll = 0;
  m_textColor = 0xffffff;
  m_unknownEc = 0xff;
  m_unknownF0 = 0x808080;
  m_shadowOffset = 1;
  m_selStart = -1;
  m_selEnd = -1;
  m_caret = -1;
}

CoolStringField::~CoolStringField()
{
  if (m_buffer) {
    delete m_buffer;
  }
  if (m_fillBitmap) {
    delete m_fillBitmap;
  }
}

void CoolStringField::SetFont(int fontSlot)
{
  CoolWindow::SetFont(fontSlot);
  MeasureText();
  UpdateCaret();
}

void CoolStringField::SetText(const char* text)
{
  if (!text) {
    Clear();
    m_length = 0;
    m_scroll = 0;
    m_caret = -1;
    return;
  }
  char truncated[kTruncationBufferSize];
  if (m_maxLength != 0 && static_cast<uint32_t>(m_maxLength) <= static_cast<uint32_t>(std::strlen(text))) {
    std::memset(truncated, 0, sizeof(truncated));
    std::memcpy(truncated, text, m_maxLength - 1);
    text = truncated;
  }
  m_text = text;
  m_caret = -1;
  m_length = m_text.size() & 0xffff;
  if (m_hwnd) {
    MeasureText();
    UpdateCaret();
    reb::compat::InvalidateRect(m_hwnd, nullptr, FALSE);
  }
}

void CoolStringField::Clear()
{
  m_text.clear();
  m_caret = -1;
  m_length = 0;
  m_scroll = 0;
  UpdateCaret();
  reb::compat::InvalidateRect(m_hwnd, nullptr, FALSE);
}

void CoolStringField::SetMaxLength(uint16_t maxLength)
{
  if (maxLength < kMaxFieldLength + 1) {
    m_maxLength = maxLength;
  } else {
    m_maxLength = kMaxFieldLength;
  }
  if (m_maxLength < m_length) {
    SetText(m_text.c_str());
  }
}

void CoolStringField::SetSelection(int start, int end)
{
  if (start != m_selStart || end != m_selEnd) {
    reb::compat::InvalidateRect(m_hwnd, nullptr, FALSE);
  }
  m_selStart = start;
  m_selEnd = end;
  MeasureText();
  UpdateCaret();
}

void CoolStringField::DeleteSelection()
{
  int start = m_selStart;
  int end = m_selEnd;
  if (start == end) {
    return;
  }
  if (start == -1 && end > -1) {
    start = m_length;
  } else {
    if (end == -1 && start > -1) {
      RemoveStringRange(m_text, start, m_length);
      m_caret = m_selEnd;
      m_selEnd = -1;
      m_selStart = -1;
      return;
    }
    if (start <= end) {
      RemoveStringRange(m_text, start, end);
      m_caret = m_selStart;
      m_selEnd = -1;
      m_selStart = -1;
      return;
    }
  }
  RemoveStringRange(m_text, end, start);
  m_caret = m_selEnd;
  m_selEnd = -1;
  m_selStart = -1;
}

void CoolStringField::MeasureText()
{
  const char* text = m_text.c_str();
  HDC dc = reb::compat::GetDC(m_hwnd);
  RECT bounds = {0, 0, 0, 0};
  if (text) {
    SelectCurrentFont(dc);
    reb::compat::DrawTextA(dc, text, -1, &bounds, kMeasureFormat);
    LONG width = bounds.right;
    if (bounds.right <= m_clientWidth) {
      width = m_clientWidth;
    }
    if (m_buffer->Width() < width) {
      if (m_buffer) {
        delete m_buffer;
      }
      m_buffer = NewOrNull<DibBitmap>(static_cast<LONG>((width + m_shadowOffset) * 2),
                                      static_cast<LONG>((m_clientHeight + m_shadowOffset) * 2), m_backdrop, dc);
    }
    RestoreFont(dc);
    m_length = m_text.size() & 0xffff;
  }
  reb::compat::ReleaseDC(m_hwnd, dc);
}

void CoolStringField::UpdateCaret()
{
  HDC dc = reb::compat::GetDC(m_hwnd);
  const char* text = m_text.c_str();
  int scroll = m_scroll;
  RECT bounds = {0, 0, 0, 0};
  if (text) {
    SelectCurrentFont(dc);
    const int bufferSize = m_text.empty() ? 0 : static_cast<int>((m_text.size() + 1) & 0xffff);
    if (bufferSize < m_caret) {
      m_caret = bufferSize;
    }
    reb::compat::DrawTextA(dc, text, m_caret, &bounds, kMeasureFormat);
    const LONG caretRight = bounds.right;
    const int selEnd = m_selEnd;
    if (m_selStart != selEnd) {
      LONG right;
      if (selEnd < 0) {
        right = m_buffer->Width() - m_shadowOffset;
      } else {
        reb::compat::DrawTextA(dc, text, selEnd, &bounds, kMeasureFormat);
        right = bounds.right;
      }
      m_selRightPixel = right;
      const int start = m_selStart;
      if (start < 1) {
        if (start == -1) {
          m_selLeftPixel = m_buffer->Width() - m_shadowOffset;
        } else {
          m_selLeftPixel = 0;
        }
      } else {
        reb::compat::DrawTextA(dc, text, start, &bounds, kMeasureFormat);
        m_selLeftPixel = bounds.right;
      }
      const int left = m_selLeftPixel;
      if (m_selRightPixel < left) {
        m_selLeftPixel = m_selRightPixel;
        m_selRightPixel = left;
      }
    }
    RestoreFont(dc);
    int caretX = caretRight - m_scroll;
    if (m_clientWidth - m_shadowOffset < caretX) {
      scroll = m_shadowOffset + caretRight - m_clientWidth;
      caretX = caretRight - scroll;
    } else if (caretX < 0) {
      scroll = m_scroll + caretX;
      caretX = caretRight - scroll;
    }
    if (m_flags & kFieldFocused) {
      reb::compat::SetCaretPos(caretX, 0);
    }
  }
  if (scroll != m_scroll) {
    m_scroll = scroll;
    if (m_hwnd) {
      reb::compat::InvalidateRect(m_hwnd, nullptr, FALSE);
    }
  }
  reb::compat::ReleaseDC(m_hwnd, dc);
}

int CoolStringField::CharIndexFromX(int x, int)
{
  int target = m_scroll + x;
  const char* text = m_text.c_str();
  int count = 0;
  HDC dc = reb::compat::GetDC(m_hwnd);
  RECT bounds = {0, 0, 0, 0};
  SelectCurrentFont(dc);
  if (target > 0) {
    do {
      ++count;
      reb::compat::DrawTextA(dc, text, count, &bounds, kMeasureFormat);
      if (m_length < count) {
        count = m_length + 1;
        target = bounds.right;
      }
    } while (bounds.right < target);
  }
  if (count > 0) {
    --count;
  }
  RestoreFont(dc);
  reb::compat::ReleaseDC(m_hwnd, dc);
  return count;
}

namespace
{

void NotifyChanged(CoolStringField* field)
{
  reb::compat::PostMessageA(field->m_parent->m_hwnd, kFieldChangedMessage, field->m_childId, 0);
}

void ClearSelection(CoolStringField* field)
{
  field->m_selEnd = -1;
  field->m_selStart = -1;
}

void PaintField(CoolStringField* field)
{
  HWND hwnd = field->m_hwnd;
  const int length = field->m_text.size() & 0xffff;
  int low = field->m_selStart < 0 ? 0 : field->m_selStart;
  int high = length;
  if (field->m_selEnd >= 0) {
    high = field->m_selEnd;
  }
  if (high < low) {
    const int swapped = low;
    low = high;
    high = swapped;
  }
  const int margin = field->m_shadowOffset;
  RECT area;
  area.left = 0;
  area.top = 0;
  area.right = field->m_buffer->Width() - margin;
  area.bottom = field->m_clientHeight - margin;

  PAINTSTRUCT paint;
  HDC screen = reb::compat::BeginPaint(hwnd, &paint);
  HDC memory = reb::compat::CreateCompatibleDC(screen);
  field->SelectCurrentFont(memory);
  HGDIOBJ previousBitmap = reb::compat::SelectObject(memory, field->m_buffer->DibSectionHandle());
  HPALETTE previousPalette = reb::compat::SelectPalette(memory, g_palette, FALSE);
  TEXTMETRICA metrics;
  reb::compat::GetTextMetricsA(memory, &metrics);

  bool setTransparent = true;
  if (!(field->m_flags & kFieldOverBackdrop)) {
    DibBitmap* fill = field->m_fillBitmap;
    if (!fill || !(field->m_flags & kFieldFilled)) {
      setTransparent = false;
    } else {
      fill->Blit(field->m_buffer, field->m_scroll, 0, 0, 0, field->m_viewWidth, field->m_viewHeight);
    }
  } else if (field->m_backdrop) {
    field->m_backdrop->Blit(field->m_buffer, field->m_scroll, 0, field->m_backdropX, field->m_backdropY,
                            field->m_viewWidth, field->m_viewHeight);
  }
  if (setTransparent) {
    reb::compat::SetBkMode(memory, TRANSPARENT);
  }

  if (!(field->m_flags & kFieldFocused) || field->m_selStart == field->m_selEnd) {
    DrawShadowedText(memory, field->m_text.c_str(), &area, field->m_textColor, field->m_shadowColor,
                     kShadowedTextFormat, margin, -1);
  } else {
    StringCursor cursor(field->m_text);
    cursor.Rewind();
    if (low != high && low > 0) {
      area.right = field->m_selLeftPixel;
      area.left = 0;
      DrawShadowedText(memory, field->m_text.c_str(), &area, field->m_textColor, field->m_shadowColor,
                       kShadowedTextFormat, margin, low);
    }
    area.right = field->m_selRightPixel;
    area.left = field->m_selLeftPixel;
    for (int skipped = low; skipped > 0; --skipped) {
      ++cursor;
    }
    int selected = high - low;
    reb::compat::SetBkMode(memory, OPAQUE);
    reb::compat::SetBkColor(memory, field->m_textColor);
    reb::compat::SetTextColor(memory, field->m_shadowColor);
    reb::compat::DrawTextA(memory, cursor.m_current, selected, &area, kShadowedTextFormat);
    reb::compat::SetBkMode(memory, TRANSPARENT);
    if (high < length) {
      for (; selected > 0; --selected) {
        ++cursor;
      }
      area.left = field->m_selRightPixel;
      area.right = field->m_buffer->Width() - margin;
      DrawShadowedText(memory, cursor.m_current, &area, field->m_textColor, field->m_shadowColor,
                       kShadowedTextFormat & ~DT_NOPREFIX, margin, -1);
    }
  }
  HPALETTE screenPalette = reb::compat::SelectPalette(screen, g_palette, FALSE);
  reb::compat::BitBlt(screen, 0, 0, field->m_viewWidth, field->m_viewHeight, memory, field->m_scroll, 0, SRCCOPY);
  reb::compat::SelectPalette(screen, screenPalette, FALSE);
  reb::compat::SelectPalette(memory, previousPalette, FALSE);
  reb::compat::SelectObject(memory, previousBitmap);
  reb::compat::DeleteDC(memory);
  reb::compat::EndPaint(hwnd, &paint);
}

LRESULT HandleKeyDown(CoolStringField* field, WPARAM key, LPARAM lParam)
{
  HWND hwnd = field->m_hwnd;
  if (field->m_selStart >= 0 && field->m_selEnd == -1) {
    field->m_selEnd = field->m_text.size() & 0xffff;
  }
  switch (key) {
    case VK_BACK:
      if (field->m_selStart == field->m_selEnd) {
        const int caret = field->m_caret;
        if (caret > 0) {
          RemoveStringRange(field->m_text, caret - 1, caret);
          field->m_caret = field->m_caret - 1;
          NotifyChanged(field);
        } else if (caret == -1) {
          const int length = field->m_text.size() & 0xffff;
          RemoveStringRange(field->m_text, length - 1, length);
          NotifyChanged(field);
        }
      } else {
        field->DeleteSelection();
        NotifyChanged(field);
      }
      reb::compat::InvalidateRect(hwnd, nullptr, FALSE);
      field->MeasureText();
      break;
    default:
      return field->m_parent->WindowProc(WM_KEYDOWN, key, lParam);
    case VK_RETURN:
      reb::compat::PostMessageA(field->m_parent->m_hwnd, kFieldReturnMessage, field->m_childId, reinterpret_cast<LPARAM>(field));
      return 0;
    case VK_END:
      if (!IsShiftHeld()) {
        if (field->m_selStart >= 0) {
          reb::compat::InvalidateRect(hwnd, nullptr, FALSE);
        }
        ClearSelection(field);
      } else if (field->m_caret < field->m_length) {
        field->m_selStart = field->m_caret;
        field->m_selEnd = field->m_length;
        reb::compat::InvalidateRect(hwnd, nullptr, FALSE);
        field->m_caret = field->m_length;
        break;
      }
      field->m_caret = field->m_length;
      break;
    case VK_HOME:
      if (!IsShiftHeld()) {
        if (field->m_selStart >= 0) {
          reb::compat::InvalidateRect(hwnd, nullptr, FALSE);
        }
        ClearSelection(field);
      } else if (field->m_caret > 0) {
        field->m_selEnd = field->m_caret;
        field->m_selStart = 0;
        reb::compat::InvalidateRect(hwnd, nullptr, FALSE);
        field->m_caret = 0;
        break;
      }
      field->m_caret = 0;
      break;
    case VK_LEFT:
      {
        int caret = field->m_caret;
        if (caret < 1) {
          if (caret == -1) {
            field->m_caret = (field->m_text.size() & 0xffff) - 1;
          }
        } else {
          field->m_caret = caret - 1;
        }
        if (IsShiftHeld()) {
          const int start = field->m_selStart;
          caret = field->m_caret;
          if (caret == start - 1) {
            field->m_selStart = caret;
            reb::compat::InvalidateRect(hwnd, nullptr, FALSE);
          } else if (caret == field->m_selEnd - 1) {
            field->m_selEnd = caret;
            if (caret == start) {
              ClearSelection(field);
            }
            reb::compat::InvalidateRect(hwnd, nullptr, FALSE);
          } else if (start == field->m_selEnd) {
            field->m_selStart = caret;
            field->m_selEnd = caret + 1;
            reb::compat::InvalidateRect(hwnd, nullptr, FALSE);
          }
          break;
        }
        if (field->m_selStart != -1) {
          reb::compat::InvalidateRect(hwnd, nullptr, FALSE);
        }
        ClearSelection(field);
        break;
      }
    case VK_RIGHT:
      {
        int caret = field->m_caret;
        if (caret > -1 && caret < static_cast<int>(field->m_text.size() & 0xffff)) {
          field->m_caret = caret + 1;
        }
        if (IsShiftHeld()) {
          const int end = field->m_selEnd;
          caret = field->m_caret;
          if (caret == end + 1) {
            field->m_selEnd = caret;
            reb::compat::InvalidateRect(hwnd, nullptr, FALSE);
          } else if (caret == field->m_selStart + 1) {
            field->m_selStart = caret;
            if (caret == end) {
              ClearSelection(field);
            }
            reb::compat::InvalidateRect(hwnd, nullptr, FALSE);
          } else if (field->m_selStart == end) {
            field->m_selEnd = caret;
            field->m_selStart = caret - 1;
            reb::compat::InvalidateRect(hwnd, nullptr, FALSE);
          }
          break;
        }
        if (field->m_selStart != -1) {
          reb::compat::InvalidateRect(hwnd, nullptr, FALSE);
        }
        ClearSelection(field);
        break;
      }
    case VK_INSERT:
      field->m_flags ^= kFieldOverwrite;
      return 0;
    case VK_DELETE:
      if (field->m_selStart == field->m_selEnd) {
        const int caret = field->m_caret;
        if (caret > -1) {
          RemoveStringRange(field->m_text, caret, caret + 1);
          NotifyChanged(field);
        }
      } else {
        field->DeleteSelection();
        NotifyChanged(field);
      }
      reb::compat::InvalidateRect(hwnd, nullptr, FALSE);
      field->MeasureText();
      break;
  }
  field->UpdateCaret();
  return 0;
}

LRESULT HandleChar(CoolStringField* field, WPARAM key)
{
  if (field->m_selStart >= 0 && field->m_selEnd == -1) {
    field->m_selEnd = field->m_text.size() & 0xffff;
  }
  if (key > 0xff || !std::isprint(static_cast<int>(key))) {
    return 0;
  }
  if (field->m_flags & kFieldFocused) {
    if (field->m_selStart == field->m_selEnd) {
      const int caret = field->m_caret;
      if ((field->m_flags & kFieldOverwrite) && caret > 0 && caret < static_cast<int>(field->m_text.size() & 0xffff)) {
        RemoveStringRange(field->m_text, caret, caret + 1);
      }
    } else {
      field->DeleteSelection();
    }
    if (field->m_caret == -1) {
      field->m_caret = field->m_text.size() & 0xffff;
    }
    if (field->m_maxLength == 0 || field->m_caret < field->m_maxLength) {
      const char character = static_cast<char>(key);
      InsertCharacter(field->m_text, character, field->m_caret);
      field->m_caret = field->m_caret + 1;
    }
    field->MeasureText();
    field->UpdateCaret();
    reb::compat::InvalidateRect(field->m_hwnd, nullptr, FALSE);
    NotifyChanged(field);
  }
  if (field->m_flags & kFieldSelecting) {
    field->m_flags &= ~kFieldSelecting;
  }
  return 0;
}

void SelectWordAtCaret(CoolStringField* field)
{
  StringCursor cursor(field->m_text);
  int index = 0;
  bool atWordStart = true;
  field->m_selStart = 0;
  field->m_selEnd = 0;
  do {
    if (!cursor.HasMore()) {
      break;
    }
    const int character = cursor.Current();
    if (character < 0 || character > 0xff || (!std::isspace(character) && !std::ispunct(character))) {
      if (atWordStart) {
        if (index <= field->m_caret) {
          field->m_selStart = index;
        }
        atWordStart = false;
      }
    } else if (field->m_caret < index) {
      field->m_selEnd = index;
    } else {
      atWordStart = true;
    }
    ++index;
    ++cursor;
  } while (field->m_selEnd == 0);
  if (field->m_selEnd == 0) {
    field->m_selEnd = field->m_length;
  } else if (field->m_selEnd < field->m_length) {
    field->m_selEnd = field->m_selEnd + 1;
  }
  field->m_caret = field->m_selEnd;
  field->UpdateCaret();
  reb::compat::InvalidateRect(field->m_hwnd, nullptr, FALSE);
}

} // namespace

LRESULT CoolStringField::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
  HWND hwnd = m_hwnd;
  switch (message) {
    case WM_ACTIVATE:
      {
        const uint32_t activation = wParam & 0xffff;
        if (activation == 0) {
          if (m_flags & kFieldFocused) {
            reb::compat::DestroyCaret();
            reb::compat::HideCaret(hwnd);
            m_flags &= ~kFieldFocused;
          }
        } else if (activation < 3) {
          reb::compat::SetFocus(hwnd);
        }
        return 0;
      }
    case WM_CREATE:
      MeasureText();
      UpdateCaret();
      return 0;
    case WM_SETFOCUS:
      if (reb::compat::GetFocus() != hwnd) {
        return 0;
      }
      m_flags |= kFieldFocused;
      reb::compat::CreateCaret(hwnd, nullptr, 2, m_clientHeight);
      if (!reb::compat::ShowCaret(hwnd)) {
        GetLastError();
      }
      UpdateCaret();
      if (m_selStart < 0) {
        return 0;
      }
      reb::compat::InvalidateRect(m_hwnd, nullptr, FALSE);
      return 0;
    case WM_KILLFOCUS:
      if (m_flags & kFieldFocused) {
        m_flags &= ~(kFieldFocused | kFieldSelecting);
        reb::compat::DestroyCaret();
        reb::compat::HideCaret(hwnd);
        reb::compat::InvalidateRect(m_hwnd, nullptr, FALSE);
      }
      return 0;
    case WM_PAINT:
      PaintField(this);
      return 0;
    case WM_ERASEBKGND:
      return reb::compat::DefWindowProcA(hwnd, WM_ERASEBKGND, wParam, lParam);
    case WM_MOUSEMOVE:
      if (m_flags & kFieldSelecting) {
        const int index = CharIndexFromX(static_cast<short>(lParam), static_cast<short>(lParam >> 16));
        if ((wParam & MK_LBUTTON) == 0) {
          m_flags &= ~kFieldSelecting;
          m_selEnd = -1;
          m_selStart = -1;
        } else {
          m_caret = index;
          m_selEnd = index;
        }
        UpdateCaret();
        reb::compat::InvalidateRect(m_hwnd, nullptr, FALSE);
      }
      return 0;
    case WM_CHAR:
      return HandleChar(this, wParam);
    case WM_KEYDOWN:
      return HandleKeyDown(this, wParam, lParam);
    case WM_LBUTTONDOWN:
      {
        const uint32_t y = static_cast<uint32_t>(lParam) >> 16;
        const uint32_t x = static_cast<uint32_t>(lParam) & 0xffff;
        if (m_selStart != m_selEnd) {
          m_selEnd = -1;
          m_selStart = -1;
          reb::compat::InvalidateRect(hwnd, nullptr, FALSE);
          reb::compat::UpdateWindow(m_hwnd);
        }
        const int index = CharIndexFromX(x, y);
        m_caret = index;
        m_selEnd = index;
        m_selStart = index;
        m_flags |= kFieldSelecting;
        SetCaptureWindow(m_hwnd);
        reb::compat::SetFocus(m_hwnd);
        UpdateCaret();
        return 0;
      }
    case WM_LBUTTONUP:
      m_flags &= ~kFieldSelecting;
      if (reb::compat::GetCapture() == m_hwnd) {
        reb::compat::ReleaseCapture();
      }
      return 0;
    case WM_LBUTTONDBLCLK:
      SelectWordAtCaret(this);
      return 0;
  }
  return reb::compat::DefWindowProcA(hwnd, message, wParam, lParam);
}
