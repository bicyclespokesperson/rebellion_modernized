#include "ai/ai_records.h"

#include "ai/ai_class_records.h"
#include "ai/ai_common.h"
#include "ai/ai_members.h"
#include "ai/ai_side_controller.h"
#include "character/role.h"
#include "core/checked_new.h"
#include "fleet/capship.h"
#include "fleet/fight_squad.h"
#include "fleet/troop_reg.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"

using ai::InFamilies;

namespace {

constexpr uint32_t kStateCompleted = 4;
constexpr uint32_t kStateDestroyed = 8;
constexpr uint32_t kStateEnroute = 0x10;
constexpr uint32_t kStateEnrouteActive = 0x20;
constexpr uint32_t kStateDamaged = 0x200;

ObjectKey KeyOfObject(GameObjNotifier* object) {
    ObjectKey key;
    GetObjectKey(object, &key);
    return key;
}

}  // namespace

AiShipRecord::AiShipRecord()
    : ObjectKeyNode(), m_dirty(0), m_view(0), m_flags(0), m_classKey(), m_fleetKey(), m_x30(0), m_x34(0), m_fleets(nullptr),
      m_classes(nullptr), m_owner(nullptr), m_stat() {
    m_classKey.m_value = 0;
}

AiShipRecord::AiShipRecord(GameObjNotifier* ship, int view, AiFleetEvalSet* fleets, AiClassTable* classes,
                           AiSideController* owner)
    : ObjectKeyNode(KeyOfObject(ship)), m_flags(0), m_classKey(), m_fleetKey() {
    m_view = view;
    GetAiRecordKey(ship->m_record, &m_classKey);
    m_fleets = fleets;
    m_classes = classes;
    m_owner = owner;
    m_x30 = 0;
    m_x34 = 0;
    AiClassRecordBase* entry = classes->Find(&m_classKey);
    m_flags.m_value |= entry->m_flags.m_value & 0x4ff;
    m_flags.m_value |= 0x20000;
    for (int& value : m_stat) value = 0;
    m_dirty = 1;
}

AiShipRecord::~AiShipRecord() {
    m_fleets = nullptr;
    m_classes = nullptr;
    m_owner = nullptr;

}

void AiShipRecord::ReadRecord(istream* in, AiFleetEvalSet* fleets, AiClassTable* classes, AiSideController* owner) {
    ObjectKeyNode::Load(in);
    m_fleets = fleets;
    m_classes = classes;
    m_owner = owner;
    ReadValue32(in, &m_view);
    m_flags.Read(in);
    m_classKey.Read(in);
    m_fleetKey.Read(in);
    ReadValue32(in, &m_x30);
    ReadValue32(in, &m_x34);
    ReadValue32(in, &m_dirty);
    for (int& value : m_stat) ReadValue32(in, &value);
    m_dirty = 1;
}

void AiShipRecord::Save(ostream* out) {
    ObjectKeyNode::Save(out);
    WriteValue32(out, &m_view);
    m_flags.Write(out);
    m_classKey.Write(out);
    m_fleetKey.Write(out);
    WriteValue32(out, &m_x30);
    WriteValue32(out, &m_x34);
    WriteValue32(out, &m_dirty);
    for (int& value : m_stat) WriteValue32(out, &value);
}

int AiShipRecord::IsUnusable() {
    int result = 0;
    ObjectKey fleetKey = m_fleetKey;
    AiFleetEval* fleet = m_fleets->FindEval(fleetKey);
    if (fleet) {
        AiSystemEval* system = m_owner->m_systems.FindEval(fleet->m_systemKey);
        if ((system && (system->m_flags30.m_value & 4)) || (fleet->m_flags.m_value & 0xf0000000)) result = 1;
    }
    return result;
}

void AiShipRecord::ClearFleet() {
    m_dirty = 1;
    m_fleets->NoteFleet(&m_fleetKey);
}

void AiShipRecord::MoveToFleet(const ObjectKey* newFleet, const ObjectKey* oldFleet) {
    ClearFleet();
    m_fleetKey.Reset();
    ObjectKey shipKey;
    GetKey(&shipKey);
    ObjectKey old = *oldFleet;
    m_fleets->RemoveShip(&old, &shipKey);
    m_fleets->AddShip(newFleet, &shipKey);
}

int AiShipRecord::ChangeFleet(const ObjectKey* shipKey) {
    ObjectKey ownKey;
    GetKey(&ownKey);
    if (shipKey->m_value == ownKey.m_value) {
        m_fleets->RemoveShip(&m_fleetKey, &m_key);
        return 1;
    }
    ClearFleet();
    return 0;
}

void AiShipRecord::Refresh() {
    if (m_dirty == 0) return;
    m_dirty = 0;
    m_flags.m_value &= 0x1b0204ff;
    for (int& value : m_stat) value = 0;
    auto stat = [this](int offset) -> int& { return m_stat[(offset - 0x44) / 4]; };

    const uint32_t viewBit = m_view == 1 ? 1 : (m_view == 2 ? 2 : 0);
    CapShip* ship = static_cast<CapShip*>(GetObjectInFamilies_14_1c(m_view, &m_key));
    if (!ship || (ship->m_stateFlags & kStateDestroyed)) return;

    if (ship->ControlSide() == viewBit) m_flags.m_value |= 0x4000;
    if (ship->m_stateFlags & kStateEnroute) m_flags.m_value |= 0x800;
    if ((ship->m_stateFlags & kStateCompleted) == 0) m_flags.m_value = (m_flags.m_value & 0xf7ffffff) | 0x2000;
    if (ship->m_stateFlags & kStateDamaged) {
        if (ship->GetTransitSpeed(1) == 0)
            m_flags.m_value |= 0x800000;
        else
            m_flags.m_value |= 0x1000;
    }

    AiCapitalShipClass* ownClass = static_cast<AiCapitalShipClass*>(m_classes->Find(&m_classKey));
    stat(0x44) = ownClass->m_x40;
    if ((m_flags.m_value & 0x1000) == 0) stat(0x48) = ownClass->m_x38;
    stat(0x50) = ownClass->ScaleC(ship->GetShieldRechargeRate() - ship->GetShieldRechargeRateLoss());
    stat(0x7c) = ownClass->ScaleA(ship->GetWeaponRechargeRate() - ship->GetWeaponRechargeRateLoss());
    stat(0x90) = ownClass->ScaleA(ship->GetWeaponRechargeRate() - ship->GetWeaponRechargeRateLoss());
    if (m_flags.m_value & 2) {
        stat(0x80) = ownClass->ScaleB(ship->GetWeaponRechargeRate() - ship->GetWeaponRechargeRateLoss());
        stat(0x94) = ownClass->ScaleB(ship->GetWeaponRechargeRate() - ship->GetWeaponRechargeRateLoss());
    }
    stat(0x60) = ownClass->m_x44;
    stat(0x6c) = ownClass->m_x48;
    stat(0x70) = ownClass->m_valueB;
    ++stat(0x88);

    FightSquadIterator squadrons(ship, 3);
    squadrons.Begin();
    while (squadrons.m_current) {
        FightSquad* squadron = static_cast<FightSquad*>(squadrons.m_current);
        ObjectKey recordKey;
        GetObjectRecordKey(squadron, &recordKey);
        AiFighterClass* fighterClass = static_cast<AiFighterClass*>(m_classes->Find(&recordKey));
        stat(0x78) += fighterClass->m_valueB;
        ++stat(0x58);
        m_flags.m_value |= 0x100;
        if (((squadron->m_stateFlags >> 2) & 1) == 0) {
            m_flags.m_value |= 0x8000;
        } else {
            stat(0x90) += fighterClass->ScaleX30(squadron->GetSquadSize() - squadron->GetSquadSizeDamage());
            stat(0x94) += fighterClass->ScaleX34(squadron->GetSquadSize() - squadron->GetSquadSizeDamage());
            if (((squadron->m_stateFlags >> 5) & 1) == 0) {
                stat(0x84) += fighterClass->ScaleX38(squadron->GetSquadSize() - squadron->GetSquadSizeDamage());
                stat(0x7c) += fighterClass->ScaleX30(squadron->GetSquadSize() - squadron->GetSquadSizeDamage());
                stat(0x80) += fighterClass->ScaleX34(squadron->GetSquadSize() - squadron->GetSquadSizeDamage());
                stat(0x48) += fighterClass->m_x40;
                ++stat(0x88);
                ++stat(0x5c);
            }
        }
        if ((squadron->m_stateFlags >> 5) & 1) m_flags.m_value |= 0x10000;
        squadrons.Advance();
    }

    TroopRegIterator troops(ship, 3);
    troops.Begin();
    while (troops.m_current) {
        ObjectKey recordKey;
        GetObjectRecordKey(troops.m_current, &recordKey);
        AiTroopClass* troopClass = static_cast<AiTroopClass*>(m_classes->Find(&recordKey));
        stat(0x74) += troopClass->m_valueB;
        m_flags.m_value |= 0x200;
        ++stat(0x64);
        const uint32_t troopState = static_cast<GameObj*>(troops.m_current)->m_stateFlags;
        if (((troopState >> 2) & 1) == 0 || ((troopState >> 5) & 1) != 0) {
            m_flags.m_value |= 0x8000;
        } else {
            stat(0x4c) += troopClass->m_attackStrength;
            ++stat(0x88);
            ++stat(0x68);
        }
        if ((troopState >> 5) & 1) m_flags.m_value |= 0x10000;
        troops.Advance();
    }

    RoleIterator roles(ship, 3);
    roles.Begin();
    while (roles.m_current) {
        ObjectKey key;
        GetObjectKey(roles.m_current, &key);
        if (InFamilies(key.Family(), 0x30, 0x3c)) {
            AiCharacterRecord* record = static_cast<AiCharacterRecord*>(m_owner->m_characters.Find(&key));
            if (record) {
                const uint32_t characterFlags = record->m_flags.m_value;
                if (characterFlags & 0x2000000) m_flags.m_value |= 0x20000000;
                if ((characterFlags & 4) == 0) {
                    m_flags.m_value |= (characterFlags & 0x40) == 0 ? 0x400000 : 0x80000;
                } else if ((characterFlags & 0x40) == 0) {
                    ObjectKey storyKey;
                    GetObjectKey(roles.m_current, &storyKey);
                    if (IsStoryKeyOf6(storyKey.m_value) == 0) {
                        if ((characterFlags & 0x20) == 0) m_flags.m_value |= 0x100000;
                    } else {
                        if ((characterFlags & 0x20) == 0) m_flags.m_value |= 0x40000;
                        if ((m_owner->m_flags.Get() & 1) == 0) {
                            GetObjectKey(roles.m_current, &storyKey);
                            if (IsStoryKeyOf4(storyKey.m_value) != 0) ++stat(0x8c);
                        }
                    }
                    if (characterFlags & 0x7000000) m_flags.m_value |= 0x4000000;
                } else {
                    m_flags.m_value |= 0x200000;
                }
            }
            if ((static_cast<GameObj*>(roles.m_current)->m_stateFlags >> 5) & 1) m_flags.m_value |= 0x10000;
        }
        roles.Advance();
    }
}
