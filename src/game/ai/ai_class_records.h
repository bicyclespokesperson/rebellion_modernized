#pragma once
#include <cstdint>

#include "core/entry_identity.h"
#include "core/utility_classes.h"
#include "gameobj/object_key.h"

// The AI's summary of one unit class of the data tables (0x472bc0-0x473fd0, base 0x4ad7e0). AiClassTable (ai_members.h)
// builds one record per data record of the unit families and keeps them in a tree ordered by record key.
//
//   AiClassRecordBase        VT 0x65c0a8 (0x30 bytes): m_flags describe the class, the base's slot 1 returns the stat
//                            a request mask asks for (the mask bits are the ones the selectors pass)
//   AiSpecialForceClass      VT 0x65a370 (0x40): special forces, families 0x3c-0x3f
//   AiManufacturingClass     VT 0x65a378 (0x34): manufacturing facilities, families 0x28-0x2b
//   AiDefenseClass           VT 0x65a380 (0x38): defense facilities, families 0x22-0x27
//   AiTroopClass             VT 0x65a388 (0x44): troop regiments, families 0x10-0x13
//   AiFighterClass           VT 0x65a390 (0x50): fighter squadrons, families 0x1c-0x1f
//   AiCapitalShipClass       VT 0x65a398 (0x60): capital ships, families 0x14-0x1b
//
// m_view is 1 for the classes the Alliance can build (the data record's field44 is 0) and 2 for the Empire's; the
// special forces and the facilities use 0 for classes both sides can use (field40 != 0).

class DatRecord;
class SdRecord;
class SdRecordGrid;
class SpecialForceSD;
class ManufacturingFacilitySD;
class DefenseFacilitySD;
class TroopSD;
class FighterSD;
class CapitalShipSD;

namespace ai_class_flag
{
constexpr uint32_t kSpecialForceType1 = 0x1000000;
constexpr uint32_t kSpecialForceType2 = 0x2000000;
constexpr uint32_t kSpecialForceType3 = 0x4000000;
constexpr uint32_t kSpecialForceType4 = 0x8000000;
} // namespace ai_class_flag

class AiClassRecordBase : public AvlNode
{
public:
  explicit AiClassRecordBase(const ObjectKey& key); // 0x4ad7e0
  ~AiClassRecordBase() override; // 0: 0x4ad7c0 (deleting), 0x4ad860
  virtual int GetValue(const PersistentInt* mask) = 0; // 1: the stat selected by the lowest matching mask bit

  PersistentInt m_flags; // +0x18
  int m_valueA; // +0x1c SdRecord::GetField48
  int m_valueB; // +0x20 SdRecord::GetField4c
  int m_valueC; // +0x24 SdRecord field 0x50
  int m_view; // +0x28
  ObjectKey m_key; // +0x2c the data record's key
};

class AiSpecialForceClass : public AiClassRecordBase
{
public:
  explicit AiSpecialForceClass(DatRecord* record); // 0x472be0
  ~AiSpecialForceClass() override; // 0: 0x472bc0 (deleting), 0x472c50
  int GetValue(const PersistentInt* mask) override; // 1: 0x472c60
  void Fill(SpecialForceSD* record); // 0x472cc0

  int m_diplomacy; // +0x30
  int m_espionage; // +0x34
  int m_combat; // +0x38
  int m_leadership; // +0x3c
};

class AiManufacturingClass : public AiClassRecordBase
{
public:
  explicit AiManufacturingClass(DatRecord* record); // 0x472fd0
  ~AiManufacturingClass() override; // 0: 0x472fb0 (deleting), 0x473050
  int GetValue(const PersistentInt* mask) override; // 1: 0x473060
  void Fill(ManufacturingFacilitySD* record); // 0x473090

  int m_processingRate; // +0x30
};

class AiDefenseClass : public AiClassRecordBase
{
public:
  explicit AiDefenseClass(DatRecord* record); // 0x4731c0
  ~AiDefenseClass() override; // 0: 0x4731a0 (deleting), 0x473240
  int GetValue(const PersistentInt* mask) override; // 1: 0x473250
  void Fill(DefenseFacilitySD* record); // 0x473290

  int m_attackStrength; // +0x30
  int m_shieldStrength; // +0x34
};

class AiTroopClass : public AiClassRecordBase
{
public:
  explicit AiTroopClass(DatRecord* record); // 0x473400
  ~AiTroopClass() override; // 0: 0x4733e0 (deleting), 0x473480
  int GetValue(const PersistentInt* mask) override; // 1: 0x473490
  void Fill(TroopSD* record); // 0x4734d0

  int m_attackStrength; // +0x30
  int m_defenseStrength; // +0x34
  int m_bombardmentDefense; // +0x38
  int m_detection; // +0x3c
  int m_unused40; // +0x40 never written by the original
};

class AiFighterClass : public AiClassRecordBase
{
public:
  explicit AiFighterClass(DatRecord* record); // 0x473570
  ~AiFighterClass() override; // 0: 0x473550 (deleting), 0x473600
  int GetValue(const PersistentInt* mask) override; // 1: 0x473640
  void Fill(SdRecordGrid* record); // 0x4736c0
  // Per-squadron-size scalings of the stats (the amount is in the units of m_squadronSize).
  int ScaleX30(int amount); // 0x473610: m_x30 * amount / m_squadronSize
  int ScaleX34(int amount); // 0x473620
  int ScaleX38(int amount); // 0x473630

  int m_x30; // +0x30
  int m_x34; // +0x34
  int m_x38; // +0x38
  int m_x3c; // +0x3c
  int m_x40; // +0x40
  int m_x44; // +0x44
  int m_x48; // +0x48
  int m_squadronSize; // +0x4c
};

class AiCapitalShipClass : public AiClassRecordBase
{
public:
  explicit AiCapitalShipClass(DatRecord* record); // 0x473830
  ~AiCapitalShipClass() override; // 0: 0x473810 (deleting), 0x4738c0
  int GetValue(const PersistentInt* mask) override; // 1: 0x473930
  void Fill(SdRecordGrid* record); // 0x4739b0
  int ScaleA(int amount); // 0x4738d0: m_x30 * amount / m_x50 (0 if m_x50 <= 0)
  int ScaleB(int amount); // 0x4738f0: m_x34 * amount / m_x50
  int ScaleC(int amount); // 0x473910: m_x3c * amount / m_x54

  int m_x30; // +0x30
  int m_x34; // +0x34
  int m_x38; // +0x38
  int m_x3c; // +0x3c
  int m_x40; // +0x40
  int m_x44; // +0x44
  int m_x48; // +0x48
  int m_x4c; // +0x4c
  int m_x50; // +0x50
  int m_x54; // +0x54
  int m_x58; // +0x58
  int m_x5c; // +0x5c
};
