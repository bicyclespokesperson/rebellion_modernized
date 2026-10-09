#pragma once
// STAND-IN declarations for code owned by neighbouring modules (the manufacturing
// manager 0x527000-0x52e9ff, commands 0x57d000-0x57f000), declared by their original addresses for the order classes
// that call them. Replace each by the owner's real declaration when the module is merged and drop this header.
#include <cstdint>

#include "core/containers.h"
#include "gameobj/game_obj_root.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_key.h"
#include "gameobj/status_code.h"
#include "orders/command_decl.h"

class DatRecord;

// Manufacturing manager (family 0xa0-0xaf) pieces.
// The set of objects the manager's facility list holds (a static empty one for a sided object).
int __stdcall FUN_00555f00(GameObjNotifier* manager, const ObjectKey* item, int count, StatusCode* status, int* out);
int __stdcall FUN_00555f90(GameObjNotifier* manager, const ObjectKey* item, StatusCode* status, int* out);

// Constructors of the commands the orders create (0x57ddc0 destination, 0x57e460 rename, 0x57f230 manufacture,
// 0x57e950 kind 0x213, 0x57ed20 kind 0x216). Each initialises the Command base and returns `memory`.
Command* __thiscall FUN_0057ddc0(void* memory);
Command* __thiscall FUN_0057e460(void* memory);
Command* __thiscall FUN_0057f230(void* memory);
Command* __thiscall FUN_0057e950(void* memory);
Command* __thiscall FUN_0057ed20(void* memory);
