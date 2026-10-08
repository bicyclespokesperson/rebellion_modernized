#pragma once
#include <cstdint>

#include "tactical/tactical_math.h"
#include "tactical/tactical_simulator.h"
#include "tactical/tactical_units.h"

// Functions of the capital-ship / fighter / Death Star / turn-sync / formation modules that the ship-group and tactical
// AI code calls. Declared by original address; the owning modules name them (drop the declarations when they land).

class ShipGroup;
class TacticalHub;

float __thiscall FUN_005b1640(TacticalShipBase* unit);
float __thiscall FUN_005b1710(TacticalShipBase* unit);
float __thiscall FUN_005b18d0(TacticalShipBase* unit);                   // the unit's strongest weapon figure
int __thiscall FUN_005b1b70(TacticalShipBase* unit);
int __thiscall FUN_005b69f0(TacticalShipBase* unit);
int __thiscall FUN_005b7ce0(TacticalShipBase* unit);
int __thiscall FUN_005b7d10(TacticalShipBase* unit);
float __thiscall FUN_005b7780(TacticalShipBase* unit, TacticalShipBase* target, int b);
float __thiscall FUN_005b7a80(TacticalShipBase* unit, int a, int b);     // the unit's strength (a is 0)
void __thiscall FUN_005b54d0(TacticalShipBase* unit, float damage);
void __thiscall FUN_005b6320(TacticalShipBase* unit, float seconds);

int __thiscall FUN_005ba420(TacticalUnit* deathStar);
int __thiscall FUN_005ba450(TacticalUnit* deathStar);
void __thiscall FUN_005ba710(TacticalUnit* deathStar, int handle);
void __thiscall FUN_005ba7f0(TacticalUnit* deathStar, float seconds);

void __thiscall FUN_005af7a0(HeadlessSimulator* simulator, TacticalShipBase* unit);
void __thiscall FUN_005af910(HeadlessSimulator* simulator, TacticalShipBase* unit);
int __thiscall FUN_005afe40(HeadlessSimulator* simulator, TacticalShipBase* unit);
int __thiscall FUN_005afcc0(HeadlessSimulator* simulator);
float __thiscall FUN_005afa30(HeadlessSimulator* simulator, int side);
void __thiscall FUN_005aefc0(HeadlessSimulator* simulator, ShipGroup* group);
void __thiscall FUN_005afa00(HeadlessSimulator* simulator, TacticalShipBase* target, ShipGroup* group);  // the group's target becomes `target`
void __stdcall FUN_005afde0(ShipGroup* group);  // registers a fighter group
void __stdcall FUN_005afe10(ShipGroup* group);  // registers a capital-ship group

void __thiscall FUN_005cb1a0(ShipGroup* group, int a);
void __thiscall FUN_005a0320(TacticalHub* hub, int groupId);
TacticalHub* FUN_0059ed70();
