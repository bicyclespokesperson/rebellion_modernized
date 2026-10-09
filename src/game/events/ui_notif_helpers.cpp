#include "core/reb_abi.h"
#include "events/ui_notif_helpers.h"

#include <new>


ObjectKey* REB_THISCALL copyFirstKey(KeyPairPayload* self, ObjectKey* out) { return reinterpret_cast<ObjectRefNode*>(self)->GetKey(out); }

ObjectKey* REB_THISCALL copySecondKey(KeyPairPayload* self, ObjectKey* out) {
    new (out) ObjectKey(self->m_keyB);
    return out;
}

ObjectKey* REB_THISCALL copySecondKeyOf(KeyPairPayload* self, ObjectKey* out) {
    copySecondKey(self, out);
    return out;
}
