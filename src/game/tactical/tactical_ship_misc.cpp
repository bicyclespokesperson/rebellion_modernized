#include "core/reb_abi.h"
#include <new>

#include "tactical/tactical_groups.h"
#include "tactical/tactical_objmgr.h"
#include "tactical/tactical_registry.h"
#include "tactical/tactical_externals.h"
#include "tactical/tactical_facade.h"
#include "tactical/tactical_game.h"
#include "tactical/tactical_ship.h"
#include "tactical/tactical_simulator.h"

// Small unit methods that did not fit elsewhere (0x5b1410-0x5b7d90, 0x5ba080-0x5ba380).

// ---- ship ---------------------------------------------------------------------------------------------------------

double TacticalShipBase::Turn(float step, float rate) {
    if (GetSpeed() == 0.0f || rate == 0.0f) return 0.0f;
    if (step <= g_shipMinTurnStep) return step;
    return MovingObject::Turn(step, rate);
}

double TacticalShipBase::GetRadius() {
    return m_unk6c.Length() * 0.5f + static_cast<double>(m_radius) * 0.5555556f;
}

int TacticalShipBase::HubQuery() { return FUN_0059f640(FUN_0059ed70(), this); }

String* TacticalShipBase::GetName(String* out) {
    new (out) String(m_name);
    return out;
}

void TacticalShipBase::Virtual48(float damage) {
    if (!(m_hull > 0.0f)) {
        m_hull = 0;
        return;
    }
    const double left = static_cast<double>(m_hull) - damage;
    m_shield = 0;
    m_hull = static_cast<float>(left);
    if (left <= 0.0f) m_hull = 0;
}

void TacticalShipBase::GetGroupPosition(TacticalVector* out) {
    float buffer[4];
    TacticalVector* scratch = reinterpret_cast<TacticalVector*>(buffer);
    const TacticalVector* position;
    if (m_groupHandle == 0) {
        position = FUN_005a7490(GetObjectManager(), scratch, m_side);
    } else {
        TacticalRegistry* registry = GetTacticalRegistry();
        ShipGroup* group = ShipGroupOf(m_unitKind == 1 ? registry->FindCapitalShipGroup(m_groupHandle)
                                                       : registry->FindFighterGroup(m_groupHandle));
        position = group->GetPosition(scratch);
    }
    out->m_x = position->m_x;
    out->m_y = position->m_y;
    out->m_z = position->m_z;
    out->m_cachedLength = position->m_cachedLength;
}

int TacticalShipBase::IsUnarmed() { return !HasKind1Weapons() && !HasOnlyKind1Weapons(); }

int TacticalShipBase::IsCrippled() {
    if (m_speedCH < 4 && m_unk3b4 < 10000.0f) return 0;
    return 1;
}

void TacticalShipBase::BuildStatusRecord(TacticalShipStatusRecord* out) {
    TacticalShipStatusRecord record = {};
    record.m_state = m_state;
    record.m_id = static_cast<int>(m_id);
    record.m_groupHandle = m_groupHandle;
    record.m_unk178 = m_unk178;
    record.m_speedCH = m_speedCH;
    record.m_shieldCH = m_shieldCH;
    record.m_hyperdriveCH = m_hyperdriveCH;
    record.m_tractorCH = m_tractorCH;
    record.m_weaponCH = m_weaponCH;
    record.m_tractorPower = m_tractorPower;
    record.m_shieldMax = m_shieldMax;
    record.m_shield = m_shield;
    record.m_hullMax = m_hullMax;
    record.m_hull = m_hull;
    record.m_weaponRecharge = m_weaponRecharge;
    record.m_shieldRecharge = m_shieldRecharge;
    record.m_hyperdriveCount = m_hyperdriveCount;
    record.m_speed = m_speed;
    record.m_commanderBonus = m_commanderBonus;
    int handle = m_unk178;
    if (m_channelB.m_data != nullptr) handle = m_groupHandle;
    if (handle != 0) {
        ShipGroup* group = ShipGroupOf(GetTacticalRegistry()->FindShipGroup(handle));
        if (group != nullptr) {
            record.m_groupMode = FUN_005cb980(group);
            if (TacticalUnitKind(group) == 3) record.m_hubValue = FUN_0059f640(FUN_0059ed70(), this);
            record.m_groupField68 = group->m_unk68;
            record.m_groupField5c = group->m_unk5c;
            record.m_groupField38 = group->m_unk38;
            UnitRefNode* first = static_cast<UnitRefNode*>(group->m_formationB.FirstNode());
            if (first != nullptr) record.m_firstMember = static_cast<UnitRefNode*>(group->m_formationB.FirstNode())->Key();
            AvlNode* second = group->m_targetsB.FirstNode();
            if (second != nullptr) record.m_secondGroupItem = static_cast<IdNode*>(group->m_targetsB.FirstNode())->m_id;
        }
    }
    *out = record;
}

double TacticalCapitalShip::Virtual43() { return 3.0f; }

double TacticalShipBase::Virtual43() { return 3.0f; }

// ---- fighter squadrons ----------------------------------------------------------------------------------------------

void TacticalFighter::ApplyStatus(const FightSquadResult* record) {
    m_state = record->m_unk1c;
    m_carrierHandle = record->m_unk10;
    m_hull = static_cast<float>(static_cast<double>(static_cast<uint32_t>(record->m_squadSize)) * 8.0f);
}

uint32_t TacticalFighter::SquadsAlive() {
    const uint32_t count = static_cast<uint32_t>(ScaleByHull(static_cast<float>(m_hullMax / 8.0f)));
    if (count < 1) return 1;
    return static_cast<uint32_t>(ScaleByHull(static_cast<float>(m_hullMax / 8.0f)));
}

uint32_t TacticalFighter::SquadsMax() {
    const uint32_t count = static_cast<uint32_t>(m_hullMax / 8.0f);
    return count < 1 ? 1 : count;
}

void REB_CDECL TacticalFighterStatusEvent(TacticalEvent* event) {
    if (event == nullptr) return;
    FightSquadResult* record = reinterpret_cast<FightSquadResult*>(reinterpret_cast<char*>(event->m_args) + 8);
    if (record == nullptr) return;
    TacticalFighter* fighter =
        static_cast<TacticalFighter*>(GetTacticalRegistry()->FindShip(record->m_unk00));
    if (fighter != nullptr) fighter->ApplyStatus(record);
}

void TacticalShipFacetC::Write(ostream* out) { WriteValue32(out, &m_fC_04); }

void TacticalShipFacetC::Read(istream* in) { ReadValue32(in, &m_fC_04); }
