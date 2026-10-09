#include "events/ui_notif_helpers.h"

#include <new>


ObjectKey* __thiscall copyFirstKey(KeyPairPayload* self, ObjectKey* out) { return reinterpret_cast<ObjectRefNode*>(self)->GetKey(out); }

ObjectKey* __thiscall copySecondKey(KeyPairPayload* self, ObjectKey* out) {
    new (out) ObjectKey(self->m_keyB);
    return out;
}

ObjectKey* __thiscall copySecondKeyOf(KeyPairPayload* self, ObjectKey* out) {
    copySecondKey(self, out);
    return out;
}
