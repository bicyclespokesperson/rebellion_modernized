#include <cstdint>
#include <string>

#include "character/character.h"
#include "data/dat_record.h"
#include "fleet/capship.h"
#include "gameobj/game_obj.h"
#include "strategy_ui/object_status_helpers.h"
#include "strategy_ui/object_status_window.h"
#include "ui/cool_label.h"
#include "ui/cool_list_item.h"

using namespace status_page;

namespace
{

uint32_t LeftOfGrid(CapShip* ship, uint32_t a, uint32_t b)
{
  const uint32_t lost = ship->GetDamageGridValue(a, b);
  const uint32_t total = ship->GetGridValue(a, b);
  return total - lost;
}

void AddGridRow(std::string* text,
                uint16_t labelId,
                int row,
                CapShip* ship,
                uint32_t a,
                uint32_t b,
                CoolListItemList* labels,
                CoolListItemList* values)
{
  AddLabelRow(text, labelId, row, labels);
  text->append(std::to_string(static_cast<unsigned long>(LeftOfGrid(ship, a, b))));
  AddStatusRow(row, text, values);
}

void AddEmptyRow(std::string* text, uint16_t labelId, int row, CoolListItemList* labels, CoolListItemList* values)
{
  AddLabelRow(text, labelId, row, labels);
  Append(text, g_emptyText);
  AddStatusRow(row, text, values);
}

void AddGridGroup(std::string* text,
                  int firstRow,
                  CapShip* ship,
                  uint32_t a,
                  CoolListItemList* labels,
                  CoolListItemList* values)
{
  AddGridRow(text, 0x8718, firstRow, ship, a, 0, labels, values);
  AddGridRow(text, 0x8661, firstRow + 1, ship, a, 1, labels, values);
  AddGridRow(text, 0x8719, firstRow + 2, ship, a, 2, labels, values);
}

} // namespace

void ObjectStatusWindow::FillCapitalShipPage(CoolListItemList* labels, CoolListItemList* values)
{
  std::string text;
  SetText(&text, 0x8702);
  m_titleLabel.SetText(text.c_str());
  std::string unusedText;
  CapShip* ship = static_cast<CapShip*>(m_object);
  AddNameRow(1, labels, values);

  text.clear();
  SetText(&text, 0x8803);
  AddStatusRow(2, &text, labels);
  text.clear();
  {
    std::string recordName;
    ship->m_record->GetNameCopy(&recordName);
    Append(&text, recordName.c_str());
  }
  AddStatusRow(2, &text, values);
  text.clear();
  SetText(&text, 0x8703);
  AddStatusRow(3, &text, labels);
  text.clear();
  Append(&text, static_cast<GameObj*>(static_cast<GameObjRoot*>(m_object)->m_parent)->GetName()->c_str());
  AddStatusRow(3, &text, values);

  SetText(&text, 0x8666);
  AddStatusRow(4, &text, labels);
  text.clear();
  const uint32_t state = ship->m_stateFlags;
  SetText(&text, (state & 0x10) != 0 ? 0x8744 : ((state & 4) != 0 ? 0x8748 : 0x8757));
  AddStatusRow(4, &text, values);
  const int row = AddRuleValueRows(&text, m_object, 5, labels, values);

  AddLabelRow(&text, 0x8651, row + 1, labels);
  text.append(std::to_string(static_cast<unsigned long>(ship->GetRecordField4c())));
  AddStatusRow(row + 1, &text, values);
  AddEmptyRow(&text, 0x8704, row + 2, labels, values);

  AddLabelRow(&text, 0x8699, row + 3, labels);
  text.append(std::to_string(static_cast<unsigned long>(ship->GetFighterCapacity())));
  AddStatusRow(row + 3, &text, values);
  AddLabelRow(&text, 0x8705, row + 4, labels);
  text.append(std::to_string(static_cast<unsigned long>(ship->GetTroopCapacity())));
  AddStatusRow(row + 4, &text, values);
  AddEmptyRow(&text, 0x8706, row + 5, labels, values);
  AddLabelRow(&text, 0x8699, row + 6, labels);
  text.append(std::to_string(static_cast<unsigned long>(ship->CountFighterSquadrons())));
  AddStatusRow(row + 6, &text, values);
  AddLabelRow(&text, 0x8705, row + 7, labels);
  text.append(std::to_string(static_cast<unsigned long>(ship->CountTroopRegiments())));
  AddStatusRow(row + 7, &text, values);

  AddLabelRow(&text, 0x8707, row + 8, labels);
  {
    CharacterIterator characters(ship, 3);
    text.append(std::to_string(static_cast<long>(characters.Count())));
    AddStatusRow(row + 8, &text, values);
  }
  AddLabelRow(&text, 0x8708, row + 9, labels);
  SetText(&text, YesNoId((state & 0x200) != 0 ? 1 : 0));
  AddStatusRow(row + 9, &text, values);

  AddLabelRow(&text, 0x8709, row + 10, labels);
  {
    const int lost = ship->GetDamageField6c();
    const int total = ship->GetField6c();
    int left = total - lost;
    if (left == 0)
    {
      const int lost70 = ship->GetDamageField70();
      const int total70 = ship->GetField70();
      left = total70 - lost70;
    }
    text.append(std::to_string(static_cast<unsigned long>(left)));
    Append(&text, g_statusSeparator);
    text.append(std::to_string(static_cast<long>(ship->GetField6c())));
  }
  AddStatusRow(row + 10, &text, values);

  AddLabelRow(&text, 0x8710, row + 11, labels);
  {
    const uint32_t lost = ship->GetHullValueDamage();
    const uint32_t hull = ship->GetHull();
    AppendLeftOfTotal(&text, hull - lost, ship->GetHull());
  }
  AddStatusRow(row + 11, &text, values);

  AddLabelRow(&text, 0x8711, row + 12, labels);
  {
    const uint32_t lost = ship->GetDamageControlLoss();
    const uint32_t control = ship->GetDamageControl();
    text.append(std::to_string(static_cast<unsigned long>(control - lost)));
  }
  AddStatusRow(row + 12, &text, values);

  AddLabelRow(&text, 0x8712, row + 13, labels);
  {
    const uint32_t lost = ship->GetShieldRechargeRateLoss();
    const uint32_t rate = ship->GetShieldRechargeRate();
    AppendLeftOfTotal(&text, rate - lost, ship->GetShieldRechargeRate());
  }
  AddStatusRow(row + 13, &text, values);

  AddLabelRow(&text, 0x8654, row + 14, labels);
  text.append(std::to_string(static_cast<unsigned long>(ship->GetField60())));
  AddStatusRow(row + 14, &text, values);

  AddLabelRow(&text, 0x8713, row + 15, labels);
  {
    const uint32_t lost = ship->GetTractorBeamPowerLoss();
    const uint32_t power = ship->GetTractorBeamPower();
    AppendLeftOfTotal(&text, power - lost, ship->GetTractorBeamPower());
  }
  AddStatusRow(row + 15, &text, values);

  AddLabelRow(&text, 0x8714, row + 16, labels);
  {
    const uint32_t lost = ship->GetDamageField64();
    const uint32_t total = ship->GetField64();
    AppendLeftOfTotal(&text, total - lost, ship->GetField64());
  }
  AddStatusRow(row + 16, &text, values);

  AddLabelRow(&text, 0x8656, row + 17, labels);
  {
    const uint32_t lost = ship->GetDamageField68();
    const uint32_t total = ship->GetField68();
    text.append(std::to_string(static_cast<unsigned long>(total - lost)));
  }
  AddStatusRow(row + 17, &text, values);

  AddLabelRow(&text, 0x8657, row + 18, labels);
  {
    const uint32_t first = ship->GetRecordField5c();
    const uint32_t second = ship->Virtual115();
    text.append(std::to_string(static_cast<unsigned long>(first - second)));
  }
  AddStatusRow(row + 18, &text, values);

  AddLabelRow(&text, 0x8715, row + 19, labels);
  {
    const uint32_t lost = ship->GetWeaponRechargeRateLoss();
    const uint32_t rate = ship->GetWeaponRechargeRate();
    AppendLeftOfTotal(&text, rate - lost, ship->GetWeaponRechargeRate());
  }
  AddStatusRow(row + 19, &text, values);

  AddLabelRow(&text, 0x8716, row + 20, labels);
  {
    const uint32_t lost = ship->GetBombardmentModifierLoss();
    const uint32_t modifier = ship->GetBombardmentModifier();
    text.append(std::to_string(static_cast<unsigned long>(modifier - lost)));
  }
  AddStatusRow(row + 20, &text, values);

  AddEmptyRow(&text, 0x8717, row + 21, labels, values);
  AddGridGroup(&text, row + 22, ship, 0, labels, values);
  AddEmptyRow(&text, 0x8720, row + 25, labels, values);
  AddGridGroup(&text, row + 26, ship, 1, labels, values);
  AddEmptyRow(&text, 0x8721, row + 29, labels, values);
  AddGridGroup(&text, row + 30, ship, 3, labels, values);
  AddEmptyRow(&text, 0x8722, row + 33, labels, values);
  AddGridGroup(&text, row + 34, ship, 2, labels, values);
}
