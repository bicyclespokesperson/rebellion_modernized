#pragma once
// Classes the drag list (0x607ea0-0x60a910) uses from other ranges. CoolScrollBar, CoolListItem and
// CoolDragImage have their real headers; the two concrete scroll bars (misc-controls range, ui/cool_scroll_bars.h)
// are declared minimally here until that header is merged.
#include <cstdint>

#include "platform/win32.h"
#include "ui/cool_drag_image.h"
#include "ui/cool_list_item.h"
#include "ui/cool_scroll_bar.h"

class CoolScrollable;

class CoolVerticalScrollBar : public CoolScrollBar {
public:
    CoolVerticalScrollBar(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                          CoolScrollable* client, uint16_t resourceSlot, uint16_t resourceId, int total, int first,
                          int last, uint32_t unknownEc);  // 0x60f640
};

class CoolHorizontalScrollBar : public CoolScrollBar {
public:
    CoolHorizontalScrollBar(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                            CoolScrollable* client, uint16_t resourceSlot, uint16_t resourceId, int total, int first,
                            int last, uint32_t unknownEc);  // 0x6100d0
};
