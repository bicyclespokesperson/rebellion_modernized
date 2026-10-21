#include "ui/cool_label.h"
#include <algorithm>
#include <string>
#include <tuple>

#include "core/checked_new.h"
#include "core/game_string.h"
#include "ui/cool_app.h"
#include "ui/cool_window.h"
#include "ui/resource_dll.h"

namespace
{

constexpr COLORREF kBlackPaletteRgb = 0x02000000;
constexpr UINT kDrawTextCalcRect = 0x400;
constexpr UINT kDrawTextTabStop = 0x80;
constexpr UINT kDrawTextNoClip = 0x100;

} // namespace

void CoolLabel::InitializeRect(int left, int top, int width, int height, UINT drawFlags, UINT layoutFlags)
{
  m_width = width;
  m_height = height;
  m_rect.right = width + left;
  m_rect.bottom = height + top;
  m_rect.top = top;
  m_rect.left = left;
  m_text = nullptr;
  m_drawFlags = drawFlags;
  m_color = kBlackPaletteRgb;
  m_font = nullptr;
  m_layoutFlags = layoutFlags;
  m_hidden = 0;
}

CoolLabel::CoolLabel(uint16_t stringId, int left, int top, int width, int height, COLORREF color, int fontSlot,
                     UINT drawFlags, UINT layoutFlags)
{
  InitializeRect(left, top, width, height, drawFlags, layoutFlags);
  SetColor(color, nullptr);
  SetFont(fontSlot);
  if (stringId != 0) {
    char text[0x100] = {};
    LoadResourceString(stringId, text, sizeof(text));
    m_text = NewOrNull<std::string>(StringFromText(static_cast<const char*>(text)));
  }
}

CoolLabel::CoolLabel(const ResourceStringId* stringId, int left, int top, int width, int height, UINT drawFlags,
                     UINT layoutFlags)
{
  InitializeRect(left, top, width, height, drawFlags, layoutFlags);
  m_text = NewOrNull<std::string>(LoadStringResource(stringId));
  SetFont(GetDefaultFontSlot());
}

CoolLabel::CoolLabel(uint16_t stringId, int left, int top, int width, int height, UINT drawFlags, UINT layoutFlags)
{
  InitializeRect(left, top, width, height, drawFlags, layoutFlags);
  char text[0x100] = {};
  LoadResourceString(stringId, text, sizeof(text));
  m_text = NewOrNull<std::string>(StringFromText(static_cast<const char*>(text)));
  SetFont(GetDefaultFontSlot());
}

CoolLabel::CoolLabel(const char* text, int left, int top, int width, int height, UINT drawFlags, UINT layoutFlags)
{
  InitializeRect(left, top, width, height, drawFlags, layoutFlags);
  m_text = text ? NewOrNull<std::string>(StringFromText(text)) : nullptr;
  SetFont(GetDefaultFontSlot());
}

CoolLabel::~CoolLabel()
{
  if (m_text) {
    delete m_text;
  }
}

void CoolLabel::SetTextFromResource(const ResourceStringId* stringId)
{
  if (m_text) {
    delete m_text;
  }
  m_text = NewOrNull<std::string>(LoadStringResource(stringId));
  if ((m_layoutFlags & kLabelFixedWidth) == 0) {
    m_rect.right = m_rect.left;
  }
  if ((m_layoutFlags & kLabelFixedHeight) == 0) {
    m_rect.bottom = m_rect.top;
  }
}

void CoolLabel::SetText(const char* text)
{
  if (m_text) {
    delete m_text;
    m_text = nullptr;
  }
  if (text) {
    m_text = NewOrNull<std::string>(StringFromText(text));
  }
  if ((m_layoutFlags & kLabelFixedWidth) == 0) {
    m_rect.right = m_rect.left;
  }
  if ((m_layoutFlags & kLabelFixedHeight) == 0) {
    m_rect.bottom = m_rect.top;
  }
}

void CoolLabel::SetTextFromStream(StrStream* stream)
{
  ostream_operator_ls_char(stream->Out(), 0);
  SetText(strstreambuf_str(StreambufOf(stream)));
  strstreambuf_freeze(StreambufOf(stream), 0);
  ostream_seekp_pos(stream->Out(), 0);
  ClearIosState(stream);
}

const char* CoolLabel::GetText() const
{
  return m_text ? m_text->c_str() : nullptr;
}

void CoolLabel::MoveTo(int left, int top)
{
  const int width = (m_layoutFlags & kLabelFixedWidth) == 0 ? m_rect.right - m_rect.left : m_width;
  const int height = (m_layoutFlags & kLabelFixedHeight) == 0 ? m_rect.bottom - m_rect.top : m_height;
  m_rect.top = top;
  m_rect.left = left;
  m_rect.right = width + left;
  m_rect.bottom = height + top;
}

void CoolLabel::Measure(HDC dc)
{
  if (!m_text || m_text->empty()) {
    return;
  }
  HGDIOBJ previousFont = nullptr;
  if (m_font) {
    previousFont = reb::compat::SelectObject(dc, m_font);
  }
  const UINT format = (m_drawFlags & ~kDrawTextTabStop) | kDrawTextCalcRect;
  const LONG previousRight = m_rect.right;
  reb::compat::DrawTextA(dc, m_text->c_str(), -1, &m_rect, format);
  if ((m_drawFlags & DT_CENTER) != 0 && (m_layoutFlags & kLabelFixedWidth) == 0) {
    m_drawFlags &= ~static_cast<UINT>(DT_CENTER);
    const LONG measuredRight = m_rect.right;
    if (measuredRight < previousRight) {
      m_rect.right = previousRight;
    } else {
      m_rect.left = m_rect.left - 1;
      m_rect.right = measuredRight + 1;
    }
  }
  if ((m_layoutFlags & kLabelFixedWidth) != 0) {
    m_rect.right = m_width + m_rect.left;
  }
  if ((m_layoutFlags & kLabelFixedHeight) != 0) {
    m_rect.bottom = m_rect.top + m_height;
  }
  if (m_font) {
    reb::compat::SelectObject(dc, previousFont);
  }
}

void CoolLabel::SetFont(int fontSlot)
{
  m_fontSlot = fontSlot;
  m_font = CoolGetFont(fontSlot);
  if ((m_layoutFlags & kLabelFixedWidth) == 0) {
    m_rect.right = m_rect.left;
  }
  if ((m_layoutFlags & kLabelFixedHeight) == 0) {
    m_rect.bottom = m_rect.top;
  }
}

void CoolLabel::SetColor(COLORREF color, HWND invalidateIn)
{
  if (m_color == color) {
    return;
  }
  m_color = color;
  if (invalidateIn) {
    RECT area = m_rect;
    reb::compat::InvalidateRect(invalidateIn, &area, FALSE);
  }
}

void CoolLabel::Draw(HDC dc)
{
  if (m_hidden != 0) {
    return;
  }
  if (!m_text || m_text->empty()) {
    return;
  }
  bool measured = false;
  if (m_rect.right != m_rect.left) {
    const LONG bottom = m_rect.bottom;
    if (bottom != m_rect.top && m_rect.right != 0 && bottom != 0 && m_rect.left <= m_rect.right
        && m_rect.top <= bottom) {
      measured = true;
    }
  }
  if (!measured) {
    Measure(dc);
  }

  HGDIOBJ previousFont = nullptr;
  if (m_font) {
    previousFont = reb::compat::SelectObject(dc, m_font);
  }
  reb::compat::SetBkMode(dc, TRANSPARENT);
  const COLORREF previousColor = reb::compat::SetTextColor(dc, m_color);
  const UINT format = ((~m_layoutFlags & kLabelClip) << 4) | m_drawFlags;
  reb::compat::DrawTextA(dc, m_text->c_str(), -1, &m_rect, format);
  reb::compat::SetTextColor(dc, previousColor);
  if (m_font) {
    reb::compat::SelectObject(dc, previousFont);
  }
}

void CoolLabel::Invalidate(HWND hwnd, BOOL erase)
{
  reb::compat::InvalidateRect(hwnd, &m_rect, erase);
}

void CoolLabel::ReplaceTextFromResource(HWND hwnd, HDC dc, const ResourceStringId* stringId)
{
  SetTextFromResource(stringId);
  Invalidate(hwnd, TRUE);
  Measure(dc);
  Invalidate(hwnd, TRUE);
}

void CoolLabel::ReplaceText(HWND hwnd, HDC dc, const char* text)
{
  SetText(text);
  Invalidate(hwnd, TRUE);
  Measure(dc);
  Invalidate(hwnd, TRUE);
}

void CoolLabel::ReplaceTextFromStream(HWND hwnd, HDC dc, StrStream* stream)
{
  SetTextFromStream(stream);
  Invalidate(hwnd, TRUE);
  Measure(dc);
  Invalidate(hwnd, TRUE);
}

RectTree::~RectTree()
{
  Clear();
}

void RectTree::Clear()
{
  std::list<std::unique_ptr<RectNode>> oldRectangles;
  oldRectangles.swap(m_rectangles);
}

void RectTree::Insert(RectNode* node)
{
  auto position = std::find_if(m_rectangles.begin(), m_rectangles.end(), [node](const auto& other) {
    return std::tie(node->m_left, node->m_right, node->m_top, node->m_bottom)
           < std::tie(other->m_left, other->m_right, other->m_top, other->m_bottom);
  });
  m_rectangles.emplace(position, node);
}

RectNode* RectTree::FindContaining(int x, int y)
{
  for (const auto& entry : m_rectangles) {
    auto* node = entry.get();
    if (x >= node->m_left && node->m_right >= x && y >= node->m_top && node->m_bottom >= y) {
      return node;
    }
  }
  return nullptr;
}
