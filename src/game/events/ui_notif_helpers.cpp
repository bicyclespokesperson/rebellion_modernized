#include "events/ui_notif_helpers.h"

#include <new>

ObjectKey* __thiscall FUN_00559950(void* self, ObjectKey* out);

ObjectKey* __thiscall copyFirstKey(KeyPairPayload* self, ObjectKey* out) { return FUN_00559950(self, out); }

ObjectKey* __thiscall copySecondKey(KeyPairPayload* self, ObjectKey* out) {
    new (out) ObjectKey(self->m_keyB);
    return out;
}

ObjectKey* __thiscall copySecondKeyOf(KeyPairPayload* self, ObjectKey* out) {
    copySecondKey(self, out);
    return out;
}
