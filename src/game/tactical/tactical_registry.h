#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "tactical/tactical_units.h"

// The tactical registry (singleton DAT_006bc470, 0x48 bytes, created by GetTacticalRegistry, 0x596bd0): the battle's
// units by the handle the strategic game knows them by. Capital ships and fighters (class SimUnit) are looked up through
// a TacticalUnit sub-object at +0xd4 of the unit, which is the node the registry holds; ship groups, characters and
// special forces/troops are the nodes themselves.

class SimUnit;

class TacticalRegistry {
public:
    TacticalRegistry();           // 0x5a6290
    virtual ~TacticalRegistry();  // 0x5a6590 (scalar deleting destructor 0x5a6320)

    // Next free handle on a side (0 or 1): side * 1,000,000 + a running count. Returns 0 for any other side.
    uint32_t NextUnitHandle(unsigned side);   // 0x5a6620
    uint32_t NextGroupHandle(unsigned side);  // 0x5a6640

    SimUnit* FindShip(int handle);                         // 0x5a6340 capital ship or fighter
    TacticalUnit* FindLaunchInterface(int handle);         // 0x5a6360 a carrier's launch interface, or the Death Star's
    IdNode* FindShipOrGroup(int handle);                   // 0x5a63e0
    IdNode* FindGroup(int handle);                         // 0x5a6450
    IdNode* FindCharacter(int handle);                     // 0x5a6460
    IdNode* FindSpecialForce(int handle);                  // 0x5a6470
    IdNode* FindCharacterOrSpecialForce(int handle);       // 0x5a6480
    IdNode* RemoveCharacterOrSpecialForce(int handle);     // 0x5a64b0 detaches (does not delete)
    uint32_t AddShip(SimUnit* ship);                       // 0x5a64e0
    uint32_t AddGroup(IdNode* group);                      // 0x5a6510
    uint32_t AddCharacter(IdNode* character);              // 0x5a6520
    uint32_t AddSpecialForce(IdNode* force);               // 0x5a6530
    uint32_t AddCharacterOrSpecialForce(TacticalUnit* unit);  // 0x5a6540 by the unit's kind (3 character, 4 special force)
    TacticalUnit* GetDeathStar();                          // 0x5a66a0

    uint32_t m_handleCounter[2];  // +0x04 last handle given per side
    uint32_t m_groupCounter[2];   // +0x0c last group id given per side
    IdAvlTree m_ships;            // +0x14
    IdAvlTree m_groups;           // +0x20
    IdAvlTree m_characters;       // +0x2c
    IdAvlTree m_specialForces;    // +0x38
    TacticalUnit* m_deathStar;    // +0x44
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(TacticalRegistry, 0x48);
REB_ASSERT_OFFSET(TacticalRegistry, m_ships, 0x14);
REB_ASSERT_OFFSET(TacticalRegistry, m_specialForces, 0x38);
REB_ASSERT_OFFSET(TacticalRegistry, m_deathStar, 0x44);
#pragma clang diagnostic pop
