#pragma once
#include <cstdint>

#include "gameobj/object_key.h"
#include "platform/win32.h"
#include "ui/cool_game_window.h"
#include "ui/cool_label.h"

class CoolDragList;
class CoolListItemList;
class CoolSelectionBox;
class CoolTabControl;
class DatRecord;
class GalaxyWindow;
class MissionOrder240;
class ObjectKeyValueSet;

// The mission selection dialog (galaxy view child 0x1e, GalaxyWindow::IssueTargetedCommand): it takes ownership of a
// MissionOrder240 and lets the player choose the mission (a combo box of the kinds the target allows) and the team
// (two lists of characters, the arrows move them between "available" and "selected"). OK sends the order to the
// human player; Cancel or close deletes it.
class MissionSelectionWindow : public CoolGameWindow
{
public:
  MissionSelectionWindow(HINSTANCE instance, CoolWindow* parent, uint32_t childId, MissionOrder240* order); // 0x46a750
  ~MissionSelectionWindow() override; // 0x46a880 (scalar deleting destructor 0x46a860)

  void OnCreate1() override; // slot 14, 0x46a9c0
  void FocusCurrentPageControl() override; // slot 21, 0x46c9e0
  void OnCommand(int a, int b, int c) override; // slot 18, 0x46c3c0
  void OnKeyDown(WPARAM key, LPARAM lParam) override; // slot 19, 0x46ca10

  // Shows page `page` (0x97: the mission combo box, 0x98: the team lists) unless it is the current one.
  void SelectPage(int page); // 0x46c8a0

  GalaxyWindow* m_galaxy; // +0x114
  int m_page; // +0x118 0x97 or 0x98, 0 before the tab control selects one
  CoolTabControl* m_tabs; // +0x11c
  MissionOrder240* m_order; // +0x120 owned until it is sent
  CoolListItemList* m_missionItems; // +0x124 the mission kinds; every row's m_userData is an owned ObjectKey
  CoolSelectionBox* m_missionBox; // +0x128
  CoolListItemList* m_availableItems; // +0x12c characters that can still join
  CoolDragList* m_availableList; // +0x130
  CoolListItemList* m_selectedItems; // +0x134 characters of the mission
  CoolDragList* m_selectedList; // +0x138
  HMODULE m_gokres; // +0x13c resource slot 10 (GOKRES.DLL)
  CoolLabel m_unusedLabel; // +0x140
};

// The row id a mission kind gets in the mission combo box (the kinds of the records at +0x2c); an unknown kind gets
// the next number of a counter (0x6a8794).
int MissionKindItemId(DatRecord* record); // 0x46cab0
// Copies of the order's team (0x46c850) and of its selected characters (0x46c880).
ObjectKeyValueSet* MissionOrderCopyTeam(MissionOrder240* order, ObjectKeyValueSet* out);
ObjectKeyValueSet* MissionOrderCopyCharacters(MissionOrder240* order, ObjectKeyValueSet* out);
