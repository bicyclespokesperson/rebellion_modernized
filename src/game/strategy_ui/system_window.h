#pragma once
#include <cstdint>

#include "gameobj/object_key.h"
#include "platform/win32.h"
#include "ui/custom_dialog_box.h"

class BitmapStatusBar;
class CoolDragImage;
class CoolDragList;
class CoolLabel;
class CoolListItemList;
class CoolStrobeButton;
class CoolStringField;
class CoolTabControl;
class DibBitmap;
class GalaxyWindow;
class GameObjNotifier;
class ObjectListItem;
class TextMessage;
class String;

// One of the three manufacturing manager panels (slot 1 shipyard, 2 training facility, 3 construction yard) of the
// first page of a system window: what the manager builds, how much of it, who commands it and the progress.
struct SystemSlot {
    RECT m_rect;                       // +0x00 where the panel is drawn
    ObjectKey m_productKey;            // +0x10 the object under construction
    ObjectKey m_managerKey;            // +0x14
    ObjectKey m_characterKey;          // +0x18 the character whose name is shown
    DibBitmap* m_productPicture;       // +0x1c owned
    CoolLabel* m_headingLabel;         // +0x20 owned
    CoolLabel* m_statusLabel;          // +0x24 owned, the manager's name (editable)
    CoolLabel* m_countLabel;           // +0x28 owned
    CoolLabel* m_characterLabel;       // +0x2c owned
    uint32_t m_flags;                  // +0x30 bit 0: highlighted
    BitmapStatusBar* m_progressBar;    // +0x34 owned, a child window
};

// The star system window (galaxy view child type 9, id (system id << 6) | 9): the first page (0x67) shows the three
// manager panels, the other pages (0x68-0x6c) the system's mines, refineries, shipyards... as a picture list. The tab
// buttons (child 0x70) switch between them.
class SystemWindow : public CustomDialogBox {
public:
    SystemWindow(HINSTANCE instance, int x, int y, int width, int height, GalaxyWindow* parent, uint32_t childId,
                 GameObjNotifier* system);  // 0x452fc0
    virtual ~SystemWindow();  // 0x4532c0 (scalar deleting destructor 0x4531e0)

    virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;  // slot 5, 0x4534f0
    virtual void SaveState(ostream* out) override;                  // slot 9, 0x458ea0
    virtual void LoadState(istream* in) override;                   // slot 10, 0x458f50
    virtual void OnCreate1() override;                              // slot 14, 0x455060
    virtual void OnKeyDown(WPARAM key, LPARAM lParam) override;     // slot 19, 0x458980
    virtual void GetSelection(ObjectKeyValueSet* out, CoolWindow* source) override;  // slot 22, 0x453ee0
    virtual void OnChildAdded(const ObjectKey* key, GameObjNotifier* object) override;  // slot 23, 0x454160
    virtual void OnChildMoved(const ObjectKey* key, GameObjNotifier* object, const ObjectKey* otherKey,
                              GameObjNotifier* otherObject) override;                    // slot 24, 0x454b30
    virtual void OnObjectUpdated(const ObjectKey* key, GameObjNotifier* object) override;  // slot 25, 0x4540b0
    virtual ObjectKey GetKeyAt(int x, int y) override;              // slot 26, 0x458b80
    virtual void ShowObjectPage(const ObjectKey* key) override;     // slot 27, 0x458d20
    virtual ObjectKey GetDropKeyAt(int x, int y) override;          // slot 28, 0x4aa470 (shared with other windows)
    virtual int Virtual29() override;                               // slot 29, 0x458cf0: the bitmap of the system's side

    // The slot (1-3) whose panel belongs to the manufacturing manager `manager`; 0 if none does.
    int SlotOfManager(const ObjectKey* manager);  // 0x4583e0
    // Shows the name of the character `character` on the slot's heading (unless it already does).
    void SetSlotCharacter(int slot, GameObjNotifier* manager, const ObjectKey* character);  // 0x457f30
    // Sets the slot's status text to `text`, or to the default text of the slot / the manager's object.
    void SetSlotStatusText(int slot, GameObjNotifier* manager, String* text);  // 0x457b40
    // Redraws the picture and count of what the slot's manager produces.
    void SetSlotProduction(int slot, GameObjNotifier* manager);  // 0x457c90
    // Sets the slot's progress bar to `position` of `range`.
    void SetSlotProgress(int slot, GameObjNotifier* manager, int position, int range);  // 0x458040
    // Starts renaming for `request` once the window's tab is shown.
    void BeginRenameRequest(TextMessage* request);  // 0x458b50

    // Prepares the slot's labels for `manager` (0x458080).
    void InitSlot(int slot, GameObjNotifier* manager);
    // The picture of the page `page` for the system's control side (0x456230), and the page switch (0x4568a0).
    void LoadTabArt();
    int SelectPage(int page);
    void DrawContents(RECT paintRect, HDC dc);                 // 0x457690
    void ClearSlotHighlight(SystemSlot* slot);                  // 0x458410: null clears every slot
    void ClearSlots();                                          // 0x458480
    void DestroySlots();                                        // 0x458500
    void HighlightSlot(SystemSlot* slot);                       // 0x4585a0
    void ShowProgressBars(int hide);                            // 0x458600: 0 shows the bars, otherwise hides them
    void BeginSlotRename(SystemSlot* slot);                     // 0x458640
    void EndSlotRename(SystemSlot* slot);                       // 0x458780

    SystemSlot& Slot(int number) { return m_slots[number - 1]; }

    uint32_t m_unknown140;              // +0x140 BUG(original): not initialised
    ObjectKey m_systemKey;              // +0x144
    int m_controlSide;                  // +0x148 the side in control of the system (0-3)
    GalaxyWindow* m_galaxy;             // +0x14c
    CoolStrobeButton* m_closeButton;    // +0x150 child id 0x65
    CoolStrobeButton* m_sideButton;     // +0x154 child id 100: to the other side of the screen
    CoolStrobeButton* m_sectorButton;   // +0x158 child id 0xc9: opens the sector window
    DibBitmap* m_tabIcon;               // +0x15c the picture drawn over the list pictures
    int m_page;                         // +0x160 0x67-0x6c
    uint32_t m_uiFlags;                 // +0x164 2: dragging a panel, 4: rename allowed, 8: renaming
    HMODULE m_gokres;                   // +0x168 GOKRES.DLL (resource slot 10)
    CoolTabControl* m_tabs;             // +0x16c child id 0x70
    CoolStringField* m_renameField;     // +0x170 over the status text of a panel
    CoolDragList* m_list;               // +0x174 child id 2: the pages 0x68-0x6c
    CoolListItemList* m_items;          // +0x178
    CoolLabel* m_pageLabel;             // +0x17c
    DibBitmap* m_headerActive;          // +0x180 the title bar while the window is in front
    DibBitmap* m_headerInactive;        // +0x184
    DibBitmap* m_panelBackground;       // +0x188
    DibBitmap* m_panelSelected;         // +0x18c
    DibBitmap* m_panelNormal;           // +0x190
    DibBitmap* m_panelCanvas;           // +0x194 the panel being drawn
    DibBitmap* m_countsCanvas;          // +0x198 the strip of the facility counts
    CoolLabel* m_countsLabel;           // +0x19c
    SystemSlot m_slots[3];              // +0x1a0
    SystemSlot* m_selectedSlot;         // +0x248
    int m_dragStartX;                   // +0x24c
    int m_dragStartY;                   // +0x250
    DibBitmap* m_dragPictures[3];       // +0x254 one per number of panels dragged
    int m_selectedCount;                // +0x260 the highlighted panels
    CoolDragImage* m_dragImage;         // +0x264
    TextMessage* m_pendingRename;     // +0x268
};

// The picture of a facility or manufacturing manager (0x458fe0): null if none; the caller owns it.
DibBitmap* SystemObjectPicture(GameObjNotifier* object);
