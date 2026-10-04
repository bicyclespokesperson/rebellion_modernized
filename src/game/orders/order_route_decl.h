#pragma once
// STAND-IN declarations for code owned by neighbouring modules (route planning 0x54f830-0x554020, the manufacturing
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

// The route planner's working state (0x551060, 0x24 bytes).
void __thiscall FUN_00551060(void* context);
void __thiscall FUN_005510b0(void* context);
class RouteContext {
public:
    RouteContext() { FUN_00551060(this); }
    ~RouteContext() { FUN_005510b0(this); }

    uint8_t m_data[0x24];
};

// One planned leg (0x54fa40 / 0x54fa90, 0x1c bytes).
void __thiscall FUN_0054fa40(void* leg);
void __thiscall FUN_0054fa90(void* leg);
class RouteLeg {
public:
    RouteLeg() { FUN_0054fa40(this); }
    ~RouteLeg() { FUN_0054fa90(this); }

    uint8_t m_data[0x1c];
};

// Plans the route for `request` for `side`; *mode is read and updated.
int __thiscall FUN_00551190(RouteContext* context, ActivityRequest* request, uint32_t side, int* mode);
// Checks that `destination` can be reached with the planned route.
int __thiscall FUN_005513a0(RouteContext* context, const ObjectKey* destination);
// Plans one leg for a unit (0x551900) / an item (0x551a20): fills `leg` and `legKey`.
int __thiscall FUN_00551900(RouteContext* context, const ObjectKey* unit, const ObjectKey* destination, StatusCode* status,
                            RouteLeg* leg, ObjectKey* legKey);
int __thiscall FUN_00551a20(RouteContext* context, const ObjectKey* item, const ObjectKey* origin, StatusCode* status,
                            RouteLeg* leg, ObjectKey* legKey);
// Per-manager route checks.
int __thiscall FUN_00552210(RouteContext* context, GameObjNotifier* manager, StatusCode* status);
// Fills *side with the control side of the order's team (first command's object); 0 if it is not 1 or 2.
int FUN_00553b80(CommandQueue* commands, uint32_t* side);
int FUN_00553410(GameObjNotifier* destination, int kind, uint32_t side, int a, int b, StatusCode* status);

// Manufacturing manager (family 0xa0-0xaf) pieces.
// The set of objects the manager's facility list holds (a static empty one for a sided object).
ObjectKeyValueSet* __thiscall FUN_00528820(GameObjNotifier* manager);
int __thiscall FUN_00528040(GameObjNotifier* manager, int includeBusy);
ObjectKey* __thiscall FUN_004c27b0(GameObjNotifier* manager, ObjectKey* out);  // the key at +0x7c
int __thiscall FUN_0052e970(GameObjNotifier* side, const NullableKey* item, int count, StatusCode* status);
int __thiscall FUN_0052e580(GameObjNotifier* side, int kind, AvlTree* choices);
int __stdcall FUN_00555f00(GameObjNotifier* manager, const ObjectKey* item, int count, StatusCode* status, int* out);
int __stdcall FUN_00555f90(GameObjNotifier* manager, const ObjectKey* item, StatusCode* status, int* out);

// Commands (0x54eee0 base): the object key a command is about, at +0x3c.
ObjectKey* __thiscall FUN_004fa150(Command* command, ObjectKey* out);
// Adds {a, b} to the key-pair tree at command +0x50 of the manufacture command.
int __thiscall FUN_0053e020(void* tree, const ObjectKey* a, const ObjectKey* b);

// Constructors of the commands the orders create (0x57ddc0 destination, 0x57e460 rename, 0x57f230 manufacture,
// 0x57e950 kind 0x213, 0x57ed20 kind 0x216). Each initialises the Command base and returns `memory`.
Command* __thiscall FUN_0057ddc0(void* memory);
Command* __thiscall FUN_0057e460(void* memory);
Command* __thiscall FUN_0057f230(void* memory);
Command* __thiscall FUN_0057e950(void* memory);
Command* __thiscall FUN_0057ed20(void* memory);
