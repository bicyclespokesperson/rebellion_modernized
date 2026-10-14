#pragma once
#include <cstdint>
#include <string>

#include "core/game_string.h"
#include "ui/cool_game_window.h"
#include "ui/cool_label.h"

class CoolDragList;
class CoolListItemList;
class CoolStrobeButton;
class CoolTabControl;
class GalaxyWindow;
class GameObjNotifier;
class UiTextNotice;

// Window type 0x1c of the galaxy view (GalaxyWindow::OpenType1cWindow): the report view of a text notice. A tab
// control (child 100) selects the report page (0x65), the two participant lists (0x66 / 0x67) or the options page
// (0x68); the lists are built by FillList from the sub tab (0x97-0x9c, one per kind of unit) that is selected.
class ReportWindow : public CoolGameWindow
{
public:
  ReportWindow(HINSTANCE instance, GalaxyWindow* parent, uint32_t childId, UiTextNotice* notice); // 0x44c410
  ~ReportWindow() override; // 0x44c550 (scalar deleting destructor 0x44c530)

  void OnCreate1() override; // slot 14, 0x44c630
  void DrawOverlay(HDC dc) override; // slot 17, 0x44e290
  void OnCommand(int a, int b, int c) override; // slot 18, 0x44df90
  void OnKeyDown(WPARAM key, LPARAM lParam) override; // slot 19, 0x44e180

  void SelectPage(int tabId); // 0x44e3d0
  void FillList(int subTabId, int force); // 0x44e500
  void AddListItem(GameObjNotifier* object, int threeColumns, const std::string* emptyRowHeading); // 0x44edb0

  uint8_t m_unk114[4]; // +0x114
  GalaxyWindow* m_galaxy; // +0x118
  uint8_t m_unk11c[4]; // +0x11c
  CoolDragList* m_list; // +0x120 child id 6: the participants
  CoolListItemList* m_items; // +0x124 the rows of m_list
  CoolLabel m_label; // +0x128 draws the tab captions (DrawOverlay)
  CoolTabControl* m_pageTabs; // +0x15c child id 100
  CoolTabControl* m_unitTabs; // +0x160 child id 0x96: the kinds of unit of the lists
  CoolStrobeButton* m_closeButton; // +0x164 child id 10
  CoolStrobeButton* m_previousButton; // +0x168 child id 12
  CoolStrobeButton* m_nextButton; // +0x16c child id 11
  uint8_t m_unk170[4]; // +0x170
  UiTextNotice* m_notice; // +0x174
  HMODULE m_gokres; // +0x178 GOKRES.DLL (resource slot 10)
  int m_currentPage; // +0x17c 0x65-0x68
  int m_listKind; // +0x180 the unit tab (0x97-0x9c) the list was built for
  int m_nextItemId; // +0x184
};
