#pragma once
#include <cstdint>

#include "core/utility_classes.h"
#include "gameobj/object_key.h"
#include "platform/win32.h"
#include "ui/cool_window.h"
#include "ui/custom_dialog_box.h"

class CoolDragList;
class CoolLabel;
class CoolListItemList;
class CoolTabControl;
class DibBitmap;
class FleetListItem;
class GalaxyWindow;
class GameObjNotifier;

// Bits of FleetWindow::m_uiFlags.
enum FleetWindowFlags : uint32_t {
    kFleetShipListActive = 0x01,   // the ship list (not the fleet list) is the one the user works in
    kFleetOwnSelected = 0x04,      // the selected fleets belong to the player's side
    kFleetShipSelected = 0x08,     // the selection is one ship (shown with its own picture)
    kFleetOwnTabsShown = 0x10,
    kFleetOtherTabsShown = 0x20,
    kFleetOtherSelected = 0x40,    // the selected fleets belong to the other side
    kFleetNeedsRefresh = 0x10000000,
};

// The window of a system's fleets (galaxy view child kind 0x10, id (system id << 6) | 4): the left list shows the fleets
// at the system, each with the capital ships below it (a double click opens or closes a fleet); the right list shows what
// the selected fleets carry - ships, fighter squadrons, troop regiments or personnel, chosen by the tabs.
class FleetWindow : public CustomDialogBox {
public:
    FleetWindow(HINSTANCE instance, int x, int y, CoolWindow* parent, uint32_t childId,
                GameObjNotifier* system);  // 0x4a2630
    virtual ~FleetWindow();  // 0x4a2820 (scalar deleting destructor 0x4a2800)

    virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;  // slot 5, 0x4a29c0
    virtual int OnRightButtonUp(int x, int y, CoolWindow* sender) override;  // slot 7, 0x4a2c40
    virtual void SaveState(ostream* out) override;                  // slot 9, 0x4a7300
    virtual void LoadState(istream* in) override;                   // slot 10, 0x4a7290
    virtual void OnCreate1() override;                              // slot 14, 0x4a4b10
    virtual void OnKeyDown(WPARAM key, LPARAM lParam) override;     // slot 19, 0x4a7390
    virtual void GetSelection(ObjectKeyValueSet* out, CoolWindow* source) override;  // slot 22, 0x4a2c80
    virtual void OnChildAdded(const ObjectKey* key, GameObjNotifier* object) override;  // slot 23, 0x4a2e70
    virtual void OnChildMoved(const ObjectKey* key, GameObjNotifier* object, const ObjectKey* otherKey,
                              GameObjNotifier* otherObject) override;                    // slot 24, 0x4a2d60
    virtual void OnObjectUpdated(const ObjectKey* key, GameObjNotifier* object) override;  // slot 25, 0x4a2e70
    virtual ObjectKey GetKeyAt(int x, int y) override;              // slot 26, 0x4a2f80
    virtual void ShowObjectPage(const ObjectKey* key) override;     // slot 27, 0x4a73c0
    virtual ObjectKey GetDropKeyAt(int x, int y) override;          // slot 28, 0x4a3130
    virtual int Virtual29() override;                               // slot 29, 0x4a76e0: the bitmap of the side to show
    virtual RECT* GetRenameRect(RECT* out) override;                // slot 34, 0x4a71d0: over the selected row's text
    // Rebuilds the rows when something below the system changed (flag kFleetNeedsRefresh).
    virtual void Virtual35() override;                              // slot 35, 0x4a7370

    // The WM_COMMAND of the buttons, tabs and lists.
    LRESULT OnWmCommand(UINT message, WPARAM wParam, LPARAM lParam);  // 0x4a6390
    // Brings the fleet rows (and, through SyncShipRows, the ship rows) up to date with the fleets at the system.
    void SyncFleetRows();  // 0x4a3340
    // Inserts a row for `fleet` before *position, then its ship rows; *position ends after them.
    void AddFleetRow(GameObjNotifier* fleet, FleetListItem** position);  // 0x4a37c0
    // Brings the ship rows after the fleet row *position up to date with the capital ships of `fleet`.
    void SyncShipRows(GameObjNotifier* fleet, FleetListItem** position);  // 0x4a3d40
    // Removes the row *position and the ship rows after it; *position ends at the next fleet row.
    void RemoveRows(FleetListItem** position);  // 0x4a4ac0
    // Shows the selection: its name, its picture, the tabs and the right-hand list.
    void UpdateShipList();  // 0x4a5c00
    // Shows the tab control of the side of the selection with the tab `m_page`; greys out the tabs of kinds that the
    // selected fleets do not carry.
    void UpdateTabs();  // 0x4a6980
    // Adds the rows of the page `m_page` for `object` (a fleet: for its ships; a ship: its units, or itself).
    void AddShipRows(GameObjNotifier* object);  // 0x4a6be0
    void AddShipRow(GameObjNotifier* object);   // 0x4a6e70

    uint32_t m_unknown140;              // +0x140 BUG(original): not initialised
    ObjectKey m_systemKey;              // +0x144
    GalaxyWindow* m_galaxy;             // +0x148
    PersistentInt m_uiFlags;            // +0x14c FleetWindowFlags
    ChildWindowTree m_buttons;          // +0x150 the title bar buttons: sector 0xca, close 0x65, side 100
    CoolTabControl* m_ownTabs;          // +0x158 child id 0x6a: the tabs when the selection is the player's
    CoolTabControl* m_otherTabs;        // +0x15c child id 0x6b
    CoolDragList* m_fleetList;          // +0x160 child id 0xc9
    CoolDragList* m_shipList;           // +0x164 child id 200
    DibBitmap* m_infoBitmap;            // +0x168 owned: the selection's picture and the totals
    int m_controlSide;                  // +0x16c the side whose colours the window shows
    int m_viewSide;                     // +0x170 1 for the Alliance, 2 for the Empire
    int m_galaxySide;                   // +0x174 the player's side
    DibBitmap* m_headerActive;          // +0x178 the title bar while the window is in front
    DibBitmap* m_headerInactive;        // +0x17c
    DibBitmap* m_infoBackdropOwn;       // +0x180
    DibBitmap* m_infoBackdropOther;     // +0x184
    CoolListItemList* m_fleetItems;     // +0x188
    CoolListItemList* m_shipItems;      // +0x18c
    CoolLabel* m_selectionLabel;        // +0x190 owned, the name of the selection
    int m_page;                         // +0x194 the shown tab, 0x66 ships, 0x67 fighters, 0x68 troops, 0x69 personnel
    int m_primaryTotal;                 // +0x198 squadrons / regiments of the selection
    int m_capacityTotal;                // +0x19c their capacity
    HPEN m_pen;                         // +0x1a0 dotted white: the connector lines
    HFONT m_fleetFont;                  // +0x1a4
    HFONT m_shipFont;                   // +0x1a8
    HDC m_dc;                           // +0x1ac memory DC the row pictures are drawn with
    HMODULE m_gokres;                   // +0x1b0 GOKRES.DLL (resource slot 10)
};

// What `object` (a fleet or a capital ship) carries and its state, as the bits the fleet rows show: 1 fighters, 2 troops,
// 4 personnel, 8 enroute, 0x10 has orders, 0x20 not completed, 0x40 cannot move (0x4a66a0).
uint32_t FleetContents(GameObjNotifier* object);
// Draws the icons for the bits of `contents` on `target`: low (when `compact`) or high row of the picture (0x4a67a0).
void DrawFleetContentIcons(DibBitmap* target, int side, uint32_t contents, int compact);
