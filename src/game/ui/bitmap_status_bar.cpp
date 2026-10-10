#include "core/reb_abi.h"
#include "ui/bitmap_status_bar.h"

#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "gfx/dib_bitmap.h"

namespace {

constexpr uint32_t kBarStyle = 0x54000000;  // WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS

// The bar's length along an axis of `extent` pixels.
uint32_t BarLength(int extent, uint32_t position, uint32_t range) {
    return range ? static_cast<uint32_t>(extent) * position / range : 0;
}

}  // namespace

BitmapStatusBar::BitmapStatusBar(int x, int y, int width, int height, CoolWindow* parent, COLORREF barColor,
                                 COLORREF backgroundColor, DibBitmap* picture, int orientation, int direction)
    : CoolwinStatusBar(x, y, width, height, parent, barColor, backgroundColor, kBarStyle) {
    m_canvas = nullptr;
    m_picture = nullptr;
    m_orientation = orientation;
    m_direction = direction;
    if (picture) SetPicture(NewOrNull<DibBitmap>(picture->Width(), picture->Height(), picture));
}

BitmapStatusBar::~BitmapStatusBar() {
    REB_KEEP_VPTR_STORE();
    if (m_picture) delete m_picture;
    if (m_canvas) delete m_canvas;
}

void BitmapStatusBar::SetPicture(DibBitmap* image) {
    HDC dc = GetDC(m_hwnd);
    // BUG(original): the old picture is freed but m_picture keeps pointing at it, so the image is then copied into
    // freed memory instead of a new picture being made.
    if (m_picture && image && m_picture) delete m_picture;
    if (image->IsLoaded() && !m_picture) {
        if (m_canvas) delete m_canvas;
        const int width = image->Width();
        const int height = image->Height();
        m_picture = NewOrNull<DibBitmap>(width, height, image);
        m_canvas = NewOrNull<DibBitmap>(static_cast<LONG>(width), static_cast<LONG>(height), image, dc);
        MoveWindow(m_hwnd, m_x, m_y, width, height - 1, FALSE);
    }
    if (!image->IsLoaded()) {
        if (m_picture) delete m_picture;
        if (m_canvas) delete m_canvas;
        m_picture = nullptr;
        m_canvas = nullptr;
    } else {
        image->Blit(m_picture, 0, 0, 0, 0, 0, 0);
        if (image) delete image;
    }
    ReleaseDC(m_hwnd, dc);
}

LRESULT REB_STDCALL BitmapStatusBar::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    HWND hwnd = m_hwnd;
    switch (message) {
    case WM_ERASEBKGND:
        return 1;
    case WM_CTLCOLORBTN:
        return reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH));
    case WM_PALETTECHANGED:
        if (reinterpret_cast<HWND>(wParam) == hwnd) return 0;
        [[fallthrough]];
    case WM_QUERYNEWPALETTE: {
        HDC dc = GetDC(hwnd);
        HPALETTE previous = SelectPalette(dc, g_palette, FALSE);
        const UINT realized = RealizePalette(dc);
        SelectPalette(dc, previous, FALSE);
        InvalidateRect(hwnd, nullptr, FALSE);
        ReleaseDC(hwnd, dc);
        return realized;
    }
    case WM_PAINT:
        break;
    default:
        return CoolwinStatusBar::WindowProc(message, wParam, lParam);
    }

    PAINTSTRUCT paint;
    // BUG(original): EndPaint is never called (as in CoolwinStatusBar).
    HDC dc = BeginPaint(hwnd, &paint);
    SelectPalette(dc, g_palette, FALSE);
    HDC memory = CreateCompatibleDC(dc);
    HBITMAP scratch = nullptr;
    HGDIOBJ previousBitmap;
    if (m_picture) {
        previousBitmap = SelectObject(memory, m_canvas->DibSectionHandle());
    } else {
        scratch = CreateCompatibleBitmap(dc, m_clientWidth, m_clientHeight);
        previousBitmap = SelectObject(memory, scratch);
    }
    if (m_backgroundBrush) {
        HGDIOBJ previousBrush = SelectObject(memory, m_backgroundBrush);
        HGDIOBJ previousPen = SelectObject(memory, GetStockObject(NULL_PEN));
        Rectangle(memory, 0, 0, m_clientWidth + 1, m_clientHeight + 1);
        SelectObject(memory, previousBrush);
        SelectObject(memory, previousPen);
    }
    if (m_barBrush && m_position) {
        HGDIOBJ previousBrush = SelectObject(memory, m_barBrush);
        HGDIOBJ previousPen = SelectObject(memory, GetStockObject(NULL_PEN));
        const int right = m_clientWidth + 1;
        const int bottom = m_clientHeight + 1;
        if (m_orientation == kBarHorizontal) {
            const uint32_t length = BarLength(m_clientWidth, m_position, m_range);
            if (m_direction == kBarFromStart)
                Rectangle(memory, 0, 0, static_cast<int>(length + 1), bottom);
            else if (m_direction == kBarFromEnd)
                Rectangle(memory, static_cast<int>(m_clientWidth - length), 0, right, bottom);
        } else if (m_orientation == kBarVertical) {
            const uint32_t length = BarLength(m_clientHeight, m_position, m_range);
            if (m_direction == kBarFromStart)
                Rectangle(memory, 0, 0, right, static_cast<int>(length + 1));
            else if (m_direction == kBarFromEnd)
                Rectangle(memory, 0, static_cast<int>(m_clientHeight - length), right, bottom);
        }
        SelectObject(memory, previousBrush);
        SelectObject(memory, previousPen);
    }
    if (m_tickStep) {
        HGDIOBJ pen = m_tickPen ? static_cast<HGDIOBJ>(m_tickPen) : GetStockObject(BLACK_PEN);
        HGDIOBJ previousPen = SelectObject(memory, pen);
        if (m_orientation == kBarHorizontal) {
            for (uint32_t tick = 0; tick < m_range; tick += m_tickStep) {
                const int x = static_cast<int>(tick * static_cast<uint32_t>(m_clientWidth) / m_range);
                MoveToEx(memory, x, 0, nullptr);
                LineTo(memory, x, m_clientHeight);
            }
        } else if (m_orientation == kBarVertical) {
            for (uint32_t tick = 0; tick < m_range; tick += m_tickStep) {
                const int y = static_cast<int>(tick * static_cast<uint32_t>(m_clientHeight) / m_range);
                MoveToEx(memory, 0, y, nullptr);
                LineTo(memory, m_clientWidth, y);
            }
        }
        SelectObject(memory, previousPen);
    }
    if (m_picture) m_picture->DrawKeyed(m_canvas, 0, 0);
    BitBlt(dc, 0, 0, m_clientWidth, m_clientHeight, memory, 0, 0, SRCCOPY);
    SelectObject(memory, previousBitmap);
    if (!m_picture) DeleteObject(scratch);
    DeleteDC(memory);
    return 0;
}
