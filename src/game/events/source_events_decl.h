#pragma once
// STAND-IN declarations for the code the source_events classes call (modules owned elsewhere); drop a
// declaration when the owning module's header provides it.
#include <cstdint>

#include "gameobj/game_hooks.h"
#include "gameobj/object_key.h"


int FUN_00514510(ObjectKey*, ObjectKey*, ObjectKey*, int, NotifyContext*);
int FUN_00514820(ObjectKey*, ObjectKey*, ObjectKey*, int, NotifyContext*);
int FUN_005140c0(ObjectKey*, ObjectKey*, ObjectKey*, int, NotifyContext*);
int FUN_005150c0(ObjectKey*, int, NotifyContext*);
int FUN_00515a50(ObjectKey*, int, NotifyContext*);
int FUN_00515bb0(ObjectKey*, int, NotifyContext*);
int FUN_00515480(ObjectKey*, int, NotifyContext*);
int FUN_00514890(ObjectKey*, int, int, NotifyContext*);
int FUN_00514a20(ObjectKey*, int, int, NotifyContext*);
int FUN_00516360(ObjectKey*, int, int, NotifyContext*);
int FUN_00516220(ObjectKey*, int, int, NotifyContext*);
int FUN_00514a60(ObjectKey*, ObjectKey*, ObjectKey*, NotifyContext*);
void __thiscall FUN_00561990(void* manager, GameObjNotifier* owner, int value, NotifyContext* ctx);
int __thiscall FUN_00546e30(GameObjNotifier* manager, ObjectKey* key, int value, NotifyContext* ctx);
