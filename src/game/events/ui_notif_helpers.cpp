#include "events/ui_notif_helpers.h"

void forwardKeysToSink(KeyPairPayload* self, void* sink) {
    if (!sink) return;
    void* const* sinkVtable = vtableOf(sink);
    int result = callRawThiscall(vtableOf(self), 3, self, &self->m_key, &self->m_keyA);
    callRawThiscall(sinkVtable, 7, sink, result);
}

void forwardThreeKeysToSink(KeyPairPayload* self, void* sink) {
    if (!sink) return;
    void* const* sinkVtable = vtableOf(sink);
    int result = callRawThiscall(vtableOf(self), 3, self, &self->m_key, &self->m_keyA, &self->m_keyB);
    callRawThiscall(sinkVtable, 6, sink, result);
}

void forwardKeyAndValueToSink(IntPairPayload* self, void* sink) {
    if (!sink) return;
    void* const* sinkVtable = vtableOf(sink);
    int result = callRawThiscall(vtableOf(self), 3, self, &self->m_key, self->m_valueB);
    callRawThiscall(sinkVtable, 8, sink, result);
}

void forwardKeyAndTwoValuesToSink(IntPairPayload* self, void* sink) {
    if (!sink) return;
    void* const* sinkVtable = vtableOf(sink);
    int result = callRawThiscall(vtableOf(self), 3, self, &self->m_key, self->m_valueB, self->m_valueA);
    callRawThiscall(sinkVtable, 9, sink, result);
}

void forwardSelfToSink(KeyedNotifPayload* self, void* sink) {
    if (!sink) return;
    void* const* sinkVtable = vtableOf(sink);
    int result = callRawThiscall(vtableOf(self), 3, self);
    callRawThiscall(sinkVtable, 5, sink, result);
}

ObjectKey* __thiscall FUN_00559950(void* self, ObjectKey* out);

ObjectKey* __thiscall copyFirstKey(KeyPairPayload* self, ObjectKey* out) { return FUN_00559950(self, out); }

void forwardKeyAndFirstKeyToSink(KeyPairPayload* self, void* sink) {
    if (!sink) return;
    void* const* sinkVtable = vtableOf(sink);
    ObjectKey keyA;
    int result = callRawThiscall(vtableOf(self), 3, self, &self->m_key, copyFirstKey(self, &keyA));
    callRawThiscall(sinkVtable, 7, sink, result);
}
