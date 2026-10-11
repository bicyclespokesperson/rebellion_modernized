#pragma once
// The data blocks of Detector and CombatUnit (0x58b1a0-0x58b300); the bodies are in fleet/unit_data.cpp.
#include "gameobj/tracked_stat.h"
#include "gameobj/unit_obj_base.h"

// Data block of Detector (VT 0x66a8b0, 0x18 bytes, ctor 0x58b2c0): UnitObjData with its own vtable.
class DetectorData : public UnitObjData
{
public:
  DetectorData(); // 0x58b2c0
  ~DetectorData() override; // 0x58b2e0 (deleting), 0x58b300
};

// Data block of CombatUnit (VT 0x66a898, 0x20 bytes, ctor 0x58b1a0): adds the tracked repair level.
class CombatUnitData : public DetectorData
{
public:
  CombatUnitData(); // 0x58b1a0
  ~CombatUnitData() override; // 0x58b1f0 (deleting), 0x58b210
  int Read(istream* in) override; // 0x58b260
  int Write(ostream* out) override; // 0x58b280
  int Checksum(Crc32* crc) override; // 0x58b2a0

  TrackedStat m_repairStat; // +0x18
};
