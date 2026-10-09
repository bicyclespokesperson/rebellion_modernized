#pragma once
// STAND-IN declarations for the code the command_classes2 classes call (modules owned elsewhere); drop a
// declaration when the owning module's header provides it.
#include <cstdint>

#include "gameobj/game_hooks.h"
#include "gameobj/object_key.h"


bool CheckMoveEnabled(const ObjectKey* key, StatusCode* status);
bool CheckMoveValid(const ObjectKey* object, const ObjectKey* destination, StatusCode* status);
bool ExecuteMove(const ObjectKey* object, const ObjectKey* destination, NotifyContext* ctx);
int __stdcall FUN_005542f0(GameObjNotifier* object, const ObjectKey* destination, ActivityRequest* request, StatusCode* status);
// The manager's key at +0x7c (copies it into *out).
ObjectKey* __thiscall FUN_004c27b0(GameObjNotifier* manager, ObjectKey* out);
// The data record of a key (via the object-type database).
DatRecord* FUN_0051cab0(const ObjectKey* key);
// Fetches the side object that controls `manager` into *out (null for neutral). Returns 0 only if that object is missing.
int FUN_00518750(GameObjNotifier* manager, void** out);
