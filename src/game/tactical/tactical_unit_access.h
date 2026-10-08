#pragma once
#include <cstdint>

#include "tactical/tactical_simulator.h"

// Field access to the capital-ship / fighter objects (SimUnit) for code outside the capital-ship and fighter modules,
// which own the real class layouts. Replace with member accesses when those classes are shared.

template <class T>
inline T& unitField(SimUnit* unit, int offset) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(unit) + offset);
}

enum UnitFieldOffset {
    kUnitHandle = 0xec,        // the registry handle (the TacticalUnit sub-object at +0xd4 holds it as its id)
    kUnitCarrierHandle = 0x164,
    kUnitSide = 0x168,
    kUnitState = 0x170,
    kUnitGroupId = 0x174,
};

inline int unitHandle(SimUnit* unit) { return unitField<int>(unit, kUnitHandle); }
inline int& unitGroupId(SimUnit* unit) { return unitField<int>(unit, kUnitGroupId); }
