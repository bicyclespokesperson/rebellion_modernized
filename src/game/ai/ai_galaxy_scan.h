#pragma once
#include <cstdint>

#include "ai/ai_members.h"
#include "ai/ai_ranked_list.h"
#include "core/entry_identity.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "gameobj/object_key.h"

// The galaxy analysis the player's advisors work from (0x4c5450-0x4c7e10, 0x4ebae0-0x4ec920, owned by AiGalaxyAnalysis at
// 0x49c6a0-0x49ecb0). It is a smaller twin of what the AI's side controller keeps (AiSectorEvalSet/AiSystemEvalSet): a record
// of every system with flag words and counters, a record of every sector summing up its up to ten systems, and sets of them
// that are built step by step, saved and kept up to date by marking the records of changed objects dirty.

class AiGalaxyAnalysis;
class AiSectorScan;
class AiSystemScanSet;
class Sector;
class System;

// ---- AiSystemScan -----------------------------------------------------------------------------------------------------

// What the advisors know of one system (VT 0x65ca10, 0x9c bytes).
class AiSystemScan : public ObjectKeyNode
{
public:
  static constexpr int kStatCount = 0x17;

  AiSystemScan(); // 0x4ebae0
  explicit AiSystemScan(System* system); // 0x4ebb90
  ~AiSystemScan() override; // 0: 0x4ebb70 (deleting), 0x4ebc50
  // slot 1 Clone is ObjectKeyNode::Clone (0x4ecda0)
  void Load(istream* in) override; // 2: 0x4ebcc0
  void Save(ostream* out) override; // 4: 0x4ebd80

  void MarkDirty(); // 0x4ebe30: dirty, and so is the sector's record
  void Scan(int view); // 0x4ebe40: re-reads the system if it is dirty

  int& Stat(int offset)
  {
    return m_stat[(offset - 0x40) / 4];
  }

  int m_dirty; // +0x1c
  int m_controlSide; // +0x20 0 neutral, 1/2 the sides
  PersistentInt m_flagsA; // +0x24
  PersistentInt m_flagsB; // +0x28
  int16_t m_x; // +0x2c packed map position of the system
  int16_t m_y; // +0x2e
  AiSectorScan* m_sector; // +0x30
  AiClassTable* m_classes; // +0x34
  int m_x38; // +0x38 (-1)
  int m_scanDay; // +0x3c the day a blockade of the system was last seen
  int m_stat[kStatCount]; // +0x40
};

// ---- AiSectorScan -----------------------------------------------------------------------------------------------------

// What the advisors know of one sector: pointers to the records of its systems and the sums of what they hold (VT 0x65c610,
// 0xc8 bytes).
class AiSectorScan : public ObjectKeyNode
{
public:
  static constexpr int kSystemCount = 10;
  static constexpr int kStatCount = 0x1a;

  AiSectorScan(); // 0x4c5e20
  explicit AiSectorScan(int view); // 0x4c5ec0
  ~AiSectorScan() override; // 0: 0x4c5ea0 (deleting), 0x4c5f40
  // slot 2 Load is ObjectKeyNode::Load (0x58f0c0); the record is read by ReadRecord
  void Save(ostream* out) override; // 4: 0x4c61d0

  void Attach(Sector* sector, AiSystemScanSet* systems); // 0x4c5fc0
  void ReadRecord(istream* in, AiSystemScanSet* systems); // 0x4c60d0
  // The systems whose flags carry all the require bits and none of the forbid bits, as a ranked list of their keys by the
  // statistic `statIndex`.
  AiRankedList* RankSystemsByStat(uint32_t requireA,
                                  uint32_t requireB,
                                  uint32_t forbidA,
                                  uint32_t forbidB,
                                  uint32_t statIndex,
                                  int descending); // 0x4c62b0
  // The same by the distance from the system `target` (or the system its object is in).
  AiRankedList* RankSystemsByDistance(const ObjectKey* target,
                                      uint32_t requireA,
                                      uint32_t requireB,
                                      uint32_t forbidA,
                                      uint32_t forbidB,
                                      int descending); // 0x4c6460
  void MarkDirty(); // 0x4c6760
  int Update(int view); // 0x4c6770: rescans the systems and sums them up; 1 if it was dirty
  void SetSystem(uint16_t index, const ObjectKey* key, AiSystemScanSet* systems); // 0x4c67f0
  void Accumulate(const int* stats, const int* flagsA, const int* flagsB); // 0x4c6830

  int& Stat(int offset)
  {
    return m_stat[(offset - 0x60) / 4];
  }

  int m_dirty; // +0x1c
  int m_view; // +0x20
  PersistentInt m_flagsA; // +0x24
  PersistentInt m_flagsB; // +0x28
  int16_t m_x2c; // +0x2c
  int16_t m_x; // +0x2e packed map position of the sector
  int16_t m_y; // +0x30
  AiGalaxyAnalysis* m_analysis; // +0x34
  AiSystemScan* m_systems[kSystemCount]; // +0x38
  int m_stat[kStatCount]; // +0x60
};

// ---- the sets ---------------------------------------------------------------------------------------------------------

// The AiSectorScans (VT 0x65c5c0, 0x14 bytes), built one sector per BuildStep.
class AiSectorScanSet : public ObjectKeySet
{
public:
  AiSectorScanSet(); // 0x4c5450
  ~AiSectorScanSet() override; // 0: 0x4c5470 (deleting), 0x4c5490
  void Save(ostream* out) override; // 6: 0x4c55d0

  void LoadAll(istream* in, AiSystemScanSet* systems, AiGalaxyAnalysis* analysis); // 0x4c54b0
  int BuildStep(AiSystemScanSet* systems, AiGalaxyAnalysis* analysis); // 0x4c5740
  void NoteSector(const ObjectKey* key); // 0x4c5850
  void ReadOne(istream* in, AiSystemScanSet* systems, AiGalaxyAnalysis* analysis); // 0x4c5880
  AiSectorScan* FindScan(ObjectKey key); // 0x49dca0

  int m_view; // +0x08
  AiSectorScan* m_cursor; // +0x0c
  GameObjNotifier* m_cursorObject; // +0x10
};

// The AiSystemScans (VT 0x65c5e8, 0x18 bytes).
class AiSystemScanSet : public ObjectKeySet
{
public:
  AiSystemScanSet(); // 0x4c5910
  ~AiSystemScanSet() override; // 0: 0x4c5930 (deleting), 0x4c5950
  void Load(istream* in) override; // 4: 0x4c5970
  void Save(ostream* out) override; // 6: 0x4c5a90

  int BuildStep(); // 0x4c5c00
  void NoteSystem(const ObjectKey* key); // 0x4c5d20
  void ReadOne(istream* in); // 0x4c5d50
  AiSystemScan* FindScan(ObjectKey key); // 0x4c5dd0

  int m_view; // +0x08
  AiSystemScan* m_cursor; // +0x0c
  GameObjNotifier* m_cursorObject; // +0x10
  AiClassTable* m_classes; // +0x14
};
