#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/game_string.h"
#include "platform/win32.h"
#include "ui/cool_scrollable.h"
#include "ui/cool_window.h"

class DibBitmap;

// Character index of the first character of a line that was scrolled past (TextScrollField::m_lineStarts).
class TextLineStart : public IdLinkNode {
public:
    explicit TextLineStart(int charIndex);  // inlined in 0x420450
    ~TextLineStart() override;               // 0x4204f0 (scalar deleting destructor 0x4204d0)

    int m_charIndex;  // +0x10
};

// A place in the text: character index and the pixel position of the caret in front of it.
struct TextPosition {
    int m_index;  // +0x00
    int m_x;      // +0x04
    int m_y;      // +0x08
};

// Bits of TextScrollField::m_state.
enum TextScrollFieldState : uint32_t {
    kTextEditable = 0x01,    // owns the caret and accepts typing and clicks
    kTextSelecting = 0x02,   // the mouse moved with the button down (the selection code itself is empty)
    kTextMouseDown = 0x04,   // the button went down in the field
    kTextInsertMode = 0x08,  // Insert key; when clear, a typed character first deletes the one before the caret
};

// "TextScrollField": a multi-line, word-wrapped text window with a vertical scroll bar, optionally editable
// with the system caret. It lays the text out itself (GetTextExtentPoint32 per word, tabs every four average
// character widths, '\n' breaks), over a copy of the parent's backdrop. Scrolling is by whole lines: the start
// index of every line scrolled past is pushed on m_lineStarts, so scrolling back pops it.
class TextScrollField : public CoolWindow, public CoolScrollable {
public:
    // `backdrop` (not owned) is the parent's background; the field is drawn over the part at (x, y) of it.
    TextScrollField(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                    DibBitmap* backdrop, int editable);  // 0x41ecf0
    ~TextScrollField() override;  // 0x41eed0 (scalar deleting destructor 0x41eeb0, thunk 0x420bc0)

    LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;  // slot 5, 0x41efe0

    int PageUp() override;                     // 0x420a10
    int PageDown() override;                   // 0x420a50
    int LineUp() override;                     // 0x420a90: returns -line height
    int LineDown() override;                   // 0x420ac0: returns the line height
    int ScrollVertical(int delta) override;    // 0x420af0
    int ScrollHorizontal(int delta) override;  // 0x48b440 (linker-folded): returns 0

    // Grabs the caret (and the focus) or hides it.
    void SetEditable(int editable);  // 0x41fb50
    // After m_text changed: back to the top, caret to m_caret.m_index, re-measure and repaint.
    void Reset();                    // 0x41fc30
    void UpdateScrollBar();          // 0x41fd00: measures the whole text and the scrolled-past part
    void DrawLines(HDC dc);          // 0x41fe70: draws the visible lines and places the caret
    void ScrollUp(int update);       // 0x4201d0: one line
    void ScrollDown(int update);     // 0x420200: one line
    void PushLineStart(int charIndex);  // 0x420450
    void PopLineStart(int* charIndex);  // 0x420500: leaves the last entry in place
    void SetTextFont(int fontSlot);  // 0x420550
    // Moves `position` (x, y in client pixels) to the nearest character: sets m_index, snaps x and y.
    void LocatePosition(TextPosition* position);  // 0x420580
    void InsertText(const String& text, int position);  // 0x420b70
    void ClearText();                                    // 0x420ba0
    void NormalizeRect(RECT* rect);                      // 0x41fb20: left <= right, top <= bottom
    void TrackSelection(POINT* point);  // 0x5ae1e0 (linker-folded empty function): selection was never written

    // Points `last` at the last character of the word at `start` (stopping at ' ', '\t', '\n' or the end) and
    // returns its length (0 if `start` is on a separator). Does not use `this`.
    int ScanWord(const StringCursor& start, StringCursor& last);  // 0x420930

private:
    // Helpers of the functions above (no original counterparts).
    void CreateCaretForFont();
    void RestoreCaret(HWND hwnd);
    void RewindToTop();
    int FitWordToLine(HDC dc, int length, SIZE* extent);
    void InvalidateFromCaretLine(HWND hwnd);
    void SetCaretToPosition();
    void MoveCaretVertically();
    void ForwardKey(WPARAM key, LPARAM lParam);
    void OnKeyDown(HWND hwnd, WPARAM key, LPARAM lParam);
    void OnChar(HWND hwnd, WPARAM wParam);
    void OnLeftButtonDown(HWND hwnd, LPARAM lParam);
    void OnMouseMove(HWND hwnd, LPARAM lParam);

public:
    String m_text;                   // +0xa0
    DibBitmap* m_background;         // +0xac owned: the backdrop under the window
    HFONT m_textFont;                // +0xb0
    COLORREF m_textColor;            // +0xb4
    RECT m_textBounds;               // +0xb8 DrawText DT_CALCRECT result
    TEXTMETRICA m_textMetrics;       // +0xc8
    StringCursor m_wordCursor;       // +0x100 layout scratch
    StringCursor m_scanCursor;       // +0x10c layout scratch
    BOOL m_hasCaret;                 // +0x118
    HDC m_dc;                        // +0x11c memory DC used for measuring
    HGDIOBJ m_savedFont;             // +0x120
    HGDIOBJ m_savedBitmap;           // +0x124
    int m_spaceWidth;                // +0x128
    int m_leftMargin;                // +0x12c always 0
    int m_lineWidth;                 // +0x130 wrap width (client width minus the scroll bar when it is shown)
    uint16_t m_scrollBarSlot;        // +0x134 resource slot of the scroll bar art (7)
    uint16_t m_scrollBarId;          // +0x136 (0x299a)
    uint32_t m_state;                // +0x138 TextScrollFieldState
    TextPosition m_caret;            // +0x13c
    TextPosition m_selectionStart;   // +0x148 never set
    TextPosition m_selectionEnd;     // +0x154 never set
    IdSortedLinkList m_lineStarts;   // +0x160 of TextLineStart
    TextLineStart* m_lastLineStart;  // +0x170
    int m_topIndex;                  // +0x174 index of the first visible character
};
