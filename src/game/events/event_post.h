#pragma once
// How objects raise events, timers and notifications (0x53f950-0x5401a0): the setters of the game classes call these
// with the kind of what they raise. Each builds an empty instance through the factory table of its band, fills in the
// common part (the object's key, the context) and hands it to the strategic layer through a game hook:
//
//   events  0x300-0x37f   KeyedEvent family, queued on the type queue        (hook 0x2AF8, QueueEvent)
//   timers  0x380-0x396   TimerEvent, scheduled `delay` days ahead           (hook 0x2AD8, ArmTimer)
//   notifications 0x100-0x242, 0x300-0x391, 0x400-0x421, 0x500-0x541, 0x600-0x621
//                         KeyedNotifPayload leaf classes (notif_event_payloads.h), delivered at once (hook 0x2AD4)
//
// Nothing is created before the game setup is complete (g_setupComplete). The functions return 1 when there is nothing
// to do or the work was done, 0 when the kind is outside the function's band or no factory is registered.
#include <cstdint>

#include "core/game_string.h"
#include "events/event_base.h"
#include "gameobj/game_hooks.h"
#include "gameobj/game_obj_root.h"
#include "gameobj/notif_payload.h"
#include "gameobj/object_key.h"
#include "gameobj/tracked_stat.h"

class TimerEvent;

// ---- events -----------------------------------------------------------------------------------------------

// Builds the event of `kind` for `object` (a KeyedEvent: its key, the parent key, the owner key, the context and the
// object's view side) and stores it in *out. Returns 0, and deletes the event, if the object has no parent key or no
// owner key; *out is null then (and when nothing was created at all, with result 1).
// BUG(original): when the factory yields nothing (or an event whose flag is 0) the keys are still written through the
// null *out.
int CreateKeyedEvent(int kind, GameObjNotifier* object, NotifyContext* ctx, KeyedEvent** out);  // 0x53ff90

// Events 0x300-0x31f (SourceEvent): sets the two values and queues the event.
int PostSourceEvent(int kind, GameObjNotifier* object, int valueA, int valueB, NotifyContext* ctx);  // 0x53f950
// PostSourceEvent(kind, object, flag == 0, flag, ctx).
int PostSourceEventFlag(int kind, GameObjNotifier* object, int flag, NotifyContext* ctx);  // 0x53f9c0
// Events 0x320-0x323 (TwoKeyEvent): sets the two keys and queues the event.
int PostTwoKeyEvent(int kind, GameObjNotifier* object, const ObjectKey* keyC, const ObjectKey* keyD,
                    NotifyContext* ctx);  // 0x53f9f0
// Events 0x300-0x37f without any data of their own.
int PostEvent(int kind, GameObjNotifier* object, NotifyContext* ctx);  // 0x53fbd0

// ---- timers -----------------------------------------------------------------------------------------------

// Timers 0x380-0x396: builds the timer for `object` with a copy of its delay record and schedules it after
// TimerEvent::RollDelay days. Returns 0 if no timer of that kind exists.
int ArmTimer(int kind, GameObjNotifier* object, TrackedStat* stat, NotifyContext* ctx);  // 0x53fb00
// Sets the stat's flag, then arms the timer if that changed the stat and the flag is now on.
int ArmTimerAfterSetFlag(int kind, int flag, GameObjNotifier* object, TrackedStat* stat, NotifyContext* ctx);  // 0x53fa60
// Sets the stat's level, then arms the timer if that changed the stat and the flag is on.
int ArmTimerAfterSetLevel(int kind, int level, GameObjNotifier* object, TrackedStat* stat, NotifyContext* ctx);  // 0x53fab0

// ---- queueing ---------------------------------------------------------------------------------------------

int QueueEvent(EventBase* event);            // 0x53fcd0, calls the type-queue hook; returns 1
int QueueEventConfirmed(EventBase* event);   // 0x53fcb0, QueueEvent and returns 1
// Callers push an assertion text as a fourth argument; the release build ignores it.
int IsInRange(int value, int first, int last, const char* message);  // 0x53fc90, first <= value <= last

// ---- notification payloads --------------------------------------------------------------------------------

// Builds the payload of `kind` (side = the object's view side, key = the object's key, +0x10/+0x14 from the context's
// reserved words) and stores it in *out. Returns 0 if no factory is registered; nothing is built before the setup.
int CreatePayload(int kind, GameObjNotifier* object, NotifyContext* ctx, KeyedNotifPayload** out);  // 0x5400f0

// IntPairPayload kinds 0x100-0x242.
int PostPayloadInts(uint32_t kind, GameObjNotifier* object, int valueA, int valueB, NotifyContext* ctx);  // 0x53fcf0
// IntQuadPayload kinds 0x600-0x621.
int PostPayloadFourInts(uint32_t kind, GameObjNotifier* object, int valueA, int valueB, int valueC, int valueD,
                        NotifyContext* ctx);  // 0x53fd60
// IntPairPayload kinds 0x100-0x242 with (flag == 0, flag).
int PostPayloadFlag(uint32_t kind, GameObjNotifier* object, int flag, NotifyContext* ctx);  // 0x53fdd0
// KeyPairPayload kinds 0x300-0x391 / 0x400-0x421.
int PostPayloadKeysA(uint32_t kind, GameObjNotifier* object, const ObjectKey* keyA, const ObjectKey* keyB,
                     NotifyContext* ctx);  // 0x53fe40
int PostPayloadKeysB(uint32_t kind, GameObjNotifier* object, const ObjectKey* keyA, const ObjectKey* keyB,
                     NotifyContext* ctx);  // 0x53feb0
// StringPairPayload kinds 0x500-0x541.
int PostPayloadStrings(uint32_t kind, GameObjNotifier* object, const String* textA, const String* textB,
                       NotifyContext* ctx);  // 0x53ff20
