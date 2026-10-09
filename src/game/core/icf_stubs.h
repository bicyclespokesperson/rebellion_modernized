#pragma once
#include <cstdint>

// Tiny functions the original linker folded (identical COMDAT folding) into one body shared across unrelated
// classes. Each has one definition here; callers and the original's vtable slots all reach it.

int ReturnOne();             // 0x40f340: mov eax,1; ret (also TacticalFighter::GetUnitClass, TacticalShipBase::Virtual44)
void __thiscall IgnoreOneArgument(void* self, uintptr_t argument);  // 0x5ae1e0: ret 4 (about 200 vtable slots)
int16_t ReturnOneShort();    // 0x479ac0: mov ax,1; ret (the String terminator size)

