#pragma once
// STAND-IN declarations for the code the object_timers classes call (modules owned elsewhere); drop a
// declaration when the owning module's header provides it.
#include <cstdint>

#include "gameobj/game_hooks.h"
#include "gameobj/object_key.h"
#include "gameobj/tracked_stat.h"

class Character;

// TrackedStat accessors the linker folded over several classes (0x5092b0 is also System::GetStat0, 0x520aa0 Mission::GetStatA, 0x520ab0 Mission::GetStatB).
TrackedStat* __thiscall FUN_005092b0(void* self);
TrackedStat* __thiscall FUN_00520aa0(void* self);
TrackedStat* __thiscall FUN_00520ab0(void* self);
// CharacterMgr method (0x560a30, ret 8): called on the game root's member +0xc0 with the timer's character.
int __thiscall FUN_00560a30(GameObjNotifier* characterManager, Character* person, NotifyContext* ctx);
