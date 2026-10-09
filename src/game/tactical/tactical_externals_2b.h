#pragma once
#include <cstdint>

#include "tactical/tactical_math.h"
#include "tactical/tactical_simulator.h"
#include "tactical/tactical_units.h"

// Functions of the capital-ship / fighter / Death Star / turn-sync / formation modules that the ship-group and tactical
// AI code calls. Declared by original address; the owning modules name them (drop the declarations when they land).

class ShipGroup;
class TacticalHub;



void __thiscall FUN_005aefc0(HeadlessSimulator* simulator, ShipGroup* group);
void __thiscall FUN_005afa00(HeadlessSimulator* simulator, TacticalShipBase* target, ShipGroup* group);  // the group's target becomes `target`

void __thiscall FUN_005cb1a0(ShipGroup* group, int a);
void __thiscall FUN_005a0320(TacticalHub* hub, int groupId);
TacticalHub* FUN_0059ed70();
