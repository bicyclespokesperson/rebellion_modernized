#pragma once
#include <cstdint>

#include "platform/win32.h"
#include "ui/cool_window.h"
#include "ui/cool_drag_image.h"
#include "ui/cool_list_item.h"
#include "ui/cool_scroll_bar.h"

class DibBitmap;

// ---- CoolScrollable ------------------------------------------------------------------------------------------

// Secondary base of windows that own scroll bars (CoolDragList at +0x94, the game window 0x41ecf0). The bars call
// the virtual scroll requests back; each returns non-zero (LineUp/LineDown: the distance) when it moved.
class CoolScrollable {
public:
    CoolScrollable();           // 0x60a420
    virtual ~CoolScrollable();  // 0x60a460 (scalar deleting destructor 0x60a440): deletes the bars

    // The base implementations do nothing and return 0 (linker-folded with other such functions).
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
    virtual void OnScrollEnd();                // slot 11: the mouse button was released over a bar

    // Create a bar (child of `parent`, owned by this object), then refresh both bars. Returns the refresh result.
    int CreateVerticalScrollBar(HINSTANCE instance, CoolWindow* parent, uint16_t resourceSlot, uint16_t resourceId,
                                int total, int first, int last, uint32_t unknownEc, DibBitmap* background);  // 0x60a490
    int CreateHorizontalScrollBar(HINSTANCE instance, CoolWindow* parent, uint16_t resourceSlot,
                                  uint16_t resourceId, int total, int first, int last, uint32_t unknownEc,
                                  DibBitmap* background);  // 0x60a560

    int ShowVerticalScrollBar();                              // 0x60a630
    int HideVerticalScrollBar();                              // 0x60a650
    int SetVerticalRange(int total);                           // 0x60a670
    int SetVerticalVisibleRange(int start, int end);           // 0x60a690
    int ScrollVerticalBar(int delta);                          // 0x60a6c0
    int GetVerticalVisibleRange(int* start, int* end);         // 0x60a6f0: 0 unless both are non-zero
    int SetHorizontalRange(int total);                         // 0x60a730
    int GetHorizontalVisibleRange(int* start, int* end);       // 0x60a750

    CoolScrollBar* m_verticalBar;    // +0x04 owned
    CoolScrollBar* m_horizontalBar;  // +0x08 owned
};
