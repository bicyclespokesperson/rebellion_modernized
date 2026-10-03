#pragma once
// Minimal declarations of the drag-list module's CoolListItemList and CoolDragList (ui/cool_drag_list.h on the
// drag-list branch), as far as CoolSelectionBox uses them, with that module's names. Drop this header at merge.
#include <cstdint>

#include "core/containers.h"
#include "core/game_string.h"
#include "core/layout.h"
#include "core/utility_classes.h"
#include "platform/win32.h"
#include "ui/cool_button_decl.h"
#include "ui/cool_list_item.h"
#include "ui/cool_scrollable.h"
#include "ui/cool_window.h"

class DibBitmap;

// The item list of a CoolDragList (vtable 0x66e148).
class CoolListItemList : public SortedLinkList {
public:
    explicit CoolListItemList(int sortMode);  // 0x60a790
    virtual ~CoolListItemList();              // 0x60a810
    virtual int IsLess(LinkNode* a, LinkNode* b) override;  // 0x60a890
    virtual CoolListItem* First();            // slot 2, 0x60beb0

    PersistentInt m_sortMode;  // +0x0c
};

// Bits of CoolDragList::m_options.
enum CoolDragListOptions : int {
    kDragListSingleSelect = 0x10000,
    kDragListNoDrag = 0x20000,
    kDragListHoverTracking = 0x40000,
};

// A list of CoolListItems in its own window, with a scroll bar.
class CoolDragList : public CoolWindow, public CoolScrollable {
public:
    CoolDragList(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent, DibBitmap* backdrop,
                 uint32_t childId, int itemWidth, int itemHeight, int unused1, int unused2, int unused3, int unused4,
                 int lineStep, int vertical, HCURSOR dragCursor);  // 0x607ea0
    virtual ~CoolDragList();

    void SetSortMode(int sortMode);                 // 0x609340
    void SetSelectedTextColor(COLORREF color);      // 0x609940
    void SetTextColor(COLORREF color);              // 0x609950
    void SetItemList(CoolListItemList* items);      // 0x608280
    void Refresh();                                 // 0x60a280
    void ScrollIntoView(uint32_t id);               // 0x609ed0
    CoolListItem* FirstItem();                      // 0x609eb0
    void SelectById(uint32_t id, int notify);       // 0x609500

    void* m_scrollBars[2];      // +0x98 CoolScrollable's vertical/horizontal bars (not in its stand-in)
    CoolListItemList* m_items;  // +0xa0
    DibBitmap* m_canvas;        // +0xa4
    DibBitmap* m_background;    // +0xa8
    uint8_t m_unknownAc[0x28];  // +0xac
    uint8_t m_fillColor;        // +0xd4
    uint8_t m_unknownD5[0x0b];  // +0xd5
    COLORREF m_hoverTextColor;  // +0xe0
    UINT m_textFormat;          // +0xe4
    int m_textOffsetX;          // +0xe8
    int m_textOffsetY;          // +0xec
    uint32_t m_unknownF0;       // +0xf0
    PersistentInt m_options;    // +0xf4 CoolDragListOptions
    uint8_t m_unknownF8[0x20];  // +0xf8
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(CoolListItemList, 0x10);
REB_ASSERT_SIZE(CoolDragList, 0x118);
REB_ASSERT_OFFSET(CoolDragList, m_items, 0xa0);
REB_ASSERT_OFFSET(CoolDragList, m_background, 0xa8);
REB_ASSERT_OFFSET(CoolDragList, m_fillColor, 0xd4);
REB_ASSERT_OFFSET(CoolDragList, m_hoverTextColor, 0xe0);
REB_ASSERT_OFFSET(CoolDragList, m_options, 0xf4);
#pragma clang diagnostic pop
