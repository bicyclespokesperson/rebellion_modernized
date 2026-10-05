#include "strategy_ui/panel_drag_button.h"

#include "core/vptr_store.h"
#include "gfx/dib_bitmap.h"
#include "ui/cool_window.h"

namespace {

constexpr uint32_t kDragButtonStyle = 0x56000000;
constexpr int kStockNullBrush = 5;
constexpr int kStockWhitePen = 6;

// The parent's window DC set up for drawing an XOR rectangle outline.
class OutlineDc {
public:
    explicit OutlineDc(HWND hwnd) : m_hwnd(hwnd), m_dc(GetWindowDC(hwnd)) {
        m_previousBrush = SelectObject(m_dc, GetStockObject(kStockNullBrush));
        m_previousPen = SelectObject(m_dc, GetStockObject(kStockWhitePen));
        SetROP2(m_dc, R2_XORPEN);
    }
    ~OutlineDc() {
        SelectObject(m_dc, m_previousBrush);
        SelectObject(m_dc, m_previousPen);
        ReleaseDC(m_hwnd, m_dc);
    }
    void Draw(const RECT& area) { Rectangle(m_dc, area.left, area.top, area.right, area.bottom); }

private:
    HWND m_hwnd;
    HDC m_dc;
    HGDIOBJ m_previousBrush;
    HGDIOBJ m_previousPen;
};

int LowWord(LPARAM value) { return static_cast<int>(static_cast<uint32_t>(value) & 0xffff); }
int HighWord(LPARAM value) { return static_cast<int>(static_cast<uint32_t>(value) >> 16); }

}  // namespace

PanelDragButton::PanelDragButton(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                                 uint32_t childId, uint32_t normalBitmapId, uint32_t pressedBitmapId,
                                 DibBitmap* backdrop, int limitLeft, int limitTop, int limitRight, int limitBottom,
                                 uint32_t flags)
    : CoolStrobeButton(instance, x, y, width, height, parent, childId, normalBitmapId, pressedBitmapId, backdrop, 0,
                       0) {
    m_flags = flags;
    m_backdropCopy = backdrop;
    m_limits.left = limitLeft;
    m_style = kDragButtonStyle;
    m_limits.top = limitTop;
    m_dragging = 0;
    m_limits.right = limitRight;
    m_limits.bottom = limitBottom;
}

PanelDragButton::~PanelDragButton() { REB_KEEP_VPTR_STORE(); }

LRESULT __stdcall PanelDragButton::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_MOUSEMOVE:
            return OnMouseMove(wParam, lParam);
        case WM_LBUTTONDOWN:
            if (m_flags & (kDragButtonMovable | kDragButtonDoubleClickOpens)) return 0;
            break;
        case WM_LBUTTONUP:
            if (m_flags & kDragButtonDoubleClickOpens) {
                if (!m_dragging) return 0;
            } else if (!m_dragging) {
                break;
            }
            return OnButtonUp();
        case WM_LBUTTONDBLCLK:
            return OnDoubleClick(lParam);
        default:
            break;
    }
    return CoolStrobeButton::WindowProc(message, wParam, lParam);
}

LRESULT PanelDragButton::OnMouseMove(WPARAM wParam, LPARAM lParam) {
    const int x = LowWord(lParam);
    const int y = HighWord(lParam);
    if (m_flags & kDragButtonMovable) {
        if (!m_dragging) {
            if (wParam & MK_LBUTTON) {
                if (m_normal->IsPixelOpaque(x, y)) {
                    m_dragging = 1;
                    SetCaptureWindow(m_hwnd);
                }
                m_outline.left = m_x;
                m_grabX = x;
                m_outline.bottom = m_clientHeight + m_y;
                m_outline.top = m_y;
                m_outline.right = m_clientWidth + m_x;
                m_grabY = y;
                return 0;
            }
        }
    }
    if (!m_dragging) return 0;
    OutlineDc outline(m_parent->m_hwnd);
    outline.Draw(m_outline);
    m_outline.left = (m_x - m_grabX) + x;
    m_outline.top = y + (m_y - m_grabY);
    m_outline.bottom = m_clientHeight + m_outline.top;
    m_outline.right = m_clientWidth + m_outline.left;
    outline.Draw(m_outline);
    return 0;
}

LRESULT PanelDragButton::OnButtonUp() {
    HWND hwnd = m_hwnd;
    m_dragging = 0;
    ReleaseCapture();
    {
        OutlineDc outline(m_parent->m_hwnd);
        outline.Draw(m_outline);
    }
    RECT area;
    area.left = m_x;
    area.top = m_y;
    area.right = m_clientWidth + m_x;
    area.bottom = m_clientHeight + m_y;
    InvalidateRect(m_parent->m_hwnd, &area, TRUE);
    int x = m_outline.left;
    int y = m_outline.top;
    ClampToLimits(&x, &y);
    MoveWindow(hwnd, x, y, m_clientWidth, m_clientHeight, TRUE);
    InvalidateRect(hwnd, nullptr, TRUE);
    return 0;
}

void PanelDragButton::ClampToLimits(int* x, int* y) {
    if (m_limits.left == 0 || m_limits.top == 0) return;
    const int originalY = m_outline.top;
    POINT corners[2] = {{m_outline.left, m_outline.top}, {m_outline.right, m_outline.bottom}};
    HDC dc = GetDC(m_parent->m_hwnd);
    DPtoLP(dc, corners, 2);
    ReleaseDC(m_parent->m_hwnd, dc);
    if (PtInRect(&m_limits, corners[0]) && PtInRect(&m_limits, corners[1])) return;
    if (m_limits.right < corners[0].x || m_limits.right < corners[1].x) {
        *x = m_limits.right - m_clientWidth;
    } else if (corners[0].x < m_limits.left || corners[1].x < m_limits.left) {
        *x = m_limits.left;
    }
    *y = m_limits.top;
    if (m_limits.top <= corners[0].y && m_limits.top <= corners[1].y) {
        if (m_limits.bottom < corners[0].y) {
            *y = m_limits.bottom - m_clientHeight;
        } else {
            *y = originalY;
            if (m_limits.bottom < corners[1].y) *y = m_limits.bottom - m_clientHeight;
        }
    }
}

LRESULT PanelDragButton::OnDoubleClick(LPARAM lParam) {
    if (!(m_flags & kDragButtonDoubleClickOpens)) return 0;
    m_hot = m_normal->IsPixelOpaque(LowWord(lParam), HighWord(lParam));
    if (!m_hot) return 0;
    PostMessageA(m_parent->m_hwnd, WM_COMMAND, GetChildId(), 0);
    return 0;
}
