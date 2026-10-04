#pragma once
// Minimal stand-ins for capital ship / combat unit entry points that the fleet module calls. Owned by the capital ship
// module (0x4fffc0-0x503ae0, 0x557410-0x558460); drop this header once its own header provides them.
#include "gameobj/game_obj_root.h"

// CombatUnit virtual slot 119 (0x501470 for capital ships: the difference of its slots 0x25c and 0x288): the per-unit
// value Fleet::SumUnitStat119 adds up. Called through the vtable because the unit classes are not declared yet.
inline int CallUnitSlot119(GameObjNotifier* unit) {
    using Slot119 = int(__thiscall*)(void*);
    void** vtable = *reinterpret_cast<void***>(unit);
    return reinterpret_cast<Slot119>(vtable[119])(unit);
}

// 0x5006f0: capital ship method that looks through the ship's children for the one whose short at +0x96 equals
// `number`; stores it (or null) in *out and returns 1.
int __thiscall FUN_005006f0(GameObjNotifier* ship, int number, GameObjNotifier** out);

// 0x5007a0: capital ship method that stores a count in *out (it always stores 0) and returns 1; ignores two more
// arguments (ret 0xc).
int __thiscall FUN_005007a0(GameObjNotifier* ship, int* out, int unused1, int unused2);
