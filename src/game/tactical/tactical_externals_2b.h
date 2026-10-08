#pragma once
#include <cstdint>

#include "tactical/tactical_math.h"
#include "tactical/tactical_simulator.h"
#include "tactical/tactical_units.h"

// Functions of the capital-ship / fighter / Death Star / turn-sync / formation modules that the ship-group and tactical
// AI code calls. Declared by original address; the owning modules name them (drop the declarations when they land).

class ShipGroup;
class GroupFormation;
class TacticalHub;

float __thiscall FUN_005b18d0(SimUnit* unit);                   // the unit's strongest weapon figure
int __thiscall FUN_005b1b70(SimUnit* unit);
int __thiscall FUN_005b69f0(SimUnit* unit);
int __thiscall FUN_005b7ce0(SimUnit* unit);
int __thiscall FUN_005b7d10(SimUnit* unit);
float __thiscall FUN_005b7780(SimUnit* unit, SimUnit* target, int b);
float __thiscall FUN_005b7a80(SimUnit* unit, int a, int b);     // the unit's strength (a is 0)
void __thiscall FUN_005b54d0(SimUnit* unit, float damage);
void __thiscall FUN_005b6320(SimUnit* unit, float seconds);
void __thiscall FUN_005b8450(TacticalUnit* launchInterface, SimUnit* fighter);

int __thiscall FUN_005ba420(TacticalUnit* deathStar);
int __thiscall FUN_005ba450(TacticalUnit* deathStar);
void __thiscall FUN_005ba710(TacticalUnit* deathStar, int handle);
void __thiscall FUN_005ba7f0(TacticalUnit* deathStar, float seconds);

void __thiscall FUN_005af7a0(HeadlessSimulator* simulator, SimUnit* unit);
void __thiscall FUN_005af910(HeadlessSimulator* simulator, SimUnit* unit);
int __thiscall FUN_005afe40(HeadlessSimulator* simulator, SimUnit* unit);
int __thiscall FUN_005afcc0(HeadlessSimulator* simulator);
float __thiscall FUN_005afa30(HeadlessSimulator* simulator, int side);
void __thiscall FUN_005aefc0(HeadlessSimulator* simulator, ShipGroup* group);
void __stdcall FUN_005afde0(ShipGroup* group);  // registers a fighter group
void __stdcall FUN_005afe10(ShipGroup* group);  // registers a capital-ship group

void __thiscall FUN_005cb1a0(ShipGroup* group, int a);
void __thiscall FUN_005a0320(TacticalHub* hub, int groupId);
TacticalHub* FUN_0059ed70();

GroupFormation* __thiscall FUN_005c72b0(void* memory, int groupHandle, TacticalVec4 position);  // capital-ship formation
GroupFormation* __thiscall FUN_005ec310(void* memory, int groupHandle, TacticalVec4 position);  // fighter formation
