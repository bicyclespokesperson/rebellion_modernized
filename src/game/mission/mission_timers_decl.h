#pragma once
// STAND-IN declarations for the event/timer bases (events subsystem, 0x54eee0-0x54f830 and 0x585fe0-0x5864c0) and for
// the mission manager (family 0xf8, 0x541fc0-0x54cd80) that the mission commands, timers and events call. Drop each
// part when its owner reconstructs it; the leaf classes in mission_events.h only need the bases for the layout, the
// vtable slots and the constructors.
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "core/stream_io.h"
#include "gameobj/game_hooks.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_key.h"
#include "gameobj/status_code.h"
#include "gameobj/tracked_stat.h"

class Crc32;
class TimerBase;
class NotifEventBase;

// Shared slot bodies of the timer base (the leaves inherit them unchanged).
void __thiscall FUN_00586100(TimerBase* self, istream* in);
void __thiscall FUN_00586110(TimerBase* self, ostream* out);
int __thiscall FUN_00586120(TimerBase* self, Crc32* crc);
int __thiscall FUN_00586150(TimerBase* self, StatusCode* status);
int __thiscall FUN_00586290(TimerBase* self, StatusCode* status);
int __thiscall FUN_005862a0(TimerBase* self);

// Slot bodies the two notification events share.
void __thiscall FUN_00541180(NotifEventBase* self, istream* in);
void __thiscall FUN_00564d50(NotifEventBase* self, ostream* out);
int __thiscall FUN_00541700(NotifEventBase* self, Crc32* crc);

// The common head of the scheduled items (0x54eee0 through 0x54f1b0): a tree node with the context the item runs with
// and the object it acts on. Commands (orders/command_decl.h) share the head but sit on the request holder 0x54f2c0.
class ScheduledItem : public AvlNode {
public:
    int m_flag;               // +0x18, 1
    int m_unk1c;              // +0x1c
    NotifyContext m_context;  // +0x20
    ObjectKey m_key;          // +0x3c, the object the item acts on
    int m_unk40;              // +0x40
};

// Timer base (0x585fe0, VT 0x66a230, 0x4c bytes): slot 8 runs the checks of slots 6 and 7, then fires when the change
// count of the stat returned by GetStat equals m_expectedChange.
class TimerBase : public ScheduledItem {
public:
    TimerBase();                        // 0x585fe0
    TimerBase(const TimerBase& other);  // 0x5860a0
    virtual ~TimerBase();               // 0x586050 (deleting 0x586030)
    virtual TimerBase* Clone() = 0;     // 1
    virtual void Read(istream* in) { FUN_00586100(this, in); }            // 2
    virtual void Write(ostream* out) { FUN_00586110(this, out); }         // 3
    virtual int Checksum(Crc32* crc) { return FUN_00586120(this, crc); }        // 4
    virtual int IsChecked() { return 0; }                                       // 5
    virtual int CheckEnabled(StatusCode* status) { return FUN_00586150(this, status); }  // 6
    virtual int CheckValid(StatusCode* status) { return FUN_00586290(this, status); }    // 7
    virtual int Execute() { return FUN_005862a0(this); }                        // 8
    virtual int GetKind() = 0;                                                  // 9
    virtual FamilyRange* GetFamilyRange(FamilyRange* out) = 0;                  // 10: families the stat's owner is in
    virtual TrackedStat* GetStat() = 0;                                         // 11
    virtual int Fire() = 0;                                                     // 12

    int m_expectedChange;  // +0x44
    uint32_t m_schedule;   // +0x48
};

// Base of the notification events (0x5863b0, VT 0x66a268, 0x54 bytes), over the keyed event base 0x594a30.
class NotifEventBase : public AvlNode {
public:
    NotifEventBase();                             // 0x5863b0
    NotifEventBase(const NotifEventBase& other);  // 0x586400
    virtual ~NotifEventBase();                    // 0x5863f0 (deleting 0x5863d0)
    virtual NotifEventBase* Clone() = 0;          // 1
    virtual void Read(istream* in) = 0;           // 2
    virtual void Write(ostream* out) = 0;         // 3
    virtual int Checksum(Crc32* crc) = 0;         // 4
    virtual int IsChecked() { return 0; }         // 5
    virtual int CheckEnabled(StatusCode* status) = 0;  // 6
    virtual int CheckValid(StatusCode* status) = 0;    // 7
    virtual int Execute() = 0;                         // 8
    virtual int GetKind() = 0;                         // 9
    virtual int GetSecondaryKind() { return 0; }       // 10, 0x48af30

    int m_flag;               // +0x18, 1
    int m_unk1c;              // +0x1c
    NotifyContext m_context;  // +0x20
    ObjectKey m_key;          // +0x3c, the object the event is about
    ObjectKey m_key40;        // +0x40
    ObjectKey m_key44;        // +0x44
    int m_unk48;              // +0x48
    int m_unk4c;              // +0x4c
    int m_unk50;              // +0x50
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(ScheduledItem, 0x44);
REB_ASSERT_OFFSET(ScheduledItem, m_context, 0x20);
REB_ASSERT_OFFSET(ScheduledItem, m_key, 0x3c);
REB_ASSERT_SIZE(TimerBase, 0x4c);
REB_ASSERT_OFFSET(TimerBase, m_expectedChange, 0x44);
REB_ASSERT_SIZE(NotifEventBase, 0x54);
REB_ASSERT_OFFSET(NotifEventBase, m_unk4c, 0x4c);
#pragma clang diagnostic pop

// Calls virtual slot `slot` of an object whose class is not declared here (thiscall, `this` passed first).
template <typename Ret, typename... Args>
Ret CallVirtualSlot(void* object, int slot, Args... args) {
    using Function = Ret(__thiscall*)(void*, Args...);
    void** table = *reinterpret_cast<void***>(object);
    return reinterpret_cast<Function>(table[slot])(object, args...);
}

// ---- the mission manager (family 0xf8, key 0xf8000006), reached through the game root's cached master --------------

class MissionMgr;
// Slot 111 of the mission manager creates a mission; 0x5429e0 / 0x542b60 are the two checks that precede it.
constexpr int kMissionMgrCreateMissionSlot = 111;
int __thiscall FUN_005429e0(MissionMgr* self, const ActivityRequest* request, int a, int b, ObjectKeyValueSet* team,
                            ObjectKeyValueSet* characters, StatusCode* status);
int __thiscall FUN_00542b60(MissionMgr* self, const ActivityRequest* request, int a, int b, ObjectKey* missionKey,
                            ObjectKey* secondKey, ObjectKeyValueSet* team, ObjectKeyValueSet* characters,
                            StatusCode* status);
int __thiscall FUN_00546e70(MissionMgr* self, ObjectKey* key, int value, NotifyContext* ctx);
int __thiscall FUN_00546ea0(MissionMgr* self, ObjectKey* key, int a, int b, NotifyContext* ctx);
int __thiscall FUN_00546f80(MissionMgr* self, NotifyContext* ctx);
int __thiscall FUN_00546fa0(MissionMgr* self, NotifyContext* ctx);

class Mission;
// Mission::ResetImpliedTeamIfPhase8 / ResumeFromPhase11 take the notify context (mission.h declares the first without).
int __thiscall FUN_00522a10(Mission* self, NotifyContext* ctx);
int __thiscall FUN_00522a30(Mission* self, NotifyContext* ctx);
