#pragma once
// DROP AT MERGE if the owner of 0x58adb0-0x58b960 declares these: the data blocks of Detector and CombatUnit live
// outside the fleet module's ranges, so only their layout and the members the fleet code calls are declared here.
// None of them is reconstructed by the fleet module; the calls go to the original code.
#include "core/layout.h"
#include "gameobj/tracked_stat.h"
#include "gameobj/unit_obj_base.h"

// Data block of Detector (VT 0x66a8b0, 0x18 bytes, ctor 0x58b2c0): UnitObjData with its own vtable.
class DetectorData : public UnitObjData {
public:
    DetectorData();           // 0x58b2c0
    virtual ~DetectorData();  // 0x58b2e0 (deleting), 0x58b300
};

// Data block of CombatUnit (VT 0x66a898, 0x20 bytes, ctor 0x58b1a0): adds the tracked repair level.
class CombatUnitData : public DetectorData {
public:
    CombatUnitData();                                // 0x58b1a0
    virtual ~CombatUnitData();                       // 0x58b1f0 (deleting), 0x58b210
    virtual int Read(istream* in) override;          // 0x58b260
    virtual int Write(ostream* out) override;        // 0x58b280
    virtual int Checksum(Crc32* crc) override;       // 0x58b2a0

    TrackedStat m_repairStat;  // +0x18
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(DetectorData, 0x18);
REB_ASSERT_SIZE(CombatUnitData, 0x20);
REB_ASSERT_OFFSET(CombatUnitData, m_repairStat, 0x18);
#pragma clang diagnostic pop
