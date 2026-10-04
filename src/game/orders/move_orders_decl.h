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

// The rename command (ctor 0x5790f0, VT 0x669580, 0x5c bytes): a command plus the new name.
class RenameCommand : public Command {
public:
    String m_name;  // +0x50
};

// Order checks (0x555d30).
int __stdcall FUN_00555d30(GameObjNotifier* object, ObjectKey* key, ObjectKey* target, int* days);

// Mission order (0x4f4560...).
MissionOrder* __thiscall FUN_004f5440(void* memory);
int __thiscall FUN_004f4850(MissionOrder* order, StatusCode* status, ObjectKeyValueSet* out);

// Command constructors (0x578-0x579 module): move, rename, scrap (0x200), ready-for-delete (0x205).
Command* __thiscall FUN_00578ab0(void* memory);
Command* __thiscall FUN_005790f0(void* memory);
Command* __thiscall FUN_00579570(void* memory);
Command* __thiscall FUN_00579940(void* memory);
