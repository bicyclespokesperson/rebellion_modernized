#pragma once
// STAND-IN declarations for the code the command_classes2 classes call (modules owned elsewhere); drop a
// declaration when the owning module's header provides it.
#include <cstdint>

#include "gameobj/game_hooks.h"
#include "gameobj/object_key.h"


bool FUN_005152e0(const ObjectKey* key, StatusCode* status);
bool FUN_00515390(const ObjectKey* object, const ObjectKey* destination, StatusCode* status);
bool FUN_00515440(const ObjectKey* object, const ObjectKey* destination, NotifyContext* ctx);
int __stdcall FUN_005542f0(GameObjNotifier* object, const ObjectKey* destination, ActivityRequest* request, StatusCode* status);
// The manager's key at +0x7c (copies it into *out).
// The data record of a key (via the object-type database).
// Fetches the side object that controls `manager` into *out (null for neutral). Returns 0 only if that object is missing.
int FUN_00518750(GameObjNotifier* manager, void** out);
