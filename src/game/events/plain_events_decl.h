#pragma once
// STAND-IN declarations for the code the plain_events classes call (modules owned elsewhere); drop a
// declaration when the owning module's header provides it.
#include <cstdint>

#include "gameobj/game_hooks.h"
#include "gameobj/object_key.h"


// GameObjRoot::GetKey (family << 24 | id), out of line.
ObjectKey* __thiscall FUN_004025b0(GameObjNotifier* object, ObjectKey* out);
