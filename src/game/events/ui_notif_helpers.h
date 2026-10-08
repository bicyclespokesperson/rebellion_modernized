#pragma once
// Shared bodies of slot 7 (DispatchToSink) of the UI notification handlers, which the original compiled once and
// folded. BUG(original): they call slot 3 (the kind getter, which takes no arguments) with arguments and hand its
// result to a virtual of the sink; no scenario reaches them. The calls go through the raw vtable to keep that shape.
#include "events/notif_event_payloads.h"

template <typename... Args>
int callRawThiscall(void* const* vtable, int slot, void* object, Args... args) {
    using Function = int(__thiscall*)(void*, Args...);
    return reinterpret_cast<Function>(vtable[slot])(object, args...);
}

inline void* const* vtableOf(const void* object) { return *static_cast<void* const* const*>(object); }

void forwardKeysToSink(KeyPairPayload* self, void* sink);                // sink slot 7
void forwardThreeKeysToSink(KeyPairPayload* self, void* sink);           // sink slot 6
void forwardKeyAndValueToSink(IntPairPayload* self, void* sink);         // sink slot 8
void forwardKeyAndTwoValuesToSink(IntPairPayload* self, void* sink);     // sink slot 9
void forwardSelfToSink(KeyedNotifPayload* self, void* sink);                // sink slot 5

// 0x43f110: copies the first key of a key-pair payload (the helper of the handlers that name a second object).
ObjectKey* __thiscall copyFirstKey(KeyPairPayload* self, ObjectKey* out);
void forwardKeyAndFirstKeyToSink(KeyPairPayload* self, void* sink);      // sink slot 7
