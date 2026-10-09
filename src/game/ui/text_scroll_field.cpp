#include "core/reb_abi.h"
#include "ui/text_scroll_field.h"

#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "gfx/dib_bitmap.h"
#include "ui/resource_dll.h"

extern "C" int REB_CDECL _isprint(int c);

extern const char g_spaceString[];    // 0x6a7f40 " " (shared literal)
extern const char g_newlineString[];  // 0x6a8170 "\n" (shared literal)

namespace {

constexpr uint32_t kTextFieldStyle = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
constexpr UINT kMeasureFormat = DT_EDITCONTROL | DT_CALCRECT | DT_WORDBREAK;
constexpr int kCaretWidth = 2;
constexpr int kDefaultFontSlot = 4;
constexpr uint16_t kScrollBarSlot = 7;
constexpr uint16_t kScrollBarId = 0x299a;
constexpr COLORREF kWhite = 0x2ffffff;  // PALETTERGB(255, 255, 255)
constexpr int kTabStopChars = 4;

int TabWidth(const TEXTMETRICA& metrics) { return metrics.tmAveCharWidth * kTabStopChars; }

int NextTabStop(int x, int tabWidth) { return x + 1 + (tabWidth - (x + 1) % tabWidth); }

}  // namespace

// ---- TextLineStart -----------------------------------------------------------------------------------------

TextLineStart::TextLineStart(int charIndex) : IdLinkNode(), m_charIndex(charIndex) {}

TextLineStart::~TextLineStart() { REB_KEEP_VPTR_STORE(); }

// ---- TextScrollField ---------------------------------------------------------------------------------------

TextScrollField::TextScrollField(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                                 DibBitmap* backdrop, int editable)
    : CoolWindow(instance, "TextScrollField", kTextFieldStyle, x, y, width, height, parent, nullptr,
                 static_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)), nullptr, LoadCursorA(nullptr, IDC_ARROW),
                 nullptr, 0, 0),
      CoolScrollable() {
    m_background = NewOrNull<DibBitmap>(width, height, backdrop);
    backdrop->Blit(m_background, 0, 0, x, y, 0, 0);
    m_textFont = nullptr;
    m_textColor = kWhite;
    m_topIndex = 0;
    m_state = 0;
    SetEditable(editable);
    m_caret.m_x = 0;
    m_caret.m_y = 0;
    m_caret.m_index = 0;
    m_leftMargin = 0;
    m_lineWidth = width;
    m_dc = nullptr;
    m_savedBitmap = nullptr;
    m_savedFont = nullptr;
    m_hasCaret = 0;
    m_scrollBarSlot = kScrollBarSlot;
    m_scrollBarId = kScrollBarId;
    m_textFont = CoolGetFont(kDefaultFontSlot);
}

TextScrollField::~TextScrollField() {
    if (m_background) delete m_background;
    if (m_dc) {
        if (m_savedBitmap) SelectObject(m_dc, m_savedBitmap);
        if (m_savedFont) SelectObject(m_dc, m_savedFont);
        DeleteDC(m_dc);
    }
    REB_KEEP_VPTR_STORE();
}

void TextScrollField::NormalizeRect(RECT* rect) {
    const LONG left = rect->left;
    if (rect->right < left) {
        rect->left = rect->right;
        rect->right = left;
    }
    const LONG top = rect->top;
    if (rect->bottom < top) {
        rect->top = rect->bottom;
        rect->bottom = top;
    }
}

void TextScrollField::TrackSelection(POINT*) {}

void TextScrollField::CreateCaretForFont() {
    TEXTMETRICA metrics;
    GetTextMetricsA(m_dc, &metrics);
    m_hasCaret = CreateCaret(m_hwnd, nullptr, kCaretWidth, metrics.tmHeight);
}

void TextScrollField::SetEditable(int editable) {
    if (((~m_state) & kTextEditable) == static_cast<uint32_t>(editable == 0)) return;
    if (!editable) {
        HideCaret(m_hwnd);
        m_state &= ~kTextEditable;
        return;
    }
    HWND hwnd = m_hwnd;
    m_state |= kTextEditable;
    m_caret.m_x = m_leftMargin;
    m_caret.m_y = 0;
    m_caret.m_index = 0;
    if (!hwnd) return;
    if (m_dc) {
        CreateCaretForFont();
        SetCaretPos(m_caret.m_x, m_caret.m_y);
        ShowCaret(hwnd);
    }
    SetFocus(hwnd);
}

void TextScrollField::Reset() {
    if (!m_hwnd) return;
    m_state |= kTextInsertMode;
    m_lineStarts.Reset();
    m_lastLineStart = nullptr;
    m_caret.m_index = 0;
    m_topIndex = 0;
    LocatePosition(&m_caret);
    SetCaretPos(m_caret.m_x, m_caret.m_y);
    if (m_textFont) {
        HGDIOBJ previous = SelectObject(m_dc, m_textFont);
        if (!m_savedFont) m_savedFont = previous;
    }
    SIZE extent;
    GetTextExtentPoint32A(m_dc, g_spaceString, 1, &extent);
    m_spaceWidth = extent.cx;
    UpdateScrollBar();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void TextScrollField::UpdateScrollBar() {
    GetTextMetricsA(m_dc, &m_textMetrics);
    SetRect(&m_textBounds, 0, 0, m_clientWidth, m_clientHeight);
    DrawTextA(m_dc, m_text.m_text, -1, &m_textBounds, kMeasureFormat);
    m_lineWidth = m_clientWidth;
    if (m_textBounds.bottom <= m_clientHeight) {
        HideVerticalScrollBar();
        return;
    }
    if (m_verticalBar) {
        m_verticalBar->Refresh();
        ShowVerticalScrollBar();
    }
    m_lineWidth = m_textBounds.right = m_clientWidth - m_verticalBar->m_width;
    DrawTextA(m_dc, m_text.m_text, -1, &m_textBounds, kMeasureFormat);
    SetVerticalRange(m_textBounds.bottom - m_textBounds.top);
    RECT scrolledPast = m_textBounds;
    if (m_topIndex) {
        DrawTextA(m_dc, m_text.m_text, m_topIndex, &scrolledPast, kMeasureFormat);
        SetVerticalVisibleRange(scrolledPast.bottom, scrolledPast.bottom + m_clientHeight);
        return;
    }
    SetVerticalVisibleRange(0, m_clientHeight);
}

void TextScrollField::RewindToTop() {
    for (int i = 0; i < m_topIndex; ++i) m_scanCursor++;
}

int TextScrollField::ScanWord(const StringCursor& start, StringCursor& last) {
    int length = 0;
    last = start;
    StringCursor probe(start);
    while (probe.HasMore() && probe.Current() != '\n' && probe.Current() != ' ' && probe.Current() != '\t') {
        last = probe++;
        ++length;
    }
    return length;
}

int TextScrollField::FitWordToLine(HDC dc, int length, SIZE* extent) {
    while (length >= 2) {
        --length;
        m_scanCursor--;
        GetTextExtentPoint32A(dc, m_wordCursor.m_current, length, extent);
        if (extent->cx <= m_lineWidth - m_leftMargin) break;
    }
    return length;
}

void TextScrollField::DrawLines(HDC dc) {
    SIZE extent;
    SIZE caretExtent;
    int y = 0;
    int x = m_leftMargin;
    const int tabWidth = TabWidth(m_textMetrics);
    bool caretPlaced = false;
    m_scanCursor.Attach(m_text);
    m_wordCursor.Attach(m_text);
    HGDIOBJ previousFont = nullptr;
    if (m_textFont) previousFont = SelectObject(dc, m_textFont);
    const COLORREF previousColor = SetTextColor(dc, m_textColor);
    SetBkMode(dc, TRANSPARENT);
    RewindToTop();
    int index = m_topIndex;
    m_wordCursor = m_scanCursor;
    while (m_scanCursor.HasMore()) {
        int length = ScanWord(m_wordCursor, m_scanCursor);
        if (length == 0) {
            const int c = m_scanCursor.Current();
            bool lineBreak;
            if (c == '\t') {
                x = NextTabStop(x, tabWidth);
                lineBreak = m_lineWidth < x;
            } else if (c == '\n') {
                lineBreak = true;
            } else {
                lineBreak = c == ' ' && (x += m_spaceWidth, m_lineWidth < x);
            }
            if (lineBreak) {
                x = m_leftMargin;
                y += m_textMetrics.tmHeight;
            }
            ++index;
            if (!caretPlaced && m_caret.m_index <= index) {
                m_caret.m_x = x;
                m_caret.m_y = y;
                SetCaretPos(x, y);
                caretPlaced = true;
            }
        } else {
            GetTextExtentPoint32A(dc, m_wordCursor.m_current, length, &extent);
            if (m_lineWidth < x + extent.cx) {
                if (m_lineWidth - m_leftMargin < extent.cx) length = FitWordToLine(dc, length, &extent);
                if (index == m_caret.m_index && caretPlaced) {
                    m_caret.m_x = m_leftMargin;
                    m_caret.m_y = m_textMetrics.tmHeight + y;
                    SetCaretPos(m_leftMargin, m_caret.m_y);
                }
                x = m_leftMargin;
                y += m_textMetrics.tmHeight;
            }
            index += length;
            if (!caretPlaced && m_caret.m_index <= index) {
                int charsBeforeCaret = m_caret.m_index + (length - index);
                if (charsBeforeCaret < 1) charsBeforeCaret = 0;
                GetTextExtentPoint32A(dc, m_wordCursor.m_current, charsBeforeCaret, &caretExtent);
                m_caret.m_y = y;
                m_caret.m_x = caretExtent.cx + x;
                SetCaretPos(caretExtent.cx + x, y);
                caretPlaced = true;
            }
            if (m_clientHeight < m_textMetrics.tmHeight + y) break;
            TextOutA(dc, x, y, m_wordCursor.m_current, length);
            x += extent.cx;
        }
        m_wordCursor = ++m_scanCursor;
    }
    if (previousFont) SelectObject(dc, previousFont);
    SetTextColor(dc, previousColor);
}

void TextScrollField::ScrollUp(int update) {
    PopLineStart(&m_topIndex);
    if (update) UpdateScrollBar();
}

void TextScrollField::ScrollDown(int update) {
    SIZE extent;
    StringCursor lineStart;
    int x = m_leftMargin;
    m_scanCursor.Attach(m_text);
    int index = m_topIndex;
    RewindToTop();
    m_wordCursor = m_scanCursor;
    lineStart = m_scanCursor;
    bool lineEnded = false;
    while (m_scanCursor.HasMore() && !lineEnded) {
        int length = ScanWord(m_wordCursor, m_scanCursor);
        lineEnded = false;
        int nextX = x;
        if (length == 0) {
            ++index;
            const int c = m_wordCursor.Current();
            if (c == '\n' || (c == ' ' && (nextX = m_spaceWidth + x, m_lineWidth < nextX))) {
                PushLineStart(m_topIndex);
                m_topIndex = index;
                lineEnded = true;
                nextX = x;
            }
        } else {
            GetTextExtentPoint32A(m_dc, m_wordCursor.m_current, length, &extent);
            if (m_lineWidth < extent.cx + x) {
                if (m_lineWidth - m_leftMargin < extent.cx) FitWordToLine(m_dc, length, &extent);
                PushLineStart(m_topIndex);
                m_topIndex = index;
                break;
            }
            index += length;
            nextX = extent.cx + x;
        }
        m_wordCursor = ++m_scanCursor;
        x = nextX;
    }
    if (update) UpdateScrollBar();
}

void TextScrollField::PushLineStart(int charIndex) {
    TextLineStart* node = NewOrNull<TextLineStart>(charIndex);
    m_lastLineStart = node;
    m_lineStarts.AddWithId(node);
}

void TextScrollField::PopLineStart(int* charIndex) {
    TextLineStart* last = m_lastLineStart;
    if (!last) return;
    *charIndex = last->m_charIndex;
    m_lastLineStart = static_cast<TextLineStart*>(m_lastLineStart->m_prev);
    if (m_lastLineStart) {
        m_lineStarts.DetachById(last->m_id);
        delete last;
    }
}

void TextScrollField::SetTextFont(int fontSlot) {
    m_textFont = CoolGetFont(fontSlot);
    Reset();
}

void TextScrollField::LocatePosition(TextPosition* position) {
    SIZE extent;
    int lineTop = 0;
    bool found = false;
    int x = m_leftMargin;
    const int tabWidth = TabWidth(m_textMetrics);
    bool onLine = false;
    m_scanCursor.Attach(m_text);
    RewindToTop();
    int index = m_topIndex;
    m_wordCursor = m_scanCursor;
    int more = m_scanCursor.HasMore();
    while (more && !found) {
        bool lineEnded = false;
        if (lineTop <= position->m_y && position->m_y < m_textMetrics.tmHeight + lineTop) {
            onLine = true;
            position->m_y = lineTop;
        }
        int nextX;
        do {
            nextX = x;
            if (!m_scanCursor.HasMore() || found) break;
            int length = ScanWord(m_wordCursor, m_scanCursor);
            if (length == 0) {
                ++index;
                const int c = m_scanCursor.Current();
                if (c == '\t') {
                    nextX = NextTabStop(x, tabWidth);
                    if (m_lineWidth < nextX) {
                        lineEnded = true;
                    } else if (onLine && position->m_x <= nextX) {
                        found = true;
                        position->m_x = x;
                        position->m_index = index;
                    }
                } else if (c == '\n') {
                    lineEnded = true;
                } else if (c == ' ') {
                    nextX = x + m_spaceWidth;
                    if (m_lineWidth < nextX) {
                        lineEnded = true;
                        nextX = x;
                    } else if (onLine && position->m_x <= nextX) {
                        found = true;
                        position->m_x = nextX - m_spaceWidth;
                        position->m_index = index;
                    }
                }
                m_wordCursor++;
            } else {
                GetTextExtentPoint32A(m_dc, m_wordCursor.m_current, length, &extent);
                if (m_lineWidth < x + extent.cx && m_leftMargin < x) {
                    nextX = x - m_spaceWidth;
                    break;
                }
                if (m_lineWidth - m_leftMargin < extent.cx) length = FitWordToLine(m_dc, length, &extent);
                if (onLine && position->m_x <= x + extent.cx) {
                    while (length >= 1) {
                        --length;
                        m_scanCursor--;
                        GetTextExtentPoint32A(m_dc, m_wordCursor.m_current, length, &extent);
                        if (position->m_x > x + extent.cx) break;
                    }
                    found = true;
                    position->m_x = x + extent.cx;
                    position->m_index = index + length;
                }
                nextX = x + extent.cx;
                index += length;
                m_wordCursor = ++m_scanCursor;
            }
            x = nextX;
        } while (!lineEnded);
        if (onLine && !found) {
            found = true;
            position->m_x = nextX;
            position->m_index = index - 1;
            break;
        }
        x = m_leftMargin;
        lineTop += m_textMetrics.tmHeight;
        more = m_scanCursor.HasMore();
    }
    if (!m_scanCursor.HasMore() && !found) {
        position->m_index = m_text.Length();
        position->m_y = lineTop;
        position->m_x = m_leftMargin;
    }
}

void TextScrollField::InsertText(const String& text, int position) {
    m_text.Insert(text, position);
    Reset();
}

void TextScrollField::ClearText() {
    m_text.Clear();
    Reset();
}

// ---- Scroll bar requests (the bar calls them on the CoolScrollable part) ------------------------------------

int TextScrollField::PageUp() {
    for (int lines = m_clientHeight / m_textMetrics.tmHeight; lines != 0; --lines) ScrollUp(0);
    InvalidateRect(m_hwnd, nullptr, FALSE);
    return 1;
}

int TextScrollField::PageDown() {
    for (int lines = m_clientHeight / m_textMetrics.tmHeight; lines != 0; --lines) ScrollDown(0);
    InvalidateRect(m_hwnd, nullptr, FALSE);
    return 1;
}

int TextScrollField::LineUp() {
    ScrollUp(0);
    InvalidateRect(m_hwnd, nullptr, FALSE);
    return -m_textMetrics.tmHeight;
}

int TextScrollField::LineDown() {
    ScrollDown(0);
    InvalidateRect(m_hwnd, nullptr, FALSE);
    return m_textMetrics.tmHeight;
}

int TextScrollField::ScrollVertical(int delta) {
    int lines = delta / m_textMetrics.tmHeight;
    if (lines > 0) {
        do ScrollDown(0);
        while (--lines != 0);
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return 1;
    }
    if (lines < 0) {
        lines = -lines;
        do ScrollUp(0);
        while (--lines != 0);
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return 1;
    }
    return 0;
}

int TextScrollField::ScrollHorizontal(int) { return 0; }

// ---- Window procedure ------------------------------------------------------------------------------------------

void TextScrollField::InvalidateFromCaretLine(HWND hwnd) {
    RECT rest;
    SetRect(&rest, 0, m_caret.m_y, m_lineWidth, m_clientHeight);
    InvalidateRect(hwnd, &rest, FALSE);
}

void TextScrollField::SetCaretToPosition() { SetCaretPos(m_caret.m_x, m_caret.m_y); }

void TextScrollField::ForwardKey(WPARAM key, LPARAM lParam) { m_parent->WindowProc(WM_KEYDOWN, key, lParam); }

void TextScrollField::OnKeyDown(HWND hwnd, WPARAM key, LPARAM lParam) {
    StringCursor cursor(m_text);
    for (int i = 0; i < m_caret.m_index; ++i) cursor++;
    SIZE extent;
    switch (key) {
    case VK_BACK:
        if (!(m_state & kTextEditable)) break;
        if (m_caret.m_index > 0) {
            cursor--;
            extent.cx = 0;
            extent.cy = 0;
            GetTextExtentPoint32A(m_dc, cursor.m_current, 1, &extent);
            m_text.RemoveRange(m_caret.m_index - 1, m_caret.m_index);
            --m_caret.m_index;
            m_caret.m_x -= extent.cx;
            if (m_caret.m_x < m_leftMargin) m_caret.m_x = m_leftMargin;
            SetCaretToPosition();
            InvalidateFromCaretLine(hwnd);
        }
        return;
    case VK_DELETE:
        if (!(m_state & kTextEditable)) break;
        m_text.RemoveRange(m_caret.m_index, m_caret.m_index + 1);
        InvalidateFromCaretLine(hwnd);
        return;
    case VK_RETURN:
        if (!(m_state & kTextEditable)) break;
        m_text.InsertChar(*g_newlineString, m_caret.m_index);
        if (m_clientHeight < m_caret.m_y + m_textMetrics.tmHeight * 2) {
            if (!IsWindowVisible(m_verticalBar->m_hwnd)) UpdateScrollBar();
            ScrollDown(1);
        } else {
            m_caret.m_y = m_textMetrics.tmHeight + m_caret.m_y;
        }
        m_caret.m_x = m_leftMargin;
        SetCaretPos(m_leftMargin, m_caret.m_y);
        ++m_caret.m_index;
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    case VK_NEXT:
        PageDown();
        // BUG(original): no break, so Page Down also pages back up (and is forwarded to the parent).
        [[fallthrough]];
    case VK_PRIOR:
        PageUp();
        break;
    case VK_END:
        m_caret.m_x = m_lineWidth;
        LocatePosition(&m_caret);
        SetCaretToPosition();
        return;
    case VK_HOME: {
        GetTextExtentPoint32A(m_dc, cursor.m_current, 1, &extent);
        int x = m_caret.m_x - extent.cx;
        while (m_leftMargin < x) {
            --m_caret.m_index;
            m_caret.m_x = x;
            cursor--;
            GetTextExtentPoint32A(m_dc, cursor.m_current, 1, &extent);
            x = m_caret.m_x - extent.cx;
        }
        m_caret.m_x = m_leftMargin;
        SetCaretPos(m_leftMargin, m_caret.m_y);
        return;
    }
    case VK_LEFT:
        if (!(m_state & kTextEditable)) break;
        if (m_caret.m_index - 1 < 0) return;
        cursor--;
        --m_caret.m_index;
        GetTextExtentPoint32A(m_dc, cursor.m_current, 1, &extent);
        if (m_caret.m_x - extent.cx < m_leftMargin || cursor.Current() == '\n') {
            const int y = m_caret.m_y - m_textMetrics.tmHeight;
            if (y < 0) {
                ScrollUp(1);
            } else {
                m_caret.m_y = y;
            }
            m_caret.m_x = m_lineWidth - m_textMetrics.tmAveCharWidth / 2;
            LocatePosition(&m_caret);
        } else {
            m_caret.m_x -= extent.cx;
        }
        SetCaretToPosition();
        return;
    case VK_RIGHT:
        if (!(m_state & kTextEditable)) break;
        if (!cursor.HasMore()) return;
        ++m_caret.m_index;
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    case VK_UP:
        if (m_caret.m_y - m_textMetrics.tmHeight < 0) {
            ScrollUp(1);
            InvalidateRect(hwnd, nullptr, FALSE);
        } else {
            m_caret.m_y -= m_textMetrics.tmHeight / 2;
        }
        MoveCaretVertically();
        return;
    case VK_DOWN: {
        const int lineHeight = m_textMetrics.tmHeight;
        if (m_caret.m_y + lineHeight * 2 < m_clientHeight) {
            m_caret.m_y = lineHeight / 2 + lineHeight + m_caret.m_y;
        } else {
            ScrollDown(1);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        MoveCaretVertically();
        return;
    }
    case VK_INSERT:
        m_state ^= kTextInsertMode;
        return;
    default:
        break;
    }
    ForwardKey(key, lParam);
}

void TextScrollField::MoveCaretVertically() {
    m_caret.m_x += m_textMetrics.tmAveCharWidth / 2;
    LocatePosition(&m_caret);
    SetCaretToPosition();
}

void TextScrollField::OnChar(HWND hwnd, WPARAM wParam) {
    const char typed = static_cast<char>(wParam);
    if (!(m_state & kTextInsertMode)) m_text.RemoveRange(m_caret.m_index - 1, m_caret.m_index);
    m_text.InsertChar(typed, m_caret.m_index);
    SIZE extent;
    GetTextExtentPoint32A(m_dc, &typed, 1, &extent);
    m_caret.m_x += extent.cx;
    SetCaretPos(m_caret.m_x, m_caret.m_y);
    if (m_clientHeight < m_caret.m_y + m_textMetrics.tmHeight) {
        ScrollDown(1);
        m_caret.m_y -= m_textMetrics.tmHeight;
    }
    ++m_caret.m_index;
    UpdateScrollBar();
    InvalidateRect(hwnd, nullptr, FALSE);
}

void TextScrollField::OnLeftButtonDown(HWND hwnd, LPARAM lParam) {
    if (GetFocus() != hwnd) SetFocus(hwnd);
    const int x = lParam & 0xffff;
    const int y = static_cast<uint32_t>(lParam) >> 16;
    if (m_state & kTextSelecting) {
        RECT selection;
        SetRect(&selection, m_selectionStart.m_x, m_selectionStart.m_y, m_selectionEnd.m_x, m_selectionEnd.m_y);
        NormalizeRect(&selection);
        InvalidateRect(hwnd, &selection, FALSE);
        UpdateWindow(hwnd);
        m_state &= ~kTextSelecting;
    }
    m_caret.m_x = x;
    m_caret.m_y = y;
    LocatePosition(&m_caret);
    SetCaretPos(m_caret.m_x, m_caret.m_y);
    ShowCaret(hwnd);
    m_state |= kTextMouseDown;
    SetCaptureWindow(hwnd);
}

void TextScrollField::OnMouseMove(HWND hwnd, LPARAM lParam) {
    POINT point = {static_cast<LONG>(lParam & 0xffff), static_cast<LONG>(static_cast<uint32_t>(lParam) >> 16)};
    if (m_state & kTextMouseDown) {
        POINT caret;
        GetCaretPos(&caret);
        TrackSelection(&caret);
        HideCaret(hwnd);
        m_state &= ~kTextMouseDown;
    }
    m_state |= kTextSelecting;
    TrackSelection(&point);
}

void TextScrollField::RestoreCaret(HWND hwnd) {
    if (m_dc) CreateCaretForFont();
    SetCaretPos(m_caret.m_x, m_caret.m_y);
    ShowCaret(hwnd);
}

LRESULT REB_STDCALL TextScrollField::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    HWND hwnd = m_hwnd;
    switch (message) {
    case WM_CREATE: {
        CreateVerticalScrollBar(m_instance, this, m_scrollBarSlot, m_scrollBarId, m_clientHeight, 0, m_clientHeight,
                                0, nullptr);
        HDC windowDc = GetDC(hwnd);
        m_dc = CreateCompatibleDC(windowDc);
        ReleaseDC(hwnd, windowDc);
        return 0;
    }
    case WM_SIZE:
        Reset();
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(hwnd, &paint);
        HPALETTE previousPalette = SelectPalette(dc, g_palette, FALSE);
        m_background->DrawToDc(dc, '@', SRCCOPY, 0, 0, 0, 0, 0, 0);
        DrawLines(dc);
        SetCaretPos(m_caret.m_x, m_caret.m_y);
        SelectPalette(dc, previousPalette, FALSE);
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_SETFOCUS:
        if (!(m_state & kTextEditable)) return 0;
        if (reinterpret_cast<HWND>(wParam) == hwnd) return 0;
        RestoreCaret(hwnd);
        return 0;
    case WM_KILLFOCUS:
        DestroyCaret();
        m_hasCaret = 0;
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_CLOSE:
        return 0;
    case WM_SHOWWINDOW:
        if (wParam == 0 || !(m_state & kTextEditable)) {
            HideCaret(hwnd);
        } else {
            if (m_dc) {
                CreateCaretForFont();
                SetCaretPos(m_caret.m_x, m_caret.m_y);
                ShowCaret(hwnd);
            }
            if (GetFocus() != hwnd) SetFocus(hwnd);
        }
        return CoolWindow::WindowProc(WM_SHOWWINDOW, wParam, lParam);
    case WM_KEYDOWN:
        OnKeyDown(hwnd, wParam, lParam);
        return 0;
    case WM_CHAR:
        if (wParam == '\t') return 0;
        if (!_isprint(static_cast<int>(wParam))) return 0;
        if (!(m_state & kTextEditable)) return 0;
        OnChar(hwnd, wParam);
        return 0;
    case WM_MOUSEMOVE:
        if (!(m_state & kTextEditable)) return 0;
        if (!(wParam & MK_LBUTTON)) return 0;
        OnMouseMove(hwnd, lParam);
        return 0;
    case WM_LBUTTONDOWN:
        if (!(m_state & kTextEditable)) return 0;
        OnLeftButtonDown(hwnd, lParam);
        return 0;
    case WM_LBUTTONUP: {
        if (!(m_state & kTextEditable)) return 0;
        POINT point = {static_cast<LONG>(lParam & 0xffff), static_cast<LONG>(static_cast<uint32_t>(lParam) >> 16)};
        if (m_state & kTextSelecting) TrackSelection(&point);
        ReleaseCapture();
        return 0;
    }
    default:
        return CoolWindow::WindowProc(message, wParam, lParam);
    }
}
