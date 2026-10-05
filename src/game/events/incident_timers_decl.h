#pragma once
// STAND-IN declarations for the incident timers: bodies in modules owned elsewhere.
#include "gameobj/tracked_stat.h"

class System;
class GameObj;
class NotifyContext;

// 0x520aa0 / 0x520ab0: the identical-code-folded stat accessors (also Mission::GetStatA / GetStatB), here on a System.
TrackedStat* __thiscall FUN_00520aa0(System* self);
TrackedStat* __thiscall FUN_00520ab0(System* self);

// The family 0xf9 object's handlers (0x556b50 / 0x556be0).
int __thiscall FUN_00556b50(GameObj* self, NotifyContext* ctx);
int __thiscall FUN_00556be0(GameObj* self, NotifyContext* ctx);
