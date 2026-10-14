#pragma once
#include <cstdint>
#include <list>
#include <memory>
#include <string>

#include "core/entry_identity.h"
#include "core/strstream_io.h"
#include "platform/win32.h"

struct ResourceStringId;

// Bits of CoolLabel::m_layoutFlags.
enum CoolLabelLayout : uint32_t
{
  kLabelFixedWidth = 0x01, // the rectangle keeps m_width whatever the text measures
  kLabelFixedHeight = 0x02, // the rectangle keeps m_height
  kLabelClip = 0x10, // draw clipped to the rectangle (otherwise DT_NOCLIP)
};

// A piece of text drawn straight onto a window's DC (no window of its own): a string, a rectangle, a DrawText
// format and a font. The rectangle is measured with DT_CALCRECT unless it is fixed. No virtual functions.
class CoolLabel
{
public:
  // Text from a string resource of the string DLL (g_stringModule); 0 means no text.
  CoolLabel(uint16_t stringId,
            int left,
            int top,
            int width,
            int height,
            COLORREF color,
            int fontSlot,
            UINT drawFlags,
            UINT layoutFlags); // 0x601620
  // Text from the game's std::string resource loader.
  CoolLabel(const ResourceStringId* stringId,
            int left,
            int top,
            int width,
            int height,
            UINT drawFlags,
            UINT layoutFlags); // 0x601700
  CoolLabel(uint16_t stringId,
            int left,
            int top,
            int width,
            int height,
            UINT drawFlags,
            UINT layoutFlags); // 0x6017a0: LoadStringA
  CoolLabel(const char* text, int left, int top, int width, int height, UINT drawFlags,
            UINT layoutFlags); // 0x601880
  ~CoolLabel(); // 0x601990

  void SetTextFromResource(const ResourceStringId* stringId); // 0x6019a0 (std::string resource loader)
  void SetTextFromStream(StrStream* stream); // 0x601a20: takes the text a strstream collected
  void SetText(const char* text); // 0x601aa0
  const char* GetText() const; // 0x601b20: null without text
  void MoveTo(int left, int top); // 0x601b30: keeps the size of a fixed rectangle
  void Measure(HDC dc); // 0x601b80
  void SetFont(int fontSlot); // 0x601c60
  void SetColor(COLORREF color, HWND invalidateIn); // 0x601c90
  void Draw(HDC dc); // 0x601ce0
  void Invalidate(HWND hwnd, BOOL erase); // 0x601dc0: InvalidateRect on the rectangle

  // SetText... followed by invalidate / measure / invalidate.
  void ReplaceTextFromResource(HWND hwnd, HDC dc, const ResourceStringId* stringId); // 0x601de0
  void ReplaceText(HWND hwnd, HDC dc, const char* text); // 0x601e20
  void ReplaceTextFromStream(HWND hwnd, HDC dc, StrStream* stream); // 0x601e60

  int m_width; // +0x00 requested size (used for fixed sides)
  int m_height; // +0x04
  std::string* m_text; // +0x08 owned, may be null
  COLORREF m_color; // +0x0c starts as PALETTERGB(0,0,0)
  UINT m_drawFlags; // +0x10 DrawText format
  RECT m_rect; // +0x14
  int m_fontSlot; // +0x24
  HFONT m_font; // +0x28
  uint32_t m_layoutFlags; // +0x2c CoolLabelLayout
  int m_hidden; // +0x30 non-zero: Draw does nothing

private:
  void InitializeRect(int left, int top, int width, int height, UINT drawFlags, UINT layoutFlags); // 0x601930
};

// Nodes that carry a rectangle at +0x18 (left, top, right, bottom) and are looked up by point.
class RectNode : public AvlNode
{
public:
  int m_left; // +0x18
  int m_top; // +0x1c
  int m_right; // +0x20 inclusive
  int m_bottom; // +0x24 inclusive
};

// Tree of RectNodes ordered by (left, right, top, bottom).
class RectTree
{
public:
  ~RectTree();
  void Clear();
  void Insert(RectNode* node);

  // First node, in tree order, whose rectangle contains the point.
  RectNode* FindContaining(int x, int y); // 0x601550

private:
  std::list<std::unique_ptr<RectNode>> m_rectangles;
};
