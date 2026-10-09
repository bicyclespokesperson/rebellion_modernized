#pragma once
// STAND-IN declarations: the system-side handlers (0x515c00-0x515f40, the "combat_strat" module) that events 0x303-0x31b
// call, and the SideBits setter. Drop a declaration when the owning module's header provides it.
#include <cstdint>

#include "gameobj/object_key.h"
#include "gameobj/side_bits.h"

class NotifyContext;

int ApplySystemEvent31b(ObjectKey* key, SideBits* first, SideBits* second, NotifyContext* ctx);
int ApplySystemEvent303(ObjectKey* key, int value, NotifyContext* ctx);
int ApplySystemEvent304(ObjectKey* key, int value, NotifyContext* ctx);
int ApplySystemEvent309(ObjectKey* key, int value, NotifyContext* ctx);
int ApplySystemEvent310(ObjectKey* key, int value, NotifyContext* ctx);
int ApplySystemEvent30f(ObjectKey* key, int value, NotifyContext* ctx);
int ApplySystemEvent311(ObjectKey* key, int value, NotifyContext* ctx);
int ApplySystemEvent305(ObjectKey* key, int value, NotifyContext* ctx);

// 0x558880 as a call on an already constructed SideBits: resets it, then takes `value` if it is a valid side set.
void __thiscall FUN_00558880(SideBits* bits, uint32_t value);
