#include "core/reb_abi.h"
#include "ui/game_window_object.h"

#include <cstring>

#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "gfx/dib_bitmap.h"
#include "ui/resource_dll.h"

namespace {

constexpr uint32_t kGameWindowStyle = WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
constexpr char kBitBltMode = '@';

}  // namespace

GameWindowObject::GameWindowObject(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                                   DibBitmap* background, uint32_t extraStyle)
    : CoolWindow(instance, "GAME_WINDOW_OBJECT", extraStyle | kGameWindowStyle, x, y, width, height, parent,
                 nullptr, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)), nullptr, nullptr, nullptr, 0, 0) {
    m_background = background;
    m_source = nullptr;
}

GameWindowObject::~GameWindowObject() {
    if (m_background) delete m_background;
    if (m_source) delete m_source;
    REB_KEEP_VPTR_STORE();
}

void GameWindowObject::SetBackground(DibBitmap* image, int srcX, int srcY) {
    HWND hwnd = m_hwnd;
    HDC dc = GetDC(hwnd);
    if (!m_background && m_source)
        m_background = NewOrNull<DibBitmap>(static_cast<LONG>(m_clientWidth), static_cast<LONG>(m_clientHeight), m_source, dc);
    else if (image && !m_background)
        m_background = NewOrNull<DibBitmap>(static_cast<LONG>(m_clientWidth), static_cast<LONG>(m_clientHeight), image, dc);

    if (image) {
        bool consumed = true;
        if (!m_source) {
            image->Blit(m_background, 0, 0, 0, 0, 0, 0);
        } else if (m_background) {
            m_source->Blit(m_background, 0, 0, srcX, srcY, 0, 0);
            image->DrawKeyed(m_background, 0, 0);
        } else {
            // BUG(original): the image leaks when the background could not be allocated.
            consumed = false;
        }
        if (consumed) delete image;
    } else {
        if (m_background) delete m_background;
        m_background = nullptr;
    }
    ReleaseDC(hwnd, dc);
}

DibBitmap* GameWindowObject::GetBackground() { return m_background; }

void GameWindowObject::Draw(HDC dc) {
    if (m_background && m_background->IsLoaded() && g_palette) {
        HPALETTE previous = SelectPalette(dc, g_palette, FALSE);
        m_background->DrawToDc(dc, kBitBltMode, SRCCOPY, 0, 0, 0, 0, 0, 0);
        SelectPalette(dc, previous, FALSE);
    }
}

void GameWindowObject::DrawRect(HDC dc, const RECT* area) {
    if (m_background && m_background->IsLoaded() && g_palette) {
        HPALETTE previous = SelectPalette(dc, g_palette, FALSE);
        const int top = area->top;
        const int left = area->left;
        // BUG(original): the call omits the raster-operation argument, so every argument after the mode lands one
        // parameter too early (rop = left, destX = top, destY = left, srcX = top, srcY = width, width = height).
        m_background->DrawToDc(dc, kBitBltMode, left, top, left, top, area->right - left, area->bottom - top, 0);
        SelectPalette(dc, previous, FALSE);
    }
}

int GameWindowObject::Virtual0D(int, int, int, int, int) { return 0; }

// Linker-folded with other do-nothing virtuals.
void GameWindowObject::Virtual0B() {}

void GameWindowObject::Virtual0C() {}

void GameWindowObject::DrawOverlay(HDC) {}

LRESULT REB_STDCALL GameWindowObject::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    HWND hwnd = m_hwnd;
    LRESULT result = 0;
    if (message == WM_PAINT) {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(hwnd, &paint);
        Draw(dc);
        DrawOverlay(dc);
        EndPaint(hwnd, &paint);
        return result;
    }
    if (message == WM_DESTROY) return result;
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_CTLCOLORBTN) return reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH));
    if (message == WM_QUERYNEWPALETTE || message == WM_PALETTECHANGED) {
        if (message == WM_PALETTECHANGED) {
            InvalidateRect(hwnd, nullptr, FALSE);
            for (HWND child = GetWindow(hwnd, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
                SendMessageA(child, WM_PALETTECHANGED, wParam, lParam);
            if (hwnd == reinterpret_cast<HWND>(wParam)) return result;
        }
        HDC dc = GetDC(hwnd);
        HPALETTE previous = SelectPalette(dc, g_palette, FALSE);
        result = RealizePalette(dc);
        SelectPalette(dc, previous, FALSE);
        if (result) InvalidateRect(hwnd, nullptr, FALSE);
        ReleaseDC(hwnd, dc);
        return result;
    }
    return CoolWindow::WindowProc(message, wParam, lParam);
}

int GameWindowObject::DrawStringResource(UINT stringId, RECT area, int fontSlot, COLORREF color, UINT format) {
    char text[0x100];
    std::memset(text, 0, 0xff);
    bool drawn = false;
    LoadResourceString(stringId, text, 0xff);
    if (text[0] == '\0') return drawn;
    DibBitmap* target = m_source ? m_source : m_background;
    if (!target) return drawn;

    HWND hwnd = m_hwnd;
    HDC screen = GetDC(hwnd);
    HDC memory = CreateCompatibleDC(screen);
    HFONT font = CoolGetFont(fontSlot);
    drawn = memory != nullptr;
    if (drawn) {
        HPALETTE palette = g_palette;
        HGDIOBJ previousBitmap = SelectObject(memory, target->DibSectionHandle());
        HPALETTE previousPalette = SelectPalette(memory, palette, FALSE);
        HGDIOBJ previousFont = SelectObject(memory, font);
        SetTextColor(memory, color);
        SetBkMode(memory, TRANSPARENT);
        DrawTextA(memory, text, -1, &area, format);
        SelectPalette(memory, previousPalette, FALSE);
        SelectObject(memory, previousFont);
        SelectObject(memory, previousBitmap);
        DeleteDC(memory);
    }
    ReleaseDC(hwnd, screen);
    return drawn;
}
