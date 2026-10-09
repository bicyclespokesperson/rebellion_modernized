#include "events/key_property_helpers.h"
#include "core/atexit_keep.h"

#include <new>

extern "C" int __cdecl atexit(void(__cdecl* function)(void));
void DestroyEmptyKeyValueSet();
void DestroyEmptyKey();

extern ObjectKeyValueSet g_emptyKeyValueSet;  // 0x6b2960
extern uint8_t g_emptyKeyValueSetBuilt;       // 0x6b2954, bit 0
extern ObjectKey g_emptyKey;                  // 0x6b2958
extern uint8_t g_emptyKeyBuilt;               // 0x6b2950, bit 0

ObjectKeyValueSet* GetEmptyKeyValueSet() {
    if ((g_emptyKeyValueSetBuilt & 1) == 0) {
        g_emptyKeyValueSetBuilt |= 1;
        new (&g_emptyKeyValueSet) ObjectKeyValueSet();
        atexit(DestroyEmptyKeyValueSet);
    }
    return &g_emptyKeyValueSet;
}

ObjectKey* __stdcall ConstructUnsetKey(ObjectKey* key) {
    new (key) ObjectKey();
    return key;
}

ObjectKey* GetEmptyKey() {
    if ((g_emptyKeyBuilt & 1) == 0) {
        g_emptyKeyBuilt |= 1;
        new (&g_emptyKey) ObjectKey();
        atexit(DestroyEmptyKey);
    }
    return &g_emptyKey;
}

void __stdcall ResetKey(ObjectKey* key) { key->Reset(); }

int __stdcall CallSlot1(void* object) {
    struct SlotOne {
        virtual void Virtual00() {}
        virtual void Virtual01() {}
    };
    static_cast<SlotOne*>(object)->Virtual01();
    return 1;
}

// ---- destructors registered with atexit ----------------------------------------------------

void DestroyEmptyKeyValueSet() { g_emptyKeyValueSet.~ObjectKeyValueSet(); }

void DestroyEmptyKey() {
    REB_KEEP_ATEXIT_TARGET();
    g_emptyKey.~ObjectKey();
}
