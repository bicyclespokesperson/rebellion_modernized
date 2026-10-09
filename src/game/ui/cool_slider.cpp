#include "core/reb_abi.h"
#include "ui/cool_slider.h"

#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "gfx/dib_bitmap.h"

namespace {

constexpr uint32_t kSliderStyle = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS;
constexpr uint32_t kBitmapResourceKind = 2;
constexpr char kBitBltMode = '@';

}  // namespace

CoolSlider::CoolSlider(HINSTANCE instance, int x, int y, CoolWindow* parent, uint32_t childId, int orientation,
                       DibBitmap* backdrop, uint32_t trackBitmapId, uint32_t knobBitmapId, uint32_t value,
                       uint32_t range)
    : CoolWindow(instance, "slider", kSliderStyle, x, y, 0, 0, parent, nullptr,
                 static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)), nullptr, nullptr, nullptr, 0, 0) {
    m_backdropY = y;
    m_childId = childId;
    m_orientation = orientation;
    m_buffer = nullptr;
    m_backdrop = backdrop;
    m_backdropX = x;
    m_value = value;
    m_range = range;
    m_position.x = 0;
    m_position.y = 0;
    m_zeroX = 0;
    m_zeroY = 0;
    if (trackBitmapId == 0)
        m_track = nullptr;
    else
        m_track = NewOrNull<DibBitmap>(instance, trackBitmapId, kBitmapResourceKind);
    if (knobBitmapId == 0) {
        m_knob = nullptr;
    } else {
        m_knob = NewOrNull<DibBitmap>(instance, knobBitmapId, kBitmapResourceKind);
        if (m_knob) {
            m_knobWidth = m_knob->Width();
            m_knobHeight = m_knob->Height();
        }
    }
    if (m_track) {
        const int width = m_track->Width();
        const int height = m_track->Height();
        m_buffer = NewOrNull<DibBitmap>(width, height, backdrop);
        m_trackLeft = 0;
        m_trackTop = 0;
        m_width = width;
        m_height = height;
        m_trackRight = width;
        m_trackBottom = height;
        m_position.x = width / 2;
        m_position.y = height / 2;
        if (m_orientation == kSliderVertical) {
            m_zeroY = height;
            m_position.y = ((static_cast<uint32_t>(height * 100) / m_range) * m_value) / 100;
            m_knobOffset = width / 2 - m_knobWidth / 2;
        } else if (m_orientation == kSliderHorizontal) {
            m_zeroX = 0;
            m_position.x = ((static_cast<uint32_t>(width * 100) / m_range) * m_value) / 100;
            m_knobOffset = height / 2 - m_knobHeight / 2;
        }
    }
}

CoolSlider::~CoolSlider() {
    if (m_track) delete m_track;
    if (m_knob) delete m_knob;
    if (m_buffer) delete m_buffer;
    REB_KEEP_VPTR_STORE();
}

int CoolSlider::IsCursorOverTrack() {
    bool over = false;
    HWND hwnd = m_hwnd;
    POINT cursor;
    GetCursorPos(&cursor);
    ScreenToClient(hwnd, &cursor);
    if (m_trackLeft < cursor.x && cursor.x < m_trackRight && m_trackTop < cursor.y && cursor.y < m_trackBottom)
        over = true;
    return over;
}

void CoolSlider::TrackCursor() {
    bool inside = false;
    HWND hwnd = m_hwnd;
    POINT* cursor = &m_position;
    GetCursorPos(cursor);
    ScreenToClient(hwnd, cursor);
    if (m_orientation == kSliderVertical) {
        const int half = m_knobHeight / 2;
        if (m_trackLeft < cursor->x && cursor->x < m_trackRight && m_trackTop + half < cursor->y &&
            cursor->y < m_trackBottom - half)
            inside = true;
    } else if (m_orientation == kSliderHorizontal) {
        const int half = m_knobWidth / 2;
        if (m_trackLeft + half < cursor->x && cursor->x < m_trackRight - half && m_trackTop < cursor->y &&
            cursor->y < m_trackBottom)
            inside = true;
    }
    if (inside) {
        HDC dc = GetDC(hwnd);
        HPALETTE previous = SelectPalette(dc, g_palette, FALSE);
        Draw(dc);
        SelectPalette(dc, previous, FALSE);
        ReleaseDC(hwnd, dc);
    }
}

void CoolSlider::Draw(HDC dc) {
    if (m_track && m_backdrop) {
        m_backdrop->Blit(m_buffer, 0, 0, m_backdropX, m_backdropY, 0, 0);
        m_track->DrawKeyed(m_buffer, 0, 0);
    }
    int drawX;
    int drawY;
    if (m_orientation == kSliderVertical) {
        drawX = m_knobOffset;
        drawY = m_position.y - m_knobHeight / 2;
        const uint32_t zero = m_zeroY;
        uint32_t divisor = zero;
        if (zero == 0) divisor = m_clientHeight;
        m_value = ((zero - m_position.y) * m_range) / divisor;
    } else if (m_orientation == kSliderHorizontal) {
        const uint32_t zero = m_zeroX;
        drawY = m_knobOffset;
        drawX = m_position.x - m_knobWidth / 2;
        uint32_t divisor = zero;
        if (zero == 0) divisor = m_clientWidth;
        m_value = ((m_position.x - zero) * m_range) / divisor;
    } else {
        drawX = 0;
        drawY = 0;
    }
    if (m_knob) {
        m_knob->DrawKeyed(m_buffer, drawX, drawY);
        m_buffer->DrawToDc(dc, kBitBltMode, SRCCOPY, 0, 0, 0, 0, 0, 0);
    }
}

LRESULT REB_STDCALL CoolSlider::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    HWND hwnd = m_hwnd;
    if (message < WM_PAINT + 1 && message != WM_PAINT) {
        if (message != WM_CREATE) return CoolWindow::WindowProc(message, wParam, lParam);
        return 0;
    }
    if (message == WM_PAINT) {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(hwnd, &paint);
        HPALETTE previous = SelectPalette(dc, g_palette, FALSE);
        Draw(dc);
        SelectPalette(dc, previous, FALSE);
        EndPaint(hwnd, &paint);
        return 0;
    }
    if (message == WM_MOUSEMOVE) {
        if (IsCursorOverTrack() && wParam == MK_LBUTTON && GetCapture() == hwnd) {
            TrackCursor();
            PostMessageA(m_parent->m_hwnd, WM_COMMAND, m_childId, m_value);
        }
        return 0;
    }
    if (message == WM_LBUTTONDOWN) {
        SetCaptureWindow(hwnd);
        if (IsCursorOverTrack()) TrackCursor();
        return 0;
    }
    if (message != WM_LBUTTONUP) return CoolWindow::WindowProc(message, wParam, lParam);
    if (GetCapture() == hwnd) {
        ReleaseCapture();
        PostMessageA(m_parent->m_hwnd, WM_COMMAND, m_childId, m_value);
    }
    return 0;
}
