#pragma once
#include <cstdint>
#include <string>

#include "core/containers.h"
#include "core/game_string.h"
#include "core/utility_classes.h"
#include "platform/win32.h"
#include "ui/cool_drag_image.h"
#include "ui/cool_list_item.h"
#include "ui/cool_scroll_bar.h"
#include "ui/cool_scrollable.h"
#include "ui/cool_window.h"

class DibBitmap;

// ---- CoolListItemList -----------------------------------------------------------------------------------------

// Bits of CoolListItemList::m_sortMode (the first set bit wins).
enum CoolListSortMode : uint32_t
{
  kSortById = 0x01,
  kSortByLabel = 0x02, // case-insensitive
  kSortByUserData = 0x08,
};

// The items of a drag list, kept sorted. Also used on its own by many game windows.
class CoolListItemList : public SortedLinkList
{
public:
  explicit CoolListItemList(int sortMode); // 0x60a790
  ~CoolListItemList() override; // 0x60a810 (scalar deleting destructor 0x60a7f0)
  int IsLess(LinkNode* a, LinkNode* b) override; // 0x60a890
  virtual CoolListItem* First(); // slot 2, 0x60beb0 (linker-folded with CoolListItem::Next)

  CoolListItem* FindById(uint32_t id); // 0x60a860

  PersistentInt m_sortMode; // +0x0c CoolListSortMode
};

// ---- CoolDragList ----------------------------------------------------------------------------------------------

// Bits of CoolDragList::m_options (low bits are state, high bits are set by the owner).
enum CoolDragListOptions : uint32_t
{
  kDragListDragging = 0x01, // the selection is being dragged
  kDragListAnchorSet = 0x02, // m_anchor holds the last clicked point (shift-click range start)
  kDragListItemsNotOwned = 0x08, // m_items came from SetItemList and is not deleted
  kDragListClickedSelected = 0x10, // the button went down on an already selected item
  kDragListSingleSelect = 0x10000, // modifier keys are ignored
  kDragListNoDrag = 0x20000,
  kDragListHoverTracking = 0x40000,
  kDragListFlowLayout = 0x80000, // items wrap into rows/columns
  kDragListDropInside = 0x100000, // a drop inside the list is still reported
  kDragListForwardMouse = 0x200000, // clicks and keys go to the parent
};

// "CoolDragList": a scrolling list of bitmap items with labels, single/multi selection (ctrl/shift), keyboard
// navigation, type-ahead (SelectByPrefix) and dragging of the selection (a CoolDragImage follows the mouse).
// It notifies its parent with posted WM_COMMAND messages whose high word is a notification code.
class CoolDragList : public CoolWindow, public CoolScrollable
{
public:
  CoolDragList(HINSTANCE instance,
               int x,
               int y,
               int width,
               int height,
               CoolWindow* parent,
               DibBitmap* backdrop,
               uint32_t childId,
               int itemWidth,
               int itemHeight,
               int unused2c,
               int unused30,
               int unused34,
               int unused38,
               int lineStep,
               int vertical,
               HCURSOR dragCursor); // 0x607ea0
  ~CoolDragList() override; // 0x608160 (scalar deleting destructor 0x608140, thunk 0x60a410)

  LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override; // 0x6083c0
  int OnRightButtonUp(int x, int y, CoolWindow* sender) override; // 0x609fc0: forwards to the parent

  int PageUp() override; // 0x60a010
  int PageDown() override; // 0x60a060
  int LineUp() override; // 0x60a0b0
  int LineDown() override; // 0x60a100
  int ScrollVertical(int delta) override; // 0x60a150
  int PageLeft() override; // 0x60a1c0
  int PageRight() override; // 0x60a210
  int LineRight() override; // 0x60a270
  int LineLeft() override; // 0x60a260
  int ScrollHorizontal(int delta) override; // 0x58f0d0 (linker-folded)
  void OnScrollEnd() override; // 0x619730 (linker-folded): does nothing

  void SetItemList(CoolListItemList* items); // 0x608280
  void SetItemSize(int width, int height); // 0x6082c0
  void SetBackground(DibBitmap* source, int srcX, int srcY); // 0x608300
  void SetDragTarget(HWND target); // 0x609290
  void SetSortMode(int sortMode); // 0x609340
  void CopySelectionTo(CoolListItemList* out); // 0x609410
  void ClearSelection(); // 0x6094b0
  void SelectById(uint32_t id, int notify); // 0x609500
  void SelectRect(POINTS from, POINTS to); // 0x609590
  uint32_t SelectByPrefix(const std::string* prefix, int notify, int caseSensitive); // 0x609650
  CoolListItem* AddItem(uint32_t id,
                        uint16_t imageId,
                        uint16_t selectedImageId,
                        const char* text,
                        int redraw,
                        uintptr_t userData); // 0x609790
  CoolListItem* AddItemWithImages(uint32_t id,
                                  DibBitmap* image,
                                  DibBitmap* selectedImage,
                                  const char* text,
                                  int redraw,
                                  uintptr_t userData); // 0x609840
  CoolListItem* FindItem(uint32_t id); // 0x6098f0
  void RemoveAll(); // 0x609920
  void SetSelectedTextColor(COLORREF color); // 0x609940
  void SetTextColor(COLORREF color); // 0x609950
  void DrawItem(CoolListItem* item); // 0x609960
  void Redraw(); // 0x609a00
  void Layout(); // 0x609ae0
  CoolListItem* HitTest(POINTS point); // 0x609d10: point in canvas coordinates
  POINTS ClientToList(POINTS point); // 0x609d80
  POINTS ListToClient(POINTS point); // 0x609dd0
  void UpdateItemRects(); // 0x609e20
  CoolListItem* FirstItem(); // 0x609eb0
  void ScrollIntoView(uint32_t id); // 0x609ed0
  void Refresh(); // 0x60a280
  RECT GetItemTextRect(CoolListItem* item); // 0x60a2a0

  CoolListItemList* m_items; // +0xa0 owned unless kDragListItemsNotOwned
  DibBitmap* m_canvas; // +0xa4 all items drawn at their list positions
  DibBitmap* m_background; // +0xa8 cut from the parent's picture
  DibBitmap* m_composite; // +0xac paint buffer
  HCURSOR m_cursor; // +0xb0
  HCURSOR m_dragCursor; // +0xb4 may be null
  int m_unknownB8; // +0xb8 item height for horizontal lists
  int m_unknownBC; // +0xbc item width for vertical lists
  int m_lineStep; // +0xc0 LineUp/LineDown distance (default 10)
  int m_scrollPos; // +0xc4
  int m_scrollMax; // +0xc8
  int m_itemWidth; // +0xcc
  int m_itemHeight; // +0xd0
  uint8_t m_fillColor; // +0xd4 transparent colour of the canvas (the items' colour key)
  uint8_t m_padD5[3]; // +0xd5
  COLORREF m_textColor; // +0xd8
  COLORREF m_selectedTextColor; // +0xdc
  COLORREF m_hoverTextColor; // +0xe0
  UINT m_textFormat; // +0xe4 DrawText format
  int m_textOffsetX; // +0xe8
  int m_textOffsetY; // +0xec
  int m_vertical; // +0xf0 non-zero: items are stacked and scrolled vertically
  PersistentInt m_options; // +0xf4 CoolDragListOptions
  POINTS m_anchor; // +0xf8 canvas coordinates
  POINTS m_dragStart; // +0xfc client coordinates; (-1000, -1000) when not dragging
  CoolListItem* m_hoverItem; // +0x100
  CoolDragImage* m_dragImage; // +0x104 owned
  uint32_t m_unknown108; // +0x108
  uint32_t m_unknown10c; // +0x10c
  DibBitmap* m_selectionImage; // +0x110 the selected items, the picture that is dragged
  HWND m_dragWindow; // +0x114 window the drag image is drawn over
};

// Notification codes (high word of the WM_COMMAND wParam posted to the parent; the low word is the child id).
constexpr uint16_t kDragListDropped = 0x29a; // lParam: GetMessagePos() of the drop
constexpr uint16_t kDragListSelectionChanged = 0x29b;
constexpr uint16_t kDragListReselected = 0x29d; // a click on the item that was already the selection
constexpr uint16_t kDragListActivated = 0x309; // double click or Return

extern const char* g_coolDragListClassName; // 0x66e0b8 "CoolDragList"
