#pragma once
#include <cstdint>
#include <string>

#include "app/player_controller_decl.h"
#include "core/game_string.h"
#include "gameobj/object_key.h"
#include "platform/win32.h"
#include "ui/cool_game_window.h"
#include "ui/cool_label.h"

class CoolDragList;
class CoolListItemList;
class CoolStrobeButton;
class CoolTabControl;
class GalaxyWindow;
class GameObjNotifier;
class GameObj;

// The message the battle prompt shows (stand-in until its module publishes the class; only the layout the window reads):
// where the battle is, the fleet that triggered it, the two sides and the participants of each side.
class BattleMessage : public PlayerMessage
{
public:
  uint8_t m_unk24[0x20]; // +0x24
  ObjectKey m_systemKey; // +0x44 the system the battle is in
  int m_unk48; // +0x48
  ObjectKey m_fleetKey; // +0x4c the fleet that arrived
  int m_attackerSide; // +0x50 0 for none
  int m_defenderSide; // +0x54
  int m_unk58; // +0x58
  int m_unk5c; // +0x5c
  ObjectKeySet m_fleets; // +0x60 the fleets that fight
  ObjectKeySet m_fighters; // +0x68 fighter squadrons
  ObjectKeySet m_characters; // +0x70 characters
  ObjectKeySet m_allianceSide; // +0x78 participants chosen for the Alliance
  ObjectKeySet m_empireSide; // +0x80 participants chosen for the Empire
  std::string m_title; // +0x88
  std::string m_summary; // +0x94
  std::string m_caption; // +0xa0
  std::string m_sideTextA; // +0xac
  std::string m_sideTextB; // +0xb8

  ObjectKey* GetFleetKey(ObjectKey* out); // 0x4727c0
};

// The window of galaxy view child 8 (GalaxyWindow::OpenType8Window, galaxy view message "battle"): the prompt that comes up
// when forces meet in a system. Four pages (tabs 0x6f-0x72 on the right: the summary, the Alliance forces, the Empire
// forces, and a second summary with the go-to-system buttons) and, on the force pages, a list of the participants of the
// kind chosen in the small tab control (0x65 ships, 0x66 fighters, 0x67 troops, 0x68 personnel). The buttons close the
// window and answer the battle's vote.
class BattleWindow : public CoolGameWindow
{
public:
  BattleWindow(HINSTANCE instance, CoolWindow* parent, uint32_t childId, BattleMessage* battle); // 0x46f140
  ~BattleWindow() override; // 0x46f280 (scalar deleting destructor 0x46f260)

  void PostCloseRequest() override; // slot 12, 0x4727a0
  void OnCreate1() override; // slot 14, 0x46f360
  void DrawOverlay(HDC dc) override; // slot 17, 0x471200
  void OnCommand(int a, int b, int c) override; // slot 18, 0x470ea0
  void OnKeyDown(WPARAM key, LPARAM lParam) override; // slot 19, 0x4710f0

  // Stops the advice sound the window started (0x417610 plays it, 0x470e70 stops it).
  void StopAdvice(); // 0x470e70
  // Shows page `page` (0x6f ... 0x72).
  void SelectPage(int page); // 0x471340
  // Rebuilds the participant list for the kind `kind` (0x65-0x68) unless it is the current one (`force` redoes it).
  void FillList(int kind, int force); // 0x4714e0
  // Adds one row to the list: the picture and name of `object` (alternate picture when `normal` is 0), or a heading
  // (`heading`) / a blank row when there is no object.
  void AddRow(GameObj* object, int normal, const std::string* heading); // 0x4724a0
  // Deletes every row of the list.
  void ClearRows(); // 0x472750

  GameObjNotifier* m_system; // +0x114 the system of the battle
  GalaxyWindow* m_galaxy; // +0x118
  int m_unk11c; // +0x11c
  CoolDragList* m_list; // +0x120 the participants
  CoolListItemList* m_items; // +0x124
  CoolLabel m_label; // +0x128
  CoolTabControl* m_pageTabs; // +0x15c the page tabs (child id 0x6e)
  CoolTabControl* m_kindTabs; // +0x160 the kinds of participant (child id 100)
  CoolStrobeButton* m_closeButton; // +0x164 child id 0xc9
  CoolStrobeButton* m_fleetButton; // +0x168 child id 0xca: go to the fleet
  CoolStrobeButton* m_systemButton; // +0x16c child id 0xcb: go to the system
  GameObjNotifier* m_fleetSystem; // +0x170 the system the fleet is in
  BattleMessage* m_battle; // +0x174 not owned
  HMODULE m_gokres; // +0x178 resource slot 10 (GOKRES.DLL)
  int m_page; // +0x17c
  int m_kind; // +0x180
  int m_nextItemId; // +0x184
  int m_advicePlaying; // +0x188
  int m_adviceId; // +0x18c
};

// What the battle list collects for one fleet of the battle: the participants of `kind` of its capital ships, in `shown`
// (and, for personnel that are not ready, in `held`) (0x472010).
void CollectBattleParticipants(GameObjNotifier* fleet, ObjectKeySet* shown, ObjectKeySet* held, int kind);
