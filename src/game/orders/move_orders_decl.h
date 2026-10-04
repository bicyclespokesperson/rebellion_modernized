#pragma once
// STAND-IN declarations for classes and functions owned by other modules that the move orders (move_orders.h) use.
// Drop each part when its owner reconstructs it.
#include <cstdint>

#include "core/game_string.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_key.h"
#include "gameobj/status_code.h"
#include "orders/command_decl.h"
#include "orders/order_base.h"

// The 0x241 order (mission order base 0x4f4690, ctor 0x4f5440, VT 0x65d170, 0x60 bytes) the move builds for the
// characters and special forces of a team. Only the fields the move order sets are named.
class MissionOrder : public OrderBase {
public:
    int m_unk44;                     // +0x44
    int m_unk48;                     // +0x48
    ObjectKey m_missionKey;          // +0x4c
    int m_unk50;                     // +0x50
    int m_unk54;                     // +0x54
    ObjectKeyValueSet m_characters;  // +0x58
};

// The commands the move/rename orders queue (event base 0x54eee0 / 0x54f2c0): the object a command acts on and the
// copy of the order's request.
class ObjectCommand : public Command {
public:
    uint8_t m_unk18[0x24];       // +0x18
    ObjectKey m_object;          // +0x3c
    ActivityRequest m_request;   // +0x40
};

// The rename command (ctor 0x5790f0, VT 0x669580, 0x5c bytes): the object command plus the new name.
class RenameCommand : public ObjectCommand {
public:
    String m_name;  // +0x50
};

// Route planning (0x550620-0x552d10, owned by the route module): the context a move builds once per order, and the
// per-object leg plan.
class RouteContext;
class RouteLegPlan;
RouteContext* __thiscall FUN_00551060(void* memory);  // ctor, 0x24 bytes
void __thiscall FUN_005510b0(RouteContext* context);  // dtor
int __thiscall FUN_00551190(RouteContext* context, ActivityRequest* request, uint32_t side, int* mode);
int __thiscall FUN_005513a0(RouteContext* context, ObjectKey* destination);
RouteLegPlan* __thiscall FUN_0054fa40(void* memory);  // ctor, 0x1c bytes
void __thiscall FUN_0054fa90(RouteLegPlan* plan);     // dtor
int __thiscall FUN_00551630(RouteContext* context, ObjectKey* object, ObjectKey* destination, StatusCode* status,
                            RouteLegPlan* plan, ObjectKey* leg);

// Order checks (0x553aa0-0x555d30).
int FUN_00553aa0(ActivityRequest* request, ObjectKeyValueSet* team, StatusCode* status);  // cdecl
int FUN_00553b80(CommandQueue* commands, uint32_t* side);                                 // cdecl
int __stdcall FUN_00555d30(GameObjNotifier* object, ObjectKey* key, ObjectKey* target, int* days);

// Mission order (0x4f4560...).
MissionOrder* __thiscall FUN_004f5440(void* memory);
int __thiscall FUN_004f4850(MissionOrder* order, StatusCode* status, ObjectKeyValueSet* out);

// Command constructors (0x578-0x579 module): move, rename, scrap (0x200), ready-for-delete (0x205).
Command* __thiscall FUN_00578ab0(void* memory);
Command* __thiscall FUN_005790f0(void* memory);
Command* __thiscall FUN_00579570(void* memory);
Command* __thiscall FUN_00579940(void* memory);
