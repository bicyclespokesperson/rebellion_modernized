#pragma once
#include <cstdint>

#include "gameobj/object_key.h"
#include "platform/win32.h"
#include "ui/custom_dialog_box.h"

class CoolDragList;
class CoolLabel;
class CoolListItemList;
class CoolStringField;
class CoolTabControl;
class GalaxyWindow;
class GameObj;

// The Troop Finder (galaxy view child 0x16, F4): the systems and fleets that hold troop regiments of the side the
// two tabs select, with the number of each kind of regiment. Opening a row shows its first regiment.
class TroopFinderWindow : public CustomDialogBox {
public:
    TroopFinderWindow(HINSTANCE instance, int x, int y, int width, int height, GalaxyWindow* parent,
                      uint32_t childId);  // 0x46cbc0
    virtual ~TroopFinderWindow();  // 0x46cd20 (scalar deleting destructor 0x46cd00)

    virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;  // slot 5, 0x46e5f0
    virtual void OnCreate1() override;                    // slot 14, 0x46ce40
    // Slot 17 is the original's 0x460fe0, shared with SystemFinderWindow (the label is at +0x154 in both).
    virtual void DrawOverlay(HDC dc) override;
    virtual void OnCommand(int a, int command, int c) override;  // slot 18, 0x46df90
    virtual void OnKeyDown(WPARAM key, LPARAM lParam) override;  // slot 19, 0x46efc0

    // Rebuilds the rows for the side tab `filter` (1 Alliance, 2 Empire) unless it is the current one.
    void SetFilter(int filter, int unused);  // 0x46ea10
    // Draws the number of troop regiments of each of the five kinds that `object` (a system or a fleet) holds for `side`.
    int DrawTroopCounts(GameObj* object, int side, HDC dc);  // 0x46d8d0

    uint32_t m_unknown140;        // +0x140 not initialised
    CoolTabControl* m_tabs;       // +0x144 child id 100: the side tabs 1 (Alliance) and 2 (Empire)
    CoolStringField* m_nameField;  // +0x148 child id 0xcb
    CoolDragList* m_list;         // +0x14c child id 0xca
    GalaxyWindow* m_galaxy;       // +0x150
    CoolLabel* m_filterLabel;     // +0x154 tooltip text of the selected tab
    int m_filter;                 // +0x158 side tab shown, 0 before OnCreate1 ends
    HMODULE m_resourceModule;     // +0x15c resource slot 10 (GOKRES.DLL)
    HDC m_dc;                     // +0x160 memory DC the row pictures are drawn with
    HBRUSH m_brushes[5];          // +0x164 cell colours: red, green, grey, dark red, dark green
    CoolListItemList* m_items;    // +0x178 owned
    ObjectKey m_selected;         // +0x17c
};
