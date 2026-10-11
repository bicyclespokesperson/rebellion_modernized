#pragma once
#include <string>
#include <cstdint>

#include "core/containers.h"
#include "core/game_string.h"
#include "core/utility_classes.h"
#include "platform/win32.h"

class DibBitmap;

// Bits of CoolListItem::m_state.
enum CoolListItemState : uint32_t {
    kListItemSelected = 0x01,  // drawn with m_selectedImage
    kListItemHovered = 0x02,   // set by the drag list's hover tracking
    kListItemDisabled = 0x04,  // always drawn with m_image
};

// One row of a Cool list control (CoolDragList, CoolSelectionBox): an id, a text, an image for the normal and one
// for the selected state (RT_BITMAP resources, or bitmaps handed over by the caller), the row rectangle and an
// application value. Game code derives its own rows from it (e.g. 0x442130).
class CoolListItem : public LinkNode {
public:
    explicit CoolListItem(uint32_t id);            // 0x60ba00
    CoolListItem(const CoolListItem& other);       // 0x60bac0: deep copy (images are duplicated)
    ~CoolListItem() override;                       // 0x60bc60 (scalar deleting destructor 0x60baa0)
    virtual CoolListItem* Next();                  // slot 3, 0x60beb0: m_next (linker-folded getter)
    virtual CoolListItem* Previous();              // slot 4, 0x60bec0: m_prev (linker-folded getter)

    DibBitmap* CurrentImage();                     // 0x60bd00: the image for the current state
    // Loads RT_BITMAP `imageId` / `selectedImageId` from `module` when they differ from the current ids;
    // id 0 means no image.
    void SetImageIds(uint16_t imageId, uint16_t selectedImageId, HMODULE module);  // 0x60bd20
    // Takes ownership of the non-null bitmaps, replacing the current ones.
    void SetImages(DibBitmap* image, DibBitmap* selectedImage);                   // 0x60be60

    uint32_t m_id;               // +0x0c
    uint16_t m_imageId;          // +0x10
    uint16_t m_selectedImageId;  // +0x12
    std::string m_text;               // +0x14
    DibBitmap* m_image;          // +0x20 owned
    DibBitmap* m_selectedImage;  // +0x24 owned
    int m_x;                     // +0x28 position in the list's canvas
    int m_y;                     // +0x2c
    int m_textOffsetX;           // +0x30 added to the list's own text offset
    int m_textOffsetY;           // +0x34
    int m_fontSlot;              // +0x38 0: the list's font
    PersistentInt m_state;       // +0x3c CoolListItemState
    RECT m_rect;                 // +0x40 list-space item bounds, initially empty
    UINT m_textFormat;           // +0x50 DrawText format, 0: the list's
    // +0x54 also a sort key of the drag list. A row that owns an object keeps the pointer in the same slot; the pointer
    // view is pointer-sized, the value view stays 32 bits wide.
    union {
        uint32_t m_userData;
        uintptr_t m_userPointer;
    };
    int m_ownColors;             // +0x58 non-zero: draw with the colours below
    COLORREF m_selectedColor;    // +0x5c white
    COLORREF m_color;            // +0x60 white
    COLORREF m_hoverColor;       // +0x64 white
};
