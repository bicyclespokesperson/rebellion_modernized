#pragma once
// STAND-IN declarations: the system-side handlers (0x515c00-0x515f40, the "combat_strat" module) that events 0x303-0x31b
// call, and the SideBits setter. Drop a declaration when the owning module's header provides it.
#include <cstdint>

#include "gameobj/object_key.h"
#include "gameobj/side_bits.h"

class NotifyContext;

int FUN_00515c00(ObjectKey* key, SideBits* first, SideBits* second, NotifyContext* ctx);
int FUN_00515cd0(ObjectKey* key, int value, NotifyContext* ctx);
int FUN_00515dc0(ObjectKey* key, int value, NotifyContext* ctx);
int FUN_00515e30(ObjectKey* key, int value, NotifyContext* ctx);
int FUN_00515e70(ObjectKey* key, int value, NotifyContext* ctx);
int FUN_00515eb0(ObjectKey* key, int value, NotifyContext* ctx);
int FUN_00515ef0(ObjectKey* key, int value, NotifyContext* ctx);
int FUN_00515f40(ObjectKey* key, int value, NotifyContext* ctx);
