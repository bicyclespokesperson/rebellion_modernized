#pragma once
#include <cstdint>

#include "ai/ai_galaxy_scan.h"
#include "ai/ai_members.h"
#include "ai/ai_ranked_list.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "gameobj/object_key.h"

class IntPair;

// What the player's advisors know of the galaxy (0xfc bytes, no vtable; 0x49c780-0x49db10, embedded in the player's advisor
// object at +0x3c): the scans of every system and sector, the unit class table and totals over all sectors. Step() builds
// it in four steps (class table, system scans, sector scans, then the analysis passes) and the changes of the game
// objects are fed to NoteChanged() so the passes only re-scan what changed.
class AiGalaxyAnalysis
{
public:
  AiGalaxyAnalysis(); // 0x49c780
  ~AiGalaxyAnalysis(); // 0x49c810
  void Init(int view); // 0x49c880
  void Load(istream* in); // 0x49c8a0
  void Save(ostream* out); // 0x49c9f0
  // One step of building and refreshing the analysis; 1 when a pass over the sectors has found something new.
  int Step(); // 0x49cb30
  int AnalyzeStep(); // 0x49cc80
  void SetFocusKey(const uint32_t* key); // 0x49cd70: the side's key (first word of its pair), marks the totals stale
  void SetFocusValue(int value); // 0x49cd90
  // The changed object's system or sector (reached through the parents) is marked for a new scan.
  void NoteChanged(const ObjectKey* key, int unused); // 0x49cdb0
  void NoteChangedAlt(const ObjectKey* key); // 0x49d0b0
  void NoteMoved(const ObjectKey* key, const ObjectKey* from, const ObjectKey* to); // 0x49d3b0
  AiRankedList* RankSectors(uint32_t requireA,
                            uint32_t requireB,
                            uint32_t forbidA,
                            uint32_t forbidB,
                            uint32_t statIndex,
                            int descending); // 0x49d610
  AiRankedList* RankSectorsByDistance(const ObjectKey* target,
                                      uint32_t requireA,
                                      uint32_t requireB,
                                      uint32_t forbidA,
                                      uint32_t forbidB,
                                      int descending); // 0x49d770
  AiRankedList* RankSystemsOfSector(const ObjectKey* sector,
                                    uint32_t requireA,
                                    uint32_t requireB,
                                    uint32_t forbidA,
                                    uint32_t forbidB,
                                    uint32_t statIndex,
                                    int descending); // 0x49da10
  AiRankedList* RankSystemsOfSectorByDistance(const ObjectKey* sector,
                                              const ObjectKey* target,
                                              uint32_t requireA,
                                              uint32_t requireB,
                                              uint32_t forbidA,
                                              uint32_t forbidB,
                                              int descending); // 0x49da70
  int ChooseFromSide(ObjectKey* out, int recordType, uint32_t requiredFlags, int mode, int descending); // 0x49dad0
  void AddSectorTotals(const int* stats, const int* flags); // 0x49db10

  AiSystemScan* FindSystemScan(const ObjectKey* key); // 0x4c7cd0

  int& Total(int offset)
  {
    return m_stat[(offset - 0x80) / 4];
  }

  int m_view; // +0x00
  PersistentInt m_flags; // +0x04 bit 31: the totals are stale, bit 29: sums are wanted, bit 30
  AiSystemScanSet m_systemScans; // +0x08
  AiSectorScanSet m_sectorScans; // +0x20
  AiClassTable m_classes; // +0x34
  AiSectorScan* m_cursor; // +0x74 the sector the pass is at
  int m_step; // +0x78 Step()'s state
  int m_phase; // +0x7c AnalyzeStep()'s state
  int m_stat[0x1e]; // +0x80 [0] focus key, [1] focus value, [4..] sums of the sectors' statistics
  int m_xf8; // +0xf8
};
