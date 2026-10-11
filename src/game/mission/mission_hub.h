#pragma once
#include "events/event_post.h"
#include "gameobj/game_obj.h"

// Entry points of the strategic notification hub (0x53f9c0-0x53fe40) that mission code calls; all cdecl, known only by
// shape. The ids are the event kinds (0x1f0/0x1f1 UI notifications, 0x30c/0x38c/... game events).

inline int RaiseValueChangeEvent(int eventId, GameObj* object, int oldValue, int newValue, NotifyContext* ctx)
{
  return PostPayloadInts(eventId, object, oldValue, newValue, ctx);
}
