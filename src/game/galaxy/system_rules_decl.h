#pragma once
#include <cstdint>

#include "gameobj/game_obj.h"

class System;
class ChildIterator;

// Functions and globals of other modules (facilities, fleets, troops, the 0x509xxx System helpers, the 0x559xxx rule
// formulas) that the System rules call. They are declared by their original addresses until their owners name them;
// drop this header when they do.

// ---- GNPRTB 0x1e parameters (0x558bb0) ----
extern int DAT_006bb3a0;
extern int DAT_006bb3a4;
extern int DAT_006bb3c4;
extern int DAT_006bb470;
extern int DAT_006bb3d8;
extern int DAT_006bb420;
extern int DAT_006bb49c;
extern int DAT_006bb3d0;
extern int DAT_006bb484;
extern int DAT_006bb4c8;
extern int DAT_006bb430;
extern int DAT_006bb48c;
extern int DAT_006bb494;

// ---- 0x559xxx: rule formulas (cdecl) ----
int FUN_00559b80(ChildIterator* it);
int FUN_00559be0(int a, int side, int deathStar);
int FUN_00559c10(int share);
int FUN_00559c40(int side, int share, int deathStar, int a, int b, int c, int d);
int FUN_00559ce0(int side, int share, int deathStar, int troops, int a, int b, int c, int* firstEvent, int* secondEvent);
int FUN_00559db0(int* event);
int FUN_00559ee0(int* rawMaterial, int* energy, int sites, int facilities);
int FUN_00559f90(int share, int* occurs);
int FUN_00559fb0(int side, int share, int deathStar, int a, int b, int troops);
int FUN_00559fe0(int side, int share, int deathStar, int uprising, int a);
int FUN_0055a020(int ships, int units);
int FUN_0055a080(int loyalty);

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
int __thiscall FUN_0050eea0(System* self, GameObjNotifier* target, NotifyContext* ctx);

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
int FUN_0053fa60(int kind, int flag, System* owner, TrackedStat* stat, NotifyContext* ctx);
int __thiscall FUN_005037f0(System* self, NotifyContext* ctx);  // System::ApplyEnergyAllocated (folded)
