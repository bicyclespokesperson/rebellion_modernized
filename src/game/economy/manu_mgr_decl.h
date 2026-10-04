#pragma once
// Minimal declarations of classes owned by the facilities slice, so that ManuMgr compiles on its own. DROP AT MERGE
// (include economy/manufacturing_facility.h instead and use its Facility members).
#include <cstdint>

#include "gameobj/child_iterator.h"
#include "gameobj/game_obj.h"

// Iterator over the manufacturing facilities (families 0x28-0x2c) below an object, VT 0x660698; filter 1 (usable).
class ManufacturingFacilityIterator : public ChildIterator {
public:
    ManufacturingFacilityIterator(GameObjNotifier* parent);  // 0x52cda0
    virtual ~ManufacturingFacilityIterator();                // 0x52cec0 (not hooked here)
};

// The fields of a manufacturing facility that ManuMgr reads (GameObj is 0x58 bytes).
struct ManufacturingFacilityFields {
    uint8_t m_gameObj[0x58];
    int m_mode;       // +0x58: 1-3 working, 4 idle
    int m_progress;   // +0x5c
    uint32_t m_flags;  // +0x60, bit 0 set = facility out of service
};

inline ManufacturingFacilityFields* AsFacilityFields(GameObjNotifier* facility) {
    return reinterpret_cast<ManufacturingFacilityFields*>(facility);
}

// Virtual slot 129 of the manufacturing facility classes: the kind of product it builds (compared with
// ManuMgr::GetFacilityKind()).
int GetFacilityProductKind(GameObj* facility);

// Facility methods that ManuMgr's rebalancing calls (0x53a670 region): put an idle facility to work / release one.
int __thiscall FUN_0053aac0(void* facility, NotifyContext* ctx);
int __thiscall FUN_0053ab10(void* facility, NotifyContext* ctx);
