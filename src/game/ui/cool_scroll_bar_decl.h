#pragma once
#include <cstdint>

#include "core/layout.h"
#include "platform/win32.h"
#include "ui/cool_window.h"

// Minimal stand-ins for CoolScrollBar (display/scrollbar range, ui/cool_scroll_bar.h) and CoolScrollable
// (drag-list range, ui/cool_scrollable.h), reduced to what CoolVerticalScrollBar / CoolHorizontalScrollBar use;
// names are the owners'. Drop this file for the owners' headers at merge.

class CoolScrollBar;
class DibBitmap;

// Owner of a window's scroll bars (vtable 0x66e118, ctor 0x60a420). The Line/Page requests return the distance
// scrolled (0: nothing happened), the Scroll requests whether the content moved by `delta`.
class CoolScrollable {
public:
    virtual ~CoolScrollable();                 // 0x60a460
    virtual int PageUp();                      // slot 1
    virtual int PageDown();                    // slot 2
    virtual int LineUp();                      // slot 3
    virtual int LineDown();                    // slot 4
    virtual int ScrollVertical(int delta);     // slot 5
    virtual int PageLeft();                    // slot 6
    virtual int PageRight();                   // slot 7
    virtual int LineRight();                   // slot 8
    virtual int LineLeft();                    // slot 9
    virtual int ScrollHorizontal(int delta);   // slot 10
    virtual void OnScrollEnd();                // slot 11

    CoolScrollBar* m_verticalBar;    // +0x04
    CoolScrollBar* m_horizontalBar;  // +0x08
};

// Timer ids of CoolScrollBar::StartTimer (WM_TIMER dispatches them to slots 16..20).
enum CoolScrollTimer : UINT {
    kScrollTimerDrag = 1000,         // every 50 ms while the thumb is dragged
    kScrollTimerPageBack = 1001,     // every 150 ms while the track is held
    kScrollTimerPageForward = 1002,
    kScrollTimerPageLeft = 1003,
    kScrollTimerPageRight = 1004,
};

constexpr uint16_t kScrollBackButtonId = 100;
constexpr uint16_t kScrollForwardButtonId = 101;

// The "Scroll Bar" window class (vtable 0x66e158): a track with a thumb and two arrow buttons over a range of
// m_total units of which m_first..m_last are visible. The orientation classes supply slots 9..20.
class CoolScrollBar : public CoolWindow {
public:
    CoolScrollBar(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                  CoolScrollable* owner, uint16_t artSlot, uint16_t artId, int total, int first, int last,
                  uint32_t buttonFlags);  // 0x60aa70
    virtual ~CoolScrollBar();             // 0x60ab80

    virtual LRESULT __stdcall WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;  // 0x60aca0
    virtual void OnCommand(WPARAM id) = 0;              // slot 9
    virtual int Layout() = 0;                           // slot 10
    virtual RECT* GetThumbRect(RECT* out) = 0;          // slot 11
    virtual RECT* GetTrackRect(RECT* out) = 0;          // slot 12
    virtual int BuildThumb() = 0;                       // slot 13
    virtual int BuildTrack() = 0;                       // slot 14
    virtual void OnLButtonDown(int x, int y) = 0;       // slot 15
    virtual void OnDragTimer() = 0;                     // slot 16
    virtual void PageBack() = 0;                        // slot 17
    virtual void PageForward() = 0;                     // slot 18
    virtual void PageLeft() = 0;                        // slot 19
    virtual void PageRight() = 0;                       // slot 20

    int Scroll(int delta);          // 0x60b3d0: ScrollBy and redraw into m_surface
    void StartTimer(UINT id);       // 0x60b6d0
    void StopTimer();               // 0x60b730

    UINT_PTR m_timerId;            // +0x94
    CoolScrollable* m_owner;       // +0x98
    int m_dragging;                // +0x9c
    int m_dragX;                   // +0xa0
    int m_dragY;                   // +0xa4
    int m_dragOffset;              // +0xa8
    uint16_t m_thickness;          // +0xac
    uint16_t m_padAe;              // +0xae
    HWND m_previousCapture;        // +0xb0
    DibBitmap* m_trackTile;        // +0xb4
    DibBitmap* m_thumbTop;         // +0xb8
    DibBitmap* m_thumbMiddle;      // +0xbc
    DibBitmap* m_thumbBottom;      // +0xc0
    DibBitmap* m_surface;          // +0xc4
    DibBitmap* m_background;       // +0xc8
    DibBitmap* m_track;            // +0xcc
    DibBitmap* m_thumb;            // +0xd0
    CoolWindow* m_backButton;      // +0xd4
    CoolWindow* m_forwardButton;   // +0xd8
    int m_total;                   // +0xdc
    int m_first;                   // +0xe0
    int m_last;                    // +0xe4
    uint16_t m_artSlot;            // +0xe8
    uint16_t m_artId;              // +0xea
    uint32_t m_buttonFlags;        // +0xec
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(CoolScrollable, 0x0c);
REB_ASSERT_SIZE(CoolScrollBar, 0xf0);
REB_ASSERT_OFFSET(CoolScrollBar, m_timerId, 0x94);
REB_ASSERT_OFFSET(CoolScrollBar, m_thickness, 0xac);
REB_ASSERT_OFFSET(CoolScrollBar, m_trackTile, 0xb4);
REB_ASSERT_OFFSET(CoolScrollBar, m_backButton, 0xd4);
REB_ASSERT_OFFSET(CoolScrollBar, m_total, 0xdc);
REB_ASSERT_OFFSET(CoolScrollBar, m_artSlot, 0xe8);
#pragma clang diagnostic pop
