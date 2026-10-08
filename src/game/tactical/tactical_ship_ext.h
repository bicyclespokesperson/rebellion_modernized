#pragma once
#include <cstdint>

#include "tactical/tactical_objmgr.h"
#include "tactical/tactical_ship.h"

// Functions and objects outside the unit modules that the capital ship / fighter / Death Star code calls. They are
// declared by their original addresses; their owners (ship groups and AI 0x59cd60-0x5a7500 / 0x5c5b30-0x5d2360, object
// manager 0x5a7500-0x5af4e0, turn sync 0x5bbf10) drop the declarations when they name them.

class TacticalObjectManager;
class TacticalRegistry;

// Weapon profile objects (0x14 bytes each) inside a weapon arc.
void __thiscall FUN_005d2530(void* profile);
void __thiscall FUN_005d24e0(void* profile);
void __thiscall FUN_005d2490(void* profile);
void __thiscall FUN_005d24d0(void* profile);
void __thiscall FUN_005d2520(void* profile);
void __thiscall FUN_005d2570(void* profile);

// Commander skill of a side as the object manager (3D battle) reports it.
double __thiscall FUN_005ad750(TacticalObjectManager* manager, int side);
double __thiscall FUN_005ad7e0(TacticalObjectManager* manager, int side);

// The object manager's random number between two bounds (rand() % (high - low + 1) + low).
int __thiscall FUN_005a8a70(TacticalObjectManager* manager, int low, int high);

// A ship group (owned by the ship-group module); only the fields the unit code reads.
struct TacticalGroupView {
    uint8_t m_pad00[0x18];
    int m_id;               // +0x18
    uint8_t m_pad1c[0x34];
    int m_side;             // +0x50
    uint8_t m_pad54[0x18];
    int m_memberCount;      // +0x6c
};

// Removes a unit from its group.
void __stdcall FUN_005caed0(TacticalGroupView* group, int handle);

// Gives a unit a fresh handle for its side (0x5a6620 + IdNode::SetId); `unit` is the TacticalUnit subobject.
void __thiscall FUN_005a6660(TacticalUnit* unit, int side);

// Adds a battle unit (the object, not its TacticalUnit subobject) to the registry.
uint32_t __thiscall FUN_005a64e0(TacticalRegistry* registry, TacticalShipBase* ship);

// The registry object (DAT_006bc470): only the Death Star pointer.
struct TacticalRegistryView {
    uint8_t m_pad00[0x44];
    TacticalUnit* m_deathStar;  // +0x44
};

// The tactical sound manager (presentation): created on first use.
class TacticalSoundManager;
TacticalSoundManager* FUN_005ba8b0();
void __thiscall FUN_005ba980(TacticalSoundManager* manager, int sound, int flag);
