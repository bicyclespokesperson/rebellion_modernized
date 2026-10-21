#pragma once
#include <cstdint>
#include <string>

#include "gameobj/object_key.h"
#include "platform/win32.h"
#include "ui/cool_label.h"
#include "ui/custom_dialog_box.h"

class CoolDragList;
class CoolListItemList;
class CoolStrobeButton;
class CoolTabControl;
class DibBitmap;
class GalaxyWindow;
class GameObjNotifier;
class ObjectListItem;

// The missions window of a system (galaxy view child kind 0x40, id (system id << 6) | 0xb): the left list shows the
// missions that characters at the system are on (one row per mission, with the picture of its kind); the right list
// shows the characters on the selected mission, in two tabs (0x17 the agents, 0x18 the decoys), and above it the
// picture and name of the mission's objective.
class SystemMissionsWindow : public CustomDialogBox
{
  public:
  SystemMissionsWindow(HINSTANCE instance, int x, int y, CoolWindow* parent, uint32_t childId,
                       GameObjNotifier* system); // 0x49f130
  ~SystemMissionsWindow() override;              // 0x49f340 (scalar deleting destructor 0x49f320)

  LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;   // slot 5, 0x49f4b0
  void SaveState(ostream* out) override;                                     // slot 9, 0x4a0b00
  void LoadState(istream* in) override;                                      // slot 10, 0x4a0b90
  void OnCreate1() override;                                                 // slot 14, 0x49f540
  void DrawOverlay(HDC dc) override;                                         // slot 17, 0x49fef0
  void OnCommand(int a, int command, int c) override;                        // slot 18, 0x4a0120
  void OnKeyDown(WPARAM key, LPARAM lParam) override;                        // slot 19, 0x4a0e00: Escape closes
  void GetSelection(ObjectKeyValueSet* out, CoolWindow* source) override;    // slot 22, 0x4a0400
  void OnChildAdded(const ObjectKey* key, GameObjNotifier* object) override; // slot 23, 0x4a0560
  void OnChildMoved(const ObjectKey* key, GameObjNotifier* object, const ObjectKey* otherKey,
                    GameObjNotifier* otherObject) override;                     // slot 24, 0x4a06d0
  void OnObjectUpdated(const ObjectKey* key, GameObjNotifier* object) override; // slot 25, 0x4a0870
  ObjectKey GetKeyAt(int x, int y) override;                                    // slot 26, 0x4a09a0
  void ShowObjectPage(const ObjectKey* key) override;                           // slot 27, 0x4a1e10
  ObjectKey GetDropKeyAt(int x, int y) override; // slot 28, 0x4aa470 (shared with other windows)
  int GetMinimizedIconBitmapId() override;       // slot 29, 0x4a21c0: the bitmap of the dominant side

  // The system the window is about (null, and the window closes itself, when it is gone). Linker-folded with the
  // same getter of SystemWindow (0x4a25c0).
  GameObjNotifier* GetSystem(); // 0x4a25c0
  // Forgets the selected mission if it is `row` (by id).
  void ClearSelectedMission(ObjectListItem* row); // 0x4a0c20
  // Makes `row` the selected mission (when it is another one, or `force`) and rebuilds the tabs and the objective.
  void SelectMission(ObjectListItem* row, int force); // 0x4a0c60
  // Loads the tab pictures for the side of the selected mission and shows or hides the character list.
  void UpdateTabs(); // 0x4a0ca0
  // Adds one row to the character list: the picture and name of `object`, or a heading / blank row without one.
  void AddCharacterRow(GameObjNotifier* object, int normal, const std::string* heading); // 0x4a0e10
  // Picture and name of the objective of the selected mission, drawn above the character list.
  void UpdateObjective(); // 0x4a10a0
  // Rebuilds the mission rows from the characters at the system; returns 0 when there is no system.
  int FillMissionList(); // 0x4a1590
  // Rebuilds the character rows for the mission selected and the tab `tab` (0x17 / 0x18) unless it is the shown one.
  void FillCharacterList(int tab, int force); // 0x4a1ba0
  // The title pictures for the dominant side and their rectangle.
  void LoadHeaderBitmaps(); // 0x4a2200
  // The selected row of the mission list, null for none.
  ObjectListItem* GetSelectedMission(); // 0x4a2430
  // The control side (0-3) of the characters on the selected mission.
  uint32_t SelectedMissionSide(); // 0x4a24b0

  uint32_t m_unknown140;             // +0x140
  ObjectKey m_systemKey;             // +0x144
  uint32_t m_unknown148;             // +0x148
  GalaxyWindow* m_galaxy;            // +0x14c
  CoolStrobeButton* m_closeButton;   // +0x150 child id 0x14
  CoolStrobeButton* m_sideButton;    // +0x154 child id 0x15: to the other side of the screen
  CoolStrobeButton* m_sectorButton;  // +0x158 child id 0xca: opens the sector window
  CoolTabControl* m_tabs;            // +0x15c child id 0x16: the tabs 0x17 and 0x18
  int m_tab;                         // +0x160 the shown tab
  int m_titleLeft;                   // +0x164 where the title text starts, from the left of the title bar
  DibBitmap* m_headerInactive;       // +0x168
  DibBitmap* m_headerActive;         // +0x16c the title bar while the window is in front
  DibBitmap* m_objectivePicture;     // +0x170 owned
  CoolLabel* m_objectiveLabel;       // +0x174 owned, the objective's name
  CoolLabel* m_captionLabel;         // +0x178 owned
  CoolLabel m_titleLabel;            // +0x17c
  HMODULE m_gokres;                  // +0x1b0 GOKRES.DLL (resource slot 10)
  CoolListItemList* m_missions;      // +0x1b4
  CoolDragList* m_missionList;       // +0x1b8 child id 10
  CoolListItemList* m_characters;    // +0x1bc
  CoolDragList* m_characterList;     // +0x1c0 child id 0xb
  ObjectListItem* m_selectedMission; // +0x1c4
  ObjectKey m_objectiveKey;          // +0x1c8
};

// Which side's missions are at `system`: 1 or 2 when only one side's characters are on missions there from the point
// of view of `viewSide`, 3 for both or none (0x4a1f60).
int DominantMissionSide(GameObjNotifier* system, int viewSide);
