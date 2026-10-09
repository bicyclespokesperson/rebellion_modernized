#pragma once
#include <cstdint>

#include "gameobj/game_obj.h"

class System;
class ChildIterator;

// Functions and globals of other modules (facilities, fleets, troops, the 0x509xxx System helpers, the 0x559xxx rule
// formulas) that the System rules call. They are declared by their original addresses until their owners name them;
// drop this header when they do.

// ---- 0x509xxx: System helpers (core) ----
int __thiscall FUN_00508fb0(System* self, int side, int filter);
int __thiscall FUN_00509020(System* self, int side, int filter);
int __thiscall FUN_00509090(System* self, int side, int filter);
int __thiscall FUN_00509140(System* self, int side, int filter);
int __thiscall FUN_005091f0(System* self);
GameObjNotifier* __thiscall FUN_00509670(System* self, int kind);
int __thiscall FUN_00509710(System* self, int side);
int __thiscall FUN_00509890(System* self, uint32_t* side);
int __thiscall FUN_00509980(System* self, uint32_t* flag);
int __thiscall FUN_00509ad0(System* self);
int __thiscall FUN_00509b10(System* self);
int __thiscall FUN_00509b20(System* self);
int __thiscall FUN_00509b30(System* self);
int __thiscall FUN_00509c00(System* self, int value, int side, NotifyContext* ctx);
int __thiscall FUN_00507270(System* self, int side);
int __thiscall FUN_00512d00(System* self, int side, int flag, NotifyContext* ctx);

// ---- units of other modules ----
int __thiscall FUN_0040f340(GameObjNotifier* self);
int __thiscall FUN_004fe2a0(GameObjNotifier* fleet, int value, NotifyContext* ctx);
int __thiscall FUN_004fe310(GameObjNotifier* fleet, int value, NotifyContext* ctx);
int __thiscall FUN_004fe380(GameObjNotifier* fleet, int value, NotifyContext* ctx);
int __thiscall FUN_004fe3f0(GameObjNotifier* fleet, int value, NotifyContext* ctx);
int __thiscall FUN_00504470(GameObjNotifier* troops, int percent, NotifyContext* ctx);
int __thiscall FUN_0055a7a0(GameObjNotifier* facility, int value, NotifyContext* ctx);
int __thiscall FUN_00557860(GameObjNotifier* unit, int on, NotifyContext* ctx);
int __thiscall FUN_00529dd0(GameObjNotifier* facility, NotifyContext* ctx);
int __thiscall FUN_00528040(GameObjNotifier* facility, int a);
int __thiscall FUN_0053a7f0(GameObjNotifier* facility, int on, NotifyContext* ctx);
int __thiscall FUN_005037f0(System* self, NotifyContext* ctx);  // System::ApplyEnergyAllocated (folded)
