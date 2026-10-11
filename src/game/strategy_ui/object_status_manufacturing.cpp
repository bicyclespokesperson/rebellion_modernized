#include <cstdint>
#include <string>

#include "core/game_string.h"
#include "economy/manu_mgr.h"
#include "economy/manufacturing_facility.h"
#include "gameobj/game_hooks.h"
#include "gameobj/game_obj.h"
#include "strategy_ui/object_status_helpers.h"
#include "strategy_ui/object_status_window.h"
#include "ui/cool_label.h"
#include "ui/cool_list_item.h"

using namespace status_page;

namespace
{

constexpr uint32_t kTitleDrawFlags = 0x11;
constexpr int kTitleHeight = 0x24;

struct ManufacturingKind
{
  uint32_t m_firstFamily;
  uint32_t m_endFamily;
  uint16_t m_titleId;
  int (*m_countFacilities)(GameObj* parent, uint32_t side, uint32_t filter);
  int m_isTroopTraining;
};

constexpr ManufacturingKind kKinds[] = {
  {0xa0, 0xa2, 0x1833, CountManufacturingFacility2AChildren, 0},
  {0xa2, 0xa4, 0x1829, CountManufacturingFacility28Children, 0},
  {0xa4, 0xa6, 0x1831, CountManufacturingFacility29Children, 1},
};

} // namespace

void ObjectStatusWindow::FillManufacturingPage(CoolListItemList* labels, CoolListItemList* values)
{
  std::string text;
  std::string unusedText;
  ManuMgr* manager = static_cast<ManuMgr*>(m_object);
  GameObj* parent = static_cast<GameObj*>(static_cast<GameObjRoot*>(m_object)->m_parent);
  int facilityCount = 0;
  int isTroopTraining = 0;
  for (const ManufacturingKind& kind : kKinds)
  {
    const uint32_t family = m_key.Family();
    if (family < kind.m_firstFamily || family >= kind.m_endFamily)
      continue;
    const ResourceStringId titleId = ResourceStringId{static_cast<int16_t>(kind.m_titleId), 2};
    text = LoadStringResource(&titleId);
    facilityCount = kind.m_countFacilities(parent, (static_cast<GameObjRoot*>(parent)->m_flags >> 6) & 3, 3);
    isTroopTraining = kind.m_isTroopTraining;
    break;
  }

  m_titleLabel.m_drawFlags = kTitleDrawFlags;
  m_titleLabel.m_height = kTitleHeight;
  m_titleLabel.MoveTo(0xf, 0x12);
  m_titleLabel.SetText(text.c_str());

  AddNameRow(1, labels, values);
  AddLabelRow(&text, 0x8664, 2, labels);
  Append(&text, static_cast<GameObj*>(static_cast<GameObjRoot*>(m_object)->m_parent)->GetName()->c_str());
  AddStatusRow(2, &text, values);
  text.clear();
  SetText(&text, 0x8666);
  AddStatusRow(3, &text, labels);
  text.clear();

  if (facilityCount == 0)
  {
    SetText(&text, 0x8772);
    AddStatusRow(3, &text, values);
  }
  else if (manager->m_remainingGameObjCount == 0)
  {
    SetText(&text, 0x8769);
    AddStatusRow(3, &text, values);
  }
  else
  {
    SetText(&text, isTroopTraining == 0 ? 0x8770 : 0x8771);
    AddStatusRow(3, &text, values);
    text.clear();
    SetText(&text, 0x8768);
    AddStatusRow(4, &text, labels);
    text.clear();
    text.append(std::to_string(static_cast<long>(manager->m_remainingGameObjCount)));
    AddStatusRow(4, &text, values);
    const bool sameSide = ((static_cast<GameObjRoot*>(parent)->m_flags ^ manager->m_flags) & 0xc0) == 0;
    if (manager->m_remainingGameObjCount != 0 && sameSide)
    {
      text.clear();
      SetText(&text, 0x8765);
      AddStatusRow(5, &text, labels);
      StatusCode status(1, static_cast<uint32_t>(-1));
      int ruleValue = 0;
      QueryRuleValueA(m_object, &status, &ruleValue);
      text.clear();
      text.append(std::to_string(static_cast<long>(ruleValue)));
      AddStatusRow(5, &text, values);
    }
  }
}
