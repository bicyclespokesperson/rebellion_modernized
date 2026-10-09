#pragma once
// STAND-IN declarations for classes and functions owned by other modules that the move orders (move_orders.h) use.
// Drop each part when its owner reconstructs it.
#include <cstdint>

#include "core/game_string.h"
#include "events/command_classes2.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_key.h"
#include "gameobj/status_code.h"
#include "orders/command_decl.h"
#include "orders/mission_orders.h"
#include "orders/order_base.h"

// Order checks (0x555d30).
int __stdcall FUN_00555d30(GameObjNotifier* object, ObjectKey* key, ObjectKey* target, int* days);

// Command constructors (0x578-0x579 module): move, rename, scrap (0x200), ready-for-delete (0x205).
Command* __thiscall FUN_00578ab0(void* memory);
Command* __thiscall FUN_005790f0(void* memory);
Command* __thiscall FUN_00579570(void* memory);
Command* __thiscall FUN_00579940(void* memory);
