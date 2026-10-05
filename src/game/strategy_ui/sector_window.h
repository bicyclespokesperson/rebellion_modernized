#pragma once
#include <cstdint>

#include "core/layout.h"
#include "gameobj/object_key.h"
#include "platform/win32.h"
#include "ui/cool_drag_list.h"
#include "ui/custom_dialog_box.h"
#include "ui/object_list_item.h"

class CoolDragImage;
class CoolStrobeButton;
class DibBitmap;
class GalaxyWindow;
class GameObjNotifier;

// Kinds of the marker rows around a system icon (the high word of their ids).
enum SectorMarkerKind : uint32_t {
    kMarkerFacility = 0x04,   // the system's production facilities (opens the system window)
    kMarkerDefense = 0x08,    // defense facilities, troops and fighter squadrons (opens the troop window)
    kMarkerFleet = 0x10,      // fleets
    kMarkerMission = 0x40,    // characters on missions
};

// Bits of ObjectListItem::m_state of the marker rows.
enum SectorMarkerState : uint32_t {
    kMarkerHighlighted = 0x01,
    kMarkerVisible = 0x02,
    kMarkerSideMask = 0x1c,
};

// The sector window (galaxy view child type 1, id (sector id << 6) | 1): the systems of one sector as icons with
// three status bars each (energy, raw materials, loyalty) and four marker rows (facilities, defense, fleets,
// missions) that open the system, troop, fleet and mission windows by a click or a drag.
class SectorWindow : public CustomDialogBox {
public:
    SectorWindow(HINSTANCE instance, int x, int y, int width, int height, GalaxyWindow* parent, DibBitmap* background,
                 uint32_t childId, GameObjNotifier* sector);  // 0x4591d0
    virtual ~SectorWindow();  // 0x4592e0 (scalar deleting destructor 0x4592c0)

    virtual LRESULT __stdcall WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;  // slot 5, 0x4593e0
    virtual void SaveState(ostream* out) override;   // slot 9, 0x45d2b0
    virtual void LoadState(istream* in) override;    // slot 10, 0x45d360
    virtual void OnCreate1() override;               // slot 14, 0x459e30
    virtual void GetSelection(ObjectKeyValueSet* out, CoolWindow* source) override;  // slot 22, 0x45b3b0
    virtual void OnChildAdded(const ObjectKey* key, GameObjNotifier* object) override;  // slot 23, 0x45b3e0
    virtual void OnChildMoved(const ObjectKey* key, GameObjNotifier* object, const ObjectKey* otherKey,
                              GameObjNotifier* otherObject) override;                    // slot 24, 0x45b690
    virtual void OnObjectUpdated(const ObjectKey* key, GameObjNotifier* object) override;  // slot 25, 0x45b770
    virtual ObjectKey GetKeyAt(int x, int y) override;      // slot 26, 0x45c6b0
    virtual ObjectKey GetDropKeyAt(int x, int y) override;  // slot 28, 0x45c830

    // Opens (or raises) the window the marker row `item` stands for, at the given position; null if the window of that
    // kind cannot be made.
    CoolWindow* OpenMarkerWindow(ObjectListItem* item, int x, int y);                 // 0x45aac0
    // Removes the highlight of `item`, or of every marker when it is null.
    void ClearHighlight(ObjectListItem* item);                                         // 0x45afc0
    void Highlight(ObjectListItem* item);                                              // 0x45b1b0
    void UpdateLoyaltyBar(const ObjectKey* system, int range, int position);          // 0x45baf0
    void UpdateSystemMarkers(const ObjectKey* system, int side);                       // 0x45bbb0
    void UpdateEnergyBar(const ObjectKey* system, int range, int position);           // 0x45c240
    void UpdateRawMaterialBar(const ObjectKey* system, int range, int position);      // 0x45c450
    ObjectListItem* FindSystemAt(int x, int y);                                        // 0x45c660
    // The window of child type `windowType` (4 fleets, 9 facilities, 10 defense, 11 missions) of the system `system`,
    // opened from its marker row.
    CoolWindow* OpenWindowOfType(const ObjectKey* system, int windowType);             // 0x45c8e0
    ObjectListItem* HitTestMarkers(CoolListItemList* markers, int x, int y);          // 0x45cc10
    // The marker updates take a second argument that they ignore.
    void UpdateFleetMarker(GameObjNotifier* system, GameObjNotifier* unused);          // 0x45ccc0
    void UpdateFacilityMarker(GameObjNotifier* system, GameObjNotifier* unused);       // 0x45cdc0
    void UpdateDefenseMarker(GameObjNotifier* system, GameObjNotifier* unused);        // 0x45ce80
    void UpdateMissionMarker(GameObjNotifier* system, GameObjNotifier* unused);        // 0x45d090
    void SetMarkerState(ObjectListItem* marker, int side, int count);                  // 0x45d140

    uint32_t m_inputBlocked;            // +0x140 BUG(original): not initialised by the constructor; cleared by WM_PAINT
    GameObjNotifier* m_sector;          // +0x144
    int m_dragging;                     // +0x148
    int m_dragStartX;                   // +0x14c -1000 while no drag is prepared
    int m_dragStartY;                   // +0x150
    DibBitmap* m_unknown154;            // +0x154 owned, never set
    uint8_t m_markerColorKey;           // +0x158 the transparent colour of the marker pictures
    uint8_t m_pad159[3];                // +0x159
    uint8_t* m_remapTable;              // +0x15c heap block, the background's darkening table
    HDC m_memoryDc;                     // +0x160
    CoolListItemList m_systems;         // +0x164 the system icons (rows of the sector)
    CoolListItemList m_markers;         // +0x174 the marker rows
    ObjectKeyValueSet m_marked;         // +0x184 the highlighted markers (key, kind)
    CoolStrobeButton* m_closeButton;    // +0x18c child id 0x138
    CoolStrobeButton* m_sideButton;     // +0x190 child id 0x139: moves the window to the other side of the screen
    GalaxyWindow* m_galaxy;             // +0x194
    CoolDragImage* m_dragImage;         // +0x198
    DibBitmap* m_dragLayer;             // +0x19c what the drag image is taken from
    ObjectListItem* m_hitItem;          // +0x1a0 the marker under the pressed mouse button
};

// The picture (GOKRES id) of a system of the given kind (1-26), 0 for none (0x45c970).
int SectorSystemPictureId(int index);
// The picture id of a marker row of kind `kind` (SectorMarkerKind), for the side in control and the highlight (0x45ca80).
uint32_t __stdcall SectorMarkerImageId(uint32_t kind, int state, int selected);
// The rectangle of the caption below a system icon (0x45c1f0); returns `out`.
RECT* __stdcall SectorMarkerLabelRect(RECT* out, ObjectListItem* item);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(SectorWindow, 0x1a4);
REB_ASSERT_OFFSET(SectorWindow, m_inputBlocked, 0x140);
REB_ASSERT_OFFSET(SectorWindow, m_sector, 0x144);
REB_ASSERT_OFFSET(SectorWindow, m_dragging, 0x148);
REB_ASSERT_OFFSET(SectorWindow, m_markerColorKey, 0x158);
REB_ASSERT_OFFSET(SectorWindow, m_remapTable, 0x15c);
REB_ASSERT_OFFSET(SectorWindow, m_systems, 0x164);
REB_ASSERT_OFFSET(SectorWindow, m_markers, 0x174);
REB_ASSERT_OFFSET(SectorWindow, m_marked, 0x184);
REB_ASSERT_OFFSET(SectorWindow, m_closeButton, 0x18c);
REB_ASSERT_OFFSET(SectorWindow, m_galaxy, 0x194);
REB_ASSERT_OFFSET(SectorWindow, m_hitItem, 0x1a0);
#pragma clang diagnostic pop
