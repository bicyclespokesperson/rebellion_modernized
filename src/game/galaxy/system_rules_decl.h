#pragma once
#include <cstdint>

#include "gameobj/game_obj.h"

class System;
class ChildIterator;

// Functions and globals of other modules (facilities, fleets, troops, the 0x509xxx System helpers, the 0x559xxx rule
// formulas) that the System rules call. They are declared by their original addresses until their owners name them;
// drop this header when they do.

// ---- units of other modules ----
int __thiscall FUN_0040f340(GameObjNotifier* self);
