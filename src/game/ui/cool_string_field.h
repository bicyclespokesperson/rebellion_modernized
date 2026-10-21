#pragma once
#include <cstdint>
#include <string>

#include "core/game_string.h"
#include "platform/win32.h"
#include "ui/cool_window.h"

class DibBitmap;

// Bits of CoolStringField::m_flags.
enum CoolStringFieldFlags : uint32_t {
  kFieldOverBackdrop = 0x00000001, // the background is a piece of the backdrop (otherwise m_fillBitmap)
  kFieldFilled = 0x00000004,       // m_fillBitmap is drawn
  kFieldSelecting = 0x00010000,    // the mouse is dragging a selection
  kFieldFocused = 0x10000000,      // has the focus and owns the system caret
  kFieldOverwrite = 0x20000000,    // Insert key: typed characters replace the one at the caret
};

// Messages the field posts to its parent (WM_COMMAND-like, wParam = child id).
constexpr uint32_t kFieldReturnMessage = 0x407;  // Return pressed, lParam = the field
constexpr uint32_t kFieldChangedMessage = 0x408; // the text was edited

// A single-line text field ("CoolStringField" window class) with its own text layout, selection and system caret,
// drawn through an off-screen buffer. Character positions are measured with DrawText. m_caret -1 means "at the end".
class CoolStringField : public CoolWindow
{
  public:
  // `backdrop` (not owned) is the parent's background; the field is drawn over the part at (x, y) of it.
  CoolStringField(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent, DibBitmap* backdrop,
                  uint32_t extraStyle); // 0x604cf0
  ~CoolStringField() override;          // 0x604ed0 (scalar deleting destructor 0x604eb0)

  LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override; // slot 5, 0x605160
  void SetFont(int fontSlot) override;                                     // slot 6, 0x604f70

  void SetText(const char* text);        // 0x604f90: null clears; truncated to m_maxLength - 1
  void Clear();                          // 0x605070
  void SetMaxLength(uint16_t maxLength); // 0x6050c0: at most 0x103, 0 means unlimited
  void SetSelection(int start, int end); // 0x605110
  void DeleteSelection();                // 0x605f20
  void MeasureText();                    // 0x605fc0: grows the buffer to the text width
  void UpdateCaret();                    // 0x6060f0: selection pixels, caret position and scrolling
  int CharIndexFromX(int x, int y);      // 0x6062d0: character under the point (y is ignored)

  uint32_t m_flags;        // +0x94 CoolStringFieldFlags
  std::string m_text;      // +0x98
  int m_maxLength;         // +0xa4 0: unlimited
  DibBitmap* m_backdrop;   // +0xa8 not owned
  DibBitmap* m_buffer;     // +0xac owned: wider than the window when the text is
  DibBitmap* m_fillBitmap; // +0xb0 owned, set by other code
  int m_selStart;          // +0xb4 -1: none
  int m_selEnd;            // +0xb8 -1: to the end of the text when m_selStart is set
  int m_selLeftPixel;      // +0xbc
  int m_selRightPixel;     // +0xc0
  int m_backdropX;         // +0xc4
  int m_backdropY;         // +0xc8
  int m_caret;             // +0xcc -1: at the end
  int m_length;            // +0xd0 text length, refreshed by MeasureText and SetText
  int m_scroll;            // +0xd4 horizontal scroll in pixels
  int m_viewWidth;         // +0xd8
  int m_viewHeight;        // +0xdc
  int m_shadowOffset;      // +0xe0 pixel offset of the text shadow, also the buffer margin
  COLORREF m_textColor;    // +0xe4
  COLORREF m_shadowColor;  // +0xe8
  uint32_t m_unknownEc;    // +0xec 0xff
  uint32_t m_unknownF0;    // +0xf0 0x808080
};
