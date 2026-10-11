#pragma once
#include "core/reb_abi.h"
#include <cstdint>

#include "tactical/tactical_math.h"
#include "tactical/tactical_simulator.h"
#include "tactical/tactical_units.h"

// Functions of the capital-ship / fighter / Death Star / turn-sync / formation modules that the ship-group and tactical
// AI code calls. Declared by original address; the owning modules name them (drop the declarations when they land).

class ShipGroup;
class TacticalHub;


void REB_THISCALL FUN_005cb1a0(ShipGroup* group, int a);
void REB_THISCALL FUN_005a0320(TacticalHub* hub, int groupId);
TacticalHub* FUN_0059ed70();
