#pragma once
#include <cstdint>

#include "core/containers.h"
#include "tactical/tactical_units.h"

// The tactical registry (singleton DAT_006bc470, 0x48 bytes, created by GetTacticalRegistry, 0x596bd0): the battle's
// units by the handle the strategic game knows them by. Capital ships and fighters (class TacticalShipBase) are looked up through
// a TacticalUnit sub-object at +0xd4 of the unit, which is the node the registry holds; the groups of fighters and of
// capital ships are the nodes themselves (their kind, TacticalUnit::Virtual01, is 3 for a fighter group and 4 for a
// capital-ship group; characters and special forces, kind 7, are kept by the simulator instead).

class TacticalShipBase;
class TacticalHangar;

class TacticalRegistry {
public:
    TacticalRegistry();           // 0x5a6290
    virtual ~TacticalRegistry();  // 0x5a6590 (scalar deleting destructor 0x5a6320)

    // Next free handle on a side (0 or 1): side * 1,000,000 + a running count. Returns 0 for any other side.
    uint32_t NextUnitHandle(unsigned side);   // 0x5a6620
    uint32_t NextGroupHandle(unsigned side);  // 0x5a6640

    TacticalShipBase* FindShip(int handle);                         // 0x5a6340 capital ship or fighter
    TacticalHangar* FindLaunchInterface(int handle);       // 0x5a6360 a carrier's launch interface, or the Death Star's
    IdNode* FindShipOrViewGroup(int handle);               // 0x5a63e0
    IdNode* FindViewGroup(int handle);                     // 0x5a6450
    IdNode* FindFighterGroup(int handle);                  // 0x5a6460
    IdNode* FindCapitalShipGroup(int handle);              // 0x5a6470
    IdNode* FindShipGroup(int handle);                     // 0x5a6480 a fighter group, else a capital-ship group
    IdNode* RemoveShipGroup(int handle);                   // 0x5a64b0 detaches (does not delete)
    uint32_t AddShip(TacticalShipBase* ship);                       // 0x5a64e0
    uint32_t AddViewGroup(IdNode* group);                  // 0x5a6510
    uint32_t AddFighterGroup(IdNode* group);               // 0x5a6520
    uint32_t AddCapitalShipGroup(IdNode* group);           // 0x5a6530
    uint32_t AddShipGroup(TacticalUnit* group);            // 0x5a6540 by the group's kind (3 fighters, 4 capital ships)
    TacticalUnit* GetDeathStar();                          // 0x5a66a0

    uint32_t m_handleCounter[2];  // +0x04 last handle given per side
    uint32_t m_groupCounter[2];   // +0x0c last group id given per side
    IdAvlTree m_ships;            // +0x14
    IdAvlTree m_viewGroups;       // +0x20 groups of the 3D view
    IdAvlTree m_fighterGroups;    // +0x2c
    IdAvlTree m_capitalGroups;    // +0x38
    TacticalUnit* m_deathStar;    // +0x44
};

// The kind a TacticalUnit reports (its second virtual): 3 fighter group, 4 capital-ship group, 7 character / special force.
int TacticalUnitKind(TacticalUnit* unit);
