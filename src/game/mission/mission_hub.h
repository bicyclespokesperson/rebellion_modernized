#pragma once
#include "gameobj/game_obj.h"

// Entry points of the strategic notification hub (0x53f9c0-0x53fe40) that mission code calls; all cdecl, known only by
// shape. The ids are the event kinds (0x1f0/0x1f1 UI notifications, 0x30c/0x38c/... game events).

// Raises a game event with one value (0x53f9c0).
int FUN_0053f9c0(int eventId, GameObj* object, int value, NotifyContext* ctx);
// Raises an event that carries a tracked stat (0x53fa60).
int FUN_0053fa60(int eventId, int value, GameObj* object, void* stat, NotifyContext* ctx);
// Range check of a setter's value (0x53fc90); the message is ignored by the release build.
int FUN_0053fc90(int value, int low, int high, const char* message);
// Raises a UI notification carrying the old and new value of an integer property (0x53fcf0).
int FUN_0053fcf0(int eventId, GameObj* object, int oldValue, int newValue, NotifyContext* ctx);

inline int RaiseValueChangeEvent(int eventId, GameObj* object, int oldValue, int newValue, NotifyContext* ctx) {
    return FUN_0053fcf0(eventId, object, oldValue, newValue, ctx);
}
