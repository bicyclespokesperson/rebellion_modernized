#pragma once
// Minimal declarations of Cool classes owned by other modules, as far as CoolSelectionBox uses them. Drop this
// header at merge in favour of the owners' headers: CoolListItem (ui/cool_list_item.h), CoolListItemList and
// CoolDragList (ui/cool_drag_list.h), CoolScrollable (ui/cool_scrollable.h), CoolStrobeButton (ui/cool_button.h).
#include <cstdint>

#include "core/containers.h"
#include "core/game_string.h"
#include "core/layout.h"
#include "core/utility_classes.h"
#include "platform/win32.h"
#include "ui/cool_window.h"

class DibBitmap;

enum CoolListItemState : int {
    kListItemSelected = 0x01,
};

// A row of a CoolDragList (vtable 0x66e1b0).
class CoolListItem : public LinkNode {
public:
    CoolListItem(const CoolListItem& other);  // 0x60bac0
    virtual ~CoolListItem();                  // 0x60bc60
    virtual CoolListItem* Next();             // slot 3, 0x60beb0
    virtual CoolListItem* Previous();         // slot 4, 0x60bec0

    DibBitmap* CurrentImage();  // 0x60bd00

    uint32_t m_id;              // +0x0c
    uint16_t m_imageId;         // +0x10
    uint16_t m_selectedImageId; // +0x12
    String m_text;              // +0x14
    DibBitmap* m_image;         // +0x20
    DibBitmap* m_selectedImage; // +0x24
    int m_unknown28;            // +0x28
    int m_unknown2c;            // +0x2c
    int m_textOffsetX;          // +0x30
    int m_textOffsetY;          // +0x34
    int m_unknown38;            // +0x38
    PersistentInt m_state;      // +0x3c CoolListItemState
    uint8_t m_unknown40[0x28];  // +0x40
};

// The item list of a CoolDragList (vtable 0x66e148).
class CoolListItemList : public SortedLinkList {
public:
    explicit CoolListItemList(int sortMode);  // 0x60a790
    virtual ~CoolListItemList();              // 0x60a810
    virtual int IsLess(LinkNode* a, LinkNode* b) override;  // 0x60a890
    virtual CoolListItem* First();            // slot 2, 0x60beb0

    PersistentInt m_sortMode;  // +0x0c
};

// Second base of CoolDragList (at +0x94): owns the scroll bars.
class CoolScrollable {
public:
    virtual ~CoolScrollable();
    void CreateVerticalScrollBar(HINSTANCE instance, CoolWindow* parent, uint16_t resourceSlot, uint16_t resourceId,
                                 int total, int first, int last, uint32_t unknownEc,
                                 DibBitmap* background);  // 0x60a490

    void* m_verticalBar;    // +0x04
    void* m_horizontalBar;  // +0x08
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

// A bitmap push button.
class CoolStrobeButton : public CoolWindow {
public:
    CoolStrobeButton(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent, uint32_t childId,
                     uint32_t normalBitmapId, uint32_t pressedBitmapId, DibBitmap* backdrop, int repeatRate,
                     int pixelHit);  // 0x602150

    uint8_t m_unknown94[0x80];  // +0x94
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(CoolListItem, 0x68);
REB_ASSERT_OFFSET(CoolListItem, m_text, 0x14);
REB_ASSERT_OFFSET(CoolListItem, m_textOffsetX, 0x30);
REB_ASSERT_OFFSET(CoolListItem, m_state, 0x3c);
REB_ASSERT_SIZE(CoolListItemList, 0x10);
REB_ASSERT_SIZE(CoolScrollable, 0x0c);
REB_ASSERT_SIZE(CoolDragList, 0x118);
REB_ASSERT_OFFSET(CoolDragList, m_items, 0xa0);
REB_ASSERT_OFFSET(CoolDragList, m_background, 0xa8);
REB_ASSERT_OFFSET(CoolDragList, m_fillColor, 0xd4);
REB_ASSERT_OFFSET(CoolDragList, m_hoverTextColor, 0xe0);
REB_ASSERT_OFFSET(CoolDragList, m_options, 0xf4);
REB_ASSERT_SIZE(CoolStrobeButton, 0x114);
#pragma clang diagnostic pop
