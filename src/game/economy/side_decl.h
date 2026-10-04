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

// A process facility (families 0x28-0x2d: shipyards, training facilities, construction yards, mines, refineries).
class ProcFacilDecl : public UnitObjBase {
public:
    int m_state;           // +0x58, 1 while the facility waits for materials
    int m_unk5c;           // +0x5c
    uint32_t m_procFlags;  // +0x60, bit 0 = excluded from allocation, bit 1 = waiting flag, bit 2 = may be released
    IntPair m_statePair;   // +0x64, {capacity, allocated} of a mine or refinery
};

// ProcFacil virtuals/helpers used by the Side code (0x53a6e0, 0x53a860, 0x55a820, 0x55a960, 0x555540).
uint32_t __thiscall FUN_0053a6e0(ProcFacilDecl* facility);
int __thiscall FUN_0053a860(ProcFacilDecl* facility, int on, NotifyContext* ctx);
int __thiscall FUN_0055a820(ProcFacilDecl* facility, int amount, IntPair* state, int* used, NotifyContext* ctx);
int __thiscall FUN_0055a960(ProcFacilDecl* facility, int amount, IntPair* state, int* used, NotifyContext* ctx);
// Member of the game root's object at +0xbc: finds the master that owns `object`.
int __thiscall FUN_00555540(GameObjNotifier* self, GameObj* object, GameObjNotifier** owner);

// A manufacturing manager (family 0xa0-0xaf, ManuMgr module).
class ManuMgrDecl;
int __thiscall FUN_00528040(ManuMgrDecl* manager, int includeFlagged);
void __thiscall FUN_0052a430(ManuMgrDecl* manager, NotifyContext* ctx);

// Next system after `system` (families 0x90-0x97, 0x51a5d0).
GameObjNotifier* __thiscall FUN_0051a5d0(GameObjNotifier* system);

// System::FindManufacturingManagerBySlot123 (galaxy module, 0x509670): the manager of a system whose slot value is `slot`.
ManuMgrDecl* __thiscall FUN_00509670(GameObjNotifier* system, int slot);

// Rules/object-type database wrappers (0x51c-0x51d).
int FUN_0051cb20(int view, uint32_t kind, int level, int whenBelow, int whenEqual, int whenAbove, IdAvlTree* out,
                 int* maxLevel);
int FUN_0051cc30(int view, int level, int* found, int* foundLevel, int* extraValue);
int FUN_0051cc60(int view, int level, int* found, int* foundLevel, int* extraValue);
int FUN_0051cc90(int view, int level, int* found, int* foundLevel, int* extraValue);
class SdRecord;
SdRecord* FUN_0051cab0(const ObjectKey* key);

// Notification hubs (0x53f-0x540): all cdecl.
int FUN_0053fa60(int eventId, int value, GameObj* object, void* stat, NotifyContext* ctx);
int FUN_0053fcf0(int eventId, GameObj* object, int oldValue, int newValue, NotifyContext* ctx);
int FUN_0053fd60(int eventId, GameObj* object, int oldFirst, int oldSecond, int newFirst, int newSecond,
                 NotifyContext* ctx);
int FUN_0053fdd0(int eventId, GameObj* object, int value, NotifyContext* ctx);

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
