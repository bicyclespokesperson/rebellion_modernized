#include <cstdint>

#include "character/character.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "mission/mission.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/object_status_helpers.h"
#include "strategy_ui/object_status_window.h"
#include "ui/cool_label.h"
#include "ui/cool_list_item.h"
#include "ui/object_list_item.h"

using namespace status_page;

namespace {

uint16_t ForceRankId(uint16_t force) {
    if (force < 10) return 0x8754;
    if (force < 0x14) return 0x8749;
    if (force < 0x50) return 0x8750;
    if (force < 100) return 0x8751;
    if (force < 0x78) return 0x8752;
    return 0x8753;
}

}  // namespace

void ObjectStatusWindow::FillCharacterPage(CoolListItemList* labels, CoolListItemList* values) {
    String text;
    SetText(&text, 0x8723);
    m_titleLabel.SetText(text.m_text);
    String unusedText;
    Character* character = static_cast<Character*>(m_object);
    AddNameRow(1, labels, values);
    AddLabelRow(&text, 0x8724, 2, labels);
    if (GameObjNotifier* commanded = GetObjectInFamilies_01_ff(m_galaxy->m_side, &character->m_commandingKey)) {
        Append(&text, static_cast<GameObj*>(commanded)->GetName()->m_text);
    } else {
        SetText(&text, 0x8754);
    }
    AddStatusRow(2, &text, values);
    AddAttachedRows(3, labels, values);
    AddLabelRow(&text, 0x8666, 4, labels);

    const ObjectKey unusedKey;
    ObjectKey characterKey;
    static_cast<GameObjRoot*>(m_object)->GetKey(&characterKey);
    GameObjNotifier* mission = GetObjectInFamilies_40_80(m_galaxy->m_side, &characterKey);
    SetText(&text, 0x8748);
    if ((character->m_stateFlags & 0x10) != 0) {
        SetText(&text, 0x8744);
    } else if ((character->m_flagsAc & 1) != 0) {
        SetText(&text, 0x8746);
    } else if (character->m_injury != 0) {
        SetText(&text, 0x8747);
    } else if (mission && static_cast<Mission*>(mission)->GetRecordHidden() == 0) {
        SetText(&text, 0x8745);
    }
    AddStatusRow(4, &text, values);
    const int row = AddRuleValueRows(&text, m_object, 5, labels, values);

    AddLabelRow(&text, 0x8725, row + 1, labels);
    SetText(&text, ForceRankId(static_cast<uint16_t>(character->m_force)));
    AddStatusRow(row + 1, &text, values);

    AddLabelRow(&text, 0x8726, row + 2, labels);
    text.AppendUnsigned(character->GetDiplomacy());
    AddStatusRow(row + 2, &text, values);
    AddLabelRow(&text, 0x8727, row + 3, labels);
    text.AppendUnsigned(character->GetEspionage());
    AddStatusRow(row + 3, &text, values);
    AddLabelRow(&text, 0x8728, row + 4, labels);
    text.AppendUnsigned(character->GetCombat());
    AddStatusRow(row + 4, &text, values);
    AddLabelRow(&text, 0x8729, row + 5, labels);
    text.AppendUnsigned(character->GetLeadership());
    AddStatusRow(row + 5, &text, values);

    AddLabelRow(&text, 0x8730, row + 6, labels);
    Append(&text, g_spaceString);
    AddStatusRow(row + 6, &text, values);

    AddLabelRowKeepText(&text, 0x8731, row + 7, labels);
    text.Clear();
    SetText(&text, YesNoId(character->GetShipyardRD()));
    AddStatusRow(row + 7, &text, values);
    AddLabelRowKeepText(&text, 0x8732, row + 8, labels);
    SetText(&text, YesNoId(character->GetTrainingFacilRD()));
    AddStatusRow(row + 8, &text, values);
    AddLabelRowKeepText(&text, 0x8733, row + 9, labels);
    SetText(&text, YesNoId(character->GetConstructionYardRD()));
    AddStatusRow(row + 9, &text, values);

    AddLabelRowKeepText(&text, 0x8737, row + 10, labels);
    text.Clear();
    Append(&text, g_spaceString);
    AddStatusRow(row + 10, &text, values);

    AddLabelRowKeepText(&text, 0x8683, row + 11, labels);
    SetText(&text, YesNoId(character->RecordCanBeAdmiral()));
    AddStatusRow(row + 11, &text, values);
    AddLabelRow(&text, 0x8681, row + 12, labels);
    SetText(&text, YesNoId(character->RecordCanBeGeneral()));
    AddStatusRow(row + 12, &text, values);
    AddLabelRow(&text, 0x8682, row + 13, labels);
    SetText(&text, YesNoId(character->RecordCanBeCommander()));
    AddStatusRow(row + 13, &text, values);
}
