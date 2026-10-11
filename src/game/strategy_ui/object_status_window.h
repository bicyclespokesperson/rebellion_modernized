#pragma once
#include <cstdint>
#include <string>

#include "core/containers.h"
#include "core/game_string.h"
#include "gameobj/object_key.h"
#include "gameobj/status_code.h"
#include "platform/win32.h"
#include "ui/cool_game_window.h"
#include "ui/cool_label.h"

class CoolDragList;
class CoolListItemList;
class DatRecord;
class DibBitmap;
class GalaxyWindow;
class GameObjNotifier;

// The "Status" window of a game object (galaxy view child 0x1a): a picture of the object, its name and a list of
// label/value rows that depend on the object's family (one Fill...Page per family range). Opened from the object
// pop-up menu. With an unset key it shows the player's own side (FillAlliancePage / FillEmpirePage).
//
// Two CoolListItemLists (m_labels, m_values) are filled by the page: row n of each list is one line of the table,
// row ids are the line numbers. OnCreate1 lays them out (LayoutStatusLabels) and paints them into the backdrop of
// the list (DrawStatusLabels); m_list shows the m_items list, whose rows are the object's attached pictures.
class ObjectStatusWindow : public CoolGameWindow
{
public:
  ObjectStatusWindow(HINSTANCE instance, GalaxyWindow* parent, uint32_t childId, const ObjectKey* key); // 0x442d70
  ~ObjectStatusWindow() override; // 0x442f40 (scalar deleting destructor 0x442f20)

  LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override; // slot 5, 0x44b250
  void OnCreate1() override; // slot 14, 0x443130: backdrop, list, labels and the page
  void OnCreate3() override; // slot 16, 0x443020: encyclopedia and close buttons
  void DrawOverlay(HDC dc) override; // slot 17, 0x444370: the title and the name
  void OnCommand(int a, int b, int c) override; // slot 18, 0x4443a0
  void OnKeyDown(WPARAM key, LPARAM lParam) override; // slot 19, shared 0x4a7390 (FleetWindow::OnKeyDown)

  // Replaces m_items by a new, empty list sorted by id and gives it to m_list.
  void ReplaceItemList(); // part of 0x443130
  // The window of a side without an object: its backdrop's page (FillAlliancePage / FillEmpirePage).
  void ShowSidePage(HMODULE module, DibBitmap** picture); // part of 0x443130

  // Name label (m_nameLabel) of the object, or "" / the side's name strings without one. The three arguments
  // are ignored by the original.
  void AddNameRow(int unusedRow, CoolListItemList* labels, CoolListItemList* values); // 0x44a210
  // "Attached:" and the name of the parent object, each as row `row` of its list (the resource strings without the
  // version-check result).
  void AddAttachedRows(int row, CoolListItemList* labels, CoolListItemList* values); // 0x44a2e0
  // "Status:" and the object's state (strings 0x28744 busy, 0x28763, 0x28757).
  void AddStatusRows(int row, CoolListItemList* labels, CoolListItemList* values); // 0x44a470
  // std::string 0x28651 and the record's field 0x4c (fighters/troops/capital ships) or 0.
  void AddCombatValueRow(int row, CoolListItemList* labels, CoolListItemList* values); // 0x44a620
  // Counts the units of the window's side that start at `first` and follow it in key order within `range` into
  // `tally` (cleared first; one StatusTallyNode per record id, only units whose control side is the side of the
  // galaxy view and whose state flag 0x40 is set).
  void TallyUnitsByType(IdAvlTree* tally, GameObjNotifier* first, const FamilyRange* range); // 0x44a7a0
  // One picture item per node of the tally tree (a unit type and its count) appended to m_items.
  void AddTallyItems(AvlTree* tally); // 0x44aa20

  void FillFighterSquadronPage(CoolListItemList* labels, CoolListItemList* values); // 0x4444c0 families 0x1c-0x1f
  void FillFacilityPage(CoolListItemList* labels, CoolListItemList* values); // 0x444e20 families 0x20-0x27
  void FillTroopPage(CoolListItemList* labels, CoolListItemList* values); // 0x445280 families 0x10-0x13
  void FillSpecialForcesPage(CoolListItemList* labels, CoolListItemList* values); // 0x445780 families 0x3c-0x3f
  void FillMissionPage(CoolListItemList* labels, CoolListItemList* values); // 0x445c60 families 0x50-0x7f
  void FillSystemPage(CoolListItemList* labels, CoolListItemList* values); // 0x446250 families 0x90-0x97
  void FillProductionFacilityPage(CoolListItemList* labels, CoolListItemList* values); // 0x446c50 families 0x28-0x2f
  void FillCapitalShipPage(CoolListItemList* labels, CoolListItemList* values); // 0x446fd0 families 0x14-0x1b
  void FillCharacterPage(CoolListItemList* labels, CoolListItemList* values); // 0x4486f0 families 0x30-0x3b
  void FillFleetPage(CoolListItemList* labels, CoolListItemList* values); // 0x449200 families 0x08-0x0f
  void FillManufacturingPage(CoolListItemList* labels, CoolListItemList* values); // 0x44acd0 families 0xa0-0xaf
  void FillAlliancePage(); // 0x44b2d0 no object, Alliance
  void FillEmpirePage(); // 0x44bb70 no object, Empire

  GalaxyWindow* m_galaxy; // +0x114
  CoolDragList* m_list; // +0x118 child id 0x67: the attached pictures
  CoolListItemList* m_items; // +0x11c the rows of m_list (sort mode 4, then 1 once a page is shown)
  DibBitmap* m_unknown120; // +0x120 owned, never set
  ObjectKey m_key; // +0x124 the object shown (unset: the player's side)
  NullableKey m_typeKey; // +0x128 the object's record key, for the encyclopedia button
  GameObjNotifier* m_object; // +0x12c the object in the player's view, null without a key
  CoolLabel m_titleLabel; // +0x130 the heading drawn by DrawOverlay
  CoolLabel m_unusedLabel; // +0x164 constructed and destroyed, never used
  CoolLabel m_nameLabel; // +0x198 the object's name
  uint8_t m_unknown1cc[8]; // +0x1cc
  HMODULE m_gokres; // +0x1d4 GOKRES.DLL (resource slot 10): pictures of ships, troops, ...
};

// A row of the status table: item `id` with `text`, inserted in `list`. Callee pops its arguments.
void AddStatusRow(uint32_t id, const std::string* text, CoolListItemList* list); // 0x44b1d0

// Places the label items of `labels` (left column) and `values` (right column at x = 0x67) one row per pair,
// each row as high as the taller of the two (measured in `dc`); returns the total height.
int LayoutStatusLabels(HDC dc, CoolListItemList* labels, CoolListItemList* values); // 0x449e00
// Draws the rows laid out by LayoutStatusLabels into `dc`.
void DrawStatusLabels(HDC dc, CoolListItemList* labels, CoolListItemList* values); // 0x44a050

// One unit type of the picture list: how many units of the type exist (m_count) and their summed record field 0x4c
// (m_total); the tree key is the record id.
class StatusTallyNode : public IdNode
{
public:
  StatusTallyNode(); // inlined at 0x44a7a0
  ~StatusTallyNode() override; // 0x44a9d0 (scalar deleting destructor 0x44a9b0)

  int m_total; // +0x1c
  int m_count; // +0x20
  ObjectKey m_key; // +0x24 key of the first unit of the type
};
