#pragma once
#include <cstdint>

#include "core/entry_identity.h"
#include "platform/win32.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_window.h"
#include "ui/game_window_object.h"

class CoolSelectionDropDown;
class DibBitmap;

// Colour transform of the selection box's drop-down: every palette entry becomes light grey (keeps the flags byte).
uint32_t LightGreyColor(uint32_t color); // 0x5fef30

// A combo box ("CoolSelectionBoxClass"): shows the current item (its picture and text) over a backdrop cut from the
// parent's background, with a bitmap button at the right edge. The button, Up/Down or a WM_COMMAND 1000 opens a
// CoolSelectionDropDown below (or above) the box listing the items in a CoolDragList.
class CoolSelectionBox : public CoolWindow
{
  public:
  // The nodes of `items` are copied; `backdrop` is the picture the box is drawn over (not owned; its pixels at the
  // box's position become the box's background).
  CoolSelectionBox(HINSTANCE instance, CoolWindow* parent, int x, int y, int width, int height, int visibleRows,
                   CoolListItemList* items, DibBitmap* backdrop, uint16_t buttonBitmapId,
                   uint16_t buttonPressedBitmapId, uint16_t scrollResourceSlot, uint16_t scrollResourceId,
                   int sortMode); // 0x60bed0
  ~CoolSelectionBox() override;   // 0x60c120 (scalar deleting destructor 0x60c100)

  // Slot 5. WM_CREATE adds the button (child 1000); WM_PAINT draws the current item; key presses go to the
  // parent except key releases the box uses (HandleKeyUp); WM_COMMAND 1000/0x3ea opens and 0x3e9 closes the
  // drop-down; 0x405 from the drop-down closes it unless the window that took the capture is one of ours.
  LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override; // 0x60c210

  void PressButton(); // 0x60c1f0: posts the button's WM_COMMAND (opens the drop-down)
  // Return/Escape close an open drop-down, Up/Down open it or move the selection. 1 if the key was used.
  int HandleKeyUp(WPARAM key, LPARAM keyData); // 0x60c810
  CoolListItem* GetSelection();                // 0x60c960
  // Makes the item with id `id` current (notifying the parent with WM_COMMAND 0x29b), closes the drop-down and
  // returns the current item.
  CoolListItem* Select(uint32_t id);      // 0x60c970
  void SetItems(CoolListItemList* items); // 0x60ca00: replaces the items with copies; the first becomes current
  void OpenDropDown();                    // 0x60cac0
  void CloseDropDown();                   // 0x60cbf0

  COLORREF m_textColor;                  // +0x94 white
  UINT m_textFormat;                     // +0x98 DrawText format, DT_SINGLELINE | DT_VCENTER
  int m_textOffsetX;                     // +0x9c added to the item's own text offset
  int m_textOffsetY;                     // +0xa0
  int m_rowHeight;                       // +0xa4 the box's height
  int m_visibleRows;                     // +0xa8 drop-down height in rows
  uint8_t m_listFillColor;               // +0xac the drop-down list's fill colour
  uint8_t m_padAd[3];                    // +0xad
  HPEN m_borderPen;                      // +0xb0 white pen of the drop-down's frame; owned
  uint32_t (*m_dropDownShade)(uint32_t); // +0xb4 remaps the drop-down's background (null: tile m_dropDownTile)
  uint16_t m_buttonBitmapId;             // +0xb8 0: no button
  uint16_t m_buttonPressedBitmapId;      // +0xba
  uint16_t m_scrollResourceSlot;         // +0xbc artwork of the drop-down's scroll bar
  uint16_t m_scrollResourceId;           // +0xbe
  int m_screenLeft;                      // +0xc0 area the drop-down must stay in (screen coordinates)
  int m_screenTop;                       // +0xc4
  int m_screenRight;                     // +0xc8 GetSystemMetrics(SM_CXFULLSCREEN)
  int m_screenBottom;                    // +0xcc GetSystemMetrics(SM_CYFULLSCREEN)
  CoolListItemList m_items;              // +0xd0
  CoolListItem* m_current;               // +0xe0 current item of m_items, may be null
  int m_sortMode;                        // +0xe4 order of m_items and of the drop-down's list
  CoolSelectionDropDown* m_dropDown;     // +0xe8 open drop-down (child 0x3e9), or null
  DibBitmap* m_backdrop;                 // +0xec not owned
  DibBitmap* m_image;                    // +0xf0 the box's picture (DIB section); owned
  DibBitmap* m_dropDownTile;             // +0xf4 tiled as the drop-down's background; owned, set by the owner
};

// The drop-down of a CoolSelectionBox: a topmost popup (re-parented to the application window) holding a
// CoolDragList of the box's items. It keeps the mouse captured and forwards mouse messages to the child under
// the cursor; a click outside closes it.
class CoolSelectionDropDown : public GameWindowObject
{
  public:
  CoolSelectionDropDown(HINSTANCE instance, CoolSelectionBox* owner, int x, int y, int width, int height,
                        HPEN borderPen); // 0x60cc50
  ~CoolSelectionDropDown() override;     // 0x60ccf0 (scalar deleting destructor 0x60ccd0)

  LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override; // 0x60d020

  void SyncSelection(); // 0x60cd50: selects and shows the owner's current item in the list
  // Makes the background: the screen under the window, shaded or tiled, with a frame.
  void BuildBackground(); // 0x60cd90

  CoolDragList* m_list;          // +0x9c child 0xbba; owned
  CoolSelectionBox* m_owner;     // +0xa0
  DibBitmap* m_image;            // +0xa4 handed to SetBackground (which deletes it)
  uint32_t (*m_shade)(uint32_t); // +0xa8 the owner's m_dropDownShade
  HGDIOBJ m_pen;                 // +0xac frame pen; after BuildBackground the pen it replaced
  int m_sortMode;                // +0xb0 set by the owner
};

extern int g_selectionBoxClosing; // 0x6be5c8 guards CloseDropDown against re-entry
