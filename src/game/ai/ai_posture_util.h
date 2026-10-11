#pragma once
#include <cstdint>

#include "ai/ai_order_util.h"
#include "ai/ai_posture.h"
#include "ai/ai_records.h"
#include "ai/ai_side_controller.h"
#include "ai/ai_system_record.h"

// Building blocks the six posture classes share (ai_postures.h): the system queries their "choose a system" virtuals run in
// sequence, and the assignment of a fleet to the first system that accepts it.

namespace ai_posture
{

enum class QueryKind
{
  ByStat,
  ByAge,
  Randomly,
  ByDistance
};

// One query of AiSideController::RankSystemsOfSector* over the sector of the posture's record. `kinds` is the mask of
// object kinds (AiPosture::PickRandomObject / PickRandomCharacter) the query selects the system for, `characterFlags` the
// character flags that go with it.
struct SystemQuery
{
  QueryKind kind;
  uint32_t requireA;
  uint32_t requireB;
  uint32_t requireC;
  uint32_t forbidA;
  uint32_t forbidB;
  uint32_t forbidC;
  uint32_t statIndex;
  int descending;
  uint32_t kinds;
  uint32_t characterFlags;
};

constexpr SystemQuery ByStat(uint32_t a,
                             uint32_t b,
                             uint32_t c,
                             uint32_t fa,
                             uint32_t fb,
                             uint32_t fc,
                             uint32_t stat,
                             int descending,
                             uint32_t kinds = 0,
                             uint32_t characterFlags = 0)
{
  return {QueryKind::ByStat, a, b, c, fa, fb, fc, stat, descending, kinds, characterFlags};
}
constexpr SystemQuery ByAge(uint32_t a, uint32_t b, uint32_t c, uint32_t fa, uint32_t fb, uint32_t fc, int descending)
{
  return {QueryKind::ByAge, a, b, c, fa, fb, fc, 0, descending, 0, 0};
}
constexpr SystemQuery Randomly(uint32_t a,
                               uint32_t b,
                               uint32_t c,
                               uint32_t fa,
                               uint32_t fb,
                               uint32_t fc,
                               int descending,
                               uint32_t kinds = 0,
                               uint32_t characterFlags = 0)
{
  return {QueryKind::Randomly, a, b, c, fa, fb, fc, 0, descending, kinds, characterFlags};
}

// Runs the queries in order for as long as *system is no system key, taking the best system of each result into *system.
// Returns the last query that ran (null if none did).
const SystemQuery* ChooseSystem(AiPosture* posture, ObjectKey* system, const SystemQuery* queries, int count);

template <int N>
const SystemQuery* ChooseSystem(AiPosture* posture, ObjectKey* system, const SystemQuery (&queries)[N])
{
  return ChooseSystem(posture, system, queries, N);
}

// The ranked list of a query for the posture's sector.
AiRankedList* RunQuery(AiPosture* posture, const SystemQuery& query);

// Takes the best system of the list(s) already added to the posture's selector, one after the other, until `accepts` takes
// the fleet there: the fleet's target becomes that system (marked with `systemFlag`) and the fleet gets `fleetFlag`.
// Resets the selector. Returns 1 if a system took the fleet.
template <class Accepts>
int AssignFleet(AiPosture* posture, AiFleetEval* fleet, uint32_t systemFlag, uint32_t fleetFlag, Accepts accepts)
{
  int assigned = 0;
  ObjectKey scratch;
  ObjectKey key = *posture->m_selector.PickBest(&scratch);
  while (ai::IsSystemKey(key) && assigned == 0)
  {
    AiSystemEval* system = posture->m_owner->m_systems.FindEval(key);
    if (system && accepts(system))
    {
      assigned = 1;
      system->m_flags30.m_value |= systemFlag;
      fleet->m_x2c = key;
      fleet->m_flags.m_value |= fleetFlag;
    }
    if (assigned == 0)
      key = *posture->m_selector.PickBest(&scratch);
  }
  posture->m_selector.Reset();
  return assigned;
}

// The choosers of Virtual09-12 shared by most postures. Each resets *a and *b, runs the queries over the record's sector and
// takes the system into *a; then (09) a character at the system, (12) an object of the system into *b, or (10, 11) the system
// itself.
void SelectCharacterSystem(AiPosture* posture, ObjectKey* a, ObjectKey* b, const SystemQuery* queries, int count);
void SelectObjectSystem(AiPosture* posture, ObjectKey* a, ObjectKey* b, const SystemQuery* queries, int count);
void SelectSystemByAgeOrChance(AiPosture* posture, ObjectKey* a, ObjectKey* b); // Virtual10 of kinds 1, 3-6
void SelectSystemByAgeOnce(AiPosture* posture, ObjectKey* a, ObjectKey* b); // Virtual10 of kinds 1 and 2
void SelectSystemByAge(AiPosture* posture, ObjectKey* a, ObjectKey* b); // Virtual11 of all kinds

template <int N>
void SelectCharacterSystem(AiPosture* posture, ObjectKey* a, ObjectKey* b, const SystemQuery (&queries)[N])
{
  SelectCharacterSystem(posture, a, b, queries, N);
}

template <int N>
void SelectObjectSystem(AiPosture* posture, ObjectKey* a, ObjectKey* b, const SystemQuery (&queries)[N])
{
  SelectObjectSystem(posture, a, b, queries, N);
}

// The common tail of Virtual21 (kinds 3, 5, 6): while *chosen is no system, tries the record's own system, two ranked
// queries of the sector and finally the system nearest to the fleet.
void ChooseFleetDestination(AiPosture* posture, AiFleetEval* fleet, ObjectKey* chosen);

// The fleet assignment steps of the Step state machine (Virtual15, 17, 18): the posture's target fleet is sent to the first
// system of the sector that wants it. Each returns 1 when the fleet was assigned.
int AssignTakeFleet(AiPosture* posture); // Virtual15 of kinds 3, 5, 6
int AssignOrderFleet(AiPosture* posture, uint32_t requireC, uint32_t secondStat); // Virtual17 of kinds 3, 5, 6
int AssignSupportFleet(AiPosture* posture); // Virtual18 of kind 5

// Virtual13 of kinds 5 and 6: keeps the record's target system if it still is ours and open (marking it with `setMask` unless
// it carries `markedFlag`), otherwise drops it and picks a new one. Returns 1 if the record has a target.
int ChooseMarkedTarget(AiPosture* posture, uint32_t markedFlag, uint32_t dropMask, uint32_t keepMask, uint32_t setMask);

// Virtual14 of kinds 5 and 6: the order that moves the strongest troop regiments of the target system to the fleet that will
// take them (null if there is no such fleet or no troops). `sector` is null for kind 6, which always ranks by attack.
OrderBase* OrderTroopsOfSystem(AiPosture* posture, AiSystemEval* system, AiSectorEval* sector);

AiSectorEval* FindSector(AiPosture* posture);
AiFleetEval* FindTargetFleet(AiPosture* posture);

} // namespace ai_posture
