#pragma once
#include "core/reb_abi.h"
#include <cstdint>

#include "core/layout.h"
#include "platform/win32.h"
#include "ui/cool_window.h"

class CoolScrollable;
class DibBitmap;

// Ids of the auto-repeat timers (WM_TIMER) a scroll bar runs while the mouse button is held.
enum CoolScrollTimer : UINT {
    kScrollTimerDrag = 1000,      // thumb drag, every 50 ms
    kScrollTimerPageBack = 1001,  // the others every 150 ms
    kScrollTimerPageForward = 1002,
    kScrollTimerPageLeft = 1003,
    kScrollTimerPageRight = 1004,
};

// Child ids of the two arrow buttons; their WM_COMMAND reaches OnCommand.
constexpr uint32_t kScrollBackButtonId = 100;
constexpr uint32_t kScrollForwardButtonId = 101;

// The "Scroll Bar" window: the common base of Cool's vertical and horizontal scroll bars (0x60f640 / 0x6100d0).
// It shows the range [m_first, m_last) of m_total units, owned by m_owner (the scrolled control), which it
// notifies and asks to move. Its artwork comes from an RT_RCDATA record of uint16 values in resource slot
// m_artSlot: [0] bar thickness, [1] track tile, [2]/[3] back-arrow button images, [4]/[5] forward-arrow
// button images, [6..8] thumb top/middle/bottom. The base implements the window plumbing; the geometry and
// the drawing are virtuals the two derived bars supply.
class CoolScrollBar : public CoolWindow {
public:
    CoolScrollBar(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent, CoolScrollable* owner,
                  uint16_t artSlot, uint16_t artId, int total, int first, int last,
                  uint32_t buttonFlags);  // 0x60aa70
    virtual ~CoolScrollBar();             // 0x60ab80 (scalar deleting destructor 0x60ab60)

    // Slot 5: WM_CREATE loads the artwork (LoadArt) and lays the bar out; WM_PAINT draws m_surface; WM_COMMAND
    // goes to OnCommand, WM_TIMER to OnTimer; a left click is posted to the parent as message 0x40e
    // (wParam = message << 16 | child id) before OnLButtonDown; WM_LBUTTONUP ends the drag.
    virtual LRESULT REB_STDCALL WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;  // 0x60aca0

    virtual void OnCommand(WPARAM id);             // slot 9, 0x5ae1e0: does nothing (arrow buttons)
    virtual int Layout();                          // slot 10, 0x6158b0: returns 0; sizes the bar, moves the buttons
    virtual RECT* GetThumbRect(RECT* out);         // slot 11, 0x60b4d0: empty rectangle; returns `out`
    virtual RECT* GetTrackRect(RECT* out);         // slot 12, 0x60b4d0: empty rectangle; returns `out`
    virtual int BuildThumb();                      // slot 13, 0x6158b0: returns 0
    virtual int BuildTrack();                      // slot 14, 0x6158b0: returns 0; builds m_surface/m_track/m_thumb
    virtual void OnLButtonDown(int x, int y);      // slot 15, 0x403ef0: does nothing
    virtual void OnDragTimer();                    // slot 16, 0x619730: does nothing
    virtual void PageBack();                       // slot 17, 0x619730
    virtual void PageForward();                    // slot 18, 0x619730
    virtual void PageLeft();                       // slot 19, 0x619730
    virtual void PageRight();                      // slot 20, 0x619730

    // Loads the artwork and creates the arrow buttons; returns GetLastError() when the record is missing.
    DWORD LoadArt();                             // 0x60af70
    // Copies the part of `parentImage` under the bar into m_background and lays the bar out again.
    void SetBackground(DibBitmap* parentImage);  // 0x60b2c0
    // ScrollBy(delta), then redraws background, track and thumb into m_surface; 1 if drawn.
    int Scroll(int delta);                       // 0x60b3d0
    // Layout, then BuildTrack if that succeeded; returns BuildTrack's result (0 otherwise).
    int Refresh();                               // 0x60b4b0
    void EndDrag();                              // 0x60b500: WM_LBUTTONUP
    void Draw(HDC dc);                           // 0x60b560
    // Sets m_total (ignored unless positive), re-clamps the visible range and refreshes.
    int SetRange(int total);                     // 0x60b590
    // Clamps [first, last) into [0, m_total); hides the bar when everything is visible. 0 if first >= last.
    int SetVisibleRange(int first, int last);    // 0x60b5d0
    // Moves the visible range by `delta`, keeping it inside [0, m_total); returns 1.
    int ScrollBy(int delta);                     // 0x60b660
    void StartTimer(UINT id);                    // 0x60b6d0: CoolScrollTimer
    void StopTimer();                            // 0x60b730
    void OnTimer(WPARAM id);                     // 0x60b750

    UINT m_timerId;                // +0x94 running CoolScrollTimer, 0 if none
    CoolScrollable* m_owner;       // +0x98 the scrolled control
    int m_dragging;                // +0x9c
    int m_dragX;                   // +0xa0
    int m_dragY;                   // +0xa4
    int m_dragOffset;              // +0xa8
    uint16_t m_thickness;          // +0xac art record [0]
    uint16_t m_padAe;              // +0xae
    HWND m_previousCapture;        // +0xb0 restored by EndDrag
    DibBitmap* m_trackTile;        // +0xb4 art [1]; owned
    DibBitmap* m_thumbTop;         // +0xb8 art [6]; owned
    DibBitmap* m_thumbMiddle;      // +0xbc art [7]; owned
    DibBitmap* m_thumbBottom;      // +0xc0 art [8]; owned
    DibBitmap* m_surface;          // +0xc4 what WM_PAINT shows; owned
    DibBitmap* m_background;       // +0xc8 parent image under the bar; owned
    DibBitmap* m_track;            // +0xcc owned
    DibBitmap* m_thumb;            // +0xd0 owned
    CoolWindow* m_backButton;      // +0xd4 CoolStrobeButton, a child (kScrollBackButtonId)
    CoolWindow* m_forwardButton;   // +0xd8 CoolStrobeButton, a child (kScrollForwardButtonId)
    int m_total;                   // +0xdc
    int m_first;                   // +0xe0
    int m_last;                    // +0xe4
    uint16_t m_artSlot;            // +0xe8 resource DLL slot of the artwork
    uint16_t m_artId;              // +0xea RT_RCDATA id of the art record
    uint32_t m_buttonFlags;        // +0xec passed on to the arrow buttons

private:
    void CreateArrowButton(CoolWindow*& button, HMODULE module, uint16_t normalId, uint16_t pressedId,
                           uint32_t childId);
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(CoolScrollBar, 0xf0);
REB_ASSERT_OFFSET(CoolScrollBar, m_timerId, 0x94);
REB_ASSERT_OFFSET(CoolScrollBar, m_owner, 0x98);
REB_ASSERT_OFFSET(CoolScrollBar, m_dragOffset, 0xa8);
REB_ASSERT_OFFSET(CoolScrollBar, m_thickness, 0xac);
REB_ASSERT_OFFSET(CoolScrollBar, m_previousCapture, 0xb0);
REB_ASSERT_OFFSET(CoolScrollBar, m_trackTile, 0xb4);
REB_ASSERT_OFFSET(CoolScrollBar, m_surface, 0xc4);
REB_ASSERT_OFFSET(CoolScrollBar, m_backButton, 0xd4);
REB_ASSERT_OFFSET(CoolScrollBar, m_total, 0xdc);
REB_ASSERT_OFFSET(CoolScrollBar, m_artSlot, 0xe8);
REB_ASSERT_OFFSET(CoolScrollBar, m_buttonFlags, 0xec);
#pragma clang diagnostic pop
