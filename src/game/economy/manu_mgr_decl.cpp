#include "economy/manu_mgr_decl.h"

// DROP AT MERGE together with manu_mgr_decl.h: slot 129 of the manufacturing facility classes, called through the
// vtable until their header exists.
int GetFacilityProductKind(GameObj* facility) {
    using KindGetter = int(__thiscall*)(void*);
    void** vtable = *reinterpret_cast<void***>(facility);
    return reinterpret_cast<KindGetter>(vtable[129])(facility);
}
