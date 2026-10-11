#include "ai/ai_posture.h"

#include "ai/ai_agent_bases.h"
#include "ai/ai_order_util.h"
#include "ai/ai_side_controller.h"
#include "ai/ai_system_record.h"
#include "character/role.h"
#include "data/random.h"
#include "economy/defense_facility.h"
#include "economy/manufacturing_facility.h"
#include "economy/production_facility_types.h"
#include "fleet/fight_squad.h"
#include "fleet/fleet_iterator.h"
#include "fleet/troop_reg.h"
#include "gameobj/object_lookup.h"
#include "orders/order_base.h"

using ai::IsFleetKey;
using ai::IsSystemKey;
using ai::KeyOfNode;
using ai::KeyOfObject;
using ai::NextNode;

namespace
{

// AiFleetEval::m_flags
constexpr uint32_t kFleetOurs = 1;
// AiSystemEval::m_flags30
constexpr uint32_t kSystemOurs = 1;

AiSystemEval* FindSystem(AiSideController* owner, const ObjectKey& key)
{
  return owner->m_systems.FindEval(key);
}

AiFleetEval* FindFleet(AiSideController* owner, const ObjectKey& key)
{
  return owner->m_fleets.FindEval(key);
}

AiSectorEval* FindSector(AiSideController* owner, const ObjectKey& key)
{
  return static_cast<AiSectorEval*>(owner->m_sectors.Find(&key));
}

void AddKeyNode(ObjectKeySet* set, const ObjectKey& key)
{
  ObjectKeyNode* node = NewOrNull<ObjectKeyNode>(key);
  if (node)
    set->Add(node);
}

// Collects the keys of the objects an iterator walks.
template <class Iterator>
void CollectKeys(ObjectKeySet* set, Iterator* iterator)
{
  for (iterator->Begin(); iterator->m_current; iterator->Advance())
    AddKeyNode(set, KeyOfObject(iterator->m_current));
}

// The key of the n-th node of the set (the last one if the set has fewer), or leaves `out` alone if it is empty.
void PickRandomKey(ObjectKeySet* set, ObjectKey* out)
{
  const uint16_t count = static_cast<uint16_t>(set->Count());
  if (count == 0)
    return;
  int steps = GameplayRandomNext(count);
  ObjectKeyNode* node = set->First();
  if (!node)
    return;
  while (steps > 0)
  {
    node = NextNode(node);
    --steps;
    if (!node)
      break;
  }
  if (node)
    *out = KeyOfNode(node);
}

} // namespace

AiPosture::AiPosture(int view, AiSideController* owner, AiSystemRecord* record)
: m_flags(0),
  m_selector(),
  m_targetFleet(),
  m_cursorA(),
  m_cursorB()
{
  m_view = view;
  m_state = 0;
  m_owner = owner;
  m_record = record;
  for (int& budget : m_budget)
    budget = 0;
}

AiPosture::~AiPosture()
{
  m_owner = nullptr;
  m_record = nullptr;
}

void AiPosture::Read(istream* in)
{
  ReadValue32(in, &m_view);
  ReadValue32(in, &m_state);
  m_flags.Read(in);
  m_selector.Read(in);
  m_targetFleet.Read(in);
  m_cursorA.Read(in);
  m_cursorB.Read(in);
  for (int& budget : m_budget)
    ReadValue32(in, &budget);
}

void AiPosture::Write(ostream* out)
{
  WriteValue32(out, &m_view);
  WriteValue32(out, &m_state);
  m_flags.Write(out);
  m_selector.Write(out);
  m_targetFleet.Write(out);
  m_cursorA.Write(out);
  m_cursorB.Write(out);
  for (int& budget : m_budget)
    WriteValue32(out, &budget);
}

int AiPosture::Virtual03()
{
  return 0;
}

void AiPosture::ChooseReserveSystem(ObjectKey* system)
{
  const ObjectKey& recordSystem = m_record->m_systemKey;
  if (IsSystemKey(recordSystem) && system->m_value != recordSystem.m_value)
  {
    AiSystemEval* eval = FindSystem(m_owner, recordSystem);
    if (eval && (eval->m_flags30.m_value & kSystemOurs) != 0 && (eval->m_flags28.m_value & 3) == 0)
      *system = recordSystem;
  }
  if (IsSystemKey(*system) && system->m_value != recordSystem.m_value)
  {
    AiSystemEval* eval = FindSystem(m_owner, *system);
    if (eval && ((eval->m_flags30.m_value & kSystemOurs) == 0 || (eval->m_flags28.m_value & 3) != 0))
      system->Reset();
  }
  ObjectKey scratch;
  if (!IsSystemKey(*system))
  {
    m_selector.Add(m_owner->RankSystemsOfSectorRandomly(&m_record->m_sectorKey, 0x80000000, 0, 1, 3, 0, 0, 1));
    *system = *m_selector.PickBest(&scratch);
    m_selector.Reset();
  }
  if (!IsSystemKey(*system))
  {
    m_selector.Add(m_owner->RankSystemsOfSectorRandomly(&m_record->m_sectorKey, 0x3e00000, 0, 1, 3, 0, 0, 1));
    *system = *m_selector.PickBest(&scratch);
    m_selector.Reset();
  }
  if (!IsSystemKey(*system))
  {
    m_selector.Add(m_owner->RankSystemsOfSectorRandomly(&m_record->m_sectorKey, 0, 0, 1, 3, 0, 0, 1));
    *system = *m_selector.PickBest(&scratch);
    m_selector.Reset();
  }
  if (!IsSystemKey(*system))
    *system = *m_owner->FindNearestSystemWithFleets(&scratch, &m_record->m_sectorKey, 0, 1, 0, 3, 0, 0);
}

void AiPosture::SubtractBudget(int* stats)
{
  Virtual20();
  for (int i = 0; i < kBudgetCount; ++i)
    stats[i] -= m_budget[i];
}

OrderBase* AiPosture::Step(int* done)
{
  OrderBase* order = nullptr;
  int flag = 0;
  *done = 0;
  switch (m_state)
  {
    case 1:
      MarkFleetTargets();
      m_state = 2;
      break;
    case 2:
      m_state = Virtual13() != 0 ? 3 : 5;
      break;
    case 3:
      order = Virtual14();
      m_state = 4;
      break;
    case 4:
      order = OrderFleetToTarget();
      m_state = 5;
      break;
    case 5:
      m_state = FindIdleFleet() != 0 ? 6 : 14;
      break;
    case 6:
      order = OrderCharacterFleet(&flag);
      m_state = flag != 0 ? 14 : 7;
      break;
    case 7:
      m_state = Virtual15() != 0 ? 13 : 8;
      break;
    case 8:
      m_state = Virtual16() != 0 ? 13 : 10;
      break;
    case 9:
      m_state = Virtual17() != 0 ? 13 : 12;
      break;
    case 10:
      m_state = Virtual18() != 0 ? 13 : 9;
      break;
    case 12:
      order = OrderFleetsAway();
      m_state = 14;
      break;
    case 13:
      order = OrderFleetHome();
      m_state = 14;
      break;
    case 14:
      order = OrderAssignedFleets(&flag);
      if (flag != 0)
      {
        m_state = 0;
        *done = 1;
      }
      break;
    default:
      m_state = 1;
      break;
  }
  return order;
}

int AiPosture::AgentIsNeeded(uint32_t agentId)
{
  int needed = 1;
  AiAgentBase* agent = static_cast<AiAgentBase*>(m_owner->m_agents.FindById(agentId));
  if (agent)
  {
    const int kind = agent->GetKind();
    if (kind == 3)
      needed = HasMultipleAgents();
    else if (kind == 4)
      needed = Virtual19();
    else if (kind == 5)
      needed = HasEscort();
  }
  return needed;
}

int AiPosture::CanTakeSystem(AiFleetEval* fleet, AiSystemEval* system)
{
  int allowed = 0;
  int enemyStrength = 0;
  int enemyShips = 0;
  int enemyDefense = 0;
  if (fleet && system)
  {
    const uint32_t flags = system->m_flags30.m_value;
    if ((flags & 0x200) != 0)
    {
      enemyStrength = system->Stat(0xe4);
      enemyShips = system->Stat(0xe8);
      enemyDefense = system->Stat(0xf4);
    }
    if ((flags & 4) != 0)
    {
      enemyStrength += system->Stat(0xb0);
      enemyShips += system->Stat(0xb4);
      enemyDefense += system->Stat(0xa4);
    }
    if (0 < enemyStrength && (enemyStrength * 12) / 10 < fleet->Stat(0x58))
    {
      if (0 < enemyDefense)
      {
        if (fleet->Stat(0x5c) <= enemyShips)
          return 0;
        if ((fleet->m_flags.m_value & 0x10) == 0 && (flags & 0x400) == 0)
        {
          if (fleet->Stat(0x9c) < 1)
            return fleet->Stat(0x74) < enemyDefense ? 0 : 1;
        }
        else if (fleet->Stat(0x74) <= enemyDefense)
        {
          return 0;
        }
      }
      allowed = 1;
    }
  }
  return allowed;
}

int AiPosture::CanUseFleet(AiFleetEval* fleet, AiSystemEval* system)
{
  int allowed = 0;
  int shipsNeeded = 0;
  if (fleet && system)
  {
    const uint32_t flags = system->m_flags30.m_value;
    if ((flags & 0x101) == 0 && (flags & 0x20) != 0)
    {
      int defenseNeeded = 0;
      ObjectKey systemKey;
      system->GetKey(&systemKey);
      if (systemKey.m_value != fleet->m_systemKey.m_value)
      {
        shipsNeeded = system->Stat(0xb4);
        if (shipsNeeded < m_owner->m_classes.m_x30)
          shipsNeeded = m_owner->m_classes.m_x30;
        defenseNeeded = system->Stat(0xa4);
      }
      if (shipsNeeded <= fleet->Stat(0x5c) && system->Stat(0x118) <= fleet->Stat(0x80) &&
          (((fleet->m_flags.m_value & 0x10) != 0 && defenseNeeded <= fleet->Stat(0x74)) ||
           (0 < fleet->Stat(0x9c) || defenseNeeded <= fleet->Stat(0x74))))
        allowed = 1;
    }
  }
  return allowed;
}

int AiPosture::CanOrderFleet(AiFleetEval* fleet, AiSystemEval* system, int* orderKind)
{
  int allowed = 0;
  *orderKind = 0;
  if (fleet && system && system->m_sector && (system->m_flags30.m_value & 4) != 0)
  {
    AiSectorEval* sector = system->m_sector;
    ObjectKey systemKey;
    system->GetKey(&systemKey);
    int shipsNeeded = 0;
    if (systemKey.m_value != fleet->m_systemKey.m_value)
    {
      shipsNeeded = system->Stat(0xb4);
      if (shipsNeeded < m_owner->m_classes.m_x30)
        shipsNeeded = m_owner->m_classes.m_x30;
    }
    if (shipsNeeded <= fleet->Stat(0x5c) && fleet->Stat(0x68) > system->Stat(0x68) && system->Stat(0x6c) == 0)
    {
      const int escorts = system->Stat(0x90);
      const int total = system->Stat(0x64) + escorts;
      const int wanted = (sector->Stat(0x78) + (1 - sector->Stat(0x80))) / 2;
      if (wanted < 1)
      {
        if (0 < escorts || 0 < system->Stat(0x64))
        {
          allowed = 1;
          *orderKind = 0x222;
        }
      }
      else if (total > wanted && 0 < escorts)
      {
        *orderKind = 0x220;
        return 1;
      }
    }
  }
  return allowed;
}

int AiPosture::CanSupportSystem(AiFleetEval* fleet, AiSystemEval* system)
{
  int allowed = 0;
  if (system && fleet)
  {
    const uint32_t flags = system->m_flags30.m_value;
    if ((flags & 8) == 0 && (flags & 2) != 0 && (flags & 0x200) == 0 && 0 < fleet->Stat(0x80))
      allowed = 1;
  }
  return allowed;
}

int AiPosture::HasMultipleAgents()
{
  int result = 0;
  if (m_record->m_agentCount[0] > 1 ||
      (static_cast<uint16_t>(m_record->m_fleets.Count()) == 0 && m_record->m_agentCount[0] > 0))
    result = 1;
  return result;
}

int AiPosture::Virtual19()
{
  int result = 0;
  if (FindSector(m_owner, m_record->m_sectorKey) && 0 < m_record->m_agentCount[1])
    result = 1;
  return result;
}

int AiPosture::HasEscort()
{
  const int count = m_record->m_agentCount[2];
  return count > 1 || (m_record->m_x98 == 0 && count > 0) ? 1 : 0;
}

void AiPosture::PickRandomObject(const ObjectKey* system, ObjectKey* out, uint32_t kinds)
{
  ObjectKeySet candidates;
  out->Reset();
  if (!IsSystemKey(*system))
    return;
  GameObjNotifier* systemObject = GetObjectInFamilies_90_98(m_view, system);
  AiSystemEval* eval = FindSystem(m_owner, *system);
  if (!systemObject || !eval)
    return;
  if ((kinds & 0x2000000) != 0)
  {
    AllFacilityIterator iterator(systemObject, 1);
    CollectKeys(&candidates, &iterator);
  }
  if ((kinds & 0x40000) != 0)
  {
    DefenseFacility24Iterator iterator(systemObject, 1);
    CollectKeys(&candidates, &iterator);
  }
  if ((kinds & 0x80000) != 0)
  {
    DefenseFacility25Iterator iterator(systemObject, 1);
    CollectKeys(&candidates, &iterator);
  }
  if ((kinds & 0x100000) != 0)
  {
    DefenseFacility23Iterator shields(systemObject, 1);
    CollectKeys(&candidates, &shields);
    DefenseFacility22Iterator guns(systemObject, 1);
    CollectKeys(&candidates, &guns);
  }
  if ((kinds & 0x200000) != 0)
  {
    TroopRegIterator iterator(systemObject, 1);
    CollectKeys(&candidates, &iterator);
  }
  if ((kinds & 0x400000) != 0)
  {
    FightSquadIterator iterator(systemObject, 1);
    CollectKeys(&candidates, &iterator);
  }
  if ((kinds & 0x800000) != 0)
  {
    FleetIterator fleets(systemObject, 1);
    for (fleets.Begin(); fleets.m_current; fleets.Advance())
    {
      AiFleetEval* fleet = FindFleet(m_owner, KeyOfObject(fleets.m_current));
      if (fleet && (fleet->m_flags.m_value & kFleetOurs) == 0)
      {
        for (ObjectKeyNode* ship = fleet->m_shipKeys.First(); ship; ship = NextNode(ship))
          AddKeyNode(&candidates, KeyOfNode(ship));
      }
    }
  }
  if ((kinds & 0x20000) != 0)
  {
    ManufacturingFacility29Iterator yards(systemObject, 1);
    CollectKeys(&candidates, &yards);
    ManufacturingFacility28Iterator factories(systemObject, 1);
    CollectKeys(&candidates, &factories);
    if ((m_owner->m_flags.Get() & 0x40) == 0)
    {
      ManufacturingFacility2AIterator others(systemObject, 1);
      CollectKeys(&candidates, &others);
    }
  }
  if ((kinds & 0x10000) != 0)
  {
    ProductionFacilityType2DIterator mines(systemObject, 1);
    CollectKeys(&candidates, &mines);
    ProductionFacilityType2CIterator refineries(systemObject, 1);
    refineries.Begin();
    if (refineries.m_current)
    {
      AddKeyNode(&candidates, KeyOfObject(refineries.m_current));
      refineries.Advance();
    }
  }
  PickRandomKey(&candidates, out);
}
