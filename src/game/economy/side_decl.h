#pragma once
// Minimal declarations of classes and functions owned by other modules (ManuMgr, facilities, galaxy, notification
// hubs, rules). Dropped at merge once the owning modules provide real headers.
#include <cstdint>

#include "core/containers.h"
#include "data/gnprtb.h"
#include "data/random.h"
#include "gameobj/game_hooks.h"
#include "gameobj/object_key.h"
#include "gameobj/status_code.h"
#include "gameobj/unit_obj_base.h"

// Member of the game root's object at +0xbc: finds the master that owns `object`.
int __thiscall FUN_00555540(GameObjNotifier* self, GameObj* object, GameObjNotifier** owner);

// A manufacturing manager (family 0xa0-0xaf, ManuMgr module).
class ManuMgrDecl;
int __thiscall FUN_00528040(ManuMgrDecl* manager, int includeFlagged);
void __thiscall FUN_0052a430(ManuMgrDecl* manager, NotifyContext* ctx);

// System::FindManufacturingManagerBySlot123 (galaxy module, 0x509670): the manager of a system whose slot value is `slot`.
ManuMgrDecl* __thiscall FUN_00509670(GameObjNotifier* system, int slot);

// Rules/object-type database wrappers (0x51c-0x51d).
class SdRecord;

// Timed event classes the Side schedules (0x5777c0, 0x577b00); both are 0x40 bytes and derive from the event base
// (0x54f4e0). m_ok is the base class's construction flag.
class SideTimedEvent {
public:
    virtual ~SideTimedEvent();
    uint8_t m_unk04[0x14];  // +0x04
    int m_ok;               // +0x18
    uint8_t m_unk1c[0x20];  // +0x1c
    ObjectKey m_targetKey;  // +0x3c
};
SideTimedEvent* __thiscall FUN_005777c0(void* memory);
SideTimedEvent* __thiscall FUN_00577b00(void* memory);
