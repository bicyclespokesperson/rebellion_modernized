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

// The capital ship / fighter vtable as seen by the tactical AI (only the slots it calls are typed).
class UnitVirtuals {
public:
    virtual void Slot00();
    virtual void Slot01();
    virtual void Slot02();
    virtual void Slot03();
    virtual void Slot04();
    virtual void Slot05();
    virtual void Slot06();
    virtual void Slot07();
    virtual int GetUnitClass();  // 8
    virtual void Slot09();
    virtual void Slot10();
    virtual void Slot11();
    virtual void Slot12();
    virtual void Slot13();
    virtual void Slot14();
    virtual void Slot15();
    virtual void Slot16();
    virtual void Slot17();
    virtual void Slot18();
    virtual void Slot19();
    virtual void Slot20();
    virtual void Slot21();
    virtual void Slot22();
    virtual void Slot23();
    virtual void Slot24();
    virtual float Slot25();  // +0x64
    virtual void Slot26();
    virtual void Slot27();
    virtual void Slot28();
    virtual void Slot29();
    virtual void Slot30();
    virtual void Slot31();
    virtual void Slot32();
    virtual void Slot33();
    virtual void Slot34();
    virtual void Slot35();
    virtual float Slot36();  // +0x90
    virtual void Slot37();
    virtual void Slot38();
    virtual void Slot39();
};

inline UnitVirtuals* unitVirtuals(SimUnit* unit) { return reinterpret_cast<UnitVirtuals*>(unit); }
inline int& unitState(SimUnit* unit) { return unitField<int>(unit, kUnitState); }
inline int& unitSide(SimUnit* unit) { return unitField<int>(unit, kUnitSide); }
inline int& unitCarrierHandle(SimUnit* unit) { return unitField<int>(unit, kUnitCarrierHandle); }

// The Death Star (a TacticalUnit pointer in the registry) is class TacticalDeathStar of the Death Star module.
inline int& deathStarField(TacticalUnit* deathStar, int offset) {
    return *reinterpret_cast<int*>(reinterpret_cast<char*>(deathStar) + offset);
}
inline float deathStarFloat(TacticalUnit* deathStar, int offset) {
    return *reinterpret_cast<float*>(reinterpret_cast<char*>(deathStar) + offset);
}
