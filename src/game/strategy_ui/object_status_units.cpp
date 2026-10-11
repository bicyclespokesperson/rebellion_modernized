#include <string>
#include <cstdint>

#include "ai/ai_common.h"
#include "app/app_cdcheck_version.h"
#include "character/special_forces.h"
#include "economy/defense_facility.h"
#include "economy/manufacturing_facility.h"
#include "economy/production_facility_types.h"
#include "fleet/fight_squad.h"
#include "fleet/troop_reg.h"
#include "galaxy/system.h"
#include "gameobj/game_hooks.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "mission/mission.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/object_status_helpers.h"
#include "strategy_ui/object_status_window.h"
#include "ui/cool_label.h"
#include "ui/cool_list_item.h"

using namespace status_page;


void ObjectStatusWindow::FillFighterSquadronPage(CoolListItemList* labels, CoolListItemList* values) {
    std::string text;
    SetText(&text, 0x8650);
    m_titleLabel.SetText(text.c_str());
    AddNameRow(1, labels, values);
    int row = 2;
    AddAttachedRows(2, labels, values);

    row = AddRuleValueRows(&text, m_object, row + 1, labels, values);

    FightSquad* squad = static_cast<FightSquad*>(m_object);
    AddLabelRow(&text, 0x8651, row + 1, labels);
    text.append(std::to_string(static_cast<unsigned long>(squad->GetRecordField4c())));
    AddStatusRow(row + 1, &text, values);

    AddLabelRow(&text, 0x8652, row + 2, labels);
    {
        const uint32_t lost = squad->GetSquadSizeDamage();
        const uint32_t size = squad->GetSquadSize();
        AppendLeftOfTotal(&text, size - lost, squad->GetSquadSize());
    }
    AddStatusRow(row + 2, &text, values);

    AddLabelRow(&text, 0x8653, row + 3, labels);
    {
        const uint32_t lost = squad->GetDamageField6c();
        const uint32_t total = squad->GetField6c();
        text.append(std::to_string(static_cast<long>(total - lost)));
    }
    AddStatusRow(row + 3, &text, values);

    AddLabelRow(&text, 0x8654, row + 4, labels);
    {
        const uint32_t lost = squad->GetDamageField60();
        const uint32_t total = squad->GetField60();
        AppendLeftOfTotal(&text, total - lost, squad->GetField60());
    }
    AddStatusRow(row + 4, &text, values);

    AddLabelRow(&text, 0x8655, row + 5, labels);
    {
        const uint32_t lost = squad->GetDamageField64();
        const uint32_t total = squad->GetField64();
        text.append(std::to_string(static_cast<unsigned long>(total - lost)));
    }
    AddStatusRow(row + 5, &text, values);

    AddLabelRow(&text, 0x8656, row + 6, labels);
    {
        const uint32_t lost = squad->GetDamageField68();
        const uint32_t total = squad->GetField68();
        text.append(std::to_string(static_cast<long>(total - lost)));
    }
    AddStatusRow(row + 6, &text, values);

    AddLabelRow(&text, 0x8657, row + 7, labels);
    {
        const uint32_t first = squad->GetRecordField5c();
        const uint32_t second = squad->Virtual115();
        text.append(std::to_string(static_cast<unsigned long>(first - second)));
    }
    AddStatusRow(row + 7, &text, values);

    AddLabelRow(&text, 0x8658, row + 8, labels);
    {
        const uint32_t lost = squad->GetBombardmentDefenseLost();
        const uint32_t total = squad->GetBombardmentDefense();
        AppendLeftOfTotal(&text, total - lost, squad->GetBombardmentDefense());
    }
    AddStatusRow(row + 8, &text, values);

    AddLabelRow(&text, 0x8659, row + 9, labels);
    Append(&text, g_emptyText);
    AddStatusRow(row + 9, &text, values);

    AddLabelRow(&text, 0x8660, row + 10, labels);
    {
        const uint32_t lost = squad->GetDamageColumnTotal(2);
        const uint32_t total = squad->GetGridColumnTotal(2);
        AppendLeftOfTotal(&text, total - lost, squad->GetGridColumnTotal(2));
    }
    AddStatusRow(row + 10, &text, values);

    AddLabelRow(&text, 0x8661, row + 11, labels);
    {
        const uint32_t lost = squad->GetDamageColumnTotal(1);
        const uint32_t total = squad->GetGridColumnTotal(1);
        AppendLeftOfTotal(&text, total - lost, squad->GetGridColumnTotal(1));
    }
    AddStatusRow(row + 11, &text, values);

    AddLabelRow(&text, 0x8662, row + 12, labels);
    {
        const uint32_t lost = squad->GetTorpedoesLost();
        const uint32_t total = squad->GetTotalTorpedoes();
        AppendLeftOfTotal(&text, total - lost, squad->GetTotalTorpedoes());
    }
    AddStatusRow(row + 12, &text, values);
}

void ObjectStatusWindow::FillFacilityPage(CoolListItemList* labels, CoolListItemList* values) {
    std::string text;
    SetText(&text, 0x8663);
    m_titleLabel.SetText(text.c_str());
    int row = 1;
    AddNameRow(1, labels, values);
    row = AddRuleValueRows(&text, m_object, row + 1, labels, values);

    AddLabelRow(&text, 0x8664, row + 1, labels);
    Append(&text, static_cast<GameObj*>(static_cast<GameObjRoot*>(m_object)->m_parent)->GetName()->c_str());
    AddStatusRow(row + 1, &text, values);
    AddStatusRows(row + 2, labels, values);
    int last = row + 3;
    AddCombatValueRow(last, labels, values);

    DefenseFacility* facility = static_cast<DefenseFacility*>(m_object);
    const uint32_t family = m_key.Family();
    if (family >= 0x22 && family < 0x28) {
        AddLabelRow(&text, 0x8659, row + 4, labels);
        {
            const uint32_t subtrahend = facility->Virtual117();
            const uint32_t attack = facility->GetAttackStrength();
            text.append(std::to_string(static_cast<unsigned long>(attack - subtrahend)));
        }
        AddStatusRow(row + 4, &text, values);
        last = row + 5;
        AddLabelRow(&text, 0x8743, last, labels);
        {
            const uint32_t shield = facility->GetShieldStrength();
            const uint32_t subtrahend = facility->Virtual118();
            text.append(std::to_string(static_cast<unsigned long>(shield - subtrahend)));
        }
        AddStatusRow(last, &text, values);
    }
    AddLabelRow(&text, 0x8669, last + 1, labels);
    {
        const uint32_t subtrahend = facility->Virtual113();
        const uint32_t field58 = facility->GetRecordField58();
        text.append(std::to_string(static_cast<unsigned long>(field58 - subtrahend)));
    }
    AddStatusRow(last + 1, &text, values);
}

void ObjectStatusWindow::FillTroopPage(CoolListItemList* labels, CoolListItemList* values) {
    std::string text;
    SetText(&text, 0x8665);
    m_titleLabel.SetText(text.c_str());
    AddNameRow(1, labels, values);
    AddAttachedRows(2, labels, values);
    AddLabelRow(&text, 0x8666, 3, labels);
    const uint32_t state = static_cast<GameObj*>(m_object)->m_stateFlags;
    uint16_t stateId;
    if ((state & 0x10) != 0) {
        stateId = 0x8744;
    } else if ((state & 4) != 0) {
        stateId = 0x8748;
    } else {
        stateId = 0x8771;
    }
    SetText(&text, stateId);
    AddStatusRow(3, &text, values);
    const int row = AddRuleValueRows(&text, m_object, 4, labels, values);

    TroopReg* troop = static_cast<TroopReg*>(m_object);
    AddLabelRow(&text, 0x8651, row + 1, labels);
    text.append(std::to_string(static_cast<unsigned long>(troop->GetRecordField4c())));
    AddStatusRow(row + 1, &text, values);

    AddLabelRow(&text, 0x8667, row + 2, labels);
    {
        const uint32_t subtrahend = troop->Virtual122();
        const uint32_t attack = troop->GetAttackStrength();
        text.append(std::to_string(static_cast<unsigned long>(attack - subtrahend)));
    }
    AddStatusRow(row + 2, &text, values);

    AddLabelRow(&text, 0x8668, row + 3, labels);
    {
        const uint32_t defense = troop->GetDefenseStrength();
        const uint32_t subtrahend = troop->Virtual123();
        text.append(std::to_string(static_cast<unsigned long>(defense - subtrahend)));
    }
    AddStatusRow(row + 3, &text, values);

    AddLabelRow(&text, 0x8658, row + 4, labels);
    {
        const uint32_t subtrahend = troop->Virtual121();
        const uint32_t bombardment = troop->GetBombardmentDefense();
        text.append(std::to_string(static_cast<unsigned long>(bombardment - subtrahend)));
    }
    AddStatusRow(row + 4, &text, values);

    AddLabelRow(&text, 0x8671, row + 5, labels);
    {
        const uint32_t field5c = troop->GetRecordField5c();
        const uint32_t subtrahend = troop->Virtual115();
        text.append(std::to_string(static_cast<unsigned long>(field5c - subtrahend)));
    }
    AddStatusRow(row + 5, &text, values);
}

void ObjectStatusWindow::FillSpecialForcesPage(CoolListItemList* labels, CoolListItemList* values) {
    std::string text;
    SetText(&text, 0x8670);
    m_titleLabel.SetText(text.c_str());
    SpecialForces* forces = static_cast<SpecialForces*>(m_object);
    AddNameRow(1, labels, values);
    AddAttachedRows(2, labels, values);
    AddLabelRow(&text, 0x8666, 3, labels);
    const ObjectKey unset;
    uint16_t stateId;
    if ((forces->m_stateFlags & 0x10) != 0) {
        stateId = 0x8744;
    } else if (forces->m_missionKey.m_value != unset.m_value) {
        stateId = 0x8745;
    } else if ((forces->m_stateFlags & 4) != 0) {
        stateId = 0x8748;
    } else {
        stateId = 0x8757;
    }
    SetText(&text, stateId);
    AddStatusRow(3, &text, values);
    const int row = AddRuleValueRows(&text, m_object, 4, labels, values);

    AddLabelRow(&text, 0x8651, row + 1, labels);
    text.append(std::to_string(static_cast<unsigned long>(forces->GetRecordField4c())));
    AddStatusRow(row + 1, &text, values);

    AddLabelRow(&text, 0x8672, row + 2, labels);
    text.append(std::to_string(static_cast<unsigned long>(forces->GetDiplomacy())));
    AddStatusRow(row + 2, &text, values);

    AddLabelRow(&text, 0x8673, row + 3, labels);
    text.append(std::to_string(static_cast<unsigned long>(forces->GetEspionage())));
    AddStatusRow(row + 3, &text, values);

    AddLabelRow(&text, 0x8674, row + 4, labels);
    text.append(std::to_string(static_cast<unsigned long>(forces->GetCombat())));
    AddStatusRow(row + 4, &text, values);

    AddLabelRow(&text, 0x8675, row + 5, labels);
    text.append(std::to_string(static_cast<unsigned long>(forces->GetLeadership())));
    AddStatusRow(row + 5, &text, values);
}

void ObjectStatusWindow::FillMissionPage(CoolListItemList* labels, CoolListItemList* values) {
    std::string text;
    SetText(&text, 0x8676);
    m_titleLabel.SetText(text.c_str());
    std::string unusedText;
    AddNameRow(1, labels, values);

    int memberCount = 0;
    int flaggedCount = 0;
    GameObjNotifier* firstMember = nullptr;
    for (GameObjNotifier* member = GetFirstObjectInFamilies_30_40(m_galaxy->m_side); member;) {
        ObjectKey missionKey;
        GetObjectKey(m_object, &missionKey);
        ObjectKey memberKey;
        static_cast<GameObjRoot*>(member)->GetKey(&memberKey);
        if (memberKey.m_value == missionKey.m_value) {
            if (!firstMember) firstMember = member;
            ++memberCount;
            if (static_cast<Role*>(member)->m_flags78 & 1) ++flaggedCount;
        }
        const FamilyRange characters(0x30, 0x40);
        member = static_cast<GameObj*>(member)->FindNextSibling(&characters, 1);
    }

    SetText(&text, 0x8677);
    AddStatusRow(2, &text, labels);
    ObjectKey target;
    const uint32_t side = m_galaxy->m_side == 1 ? 1 : (m_galaxy->m_side == 2 ? 2 : 0);
    if (firstMember) {
        if (((static_cast<GameObjRoot*>(firstMember)->m_flags >> 6) & 3) == side) {
            target = static_cast<Mission*>(m_object)->m_objectiveKey;
        } else {
            ObjectKey memberKey;
            static_cast<GameObjRoot*>(firstMember)->GetKey(&memberKey);
            if (memberKey.Family() >= 0x50 && memberKey.Family() < 0x60) {
                ObjectKey owner;
                static_cast<GameObjRoot*>(firstMember)->GetOwnerKey(&owner);
                if (owner.Family() >= 0x90 && owner.Family() < 0x98) target = owner;
            }
        }
    }
    GameObjNotifier* location = target.IsSet() ? GetObjectInFamilies_01_ff(m_galaxy->m_side, &target) : nullptr;
    text.clear();
    if (!location) {
        SetText(&text, 0x8527);
    } else {
        Append(&text, static_cast<GameObj*>(location)->GetName()->c_str());
    }
    AddStatusRow(2, &text, values);

    const int row = firstMember ? AddRuleValueRows(&text, firstMember, 3, labels, values) : 2;
    GetObjectInFamilies_90_98(m_galaxy->m_side, &static_cast<Mission*>(m_object)->m_targetLocationKey);
    AddLabelRow(&text, 0x8678, row + 1, labels);
    text.append(std::to_string(static_cast<unsigned long>(memberCount)));
    AddStatusRow(row + 1, &text, values);
    AddLabelRow(&text, 0x8679, row + 2, labels);
    text.append(std::to_string(static_cast<unsigned long>(flaggedCount)));
    AddStatusRow(row + 2, &text, values);
}

namespace {

void AddCharacterNameRow(std::string* text, uint16_t labelId, int row, System* system, int commandKind,
                         CoolListItemList* labels, CoolListItemList* values) {
    AddLabelRow(text, labelId, row, labels);
    GameObjNotifier* character = nullptr;
    system->FindCharacterByCommandKind(commandKind, &character);
    if (!character) {
        SetText(text, 0x8762);
    } else {
        Append(text, static_cast<GameObj*>(character)->GetName()->c_str());
    }
    AddStatusRow(row, text, values);
}

using UnitCounter = int (*)(GameObj* parent, uint32_t side, uint32_t filter);

struct CountRow {
    uint16_t m_labelId;
    UnitCounter m_count;
};

}  // namespace

void ObjectStatusWindow::FillSystemPage(CoolListItemList* labels, CoolListItemList* values) {
    std::string text;
    SetText(&text, 0x8680);
    m_titleLabel.SetText(text.c_str());
    std::string unusedText;
    System* system = static_cast<System*>(m_object);
    AddNameRow(1, labels, values);
    AddLabelRow(&text, 0x8666, 2, labels);
    const uint32_t side = (system->m_flags >> 6) & 3;
    SetText(&text, side == 1 ? 0x8758 : (side == 2 ? 0x8759 : 0x8761));
    AddStatusRow(2, &text, values);
    AddCharacterNameRow(&text, 0x8681, 3, system, 3, labels, values);
    AddCharacterNameRow(&text, 0x8682, 4, system, 1, labels, values);

    AddLabelRow(&text, 0x8684, 5, labels);
    text.append(std::to_string(static_cast<unsigned long>(system->GetLoyaltyFor(1))));
    AddStatusRow(5, &text, values);
    AddLabelRow(&text, 0x8685, 6, labels);
    text.append(std::to_string(static_cast<unsigned long>(system->GetLoyaltyFor(2))));
    AddStatusRow(6, &text, values);

    for (int row = 7; row <= 8; ++row) {
        AddLabelRow(&text, row == 7 ? 0x8686 : 0x8687, row, labels);
        text.append(std::to_string(static_cast<long>(system->m_energy)));
        const int allocated = system->m_energyAllocated;
        Append(&text, g_statusSeparator);
        text.append(std::to_string(static_cast<long>(allocated)));
        AddStatusRow(row, &text, values);
    }
    AddLabelRow(&text, 0x8688, 9, labels);
    Append(&text, g_emptyText);
    AddStatusRow(9, &text, values);

    static constexpr CountRow kBuildingRows[] = {
        {0x8689, CountProductionFacilityType2C}, {0x8690, CountProductionFacilityType2D},
        {0x8691, CountManufacturingFacility28Children}, {0x8692, CountManufacturingFacility29Children},
        {0x8693, CountManufacturingFacility2AChildren}, {0x8694, CountDefenseFacilities24},
        {0x8695, CountDefenseFacilities},               {0x8696, CountDefenseFacilities25},
    };
    int row = 10;
    for (const CountRow& entry : kBuildingRows) {
        AddLabelRow(&text, entry.m_labelId, row, labels);
        text.append(std::to_string(static_cast<unsigned long>(entry.m_count(system, side, 3))));
        AddStatusRow(row, &text, values);
        ++row;
    }
    AddLabelRow(&text, 0x8697, 18, labels);
    Append(&text, g_emptyText);
    AddStatusRow(18, &text, values);
    AddLabelRow(&text, 0x8698, 19, labels);
    text.append(std::to_string(static_cast<unsigned long>(CountTroopRegs(system, side, 3))));
    AddStatusRow(19, &text, values);
    AddLabelRow(&text, 0x8699, 20, labels);
    // BUG(original): the squadron count goes into row 21, one below its label.
    text.append(std::to_string(static_cast<unsigned long>(CountFightSquads(system, side, 3))));
    AddStatusRow(21, &text, values);
}

void ObjectStatusWindow::FillProductionFacilityPage(CoolListItemList* labels, CoolListItemList* values) {
    std::string text;
    SetText(&text, 0x8700);
    m_titleLabel.SetText(text.c_str());
    AddNameRow(1, labels, values);
    AddLabelRow(&text, 0x8664, 2, labels);
    Append(&text, static_cast<GameObj*>(static_cast<GameObjRoot*>(m_object)->m_parent)->GetName()->c_str());
    AddStatusRow(2, &text, values);
    const int row = AddRuleValueRows(&text, m_object, 3, labels, values);
    AddStatusRows(row + 1, labels, values);
    AddCombatValueRow(row + 2, labels, values);
    AddLabelRow(&text, 0x8701, row + 3, labels);
    text.append(std::to_string(static_cast<unsigned long>(static_cast<Mission*>(m_object)->GetRecordHidden())));
    AddStatusRow(row + 3, &text, values);
    AddLabelRow(&text, 0x8658, row + 4, labels);
    {
        Facility* facility = static_cast<Facility*>(m_object);
        const int subtrahend = facility->Virtual113();
        const int field58 = facility->GetRecordField58();
        text.append(std::to_string(static_cast<long>(field58 - subtrahend)));
    }
    AddStatusRow(row + 4, &text, values);
}
