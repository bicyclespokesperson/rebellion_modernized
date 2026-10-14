#pragma once
#include <cstdint>

#include "ai/ai_character_records.h"
#include "ai/ai_members.h"
#include "ai/ai_ranked_list.h"
#include "ai/ai_records.h"
#include "ai/ai_selector.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "gameobj/object_key.h"

class AiAgentSimple;

// The strategic model of the computer player of one side (0x4178f0-0x41be00, 0x394 bytes, no vtable). An AiPlayer embeds it
// at +0x84. It owns one set of records per kind of game object (systems, sectors, ships, fleets, characters), the planners
// and agents that carry out missions, and the statistics derived from them; the AI's planning code queries the sets through
// the Rank* methods, which return AiRankedLists of the candidates.
//
// Step() runs once after a game starts or is loaded and builds every set (m_step 1..6); Analyze() then runs forever:
// each pass refreshes the records marked dirty (m_flags bit 31) and recomputes the totals in m_stat (m_subStep 1, 2, 4, 5).
// The Note* methods are the AiPlayer's notification handlers: they mark the affected records dirty.
//
// m_flags (PersistentInt): 0x1 set once the class table is built, 0x2 the capabilities changed (NoteMask), 0x4/0x8
// forces of the other side are present, 0x10/0x20/0x80/0x100 key facilities of the other side are known, 0x200 (set from
// the fleets), 0x400 (the totals contain a Death Star), 0x80000000 dirty.
class AiSideController
{
public:
  static constexpr int kStatCount = 100;
  static constexpr int kBudgetCount = 12;

  AiSideController(); // 0x4178f0
  ~AiSideController(); // 0x417b40

  void SetSide(int side); // 0x417cb0: stores the side in every set
  void Load(istream* in); // 0x417cf0
  void Save(ostream* out); // 0x417fa0
  void RaiseBudget(unsigned kind); // 0x418240
  void LowerBudget(unsigned kind); // 0x418270
  int Step(); // 0x4182a0: 1 when every set is built
  void MarkDirty(); // 0x4184f0
  int Analyze(); // 0x418500: 1 once per full pass
  void NoteValueA(const int* value); // 0x418970
  void NoteValueB(int value); // 0x418990
  void NoteMask(uint32_t mask); // 0x4189b0
  void NoteObject(const ObjectKey* key); // 0x418a60: marks the record of the object (or of its parent) dirty
  void NoteObjectReplaced(const ObjectKey* oldKey); // 0x418db0
  void NoteCombat(const ObjectKey* key); // 0x419110
  void NoteMoved(const ObjectKey* key, const ObjectKey* from, const ObjectKey* to); // 0x419260
  void NoteSystemChanged(const ObjectKey* key); // 0x419520
  void NoteShipChanged(const ObjectKey* key); // 0x419580
  void OnMissionEvent(const int* key, int argument); // 0x4195f0
  void OnPlannerEvent(const int* key, int argument); // 0x419610
  void NoteCharacterChanged(const ObjectKey* key); // 0x419630
  void NoteSystemTouched(const ObjectKey* key); // 0x419740
  void NoteFacilityTier(int unused, int tier); // 0x4197b0
  void NoteTroopTier(int unused, int tier); // 0x419870
  void NoteCapitalShipTier(int unused, int tier); // 0x4198a0
  void AdvanceBitCursor(); // 0x4199b0

  // Candidate queries: each returns a ranked list of the keys of the records that carry all of the required and none of the
  // forbidden flags (the last argument says whether the highest score ranks first).
  AiRankedList* RankSectors(uint32_t requireA,
                            uint32_t requireB,
                            uint32_t requireC,
                            uint32_t forbidA,
                            uint32_t forbidB,
                            uint32_t forbidC,
                            uint32_t statIndex,
                            int descending); // 0x419a00
  AiRankedList* RankSectorsByDistance(const ObjectKey* target,
                                      uint32_t requireA,
                                      uint32_t requireB,
                                      uint32_t requireC,
                                      uint32_t forbidA,
                                      uint32_t forbidB,
                                      uint32_t forbidC,
                                      int descending); // 0x419b80
  AiRankedList* RankSectorsByX3c(uint32_t requireA,
                                 uint32_t requireB,
                                 uint32_t requireC,
                                 uint32_t forbidA,
                                 uint32_t forbidB,
                                 uint32_t forbidC,
                                 int descending); // 0x419e90
  AiRankedList* RankSectorsByAge(uint32_t requireA,
                                 uint32_t requireB,
                                 uint32_t requireC,
                                 uint32_t forbidA,
                                 uint32_t forbidB,
                                 uint32_t forbidC,
                                 int descending); // 0x41a060
  AiRankedList* RankSectorsRandomly(uint32_t requireA,
                                    uint32_t requireB,
                                    uint32_t requireC,
                                    uint32_t forbidA,
                                    uint32_t forbidB,
                                    uint32_t forbidC,
                                    int descending); // 0x41a1d0
  AiRankedList* RankSystemsOfSector(const ObjectKey* sector,
                                    uint32_t requireA,
                                    uint32_t requireB,
                                    uint32_t requireC,
                                    uint32_t forbidA,
                                    uint32_t forbidB,
                                    uint32_t forbidC,
                                    uint32_t statIndex,
                                    int descending); // 0x41a340
  AiRankedList* RankSystemsOfSectorByAge(const ObjectKey* sector,
                                         uint32_t requireA,
                                         uint32_t requireB,
                                         uint32_t requireC,
                                         uint32_t forbidA,
                                         uint32_t forbidB,
                                         uint32_t forbidC,
                                         int descending); // 0x41a3a0
  AiRankedList* RankSystemsOfSectorByDistance(const ObjectKey* sector,
                                              const ObjectKey* target,
                                              uint32_t requireA,
                                              uint32_t requireB,
                                              uint32_t requireC,
                                              uint32_t forbidA,
                                              uint32_t forbidB,
                                              uint32_t forbidC,
                                              int descending); // 0x41a400
  AiRankedList* RankSystemsOfSectorRandomly(const ObjectKey* sector,
                                            uint32_t requireA,
                                            uint32_t requireB,
                                            uint32_t requireC,
                                            uint32_t forbidA,
                                            uint32_t forbidB,
                                            uint32_t forbidC,
                                            int descending); // 0x41a460
  AiRankedList* RankFleets(uint32_t requireFlags, uint32_t forbidFlags, uint32_t statIndex, int descending); // 0x41a4c0
  AiRankedList* RankFleetsBySectorDistance(const ObjectKey* sector,
                                           uint32_t requireFlags,
                                           uint32_t forbidFlags,
                                           int descending); // 0x41a600
  AiRankedList* RankFleetsBySystemDistance(const ObjectKey* system,
                                           uint32_t requireFlags,
                                           uint32_t forbidFlags,
                                           int descending); // 0x41a8a0
  AiRankedList*
  RankFleetsByX34(uint32_t minX34, uint32_t requireFlags, uint32_t forbidFlags, int descending); // 0x41ab40
  AiRankedList* RankShipsOfFleet(const ObjectKey* fleet,
                                 uint32_t requireFlags,
                                 uint32_t forbidFlags,
                                 uint32_t statIndex,
                                 int descending); // 0x41ac80
  AiRankedList* RankFleetsByTarget(int x30,
                                   uint32_t requireFlags,
                                   uint32_t forbidFlags,
                                   uint32_t statIndex,
                                   int descending); // 0x41acd0
  AiRankedList* RankFleetsByTargetAndSystemDistance(const ObjectKey* system,
                                                    int x30,
                                                    uint32_t requireFlags,
                                                    uint32_t forbidFlags,
                                                    int descending); // 0x41ae20
  AiRankedList* RankCharacters(uint32_t requireFlags,
                               uint32_t requireCaps,
                               uint32_t forbidFlags,
                               uint32_t forbidCaps,
                               uint32_t valueIndex,
                               int minValue,
                               int maxValue,
                               int descending); // 0x41b0d0
  void MarkFleetSystemChanged(const ObjectKey* fleetKey); // 0x41b110
  AiFleetEval* GetFleetRecord(const ObjectKey* key); // 0x476110 (located between the task and system records)
  AiSystemEval* FindSystemEval(const ObjectKey* key); // 0x47aa90 (lies among the AI's planner classes)
  int ChooseClass(ObjectKey* out, int recordType, uint32_t requiredFlags, int mode, int descending); // 0x41b230
  AiRankedList* RankClasses(int recordType, uint32_t requiredFlags, uint32_t valueMask, int descending); // 0x41b270
  // The system (key in *out, unset if none) that RankSystemsOfSectorRandomly picks in the sector nearest to `fromSector`
  // that has a system matching the masks.
  ObjectKey* FindNearestSystemWithFleets(ObjectKey* out,
                                         const ObjectKey* fromSector,
                                         uint32_t requireA,
                                         uint32_t requireB,
                                         uint32_t requireC,
                                         uint32_t forbidA,
                                         uint32_t forbidB,
                                         uint32_t forbidC); // 0x41b2b0
  void RetirePlanner(uint32_t id); // 0x41b530
  void CountPlanner(AiMissionPlanner* planner); // 0x41b570
  void UncountPlanner(AiMissionPlanner* planner); // 0x41b5a0
  void CountAgent(AiAgentSimple* agent); // 0x41b5d0
  void RetireAgent(uint32_t id); // 0x41b600
  int* GetPositionOf(int* out,
                     const ObjectKey* key); // 0x41b640: map position of the system the object is in (0 if none)
  int DistanceSquared(int16_t x1, int16_t y1, int16_t x2, int16_t y2); // 0x41b7b0
  // Add one record's statistics to the totals (Analyze). The key is unused; the stats are the record's m_stat array.
  void AccumulateSector(const int* stats,
                        const int* flags24,
                        const int* flags2c,
                        const int* flags28,
                        int x30); // 0x41b7e0 (x30 unused)
  void AccumulateFleet(const ObjectKey* key, const int* stats, const int* flags); // 0x41ba80
  void AccumulateCharacter(const ObjectKey* key,
                           const int* values,
                           const int* flags,
                           const int* capabilities); // 0x41bc10
  void ResetBudget(); // 0x41bd20

  int& Stat(int offset)
  {
    return m_stat[(offset - 0x184) / 4];
  }

  int m_side; // +0x000 1 Alliance, 2 Empire
  PersistentInt m_flags; // +0x004
  uint32_t m_bitsA; // +0x008 a set of bits stepped through by AdvanceBitCursor
  uint32_t m_bitB; // +0x00c current bit (0x80 initially)
  uint32_t m_bitC; // +0x010 (0x40 initially)
  ObjectKey m_key14; // +0x014
  ObjectKey m_key18; // +0x018
  ObjectKey m_key1c; // +0x01c
  ObjectKey m_key20; // +0x020
  ObjectKey m_key24; // +0x024
  ObjectKey m_key28; // +0x028
  AiSystemEvalSet m_systems; // +0x02c
  AiSectorEvalSet m_sectors; // +0x044
  AiShipRecordSet m_ships; // +0x058
  AiFleetEvalSet m_fleets; // +0x078
  AiCharacterRecordSet m_characters; // +0x08c
  AiPlannerSet m_planners; // +0x0a8 planners at work
  AiPlannerSet m_idlePlanners; // +0x0c0 retired planners (RetirePlanner moves them here)
  AiSystemRecordSet m_systemRecords; // +0x0d8
  AiAgentSet m_agents; // +0x0ec
  AiAgentSet m_idleAgents; // +0x104
  AiTaskRecordSet m_tasks; // +0x11c
  ObjectKey m_cursorFleetKey; // +0x130
  ObjectKey m_cursorCharacterKey; // +0x134
  AiClassTable m_classes; // +0x138
  AiSectorEval* m_cursor; // +0x178 the sector the pass in progress is at
  int m_step; // +0x17c Step()'s state
  int m_subStep; // +0x180 Analyze()'s state
  int m_stat[kStatCount]; // +0x184
  int m_missionTarget; // +0x314 key id of a system chosen by the task of type 13
  int m_missionKind; // +0x318
  int m_missionSubtype; // +0x31c
  int m_plannerCount[13]; // +0x320 planners per kind (CountPlanner)
  int m_agentSystem; // +0x354
  int m_agentTarget; // +0x358
  int m_agentCount[2]; // +0x35c agents per kind (CountAgent)
  int m_budget[kBudgetCount]; // +0x364 how many of each kind of force to keep (ResetBudget sets the defaults)
};
