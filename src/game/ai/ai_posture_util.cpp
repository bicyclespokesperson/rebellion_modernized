#include "ai/ai_posture_util.h"

#include "fleet/troop_reg.h"
#include "gameobj/object_lookup.h"

using ai::IsFleetKey;
using ai::IsSystemKey;
using ai::KeyOfObject;

namespace ai_posture
{

AiRankedList* RunQuery(AiPosture* posture, const SystemQuery& query)
{
  const ObjectKey* sector = &posture->m_record->m_sectorKey;
  AiSideController* owner = posture->m_owner;
  switch (query.kind)
  {
    case QueryKind::ByStat:
      return owner->RankSystemsOfSector(sector, query.requireA, query.requireB, query.requireC, query.forbidA,
                                        query.forbidB, query.forbidC, query.statIndex, query.descending);
    case QueryKind::ByAge:
      return owner->RankSystemsOfSectorByAge(sector, query.requireA, query.requireB, query.requireC, query.forbidA,
                                             query.forbidB, query.forbidC, query.descending);
    case QueryKind::Randomly:
    default:
      return owner->RankSystemsOfSectorRandomly(sector, query.requireA, query.requireB, query.requireC, query.forbidA,
                                                query.forbidB, query.forbidC, query.descending);
  }
}

const SystemQuery* ChooseSystem(AiPosture* posture, ObjectKey* system, const SystemQuery* queries, int count)
{
  const SystemQuery* last = nullptr;
  ObjectKey scratch;
  for (int i = 0; i < count && !ai::IsSystemKey(*system); ++i)
  {
    last = &queries[i];
    posture->m_selector.Add(RunQuery(posture, queries[i]));
    *system = *posture->m_selector.PickBest(&scratch);
    posture->m_selector.Reset();
  }
  return last;
}

AiSectorEval* FindSector(AiPosture* posture)
{
  return static_cast<AiSectorEval*>(posture->m_owner->m_sectors.Find(&posture->m_record->m_sectorKey));
}

AiFleetEval* FindTargetFleet(AiPosture* posture)
{
  return posture->m_owner->m_fleets.FindEval(posture->m_targetFleet);
}

void SelectCharacterSystem(AiPosture* posture, ObjectKey* a, ObjectKey* b, const SystemQuery* queries, int count)
{
  a->Reset();
  b->Reset();
  const SystemQuery* last = nullptr;
  if (ai::IsSectorKey(posture->m_record->m_sectorKey))
    last = ChooseSystem(posture, a, queries, count);
  if (IsSystemKey(*a))
    posture->PickRandomCharacter(a, b, last->kinds, last->characterFlags);
}

void SelectObjectSystem(AiPosture* posture, ObjectKey* a, ObjectKey* b, const SystemQuery* queries, int count)
{
  a->Reset();
  b->Reset();
  const SystemQuery* last = nullptr;
  if (ai::IsSectorKey(posture->m_record->m_sectorKey))
    last = ChooseSystem(posture, a, queries, count);
  if (IsSystemKey(*a))
    posture->PickRandomObject(a, b, last ? last->kinds : 0);
}

void SelectSystemByAgeOrChance(AiPosture* posture, ObjectKey* a, ObjectKey* b)
{
  static const SystemQuery queries[] = {
    ByAge(0, 0x200, 4, 0, 0, 0, 1),
    Randomly(0, 0x200, 0, 0, 0, 0, 1),
  };
  a->Reset();
  b->Reset();
  if (ai::IsSectorKey(posture->m_record->m_sectorKey))
    ChooseSystem(posture, a, queries);
  if (IsSystemKey(*a))
    *b = *a;
}

void SelectSystemByAgeOnce(AiPosture* posture, ObjectKey* a, ObjectKey* b)
{
  static const SystemQuery queries[] = {
    ByAge(0, 0x200, 4, 0, 0, 0, 1),
  };
  a->Reset();
  b->Reset();
  if (ai::IsSectorKey(posture->m_record->m_sectorKey))
    ChooseSystem(posture, a, queries);
  if (IsSystemKey(*a))
    *b = *a;
}

void SelectSystemByAge(AiPosture* posture, ObjectKey* a, ObjectKey* b)
{
  static const SystemQuery queries[] = {
    ByAge(0, 0x80, 0, 0, 0x8000000, 0, 1),
    ByAge(0, 0x80, 0, 0, 0, 0, 1),
  };
  a->Reset();
  b->Reset();
  if (ai::IsSectorKey(posture->m_record->m_sectorKey))
    ChooseSystem(posture, a, queries);
  if (IsSystemKey(*a))
    *b = *a;
}

void ChooseFleetDestination(AiPosture* posture, AiFleetEval* fleet, ObjectKey* chosen)
{
  AiSideController* owner = posture->m_owner;
  AiSystemRecord* record = posture->m_record;
  AiRankedSelector& selector = posture->m_selector;
  ObjectKey scratch;
  if (!IsSystemKey(*chosen))
  {
    AiSystemEval* system = owner->m_systems.FindEval(record->m_systemKey);
    if (system && (system->m_flags28.m_value & 2) == 0)
      *chosen = record->m_systemKey;
  }
  if (!IsSystemKey(*chosen))
  {
    selector.Add(owner->RankSystemsOfSector(&record->m_sectorKey, 0x80000000, 0, 1, 2, 0, 0, 7, 2));
    *chosen = *selector.PickBest(&scratch);
    selector.Reset();
  }
  if (!IsSystemKey(*chosen))
  {
    selector.Add(owner->RankSystemsOfSector(&record->m_sectorKey, 0x3e00000, 0, 1, 2, 0, 0, 0x21, 1));
    *chosen = *selector.PickBest(&scratch);
    selector.Reset();
  }
  if (!IsSystemKey(*chosen))
  {
    AiSystemEval* origin = owner->m_systems.FindEval(fleet->m_systemKey);
    if (origin && (origin->m_flags30.m_value & 4) != 0)
    {
      selector.Add(
        owner->RankSystemsOfSectorByDistance(&record->m_sectorKey, &posture->m_targetFleet, 0, 0, 0, 0, 0, 0x204, 2));
      *chosen = *selector.PickBest(&scratch);
      selector.Reset();
    }
  }
}

int AssignTakeFleet(AiPosture* posture)
{
  AiFleetEval* fleet = FindTargetFleet(posture);
  if (!fleet || fleet->Stat(0x58) < 1)
    return 0;
  posture->m_selector.Add(
    posture->m_owner->RankSystemsOfSector(&posture->m_record->m_sectorKey, 0, 0, 0x200, 0, 0, 0x100000, 0x27, 1));
  return AssignFleet(posture, fleet, 0x100000, 0x10000000,
                     [fleet](AiSystemEval* system)
                     {
                       return AiPosture::CanTakeSystem(fleet, system) != 0;
                     });
}

int AssignOrderFleet(AiPosture* posture, uint32_t requireC, uint32_t secondStat)
{
  AiFleetEval* fleet = FindTargetFleet(posture);
  if (!fleet || fleet->Stat(0x68) < 1)
    return 0;
  AiSideController* owner = posture->m_owner;
  const ObjectKey* sector = &posture->m_record->m_sectorKey;
  posture->m_selector.Add(owner->RankSystemsOfSector(sector, 0, 0, requireC, 0, 0, 0x700200, 8, 2));
  posture->m_selector.Add(owner->RankSystemsOfSector(sector, 0, 0, requireC, 0, 0, 0x700200, secondStat, 1));
  return AssignFleet(posture, fleet, 0x400000, 0x40000000,
                     [posture, fleet](AiSystemEval* system)
                     {
                       int orderKind;
                       return posture->CanOrderFleet(fleet, system, &orderKind) != 0;
                     });
}

int AssignSupportFleet(AiPosture* posture)
{
  AiFleetEval* fleet = FindTargetFleet(posture);
  if (!fleet || fleet->Stat(0x80) < 1)
    return 0;
  posture->m_selector.Add(
    posture->m_owner->RankSystemsOfSector(&posture->m_record->m_sectorKey, 0, 0, 2, 0, 0, 0x100208, 0xf, 1));
  return AssignFleet(posture, fleet, 0x1000000, 0x80000000,
                     [fleet](AiSystemEval* system)
                     {
                       return AiPosture::CanSupportSystem(fleet, system) != 0;
                     });
}

int ChooseMarkedTarget(AiPosture* posture, uint32_t markedFlag, uint32_t dropMask, uint32_t keepMask, uint32_t setMask)
{
  AiSideController* owner = posture->m_owner;
  AiSystemRecord* record = posture->m_record;
  AiRankedSelector& selector = posture->m_selector;
  ObjectKey& target = record->m_systemKey;
  ObjectKey scratch;
  int result = 0;
  if (IsSystemKey(target))
  {
    AiSystemEval* eval = owner->m_systems.FindEval(target);
    if (eval)
    {
      const uint32_t flags = eval->m_flags30.m_value;
      if ((flags & 1) == 0 || (eval->m_flags28.m_value & 2) != 0)
      {
        eval->m_flags30.m_value &= dropMask;
        eval->MarkChanged();
        target.Reset();
      }
      else
      {
        if ((flags & markedFlag) == 0)
        {
          eval->m_flags30.m_value = (flags & keepMask) | setMask;
          eval->MarkChanged();
        }
        result = 1;
      }
    }
  }
  if (IsSystemKey(target))
    return result;
  ObjectKey chosen;
  selector.Add(owner->RankSystemsOfSector(&record->m_sectorKey, 0x80000000, 0, 1, 2, 0, 0x40000, 0x13, 1));
  chosen = *selector.PickBest(&scratch);
  selector.Reset();
  if (IsSystemKey(chosen))
    target = chosen;
  if (!IsSystemKey(target))
  {
    selector.Add(owner->RankSystemsOfSector(&record->m_sectorKey, 0, 0, 1, 2, 0, 0x40000, 0x13, 1));
    chosen = *selector.PickBest(&scratch);
    selector.Reset();
    if (IsSystemKey(chosen))
      target = chosen;
  }
  if (!IsSystemKey(target))
  {
    chosen = *owner->FindNearestSystemWithFleets(&scratch, &record->m_sectorKey, 0, 0, 1, 2, 0, 0x40000);
    if (IsSystemKey(chosen))
      target = chosen;
  }
  if (IsSystemKey(target))
  {
    AiSystemEval* eval = owner->m_systems.FindEval(target);
    if (eval)
    {
      eval->m_flags30.m_value = (eval->m_flags30.m_value & keepMask) | setMask;
      eval->MarkChanged();
      result = 1;
    }
  }
  return result;
}

OrderBase* OrderTroopsOfSystem(AiPosture* posture, AiSystemEval* system, AiSectorEval* sector)
{
  AiSideController* owner = posture->m_owner;
  AiSystemRecord* record = posture->m_record;
  AiRankedSelector& selector = posture->m_selector;
  OrderBase* order = nullptr;
  int fleetFound = 0;
  ObjectKey fleetKey;
  ObjectKey scratch;
  ObjectKeyValueSet team;
  if (system->Stat(0x94) <= 0)
    return order;
  selector.Add(owner->RankFleetsByTarget(record->m_id, 1, 2, 0x16, 1));
  fleetKey = *selector.PickBest(&scratch);
  while (IsFleetKey(fleetKey) && fleetFound == 0)
  {
    AiFleetEval* fleet = owner->m_fleets.FindEval(fleetKey);
    if (fleet && fleet->m_systemKey.m_value == record->m_systemKey.m_value)
    {
      fleetFound = 1;
      continue;
    }
    fleetKey = *selector.PickBest(&scratch);
  }
  selector.Reset();
  if (!IsFleetKey(fleetKey))
    return order;
  AiFleetEval* fleet = owner->m_fleets.FindEval(fleetKey);
  if (fleet)
  {
    int fleetRemaining = fleet->Stat(0xa8);
    int systemRemaining = system->Stat(0x94);
    if (fleetRemaining >= 1 && systemRemaining >= 1)
    {
      ObjectKey systemKey;
      system->GetKey(&systemKey);
      GameObjNotifier* systemObject = GetObjectInFamilies_90_98(posture->m_view, &systemKey);
      if (systemObject)
      {
        const bool byDefense = sector && (sector->m_flags2c.m_value & 8) == 0;
        TroopRegIterator troops(systemObject, 1);
        do
        {
          if (fleetRemaining < 1)
            break;
          int best = 0;
          GameObjNotifier* bestTroop = nullptr;
          for (troops.Begin(); troops.m_current; troops.Advance())
          {
            auto* troop = static_cast<TroopReg*>(troops.m_current);
            const int strength =
              byDefense ? static_cast<int>(troop->GetDefenseStrength()) : static_cast<int>(troop->GetAttackStrength());
            if (best < strength)
            {
              ObjectKey troopKey = KeyOfObject(troops.m_current);
              if (!team.Find(&troopKey))
              {
                best = strength;
                bestTroop = troops.m_current;
              }
            }
          }
          if (bestTroop)
            ai::AddToTeam(&team, KeyOfObject(bestTroop));
          --systemRemaining;
          --fleetRemaining;
        } while (systemRemaining > 0);
      }
    }
  }
  if (team.First())
    order = ai::NewOrder(ai::kMoveOrder, posture->m_view, &team, &fleetKey);
  return order;
}

} // namespace ai_posture
