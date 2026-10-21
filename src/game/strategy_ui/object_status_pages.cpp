#include <cstdint>
#include <string>

#include "app/app_cdcheck_version.h"
#include "app/enhancements.h"
#include "character/role.h"
#include "core/game_string.h"
#include "fleet/capship.h"
#include "fleet/fleet.h"
#include "gameobj/game_hooks.h"
#include "gameobj/game_obj.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/object_status_window.h"
#include "ui/cool_label.h"
#include "ui/cool_list_item.h"

#include "strategy_ui/object_status_helpers.h"

using namespace status_page;

namespace
{

void AddLabelS(std::string* text, uint16_t id, int row, CoolListItemList* labels)
{
  text->clear();
  SetText(text, id);
  Append(text, g_statusSeparator);
  AddStatusRow(row, text, labels);
}

} // namespace

void ObjectStatusWindow::FillFleetPage(CoolListItemList* labels, CoolListItemList* values)
{
  std::string text;
  SetText(&text, 0x8738);
  m_titleLabel.SetText(text.c_str());
  GameObjNotifier* fleet = m_object;
  AddNameRow(1, labels, values);
  SetText(&text, 0x8666);
  int row = 2;
  AddStatusRow(2, &text, labels);
  text.clear();
  const uint32_t state = static_cast<GameObj*>(m_object)->m_stateFlags;
  uint16_t stateId;
  if ((state & 0x10) == 0) {
    if ((state & 4) == 0) {
      stateId = 0x8757;
    } else if ((state & 0x20) != 0) {
      stateId = 0x8744;
    } else {
      stateId = 0x8748;
    }
  } else {
    stateId = 0x8744;
  }
  SetText(&text, stateId);
  AddStatusRow(2, &text, values);

  StatusCode status(1, static_cast<uint32_t>(-1));
  int ruleValue = 0;
  QueryRuleValueB(static_cast<GameObj*>(m_object), &status, &ruleValue);
  if (status.m_second == static_cast<uint32_t>(-1) && ruleValue != 0) {
    const ResourceStringId ruleLabelId = StringId(0x3814);
    std::string ruleLabel = LoadStringResource(&ruleLabelId);
    text.clear();
    {
      const ResourceStringId id = StringId(0x8766);
      std::string ruleName = LoadStringResource(&id);
      std::string ruleNameCopy(ruleName.c_str());
      text += ruleNameCopy;
    }
    row = 3;
    AddStatusRow(3, &text, labels);
    text.clear();
    {
      std::string labelCopy(ruleLabel.c_str());
      text += labelCopy;
      text.append(std::to_string(static_cast<long>(ruleValue)));
    }
    AddStatusRow(3, &text, values);
  }

  static constexpr struct {
    uint16_t m_labelId;
    int m_shipNumber;
  } kShipRows[] = {{0x3046, 2}, {0x3048, 3}, {0x3044, 1}};
  for (int i = 0; i < 3; ++i) {
    text.clear();
    SetText(&text, kShipRows[i].m_labelId);
    Append(&text, g_statusSeparator);
    AddStatusRow(row + 1 + i, &text, labels);
    text.clear();
    GameObjNotifier* ship = nullptr;
    static_cast<Fleet*>(fleet)->FindShipByNumber(kShipRows[i].m_shipNumber, &ship);
    if (!ship) {
      SetText(&text, 0x8762);
    } else {
      std::string name(static_cast<GameObj*>(ship)->GetName()->c_str());
      text += name;
    }
    AddStatusRow(row + 1 + i, &text, values);
  }
  SetText(&text, 0x8740);
  AddStatusRow(row + 4, &text, labels);
  text.clear();
  const uint32_t shipCount
      = CountCapitalShipChildren(static_cast<GameObj*>(fleet), (static_cast<GameObjRoot*>(fleet)->m_flags >> 6) & 3, 3);
  text.append(std::to_string(static_cast<long>(shipCount)));
  AddStatusRow(row + 4, &text, values);

  int damaged = 0;
  int fighterCapacity = 0;
  int troopCapacity = 0;
  int availableFighterCapacity = 0;
  int availableTroopCapacity = 0;
  int fighters = 0;
  int troops = 0;
  int roles = 0;
  CapShipIterator ships(fleet, 3);
  for (ships.Begin(); ships.Current(); ships.Advance()) {
    auto* ship = static_cast<CapShip*>(ships.Current());
    if (ship->m_stateFlags & 0x200) {
      ++damaged;
    }
    fighterCapacity += ship->GetFighterCapacity();
    troopCapacity += ship->GetTroopCapacity();
    const uint32_t shipState = ship->m_stateFlags;
    // Fleet::UpdateUsable considers a ship ready once it is completed and no longer in its enroute-active phase.
    // CreateProducts attaches queued ships to their deployment fleet before construction completes, so all children
    // counted above contribute to the assigned total while only completed, ready ships contribute to usable capacity.
    if ((shipState & 0x4) != 0 && (shipState & 0x20) == 0) {
      availableFighterCapacity += ship->GetFighterCapacity();
      availableTroopCapacity += ship->GetTroopCapacity();
    }
    fighters += ship->CountFighterSquadrons();
    troops += ship->CountTroopRegiments();
    RoleIterator shipRoles(ship, 3);
    roles += shipRoles.Count();
  }
  SetText(&text, 0x8704);
  AddStatusRow(row + 5, &text, labels);
  if (EnhancementsEnabled()) {
    text = "Ready / assigned";
    AddStatusRow(row + 5, &text, values);
    SetText(&text, 0x3820);
    Append(&text, g_statusSeparator);
    AddStatusRow(row + 6, &text, labels);
    text.clear();
    text.append(std::to_string(static_cast<long>(availableFighterCapacity)));
    text.append(" / ");
    text.append(std::to_string(static_cast<long>(fighterCapacity)));
    AddStatusRow(row + 6, &text, values);
    SetText(&text, 0x8705);
    Append(&text, g_statusSeparator);
    AddStatusRow(row + 7, &text, labels);
    text.clear();
    text.append(std::to_string(static_cast<long>(availableTroopCapacity)));
    text.append(" / ");
    text.append(std::to_string(static_cast<long>(troopCapacity)));
    AddStatusRow(row + 7, &text, values);
  } else {
    text.clear();
    Append(&text, g_emptyText);
    AddStatusRow(row + 5, &text, values);
    AddLabelS(&text, 0x3820, row + 6, labels);
    text.clear();
    text.append(std::to_string(static_cast<long>(fighterCapacity)));
    AddStatusRow(row + 6, &text, values);
    SetText(&text, 0x8705);
    AddStatusRow(row + 7, &text, labels);
    text.clear();
    text.append(std::to_string(static_cast<long>(troopCapacity)));
    AddStatusRow(row + 7, &text, values);
  }
  SetText(&text, 0x8706);
  AddStatusRow(row + 8, &text, labels);
  text.clear();
  Append(&text, g_emptyText);
  AddStatusRow(row + 8, &text, values);
  AddLabelS(&text, 0x3820, row + 9, labels);
  text.clear();
  text.append(std::to_string(static_cast<long>(fighters)));
  AddStatusRow(row + 9, &text, values);
  SetText(&text, 0x8705);
  AddStatusRow(row + 10, &text, labels);
  text.clear();
  text.append(std::to_string(static_cast<long>(troops)));
  AddStatusRow(row + 10, &text, values);
  SetText(&text, 0x8707);
  AddStatusRow(row + 11, &text, labels);
  text.clear();
  text.append(std::to_string(static_cast<long>(roles)));
  AddStatusRow(row + 11, &text, values);
  SetText(&text, 0x8741);
  AddStatusRow(row + 12, &text, labels);
  text.clear();
  text.append(std::to_string(static_cast<long>(damaged)));
  AddStatusRow(row + 12, &text, values);
  SetText(&text, 0x8653);
  AddStatusRow(row + 13, &text, labels);
  text.clear();
  auto* fleetObject = static_cast<GameObj*>(m_object);
  if (GetVersionCheckResult() == 0) {
    SetText(&text, fleetObject->GetTransitSpeed(1) == 0 ? 0x8755 : 0x8756);
  } else if (fleetObject->GetTransitSpeed(1) == 0) {
    Append(&text, g_noText);
  } else {
    Append(&text, g_yesText);
  }
  AddStatusRow(row + 13, &text, values);
}
