#pragma once
// Shared helpers of the UI notification handlers (events/ui_notifs.h).
#include <cstdint>

#include "events/notif_event_payloads.h"

// The slots 5-9 of the notices take their data as 32-bit arguments; the pointers among them are passed as such.
inline int asArg(const void* pointer) { return static_cast<int>(reinterpret_cast<uintptr_t>(pointer)); }

// 0x43f110: copies the first key of a key-pair payload (the helper of the handlers that name a second object).
ObjectKey* __thiscall copyFirstKey(KeyPairPayload* self, ObjectKey* out);

// 0x48f670: copies the second key of a key-pair payload; 0x494ed0 forwards to it.
ObjectKey* __thiscall copySecondKey(KeyPairPayload* self, ObjectKey* out);
ObjectKey* __thiscall copySecondKeyOf(KeyPairPayload* self, ObjectKey* out);
