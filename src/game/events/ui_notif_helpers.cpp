#include "core/reb_abi.h"
#include "events/ui_notif_helpers.h"

#include <new>


// The original calls ObjectRefNode::GetKey on the payload (the linker folded it with an identical accessor of the key at +0x24,
// which is m_keyA here); that is a read through another class's layout, so the field is named instead.
ObjectKey* REB_THISCALL copyFirstKey(KeyPairPayload* self, ObjectKey* out) {
    new (out) ObjectKey(self->m_keyA);
    return out;
}

ObjectKey* REB_THISCALL copySecondKey(KeyPairPayload* self, ObjectKey* out) {
    new (out) ObjectKey(self->m_keyB);
    return out;
}

ObjectKey* REB_THISCALL copySecondKeyOf(KeyPairPayload* self, ObjectKey* out) {
    copySecondKey(self, out);
    return out;
}
