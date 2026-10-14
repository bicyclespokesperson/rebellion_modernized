#pragma once
#include <cstdint>
#include <string>

#include "core/game_string.h"
#include "gameobj/game_hooks.h"
#include "gameobj/game_obj.h"
#include "strategy_ui/object_status_window.h"

extern const char* g_statusSeparator; // 0x6a872c ":"
extern const char* g_noText; // 0x6a8730 "No"
extern const char* g_yesText; // 0x6a8734 "Yes"
extern const char* g_emptyText; // 0x6b120c
extern const char* g_spaceString; // 0x6a7f40 " "
extern const int16_t g_stringGroup65d424; // 0x65d424 group of every localised string of the window

namespace status_page
{

inline ResourceStringId StringId(uint16_t id)
{
  return ResourceStringId{static_cast<int16_t>(id), g_stringGroup65d424};
}

inline void SetText(std::string* text, uint16_t id)
{
  const ResourceStringId resource = StringId(id);
  *(text) = LoadStringResource(&resource);
}

// text += `literal` (the original builds a temporary std::string for it).
inline void Append(std::string* text, const char* literal)
{
  std::string part(literal);
  *text += part;
}

// Sets `text` to the string `id`, adds it as the label of `row` and clears it for the value.
inline void AddLabelRow(std::string* text, uint16_t id, int row, CoolListItemList* labels)
{
  SetText(text, id);
  AddStatusRow(row, text, labels);
  text->clear();
}

// Like AddLabelRow, but leaves the label in `text` (the following SetText replaces it).
inline void AddLabelRowKeepText(std::string* text, uint16_t id, int row, CoolListItemList* labels)
{
  SetText(text, id);
  AddStatusRow(row, text, labels);
}

inline uint16_t YesNoId(int value)
{
  return value == 0 ? 0x8755 : 0x8756;
}

// text += "<left>:<total>"
inline void AppendLeftOfTotal(std::string* text, uint32_t left, uint32_t total)
{
  text->append(std::to_string(static_cast<unsigned long>(left)));
  Append(text, g_statusSeparator);
  text->append(std::to_string(static_cast<unsigned long>(total)));
}

// The optional "rule value" rows (a GNPRTB-style value of the object, when the rule lookup succeeds). `nextRow` is the
// row it would occupy; returns the row of the last row added (`nextRow - 1` when none).
inline int AddRuleValueRows(std::string* text,
                            GameObjNotifier* object,
                            int nextRow,
                            CoolListItemList* labels,
                            CoolListItemList* values)
{
  StatusCode status(1, static_cast<uint32_t>(-1));
  int ruleValue = 0;
  QueryRuleValueB(static_cast<GameObj*>(object), &status, &ruleValue);
  if (status.m_second != static_cast<uint32_t>(-1) || ruleValue == 0)
    return nextRow - 1;
  const ResourceStringId ruleLabelId = StringId(0x3814);
  std::string ruleLabel = LoadStringResource(&ruleLabelId);
  text->clear();
  {
    const ResourceStringId id = StringId(0x8766);
    std::string ruleName = LoadStringResource(&id);
    std::string ruleNameCopy(ruleName.c_str());
    *text += ruleNameCopy;
  }
  AddStatusRow(nextRow, text, labels);
  text->clear();
  {
    std::string labelCopy(ruleLabel.c_str());
    *text += labelCopy;
    text->append(std::to_string(static_cast<long>(ruleValue)));
  }
  AddStatusRow(nextRow, text, values);
  text->clear();
  return nextRow;
}

} // namespace status_page
