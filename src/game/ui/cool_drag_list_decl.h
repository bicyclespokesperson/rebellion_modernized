#pragma once
// Minimal declarations of classes owned by other ranges, for the drag list (0x607ea0-0x60a910) only. They are
// reconstructed in ui/cool_scroll_bar.h, ui/cool_scroll_bars.h (CoolVerticalScrollBar / CoolHorizontalScrollBar), ui/cool_list_item.h and ui/cool_drag_image.h;
// drop this header when those are merged.
#include <cstdint>

#include "core/containers.h"
#include "core/game_string.h"
#include "core/layout.h"
#include "core/utility_classes.h"
#include "platform/win32.h"
#include "ui/cool_window.h"

class CoolScrollable;
class DibBitmap;

class CoolScrollBar : public CoolWindow {
public:
    void SetBackground(DibBitmap* parentImage);  // 0x60b2c0
    int Refresh();                               // 0x60b4b0
    int SetRange(int total);                     // 0x60b590
    int SetVisibleRange(int first, int last);    // 0x60b5d0
    int ScrollBy(int delta);                     // 0x60b660

    uint8_t m_unknown94[0xdc - 0x94];  // +0x94
    int m_total;                       // +0xdc
    int m_first;                       // +0xe0
    int m_last;                        // +0xe4
    uint8_t m_unknownE8[0xf0 - 0xe8];  // +0xe8
};

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

// Bits of CoolListItem::m_state.
enum CoolListItemState : uint32_t {
    kListItemSelected = 0x01,
    kListItemHovered = 0x02,  // set by CoolDragList while the mouse is over the item (kDragListHoverTracking)
    kListItemDisabled = 0x04,
};

// The fields the owner header leaves unknown are named here from the drag list's use of them.
class CoolListItem : public LinkNode {
public:
    explicit CoolListItem(uint32_t id);       // 0x60ba00
    CoolListItem(const CoolListItem& other);  // 0x60bac0
    virtual CoolListItem* Next();             // slot 3, 0x60beb0
    virtual CoolListItem* Previous();         // slot 4, 0x60bec0

    DibBitmap* CurrentImage();                                                     // 0x60bd00
    void SetImageIds(uint16_t imageId, uint16_t selectedImageId, HMODULE module);  // 0x60bd20
    void SetImages(DibBitmap* image, DibBitmap* selectedImage);                    // 0x60be60

    uint32_t m_id;               // +0x0c
    uint16_t m_imageId;          // +0x10
    uint16_t m_selectedImageId;  // +0x12
    String m_text;               // +0x14
    DibBitmap* m_image;          // +0x20
    DibBitmap* m_selectedImage;  // +0x24
    int m_x;                     // +0x28 position in the drag list's canvas
    int m_y;                     // +0x2c
    int m_textOffsetX;           // +0x30 added to the list's text offset
    int m_textOffsetY;           // +0x34
    int m_fontSlot;              // +0x38 0: the list's font
    PersistentInt m_state;       // +0x3c CoolListItemState
    RECT m_rect;                 // +0x40 client rectangle
    UINT m_textFormat;           // +0x50 0: the list's DrawText format
    uint32_t m_userData;         // +0x54 also the sort key of kSortByUserData
    int m_ownColors;             // +0x58 non-zero: the three colours below replace the list's
    COLORREF m_selectedColor;    // +0x5c
    COLORREF m_color;            // +0x60
    COLORREF m_hoverColor;       // +0x64
};

class CoolDragImage {
public:
    explicit CoolDragImage(CoolWindow* window);  // 0x60db00
    ~CoolDragImage();                            // 0x60db40

    int SetImage(DibBitmap* source);   // 0x60dbb0
    int ClearImage();                  // 0x60dc80
    int SetHotspot(int x, int y);      // 0x60dcb0
    int BeginDrag(POINT cursor);       // 0x60dce0
    int EndDrag();                     // 0x60de50
    int MoveTo(const POINT* cursor);   // 0x60df00

    uint8_t m_unknown00[0x28];  // +0x00
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(CoolScrollBar, 0xf0);
REB_ASSERT_OFFSET(CoolScrollBar, m_total, 0xdc);
REB_ASSERT_OFFSET(CoolScrollBar, m_first, 0xe0);
REB_ASSERT_SIZE(CoolListItem, 0x68);
REB_ASSERT_OFFSET(CoolListItem, m_text, 0x14);
REB_ASSERT_OFFSET(CoolListItem, m_x, 0x28);
REB_ASSERT_OFFSET(CoolListItem, m_state, 0x3c);
REB_ASSERT_OFFSET(CoolListItem, m_rect, 0x40);
REB_ASSERT_OFFSET(CoolListItem, m_hoverColor, 0x64);
REB_ASSERT_SIZE(CoolDragImage, 0x28);
#pragma clang diagnostic pop
