#include "tactical/tactical_groups.h"
#include "tactical/tactical_objmgr.h"
#include "tactical/tactical_registry.h"
#include "tactical/tactical_externals.h"
#include "tactical/tactical_facade.h"
#include "tactical/tactical_game.h"
#include "tactical/tactical_ship.h"
#include "tactical/tactical_simulator.h"

// The 3D battle's unit events (handlers the object manager calls for the events other machines or the AI post: damage,
// repairs, tractor beams, withdrawals) and the unit methods behind them (0x5b01d0-0x5b2e60).

namespace {

TacticalShipBase* findShip(int handle) {
    return GetTacticalRegistry()->FindShip(handle);
}

float floatArg(int word) {
    float value;
    __builtin_memcpy(&value, &word, sizeof value);
    return value;
}

int controlledHere(TacticalShipBase* ship) {
#ifdef REB_NO_TACTICAL_3D
    (void)ship;
    return 0;
#else
    return FUN_005ad870(GetObjectManager(), ship);
#endif
}

void postObjectManagerEvent(int kind, int id, int argument, int delay) {
#ifndef REB_NO_TACTICAL_3D
    GetObjectManager()->PostEvent(kind, id, argument, delay);
#else
    (void)kind;
    (void)id;
    (void)argument;
    (void)delay;
#endif
}

// Takes the unit out of the battle: the hit effect flags are cleared, the wreck effect plays and the unit is destroyed.
void explode(TacticalShipBase* ship) {
    ship->Virtual52();
    const int id = static_cast<int>(ship->m_id);
    FUN_005c13d0(id, 4, ship->Virtual46());
    ship->Destroy();
}

}  // namespace

// ---- tractor beams -------------------------------------------------------------------------------------------------

int TacticalShipBase::HasTractoringShips() { return m_unk360.FirstNode() != nullptr; }

void TacticalShipBase::AddTractoringShip(TacticalShipBase* tractor) {
    if (m_unk360.FirstNode() == nullptr) {
        FUN_005ba980(FUN_005ba8b0(), 6, 1);
        if (m_hasGravityWell == 0) FUN_005c1360(static_cast<int>(m_id), 1);
    }
    m_unk360.AddUnit(tractor);
}

void TacticalShipBase::RemoveTractoringShip(int handle) {
    m_unk360.RemoveById(handle);
    if (m_unk360.FirstNode() == nullptr) {
        FUN_005ba980(FUN_005ba8b0(), 7, 1);
        if (m_hasGravityWell == 0) FUN_005c1360(static_cast<int>(m_id), 0);
    }
}

void TacticalShipBase::BeginTractoring() {
    m_unk3a4 = 1;
    FUN_005ba980(FUN_005ba8b0(), 8, 1);
    FUN_005c1320(static_cast<int>(m_id), 1);
}

void TacticalShipBase::EndTractoring() {
    m_unk3a4 = 0;
    FUN_005c1320(static_cast<int>(m_id), 0);
}

void TacticalShipBase::AddTractorJam(float amount) {
    m_unk3b4 = static_cast<float>(amount + static_cast<double>(m_unk3b4));
}

void TacticalShipBase::AddTractorDelay(float amount) {
    m_unk3b0 = static_cast<float>(amount + static_cast<double>(m_unk3b0));
}

void TacticalShipBase::StartTractoring(int target) {
    TacticalShipBase* other = findShip(target);
    m_tractorTarget = target;
    other->AddTractoringShip(this);
    m_unk3ac = 0;
}

void TacticalShipBase::AddTractorSource(int source) {
    TacticalShipBase* other = findShip(source);
    unsigned slot = 0;
    for (; slot < 4; ++slot) {
        if (m_tractoredBy[slot] == 0) break;
    }
    if (slot < 4) {
        m_tractoredBy[slot] = source;
        m_unk3a8 = 0;
        other->BeginTractoring();
    }
}

void TacticalShipBase::StopTractoring(int target) {
    TacticalShipBase* other = findShip(target);
    m_tractorTarget = 0;
    m_unk3ac = 0;
    if (other != nullptr) other->RemoveTractoringShip(static_cast<int>(m_id));
}

void TacticalShipBase::RemoveTractorSource(int source) {
    TacticalShipBase* other = findShip(source);
    unsigned slot = 0;
    for (; slot < 4; ++slot) {
        if (m_tractoredBy[slot] == source) break;
    }
    if (slot < 4) {
        m_tractoredBy[slot] = 0;
        m_unk3a8 = 0;
        other->EndTractoring();
    }
}

float TacticalShipBase::DistanceTo(TacticalShipBase* other) {
    const double dz = static_cast<double>(m_position.m_z) - other->m_position.m_z;
    const double dy = static_cast<double>(m_position.m_y) - other->m_position.m_y;
    const double dx = static_cast<double>(m_position.m_x) - other->m_position.m_x;
    const double yz = dy * dy + dz * static_cast<float>(dz);
    return static_cast<float>(reb_sqrt(yz + dx * static_cast<float>(dx)));
}

void TacticalShipBase::ConsiderTractoring(int target) {
    if (m_channelB.m_data != nullptr) return;
    if (m_unk3ac == 1) return;
    if (m_tractorPower == 0.0f) return;
    int kind = 0x29;
    int argument = m_tractorTarget;
    if (target == 0) {
        if (m_tractorTarget == 0) return;
    } else {
        TacticalShipBase* other = findShip(target);
        if (other == nullptr) return;
        if (m_tractorTarget == target) {
            if (controlledHere(other) != 0 && TractorPower() != 0.0f) {
                if (!(GetTractorRange() < DistanceTo(other))) return;
            }
        } else if (m_tractorTarget == 0) {
            if (TractorPower() == 0.0f) return;
            if (GetTractorRange() < DistanceTo(other)) return;
            kind = 0x28;
            argument = target;
        }
    }
    postObjectManagerEvent(kind, static_cast<int>(m_id), argument, 1);
    m_unk3ac = 1;
}

// ---- destruction, repairs and timers -------------------------------------------------------------------------------

int TacticalShipBase::Destroy() {
    const int state = m_state;
    if (state != 2 && state != 5 && state != 4 && state != 6 && state != 10 && state != 1) {
        m_fA_04 = 0;
        m_fB_04 = 0;
        m_fD_04 = 0;
        FUN_0059f4d0(FUN_0059ed70(), this);
        return 0;
    }
    m_hull = 0;
    if (m_channelB.m_data == nullptr) Virtual26();
    ShipGroup* group = ShipGroupOf(GetTacticalRegistry()->FindShipGroup(m_groupHandle));
    if (group != nullptr) {
        group->Virtual21(static_cast<int>(m_id));
        FUN_005caef0(group, static_cast<int>(m_id));
    }
    FUN_005a9130(GetObjectManager(), this);
    if (GetUnitClass() == 0) static_cast<TacticalCapitalShip*>(this)->TacticalHangar::Disband();
    m_state = 0xb;
    m_fA_04 = 0;
    m_fB_04 = 0;
    m_fD_04 = 0;
    FUN_0059f4d0(FUN_0059ed70(), this);
    return 1;
}

void TacticalShipBase::RepairOneHit() {
    if (m_hitCount == 0 || m_unk3f0 == 0.0f) return;
    if (!(static_cast<double>(FUN_005a8a70(GetObjectManager(), 1, 100)) <= m_unk3f0)) return;
    uint32_t pick = m_hitCount;
    if (pick < 2) {
        pick = 1;
    } else {
        pick = FUN_005a8a70(GetObjectManager(), 1, static_cast<int>(pick));
    }
    const int id = static_cast<int>(m_id);
    if (pick <= m_speedCH) {
        postObjectManagerEvent(0x26, id, static_cast<int>(m_speedCH) - 1, 1);
        return;
    }
    pick -= m_speedCH;
    if (pick <= m_shieldCH) {
        postObjectManagerEvent(0x23, id, static_cast<int>(m_shieldCH) - 1, 1);
        return;
    }
    pick -= m_shieldCH;
    if (pick <= m_hyperdriveCH) {
        postObjectManagerEvent(0x27, id, static_cast<int>(m_hyperdriveCH) - 1, 1);
        return;
    }
    pick -= m_hyperdriveCH;
    if (pick <= m_tractorCH) {
        postObjectManagerEvent(0x25, id, static_cast<int>(m_tractorCH) - 1, 1);
        return;
    }
    pick -= m_tractorCH;
    if (pick <= m_weaponCH) postObjectManagerEvent(0x24, id, static_cast<int>(m_weaponCH) - 1, 1);
}

void TacticalCapitalShip::Virtual39(float step) {
    if (0.0f < m_unk3b4) {
        const float left = static_cast<float>(m_unk3b4 - static_cast<double>(step) * 0.001f);
        m_unk3b4 = left;
        if (left < 0.0f) m_unk3b4 = 0.0f;
    }
    if (0.0f < m_unk3b0) {
        const float left = static_cast<float>(m_unk3b0 - static_cast<double>(step) * 0.001f);
        m_unk3b0 = left;
        if (left < 0.0f) m_unk3b0 = 0.0f;
    }
    if (m_channelB.m_data == nullptr && m_hitCount != 0) {
        const float timer = static_cast<float>(m_unk3f4 - static_cast<double>(step));
        m_unk3f4 = timer;
        if (timer <= 0.0f) {
            m_unk3f4 = 50000.0f;
            RepairOneHit();
        }
    }
}

void TacticalCapitalShip::Virtual26() {
    for (UnitRefNode* node = m_unk28->FirstNode(); node != nullptr; node = m_unk28->FirstNode()) {
        TacticalHangar::ReleaseSlot(node->Value());
    }
    if (m_tractorTarget != 0) {
        postObjectManagerEvent(0x29, static_cast<int>(m_id), m_tractorTarget, 1);
        m_unk3ac = 1;
    }
    for (int i = 0; i < 4; ++i) {
        if (m_tractoredBy[i] != 0) postObjectManagerEvent(0x2b, static_cast<int>(m_id), m_tractoredBy[i], 1);
        m_unk3a8 = 1;
    }
}

void TacticalFighter::Virtual26() {
    if (m_carrierHandle != 0) {
        TacticalHangar* hangar = GetTacticalRegistry()->FindLaunchInterface(m_carrierHandle);
        hangar->ReleaseSlot(this);
        m_unk35c = 0;
        m_carrierHandle = 0;
    }
}

// ---- group queries -------------------------------------------------------------------------------------------------

int TacticalCapitalShip::Virtual28() {
    if (m_groupHandle == 0) return 0;
    ShipGroup* group = ShipGroupOf(GetTacticalRegistry()->FindFighterGroup(m_groupHandle));
    return group->m_unk5c;
}

int TacticalCapitalShip::Virtual29() {
    if (m_groupHandle == 0) return 0;
    ShipGroup* group = ShipGroupOf(GetTacticalRegistry()->FindFighterGroup(m_groupHandle));
    return FUN_005cb980(group);
}

int TacticalCapitalShip::Virtual30() {
    if (m_groupHandle == 0) return 0;
    ShipGroup* group = ShipGroupOf(GetTacticalRegistry()->FindFighterGroup(m_groupHandle));
    return group->m_mode;
}

int TacticalFighter::Virtual28() {
    ShipGroup* group = ShipGroupOf(GetTacticalRegistry()->FindCapitalShipGroup(m_groupHandle));
    if (group != nullptr) return group->m_unk5c;
    return 0;
}

int TacticalFighter::Virtual29() {
    ShipGroup* group = ShipGroupOf(GetTacticalRegistry()->FindCapitalShipGroup(m_groupHandle));
    if (group != nullptr) return FUN_005cb980(group);
    return 0;
}

int TacticalFighter::Virtual30() {
    ShipGroup* group = ShipGroupOf(GetTacticalRegistry()->FindCapitalShipGroup(m_groupHandle));
    if (group != nullptr) return group->m_mode;
    return 0;
}

// ---- carrier slots -------------------------------------------------------------------------------------------------

void TacticalHangar::ReleaseSlot(TacticalShipBase* fighter) {
    if (m_unk28->Find(static_cast<int>(fighter->m_id)) == nullptr) return;
    --m_aboard;
    m_unk28->RemoveById(static_cast<int>(fighter->m_id));
    postObjectManagerEvent(0x2c, static_cast<int>(fighter->m_id), 0, 1);
}

int TacticalHangar::AllLaunched() { return m_aboard == m_capacity && m_docked->FirstNode() == nullptr; }

int TacticalHangar::ReserveSlot(TacticalShipBase* fighter) {
    if (m_aboard >= m_capacity) return 0;
    ++m_aboard;
    fighter->m_unk35c = 1;
    postObjectManagerEvent(0x2c, static_cast<int>(fighter->m_id), m_ownerHandle, 1);
    m_unk28->AddUnit(fighter);
    return 1;
}

// ---- event handlers ------------------------------------------------------------------------------------------------

void TacticalShipStatusEvent(TacticalEvent* event) {
    if (event == nullptr) return;
    CapitalShipResult* record = reinterpret_cast<CapitalShipResult*>(reinterpret_cast<char*>(event->m_args) + 8);
    if (record == nullptr) return;
    TacticalShipBase* ship = findShip(record->m_unk00);
    if (ship != nullptr) static_cast<TacticalCapitalShip*>(ship)->ApplyStatus(record);
}

void TacticalShipCarrierWaitEvent(TacticalEvent* event) {
    int* args = event->m_args;
    TacticalShipBase* ship = findShip(args[0]);
    if (ship != nullptr) {
        ship->m_unk35c = 2;
        ship->m_carrierHandle = args[1];
    }
}

void TacticalShipJumpEvent(TacticalEvent* event) {
    TacticalShipBase* ship = findShip(event->m_args[0]);
    if (ship == nullptr || controlledHere(ship) == 0) return;
    if (ship->UpdateHyperdriveReady() == 1) {
        ship->m_state = 7;
        return;
    }
    TacticalShipJumpAbortEvent(event);
}

void TacticalShipJumpAbortEvent(TacticalEvent* event) {
    TacticalShipBase* ship = findShip(event->m_args[0]);
    if (ship == nullptr || controlledHere(ship) == 0 || !(ship->m_hull > 0.0f)) return;
    ship->Virtual52();
    ship->m_fA_04 = 0;
    ship->m_fB_04 = 0;
    ship->m_fD_04 = 0;
    const int id = static_cast<int>(ship->m_id);
    const int sound = ship->Virtual46();
    FUN_005c13d0(id, 4, sound);
    ship->m_state = 2;
    ship->Destroy();
}

void TacticalShipTractorStatusEvent(TacticalEvent* event) {
    int* args = event->m_args;
    TacticalShipBase* ship = findShip(args[0]);
    if (ship == nullptr) return;
    ship->AddTractorJam(floatArg(args[3]));
    ship->AddTractorDelay(floatArg(args[2]));
    for (unsigned arc = 0; arc < 4; ++arc) {
        if (args[4 + arc] == 1) {
            ship->ClearArcCharge(arc);
            ship->QueueArcCharge(arc);
        }
    }
}

void TacticalShipStartTractorEvent(TacticalEvent* event) {
    int* args = event->m_args;
    TacticalShipBase* ship = findShip(args[0]);
    TacticalShipBase* other = findShip(args[1]);
    if (ship == nullptr || other == nullptr) return;
    if (controlledHere(ship) == 1 && controlledHere(other) == 1) ship->StartTractoring(args[1]);
}

void TacticalShipStopTractorEvent(TacticalEvent* event) {
    int* args = event->m_args;
    TacticalShipBase* ship = findShip(args[0]);
    if (ship != nullptr) ship->StopTractoring(args[1]);
}

void TacticalShipWithdrawEvent(TacticalEvent* event) {
    TacticalShipBase* ship = findShip(event->m_args[0]);
    if (ship == nullptr) return;
    if (controlledHere(ship) != 0 || ship->m_state == 7 || ship->m_state == 8) FUN_005a90c0(GetObjectManager(), ship);
}

void TacticalShipAddDelayEvent(TacticalEvent* event) {
    int* args = event->m_args;
    TacticalShipBase* ship = findShip(args[0]);
    if (ship != nullptr) ship->AddTractorDelay(floatArg(args[1]));
}

void TacticalShipClearArcEvent(TacticalEvent* event) {
    int* args = event->m_args;
    TacticalShipBase* ship = findShip(args[0]);
    if (ship != nullptr) ship->ClearArcCharge(args[1]);
}

void TacticalShipAddTractorJamEvent(TacticalEvent* event) {
    int* args = event->m_args;
    TacticalShipBase* ship = findShip(args[0]);
    if (ship != nullptr) ship->AddTractorJam(floatArg(args[1]));
}

void TacticalShipHitSpeedEvent(TacticalEvent* event) {
    TacticalShipBase* ship = findShip(event->m_args[0]);
    if (ship != nullptr) ship->ApplyCriticalHit(3);
}

void TacticalShipHitTractorEvent(TacticalEvent* event) {
    TacticalShipBase* ship = findShip(event->m_args[0]);
    if (ship != nullptr) ship->ApplyCriticalHit(2);
}

void TacticalShipHitWeaponEvent(TacticalEvent* event) {
    TacticalShipBase* ship = findShip(event->m_args[0]);
    if (ship != nullptr) ship->ApplyCriticalHit(1);
}

void TacticalShipHitShieldEvent(TacticalEvent* event) {
    TacticalShipBase* ship = findShip(event->m_args[0]);
    if (ship != nullptr) ship->ApplyCriticalHit(0);
}

void TacticalShipHitHyperdriveEvent(TacticalEvent* event) {
    TacticalShipBase* ship = findShip(event->m_args[0]);
    if (ship != nullptr) ship->ApplyCriticalHit(4);
}

void TacticalShipRepairSpeedEvent(TacticalEvent* event) {
    TacticalShipBase* ship = findShip(event->m_args[0]);
    if (ship != nullptr) ship->RepairCriticalHit(3);
}

void TacticalShipRepairTractorEvent(TacticalEvent* event) {
    TacticalShipBase* ship = findShip(event->m_args[0]);
    if (ship != nullptr) ship->RepairCriticalHit(2);
}

void TacticalShipRepairWeaponEvent(TacticalEvent* event) {
    TacticalShipBase* ship = findShip(event->m_args[0]);
    if (ship != nullptr) ship->RepairCriticalHit(1);
}

void TacticalShipRepairShieldEvent(TacticalEvent* event) {
    TacticalShipBase* ship = findShip(event->m_args[0]);
    if (ship != nullptr) ship->RepairCriticalHit(0);
}

void TacticalShipRepairHyperdriveEvent(TacticalEvent* event) {
    TacticalShipBase* ship = findShip(event->m_args[0]);
    if (ship != nullptr) ship->RepairCriticalHit(4);
}

void TacticalShipHitEvent54(TacticalEvent* event) {
    int* args = event->m_args;
    TacticalShipBase* ship = findShip(args[0]);
    if (ship != nullptr && controlledHere(ship) != 0) ship->Virtual54(floatArg(args[2]), args[0], args[1]);
}

void TacticalShipHitEvent55(TacticalEvent* event) {
    int* args = event->m_args;
    TacticalShipBase* ship = findShip(args[0]);
    if (ship != nullptr && controlledHere(ship) != 0) ship->Virtual55(floatArg(args[2]), args[0], args[1]);
}

void TacticalShipHitEvent56(TacticalEvent* event) {
    int* args = event->m_args;
    TacticalShipBase* ship = findShip(args[0]);
    if (ship != nullptr && controlledHere(ship) != 0) ship->Virtual56(floatArg(args[2]), args[0], args[1]);
}

void TacticalShipHitEvent53(TacticalEvent* event) {
    int* args = event->m_args;
    TacticalShipBase* ship = findShip(args[0]);
    if (ship != nullptr && controlledHere(ship) != 0) ship->Virtual53(floatArg(args[2]), args[0], args[1]);
}

void TacticalShipCollisionEvent(TacticalEvent* event) {
    int* args = event->m_args;
    TacticalShipBase* ship = findShip(args[0]);
    TacticalShipBase* other = findShip(args[1]);
    if (other == nullptr) {
        TacticalUnit* deathStar = FUN_005a9330(GetObjectManager());
        if (deathStar != nullptr && static_cast<int>(deathStar->m_id) == args[1]) {
            if (controlledHere(ship) == 0) return;
            explode(ship);
            return;
        }
    }
    if (ship == nullptr) return;
    if (controlledHere(ship) == 0 || !(ship->m_hull < 0.0f)) return;
    ship->Virtual52();
    const int id = static_cast<int>(ship->m_id);
    FUN_005c13d0(id, 4, ship->Virtual46());
    if (other != nullptr) FUN_005a5cf0(FUN_0059ed70(), args[0], args[1]);
    ship->Destroy();
}

void TacticalShipImpactEventB(TacticalEvent* event) {
    int* args = event->m_args;
    TacticalShipBase* source = findShip(args[0]);
    TacticalShipBase* target = findShip(args[1]);
    if (source == nullptr || target == nullptr) return;
    if (controlledHere(source) == 0 || controlledHere(target) == 0) return;
    target->Virtual49();
#ifndef REB_NO_TACTICAL_3D
    GetObjectManager()->m_impactFlag = 1;
#endif
    if (28.8f <= floatArg(args[2])) {
        FUN_005c12d0(args[1], args[0], 0, 0.5f, 2);
    } else {
        FUN_005c12d0(args[1], args[0], 0, 0.5f, 1);
    }
}

void TacticalShipImpactEventC(TacticalEvent* event) {
    int* args = event->m_args;
    TacticalShipBase* source = findShip(args[0]);
    TacticalShipBase* target = findShip(args[1]);
    if (source == nullptr || target == nullptr) return;
    if (controlledHere(source) == 0 || controlledHere(target) == 0) return;
    target->Virtual50();
#ifndef REB_NO_TACTICAL_3D
    GetObjectManager()->m_impactFlag = 1;
#endif
    if (34.666668f <= floatArg(args[2])) {
        const uint32_t type = source->GetTypeIndex();
        if (type != 0xb && type != 0xe && type != 0xd && type != 0x16 && type != 0x19) {
            FUN_005c12d0(args[1], args[0], 1, 1.0f, 3);
            return;
        }
        FUN_005c12d0(args[1], args[0], 1, 0.75f, 2);
        return;
    }
    FUN_005c12d0(args[1], args[0], 1, 0.65f, 1);
}

void TacticalShipImpactEventD(TacticalEvent* event) {
    int* args = event->m_args;
    TacticalShipBase* source = findShip(args[0]);
    TacticalShipBase* target = findShip(args[1]);
    if (source == nullptr || target == nullptr) return;
#ifndef REB_NO_TACTICAL_3D
    GetObjectManager()->m_impactFlag = 1;
#endif
    if (controlledHere(source) == 0 || controlledHere(target) == 0) return;
    target->Virtual51();
    if (32.0f <= floatArg(args[2])) {
        FUN_005c12d0(args[1], args[0], 2, 1.0f, 3);
        return;
    }
    FUN_005c12d0(args[1], args[0], 2, 0.4f, 1);
}

void TacticalShipImpactEventE(TacticalEvent* event) {
    int* args = event->m_args;
    TacticalShipBase* source = findShip(args[0]);
    TacticalShipBase* target = findShip(args[1]);
    if (source == nullptr || target == nullptr) return;
    if (controlledHere(source) == 0 || controlledHere(target) == 0) return;
    source->Virtual50();
    if (12.8f <= floatArg(args[2])) {
        FUN_005c12d0(args[1], args[0], 3, 0.2f, 3);
        return;
    }
    FUN_005c12d0(args[1], args[0], 3, 0.2f, 1);
}

// ---- carrier launches ----------------------------------------------------------------------------------------------

void TacticalHangar::OnFighterLaunched(TacticalShipBase* fighter) {
#ifdef REB_NO_TACTICAL_3D
    (void)fighter;
    return;
#else
    m_launched->AddUnit(fighter);
    m_unk28->RemoveById(static_cast<int>(fighter->m_id));
    ++m_inFlight;
    const int groupHandle = fighter->m_groupHandle;
    fighter->m_unk35c = 4;
    ShipGroup* group = ShipGroupOf(GetTacticalRegistry()->FindShipGroup(groupHandle));
    int kind = 4;
    if (group != nullptr) {
        kind = FUN_005c7150(group);
        group->Virtual21(static_cast<int>(fighter->m_id));
        FUN_005caef0(group, static_cast<int>(fighter->m_id));
    }
    FUN_005a9070(GetObjectManager(), fighter);
    const int side = fighter->m_side;
    fighter->m_state = 5;
    if (side != GetObjectManager()->m_playerSide || groupHandle == 0) return;
    if (GetTacticalRegistry()->FindShipGroup(groupHandle) != nullptr) return;
    if (fighter->m_side == 0) {
        if (kind == 0) {
            FUN_005ba980(FUN_005ba8b0(), 0x87, 1);
        } else if (kind == 1) {
            FUN_005ba980(FUN_005ba8b0(), 0x88, kind);
        } else if (kind == 2) {
            FUN_005ba980(FUN_005ba8b0(), 0x89, 1);
        } else if (kind == 3) {
            FUN_005ba980(FUN_005ba8b0(), 0x8a, 1);
        }
        return;
    }
    if (kind == 0) {
        FUN_005ba980(FUN_005ba8b0(), 0x105, 1);
    } else if (kind == 1) {
        FUN_005ba980(FUN_005ba8b0(), 0x106, kind);
    } else if (kind == 2) {
        FUN_005ba980(FUN_005ba8b0(), 0x107, 1);
    } else if (kind == 3) {
        FUN_005ba980(FUN_005ba8b0(), 0x108, 1);
    }
#endif
}

void TacticalHangar::UpdateLaunches() {
#ifdef REB_NO_TACTICAL_3D
    return;
#else
    if (m_docked->FirstNode() == nullptr) return;
    if (m_launchTime == 0.0f) {
        const float clock = static_cast<float>(GetObjectManager()->m_clock);
        const float roll = static_cast<float>(static_cast<uint32_t>(FUN_005a8a70(GetObjectManager(), 0, 8)));
        m_launchTime = static_cast<float>(GetLaunchInterval() + roll + clock);
        return;
    }
    if (static_cast<double>(GetObjectManager()->m_clock) < m_launchTime) return;
    TacticalShipBase* unit = m_docked->FirstUnit();
    void* memory = ::operator new(0x220);
    void* event = nullptr;
    if (memory != nullptr) {
        const int id = static_cast<int>(unit->m_id);
        const float clock = static_cast<float>(GetObjectManager()->m_clock);
        event = FUN_005ce500(memory, clock, 0x19, m_ownerHandle, id, 0);
    }
    GetObjectManager()->QueueEvent(event, 1);
    if (m_inFlight == 0) return;
    const float clock = static_cast<float>(GetObjectManager()->m_clock);
    const float roll = static_cast<float>(static_cast<uint32_t>(FUN_005a8a70(GetObjectManager(), 0, 8)));
    m_launchTime = static_cast<float>(GetLaunchInterval() + roll + clock);
#endif
}

void TacticalHangar::LaunchNow(TacticalShipBase* fighter) {
    m_docked->RemoveUnit(fighter);
    TacticalVector point;
    GetLaunchPoint(&point);
    fighter->m_position.m_x = point.m_x;
    fighter->m_position.m_y = point.m_y;
    fighter->m_position.m_z = point.m_z;
    fighter->m_position.m_cachedLength = point.m_cachedLength;
    fighter->m_carrierHandle = m_ownerHandle;
    fighter->SetState2();
    --m_inFlight;
    --m_aboard;
#ifndef REB_NO_TACTICAL_3D
    FUN_005a9170(GetObjectManager(), fighter);
#endif
}

void TacticalCarrierLaunchEvent(TacticalEvent* event) {
    int* args = event->m_args;
    TacticalShipBase* fighter = findShip(args[1]);
    TacticalHangar* hangar = GetTacticalRegistry()->FindLaunchInterface(args[0]);
    if (fighter == nullptr || hangar == nullptr) return;
    ShipGroup* group = ShipGroupOf(GetTacticalRegistry()->FindShipGroup(fighter->m_groupHandle));
    if (group != nullptr && group->m_mode == 2) return;
    TacticalUnit* deathStar = FUN_005a9330(GetObjectManager());
    if (deathStar != nullptr && args[0] == static_cast<int>(deathStar->m_id)) {
        if (static_cast<TacticalDeathStar*>(deathStar)->IsOperational()) hangar->LaunchNow(fighter);
        return;
    }
    TacticalShipBase* carrier = findShip(args[0]);
    if (carrier != nullptr && controlledHere(carrier) != 0) hangar->LaunchNow(fighter);
}

void TacticalCarrierLaunchedEvent(TacticalEvent* event) {
    int* args = event->m_args;
    TacticalShipBase* fighter = findShip(args[1]);
    TacticalHangar* hangar = GetTacticalRegistry()->FindLaunchInterface(args[0]);
    if (hangar == nullptr) return;
    TacticalUnit* deathStar = FUN_005a9330(GetObjectManager());
    if (deathStar != nullptr && args[0] == static_cast<int>(deathStar->m_id)) {
        if (static_cast<TacticalDeathStar*>(deathStar)->IsOperational()) hangar->OnFighterLaunched(fighter);
        return;
    }
    TacticalShipBase* carrier = findShip(args[0]);
    if (carrier != nullptr && controlledHere(carrier) != 0) hangar->OnFighterLaunched(fighter);
}

// ---- Death Star events ---------------------------------------------------------------------------------------------

void TacticalDeathStar::FireAt(int target) {
    m_charge = 0;
#ifdef REB_NO_TACTICAL_3D
    (void)target;
#else
    const int id = static_cast<int>(m_id);
    postObjectManagerEvent(0x44, target, id, 1);
    void* memory = ::operator new(0x220);
    void* event = nullptr;
    if (memory != nullptr) {
        const float time = static_cast<float>(static_cast<double>(GetObjectManager()->m_clock) + 1.0f);
        event = FUN_005ce500(memory, time, 3, target, id, 0);
    }
    GetObjectManager()->QueueEvent(event, 1);
#endif
}

void TacticalDeathStarImpactEvent(TacticalEvent* event) {
#ifdef REB_NO_TACTICAL_3D
    (void)event;
    return;
#else
    int* args = event->m_args;
    TacticalShipBase* ship = findShip(args[0]);
    if (ship != nullptr && controlledHere(ship) != 0) {
        GetObjectManager()->m_unk894 = 1;
        FUN_005ba980(FUN_005ba8b0(), 0x14, 1);
        if (GetObjectManager()->m_playerSide == 0) FUN_005ba980(FUN_005ba8b0(), 0x91, 1);
    }
    FUN_005c12d0(args[1], args[0], 0, 1.0f, 1);
#endif
}

void TacticalDeathStarStatusEvent(TacticalEvent* event) {
    TacticalUnit* deathStar = FUN_005a9330(GetObjectManager());
    if (event == nullptr || deathStar == nullptr) return;
    DeathStarResult* record = reinterpret_cast<DeathStarResult*>(reinterpret_cast<char*>(event->m_args) + 8);
    if (reinterpret_cast<intptr_t>(event->m_args) == -8) return;
    TacticalDeathStar* ds = static_cast<TacticalDeathStar*>(deathStar);
    ds->m_hull = record->m_hull;
    ds->m_fired = record->m_unk10;
}

void TacticalDeathStarFiredEvent(TacticalEvent*) {
    TacticalUnit* deathStar = FUN_005a9330(GetObjectManager());
    if (deathStar == nullptr) return;
    TacticalDeathStar* ds = static_cast<TacticalDeathStar*>(deathStar);
    if (!ds->IsOperational()) return;
    ds->m_fired = 1;
    static_cast<TacticalHangar*>(ds)->ReleaseDocked();
    FUN_005c1200(static_cast<int>(ds->m_id));
}
