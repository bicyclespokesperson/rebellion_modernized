#pragma once
#include <cstdint>

#include "core/layout.h"
#include "platform/win32.h"
#include "ui/cool_scroll_bar.h"
#include "ui/cool_scrollable.h"
#include "ui/cool_button.h"

// The two orientations of CoolScrollBar. A bar lies along the right (vertical) or bottom (horizontal) edge of
// its parent's client area with an arrow button at each end; the thumb is composed of a start cap, a tiled
// middle and an end cap. Both are created by CoolScrollable (0x60a490 / 0x60a560); the horizontal one only by
// the tactical panels.

class CoolVerticalScrollBar : public CoolScrollBar {
public:
    CoolVerticalScrollBar(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                          CoolScrollable* owner, uint16_t artSlot, uint16_t artId, int total, int first,
                          int last, uint32_t buttonFlags);  // 0x60f640
    virtual ~CoolVerticalScrollBar();  // 0x60f6c0 (scalar deleting destructor 0x60f6a0)

    virtual void OnCommand(WPARAM command) override;   // slot 9, 0x60f6d0
    virtual int Layout() override;                     // slot 10, 0x60fa80
    virtual RECT* GetThumbRect(RECT* out) override;    // slot 11, 0x60fb80
    virtual RECT* GetTrackRect(RECT* out) override;    // slot 12, 0x60fca0: between the arrows
    virtual int BuildThumb() override;                 // slot 13, 0x60f910
    virtual int BuildTrack() override;                 // slot 14, 0x60f7b0
    virtual void OnLButtonDown(int x, int y) override; // slot 15, 0x60fd50
    virtual void OnDragTimer() override;               // slot 16, 0x60fea0
    virtual void PageBack() override;                  // slot 17, 0x60fff0
    virtual void PageForward() override;               // slot 18, 0x610060
    virtual void PageLeft() override;                  // slot 19, 0x619730: does nothing
    virtual void PageRight() override;                 // slot 20, 0x619730: does nothing
};

class CoolHorizontalScrollBar : public CoolScrollBar {
public:
    CoolHorizontalScrollBar(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                            CoolScrollable* owner, uint16_t artSlot, uint16_t artId, int total, int first,
                            int last, uint32_t buttonFlags);  // 0x6100d0
    virtual ~CoolHorizontalScrollBar();  // 0x610150 (scalar deleting destructor 0x610130)

    virtual void OnCommand(WPARAM command) override;   // slot 9, 0x610160
    virtual int Layout() override;                     // slot 10, 0x610510
    virtual RECT* GetThumbRect(RECT* out) override;    // slot 11, 0x610680
    virtual RECT* GetTrackRect(RECT* out) override;    // slot 12, 0x610760: between the arrows
    virtual int BuildThumb() override;                 // slot 13, 0x6103a0
    virtual int BuildTrack() override;                 // slot 14, 0x610240
    virtual void OnLButtonDown(int x, int y) override; // slot 15, 0x610810
    virtual void OnDragTimer() override;               // slot 16, 0x610900
    virtual void PageBack() override;                  // slot 17, 0x619730: does nothing
    virtual void PageForward() override;               // slot 18, 0x619730: does nothing
    virtual void PageLeft() override;                  // slot 19, 0x610a20
    virtual void PageRight() override;                 // slot 20, 0x610aa0
};

REB_ASSERT_SIZE(CoolVerticalScrollBar, 0xf0);
REB_ASSERT_SIZE(CoolHorizontalScrollBar, 0xf0);
