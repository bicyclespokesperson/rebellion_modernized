#pragma once
#include <cstdint>

#include "core/game_string.h"
#include "gameobj/game_hooks.h"
#include "gameobj/game_obj.h"
#include "strategy_ui/object_status_window.h"

extern const char g_statusSeparator[];     // 0x6a872c ":"
extern const char g_noText[];              // 0x6a8730 "No"
extern const char g_yesText[];             // 0x6a8734 "Yes"
extern const char g_emptyText[];           // 0x6b120c
extern const char g_spaceString[];         // 0x6a7f40 " "
extern const int16_t g_stringGroup65d424;  // 0x65d424 group of every localised string of the window

namespace status_page {

inline ResourceStringId StringId(uint16_t id) {
    return ResourceStringId{static_cast<int16_t>(id), g_stringGroup65d424};
}

inline void SetText(String* text, uint16_t id) {
    const ResourceStringId resource = StringId(id);
    text->SetFromResource(&resource);
}

// text += `literal` (the original builds a temporary String for it).
inline void Append(String* text, const char* literal) {
    String part(literal);
    *text += part;
}

// Sets `text` to the string `id`, adds it as the label of `row` and clears it for the value.
inline void AddLabelRow(String* text, uint16_t id, int row, CoolListItemList* labels) {
    SetText(text, id);
    AddStatusRow(row, text, labels);
    text->Clear();
}

// Like AddLabelRow, but leaves the label in `text` (the following SetText replaces it).
inline void AddLabelRowKeepText(String* text, uint16_t id, int row, CoolListItemList* labels) {
    SetText(text, id);
    AddStatusRow(row, text, labels);
}

inline uint16_t YesNoId(int value) { return value == 0 ? 0x8755 : 0x8756; }

// text += "<left>:<total>"
inline void AppendLeftOfTotal(String* text, uint32_t left, uint32_t total) {
    text->AppendUnsigned(left);
    Append(text, g_statusSeparator);
    text->AppendUnsigned(total);
}

// The optional "rule value" rows (a GNPRTB-style value of the object, when the rule lookup succeeds). `nextRow` is the
// row it would occupy; returns the row of the last row added (`nextRow - 1` when none).
inline int AddRuleValueRows(String* text, GameObjNotifier* object, int nextRow, CoolListItemList* labels,
                            CoolListItemList* values) {
    StatusCode status(1, static_cast<uint32_t>(-1));
    int ruleValue = 0;
    QueryRuleValueB(reinterpret_cast<int>(object), reinterpret_cast<int>(&status), &ruleValue);
    if (status.m_second != static_cast<uint32_t>(-1) || ruleValue == 0) return nextRow - 1;
    const ResourceStringId ruleLabelId = StringId(0x3814);
    String ruleLabel(&ruleLabelId);
    text->Clear();
    {
        const ResourceStringId id = StringId(0x8766);
        String ruleName(&id);
        String ruleNameCopy(ruleName.m_text);
        *text += ruleNameCopy;
    }
    AddStatusRow(nextRow, text, labels);
    text->Clear();
    {
        String labelCopy(ruleLabel.m_text);
        *text += labelCopy;
        text->AppendSigned(ruleValue);
    }
    AddStatusRow(nextRow, text, values);
    text->Clear();
    return nextRow;
}

}  // namespace status_page
