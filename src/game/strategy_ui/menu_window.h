#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "platform/win32.h"
#include "ui/cool_game_window.h"

class CoolLabel;
class CoolWindow;
class DibBitmap;
struct ResourceStringId;

// The menu windows of the strategic game (the object pop-up menu and the Galactic Information Display menu): one
// MenuWindowBase per menu level, painted from a list of MenuItems.

// IdSortedLinkList whose destructor deletes the nodes (VT 0x65c6a8).
class OwningIdList : public IdSortedLinkList {
public:
    OwningIdList();           // 0x4c7f50
    virtual ~OwningIdList();  // 0x4c7f90 (scalar deleting destructor 0x4c7f70)
};

// The entries of one menu level, by command id (VT 0x65bee0).
class MenuItemList : public OwningIdList {
public:
    MenuItemList();           // 0x4aa910
    virtual ~MenuItemList();  // 0x4aa950 (scalar deleting destructor 0x4aa930)
};

class MenuWindowBase;

// A command and its state bits: only a command whose bits 0x1, 0x4 and 0x8 are clear is available.
struct MenuCommand {
    uint32_t m_command;  // +0x00
    uint32_t m_flags;    // +0x04
    uint32_t GetAvailable() const;  // 0x4ab550: m_command, or 0
};

// One entry of a menu (0x78 bytes, VT 0x65bf50): up to four pictures (normal, highlighted, disabled and a check
// mark), a text label, the rectangle it occupies in the menu window and the colours of its text. The pictures are
// loaded from RT_BITMAP `module` by id.
class MenuItemBase : public IdLinkNode {
public:
    // `text` (a string resource, may be null), `x`/`y` the position of the pictures and text, `width`/`height` the
    // rectangle (used when the item has no window); `pictureId` is the normal picture and, for a disabled item or
    // without `hotPictureId`, also the disabled one (`disabledPictureId` 0: the same id); `commandId` is the key of
    // the item in its list, `owner` the window whose DC measures the text.
    MenuItemBase(HMODULE module, const ResourceStringId* text, int x, int y, int width, int height, uint32_t pictureId,
                 uint32_t hotPictureId, uint32_t disabledPictureId, uint32_t checkPictureId, uint32_t commandId,
                 CoolWindow* owner, int enabled, int checked);  // 0x4ab560
    virtual ~MenuItemBase();  // 0x4ab760 (scalar deleting destructor 0x4ab740)
    virtual int Virtual03();  // slot 3, 0x5a5b60 (shared): 0
    virtual int Virtual04();  // slot 4, 0x5a5b60 (shared): 0

    // Lays the item out: `text` (deleted here) and the pictures start at (x, y).
    void Initialize(uint32_t commandId, int x, int y, int width, int height, DibBitmap* normal, DibBitmap* hot,
                    DibBitmap* disabled, DibBitmap* check, String* text);  // 0x4ab840
    void AttachSubMenu(MenuWindowBase* subMenu);                           // 0x4aba40
    void Draw(HDC dc, int highlighted);                                    // 0x4aba60
    int GetWidth();                                                        // 0x4abb80
    int GetHeight();                                                       // 0x4abbf0
    // Places the item at row `y` of a menu `width` x `height`; returns the y of the next row.
    int Layout(int y, int width, int height);                              // 0x4abc70
    void TileNormalPicture();                                              // 0x4abd30
    void SetColors(COLORREF color, COLORREF shadowColor, COLORREF disabledColor);  // 0x4abe10
    void BuildCheckOverlay(DibBitmap* menuBackground);                     // 0x4abe30
    DibBitmap* ComposeOverlay(DibBitmap* picture, DibBitmap* menuBackground);  // 0x4abe70
    int GetPictureWidth();                                                 // 0x4abf60
    void SetIndent(int indent);                                            // 0x4abfa0

    DibBitmap* m_hotPicture;       // +0x10 highlighted
    DibBitmap* m_normalPicture;    // +0x14
    DibBitmap* m_disabledPicture;  // +0x18
    DibBitmap* m_checkOverlay;     // +0x1c the check picture over the menu's background
    DibBitmap* m_checkPicture;     // +0x20
    DibBitmap* m_backdrop;         // +0x24 the menu's paint background
    uint32_t m_unknown28;          // +0x28
    COLORREF m_hotColor;           // +0x2c
    COLORREF m_normalColor;        // +0x30
    COLORREF m_disabledColor;      // +0x34
    CoolLabel* m_label;            // +0x38 owned
    MenuWindowBase* m_subMenu;     // +0x3c the sub-menu window this item opens
    CoolWindow* m_owner;           // +0x40
    int m_pictureX;                // +0x44
    int m_pictureY;                // +0x48
    int m_subMenuX;                // +0x4c
    int m_subMenuY;                // +0x50
    BOOL m_hit;                    // +0x54 the mouse is over it
    int m_enabled;                 // +0x58
    int m_checked;                 // +0x5c
    RECT m_area;                   // +0x60
    int m_unknown70;               // +0x70
    int m_indent;                  // +0x74 width of the pictures column
};

// A menu entry as the object pop-up menu builds them (0x90 bytes, VT 0x65bf68): the command it sends and the entries
// of its sub-menu.
class MenuItem : public MenuItemBase {
public:
    MenuItem(uint32_t command, uint32_t flags, HMODULE module, const ResourceStringId* text, int x, int y, int width,
             int height, uint32_t pictureId, uint32_t hotPictureId, uint32_t disabledPictureId,
             uint32_t checkPictureId, uint32_t entryId, CoolWindow* owner, int enabled, int checked);  // 0x4abfb0
    virtual ~MenuItem();       // 0x4ac090 (scalar deleting destructor 0x4ac070)
    virtual int Virtual03() override;  // slot 3, 0x4ac100: flags & 8 (the entry opens a sub-menu)
    virtual int Virtual04() override;  // slot 4, 0x4ac110: flags & 4 (a separator follows)

    MenuCommand m_command;  // +0x78
    MenuItemList m_subItems;  // +0x80
};

// The window of one menu level (0x4aa960-0x4ac000): paints its items over a darkened copy of `backdrop` and highlights
// the one under the mouse; clicking an item sends its command to the parent window.
class MenuWindowBase : public CoolGameWindow {
public:
    MenuWindowBase(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent, uint32_t entryId,
                   DibBitmap* backdrop);  // 0x4aa960
    virtual ~MenuWindowBase();            // 0x4aaa50 (scalar deleting destructor 0x4aaa30)
    virtual LRESULT __stdcall WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;  // slot 5, 0x4aab50
    virtual void OnCreate1() override;  // slot 14, 0x4ab2b0
    // A mouse message went to the menu item `item` (null: to the menu itself) at (x, y); nothing here.
    virtual int HandleMouse(MenuItem* item, LONG x, LONG y, int message);  // slot 22, 0x4ab540

    // Adds a clickable text entry (item key `commandId`), with its pictures and its text from a string resource.
    void AddTextEntry(HMODULE module, const ResourceStringId* text, uint32_t commandId, int x, int y, int width,
                      int height, uint32_t pictureId, uint32_t hotPictureId, uint32_t disabledPictureId,
                      uint32_t checkPictureId);  // 0x4ab1f0
    // Gives every entry the text colours (disabled: the shadow colour when 0).
    void SetEntryColors(COLORREF color, COLORREF shadowColor, COLORREF disabledColor);  // 0x4ab1b0
    // Appends the item (it takes the menu's background as backdrop).
    void AddItem(MenuItemBase* item);  // 0x4ab370
    // Opens `sub` beside the item `itemId` of this menu, with the two arrow pictures (RT_BITMAP of `resources`).
    void AttachSubMenu(HMODULE resources, uint32_t itemId, MenuWindowBase* sub, uint32_t arrowId, uint32_t pressedArrowId);  // 0x4ab3a0

    uint32_t m_unknown114;          // +0x114
    MenuItemBase* m_hotItem;        // +0x118 the item under the mouse
    DibBitmap* m_arrow;             // +0x11c the sub-menu arrow (highlighted item)
    DibBitmap* m_dimArrow;          // +0x120 the arrow of the other items
    uint8_t* m_remapTable;          // +0x124 heap block: the colour darkening of the background
    HDC m_dc;                       // +0x128
    DibBitmap* m_backdrop;          // +0x12c not owned
    uint32_t m_unknown130;          // +0x130
    UINT_PTR m_timer;               // +0x134 the timer that opens sub-menus
    OwningIdList m_items;           // +0x138
    CoolWindow* m_parentMenu;       // +0x148 the menu this one is the sub-menu of
};

// The transformation of a menu background colour (0x4ac550).
uint32_t LAB_004ac550(uint32_t color);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(OwningIdList, 0x10);
REB_ASSERT_SIZE(MenuItemList, 0x10);
REB_ASSERT_SIZE(MenuCommand, 0x08);
REB_ASSERT_SIZE(MenuItemBase, 0x78);
REB_ASSERT_OFFSET(MenuItemBase, m_hotPicture, 0x10);
REB_ASSERT_OFFSET(MenuItemBase, m_backdrop, 0x24);
REB_ASSERT_OFFSET(MenuItemBase, m_hotColor, 0x2c);
REB_ASSERT_OFFSET(MenuItemBase, m_label, 0x38);
REB_ASSERT_OFFSET(MenuItemBase, m_hit, 0x54);
REB_ASSERT_OFFSET(MenuItemBase, m_area, 0x60);
REB_ASSERT_OFFSET(MenuItemBase, m_indent, 0x74);
REB_ASSERT_SIZE(MenuItem, 0x90);
REB_ASSERT_OFFSET(MenuItem, m_command, 0x78);
REB_ASSERT_OFFSET(MenuItem, m_subItems, 0x80);
REB_ASSERT_SIZE(MenuWindowBase, 0x14c);
REB_ASSERT_OFFSET(MenuWindowBase, m_hotItem, 0x118);
REB_ASSERT_OFFSET(MenuWindowBase, m_arrow, 0x11c);
REB_ASSERT_OFFSET(MenuWindowBase, m_remapTable, 0x124);
REB_ASSERT_OFFSET(MenuWindowBase, m_dc, 0x128);
REB_ASSERT_OFFSET(MenuWindowBase, m_backdrop, 0x12c);
REB_ASSERT_OFFSET(MenuWindowBase, m_timer, 0x134);
REB_ASSERT_OFFSET(MenuWindowBase, m_items, 0x138);
REB_ASSERT_OFFSET(MenuWindowBase, m_parentMenu, 0x148);
#pragma clang diagnostic pop
