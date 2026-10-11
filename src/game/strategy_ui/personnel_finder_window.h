#pragma once
#include <cstdint>

#include "gameobj/object_key.h"
#include "platform/win32.h"
#include "ui/custom_dialog_box.h"

class Character;
class CoolDragList;
class CoolLabel;
class CoolListItemList;
class CoolStringField;
class CoolTabControl;
class GalaxyWindow;
class GameObj;
class ObjectListItem;

// The Personnel Finder (galaxy view child 0x17, F5): a side filter (Alliance / Empire tab), a page choice (the systems
// and fleets that hold special forces, with the number of each kind of force; or the characters, with where they are
// and what state they are in) and a name field. Opening a row shows the object in its window.
class PersonnelFinderWindow : public CustomDialogBox
{
public:
  PersonnelFinderWindow(HINSTANCE instance,
                        int x,
                        int y,
                        int width,
                        int height,
                        GalaxyWindow* parent,
                        uint32_t childId); // 0x463500
  ~PersonnelFinderWindow() override; // 0x463650 (scalar deleting destructor 0x463630)

  LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override; // slot 5, 0x464c20
  void OnCreate1() override; // slot 14, 0x4637a0
  void DrawOverlay(HDC dc) override; // slot 17, 0x464c00
  void OnCommand(int a, int command, int c) override; // slot 18, 0x464890
  void OnKeyDown(WPARAM key, LPARAM lParam) override; // slot 19, 0x466160

  // Rebuilds the rows of the page the page tabs show (2: the force rows in m_forceRows, 3: the character rows in
  // m_characterRows) for the side the filter tab selects, and shows `rows` in the list. `unused` is ignored.
  void BuildList(CoolListItemList* rows, int unused); // 0x464e10
  // Draws the number of special forces of each of the four kinds that `object` (a system or a fleet) holds for
  // `side`, as coloured cells over a row's picture.
  int DrawForceCounts(GameObj* object, int side, HDC dc); // 0x465540
  // Sets the text of a character row (name, where, state), or hides the row.
  void DescribeCharacter(Character* character, ObjectListItem* row); // 0x465bb0

  uint32_t m_unknown140; // +0x140 not initialised
  CoolTabControl* m_filterTabs; // +0x144 child id 100: the side tabs 1 (Alliance) and 2 (Empire)
  CoolTabControl* m_pageTabs; // +0x148 child id 0x6e: the page tabs 2 and 3
  CoolStringField* m_nameField; // +0x14c child id 0xcb
  CoolDragList* m_list; // +0x150 child id 0xca
  GalaxyWindow* m_galaxy; // +0x154
  CoolLabel* m_filterLabel; // +0x158 tooltip text of the selected filter tab
  HBRUSH m_brushes[5]; // +0x15c cell colours: red, green, grey, dark red, dark green
  CoolListItemList* m_forceRows; // +0x170 owned
  CoolListItemList* m_characterRows; // +0x174 owned
  ObjectKey m_selected; // +0x178
};
