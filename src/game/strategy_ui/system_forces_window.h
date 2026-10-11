#pragma once
#include "core/reb_abi.h"
#include <cstdint>

#include "gameobj/object_key.h"
#include "platform/win32.h"
#include "ui/custom_dialog_box.h"

class CoolDragList;
class CoolLabel;
class CoolListItemList;
class CoolTabControl;
class DibBitmap;
class GalaxyWindow;
class GameObjNotifier;

// The window of the forces at a system (galaxy view child kind 8, id (system id << 6) | 10): a list of the objects of
// the tab chosen at the left edge, for the side that controls the system. The tabs: 1 personnel, 2 troop regiments,
// 3 fighter squadrons, 4 and 5 the two kinds of defense facilities.
class SystemForcesWindow : public CustomDialogBox {
public:
    SystemForcesWindow(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent, uint32_t childId,
                       GameObjNotifier* system);  // 0x4a7790
    virtual ~SystemForcesWindow();  // 0x4a78f0 (scalar deleting destructor 0x4a78d0)

    virtual LRESULT REB_STDCALL WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;  // slot 5, 0x4a86d0
    virtual void SaveState(ostream* out) override;                  // slot 9, 0x4aa6d0
    virtual void LoadState(istream* in) override;                   // slot 10, 0x4aa760
    virtual void OnCreate1() override;                              // slot 14, 0x4a8790
    virtual void DrawOverlay(HDC dc) override;                      // slot 17, 0x4a9800
    virtual void OnCommand(int a, int command, int c) override;     // slot 18, 0x4a9890
    virtual void OnKeyDown(WPARAM key, LPARAM lParam) override;     // slot 19, 0x4aa2b0: Escape closes, left and right change tab
    virtual void GetSelection(ObjectKeyValueSet* out, CoolWindow* source) override;  // slot 22, 0x4a7a20
    virtual void OnChildAdded(const ObjectKey* key, GameObjNotifier* object) override;  // slot 23, 0x4a7b40
    virtual void OnChildMoved(const ObjectKey* key, GameObjNotifier* object, const ObjectKey* otherKey,
                              GameObjNotifier* otherObject) override;                    // slot 24, 0x4a7f10
    virtual void OnObjectUpdated(const ObjectKey* key, GameObjNotifier* object) override;  // slot 25, 0x4a7f50
    virtual ObjectKey GetKeyAt(int x, int y) override;              // slot 26, 0x4aa380
    virtual void ShowObjectPage(const ObjectKey* key) override;     // slot 27, 0x4aa4d0
    virtual ObjectKey GetDropKeyAt(int x, int y) override;          // slot 28, 0x4aa470 (shared with the other system windows)
    virtual int Virtual29() override;                               // slot 29, 0x4aa4a0: the bitmap of the controlling side

    // Shows the tab `tab` (1-5) unless it is the shown one; returns whether it changed.
    int ShowTab(int tab);  // 0x4a90d0
    // Adds the row of `object`, with `overlay` over its selected picture.
    void AddRow(GameObjNotifier* object, DibBitmap* overlay);  // 0x4a9ab0
    // The title bar and row pictures of the controlling side, and the pictures of the tabs (greyed when empty).
    void LoadHeaderBitmaps();  // 0x4a9ce0

    uint32_t m_unknown140;           // +0x140 BUG(original): not initialised
    ObjectKey m_systemKey;           // +0x144
    int m_side;                      // +0x148 the side that controls the system
    GalaxyWindow* m_galaxy;          // +0x14c
    uint8_t m_unk150[8];             // +0x150
    int m_tab;                       // +0x158 the shown tab, 0 before the first
    HMODULE m_gokres;                // +0x15c GOKRES.DLL (resource slot 10)
    CoolTabControl* m_tabs;          // +0x160 child id 100 (BUG(original): not initialised)
    CoolDragList* m_list;            // +0x164 child id 0xcb
    CoolListItemList* m_items;       // +0x168 (BUG(original): not initialised)
    CoolLabel* m_tabLabel;           // +0x16c owned, the name of the shown tab
    CoolLabel* m_infoLabel;          // +0x170 owned, "troops required" on the troop tab
    DibBitmap* m_rowOverlay;         // +0x174 owned, drawn over the selected pictures
    DibBitmap* m_unused178;          // +0x178 owned, never created
    DibBitmap* m_headerActive;       // +0x17c the title bar while the window is in front
    DibBitmap* m_headerInactive;     // +0x180
};
