#include "tactical/tactical_externals.h"
#include "tactical/tactical_facade.h"
#include "tactical/tactical_ship.h"
#include "tactical/tactical_ship_ext.h"
#include "tactical/tactical_simulator.h"

// The headless simulator's handling of units (0x5af4e0-0x5afe40): putting them into the right lists, taking them out
// of the battle, and the commanders' skill figures.

namespace {

TacticalShipBase* findShip(int handle) {
    return reinterpret_cast<TacticalShipBase*>(FUN_005a6340(GetTacticalRegistry(), handle));
}

TacticalGroupView* findGroup(int handle) {
    return reinterpret_cast<TacticalGroupView*>(FUN_005a6480(GetTacticalRegistry(), handle));
}

TacticalCharacterUnit* findCharacter(IdAvlTree& characters, int handle) {
    return static_cast<TacticalCharacterUnit*>(characters.FindById(handle));
}

// Takes the unit's group (if any) out of the side's list once the group has lost its last member.
void leaveGroup(HeadlessSimulator* simulator, TacticalShipBase* unit) {
    TacticalGroupView* group = findGroup(unit->m_groupHandle);
    if (group == nullptr) return;
    FUN_005caed0(group, unit->m_id);
    if (group->m_memberCount == 0) simulator->m_sides[group->m_side]->m_retired.RemoveById(group->m_id);
}

void removeFromStateList(HeadlessSimulator* simulator, TacticalShipBase* unit) {
    if (unit->m_state == 2) {
        simulator->m_active.RemoveById(unit->m_id);
    } else if (unit->m_state == 1) {
        simulator->m_launched.RemoveById(unit->m_id);
    }
}

}  // namespace

int HeadlessSimulator::ActivateUnit(TacticalShipBase* ship) {
    m_units.Add(ship);
    ship->m_state = 2;
    return ship->m_id;
}

void HeadlessSimulator::RetireUnit(TacticalShipBase* unit) {
    if (unit->m_state == 0xb) return;
    removeFromStateList(this, unit);
    unit->m_state = 0xb;
    unit->TakeDamage(100000.0f);
    m_retired.Add(unit);
    leaveGroup(this, unit);
    if (unit->GetUnitClass() != 0) {
        m_sides[unit->m_side]->m_hangars.RemoveById(unit->m_id);
        return;
    }
    m_sides[unit->m_side]->m_fleet.RemoveById(unit->m_id);
    TacticalCapitalShip* ship = static_cast<TacticalCapitalShip*>(unit);
    for (TacticalUnitNode* node = ship->m_docked->FirstNode(); node;) {
        TacticalUnitNode* next = node->Next();
        RetireUnit(node->Value());
        node = next;
    }
    ship->m_docked->Clear();
    for (TacticalUnitNode* node = ship->m_launched->FirstNode(); node;) {
        TacticalUnitNode* next = node->Next();
        RetireUnit(node->Value());
        node = next;
    }
    ship->m_launched->Clear();
}

void HeadlessSimulator::DockAtDeathStar(TacticalShipBase* unit) {
    if (unit->m_state == 9) return;
    if (unit->m_state == 0xb) return;
    removeFromStateList(this, unit);
    unit->m_state = 9;
    leaveGroup(this, unit);
    if (unit->GetUnitClass() == 0) {
        m_sides[unit->m_side]->m_fleet.RemoveById(unit->m_id);
        return;
    }
    m_sides[unit->m_side]->m_hangars.RemoveById(unit->m_id);
}

double HeadlessSimulator::OpponentStrength(int side) {
    if (side == 0) return m_sides[1]->m_strength;
    return m_sides[0]->m_strength;
}

void HeadlessSimulator::SetSideFlag(int side, int value) { m_sides[side]->m_flag80 = value; }

void HeadlessSimulator::SetSideValue(int side, float value) { m_sides[side]->m_value = value; }

double HeadlessSimulator::CommanderSkillA(int side) {
    const int handle = m_firstCommander[side];
    if (handle != 0) {
        TacticalCharacterUnit* commander = findCharacter(m_characters, handle);
        if (commander != nullptr) {
            TacticalShipBase* ship = findShip(commander->m_shipHandle);
            if (ship != nullptr && IsActiveUnit(ship) != 0) {
                double skill = 9.0f - static_cast<double>(commander->m_scaleA);
                if (skill <= 1.0f) skill = 1.0f;
                if (skill < 9.0f) return skill;
                return 9.0f;
            }
        }
    }
    return 1.0f;
}

double HeadlessSimulator::CommanderSkillB(int side) {
    const int handle = m_secondCommander[side];
    if (handle != 0) {
        TacticalCharacterUnit* commander = findCharacter(m_characters, handle);
        if (commander != nullptr) {
            TacticalShipBase* ship = findShip(commander->m_shipHandle);
            if (ship != nullptr) {
                double skill = static_cast<double>(commander->m_scaleB) - -1.0f;
                if (skill <= 1.0f) skill = 1.0f;
                if (skill < 9.0f) return skill;
                return 9.0f;
            }
        }
    }
    return 1.0f;
}

int HeadlessSimulator::IsActiveUnit(TacticalShipBase* ship) {
    if (ship != nullptr && m_active.Find(ship->m_id) != nullptr && ship->m_state == 2) return 1;
    return 0;
}

void HeadlessSimulator::RegisterFighter(TacticalShipBase* unit) {
    FUN_005a6660(unit, unit->m_side);
    FUN_005a64e0(GetTacticalRegistry(), unit);
}

void HeadlessSimulator::RegisterCapitalShip(TacticalShipBase* unit) {
    FUN_005a6660(unit, unit->m_side);
    FUN_005a64e0(GetTacticalRegistry(), unit);
    static_cast<TacticalCapitalShip*>(unit)->m_ownerHandle = unit->m_id;
}

void HeadlessSimulator::AssignCharacterHandle(TacticalUnit* unit) {
    FUN_005a6660(unit, static_cast<TacticalCharacterUnit*>(unit)->m_notAlliance);
}

// ---- construction ---------------------------------------------------------------------------------------------------

HeadlessSimulator::HeadlessSimulator() {
    m_elapsed = 0;
    m_random.Seed(0xbc614e);
    m_unk0c = 1;
    m_sides[0] = new SimulatorSideA();
    m_sides[1] = new SimulatorSideB();
    for (int side = 0; side < 2; ++side) {
        m_secondCommander[side] = 0;
        m_firstCommander[side] = 0;
        m_bestStrength[side] = 0;
        m_stagnantTime[side] = 0;
    }
}

HeadlessSimulator::~HeadlessSimulator() {
    delete m_sides[0];
    m_sides[0] = nullptr;
    delete m_sides[1];
    m_sides[1] = nullptr;
}
