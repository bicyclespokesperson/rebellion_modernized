#include "ai/ai_agent_bases.h"
#include "ai/ai_order_util.h"
#include "ai/ai_posture.h"
#include "ai/ai_side_controller.h"
#include "ai/ai_system_record.h"
#include "character/role.h"
#include "data/random.h"
#include "fleet/fight_squad.h"
#include "fleet/fleet_iterator.h"
#include "fleet/troop_reg.h"
#include "gameobj/object_lookup.h"
#include "orders/order_base.h"

using ai::AddToTeam;
using ai::IsFleetKey;
using ai::IsShipKey;
using ai::IsSystemKey;
using ai::KeyOfNode;
using ai::KeyOfObject;

namespace
{

// AiFleetEval::m_flags
constexpr uint32_t kFleetOurs = 1;
constexpr uint32_t kFleetBusy = 2;
constexpr uint32_t kFleetAssignmentMask = 0xf0000000;
constexpr uint32_t kFleetStale = 0xf0003802;
constexpr uint32_t kFleetKeepMask = 0x0fffffff;
constexpr uint32_t kFleetAssignedCombat = 0x10000000;
constexpr uint32_t kFleetAssignedReinforce = 0x20000000;
constexpr uint32_t kFleetAssignedSupport = 0x40000000;
constexpr uint32_t kFleetAssignedTroops = 0x80000000;
// AiSystemEval::m_flags30
constexpr uint32_t kSystemOurs = 1;

constexpr int kOrderHold = 0x223;
constexpr int kOrderReinforce = 0x234;

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
AiShipRecord* FindShip(AiSideController* owner, const ObjectKey& key)
{
  return static_cast<AiShipRecord*>(owner->m_ships.Find(&key));
}
AiCharacterRecord* FindCharacter(AiSideController* owner, const ObjectKey& key)
{
  return static_cast<AiCharacterRecord*>(owner->m_characters.Find(&key));
}

void AddKeyNode(ObjectKeySet* set, const ObjectKey& key)
{
  ObjectKeyNode* node = NewOrNull<ObjectKeyNode>(key);
  if (node)
    set->Add(node);
}

void PickRandomKey(ObjectKeySet* set, ObjectKey* out)
{
  const uint16_t count = static_cast<uint16_t>(set->Count());
  if (count == 0)
    return;
  int steps = GameplayRandomNext(count);
  auto position = set->Entries().begin();
  if (position == set->Entries().end())
    return;
  while (steps > 0)
  {
    ++position;
    --steps;
    if (position == set->Entries().end())
      break;
  }
  if (position != set->Entries().end())
    *out = KeyOfNode(static_cast<ObjectKeyNode*>((*position).get()));
}

// A character of the side's records that the AI may send somewhere: the character's record when the flags say it is a
// character of the other view that is not travelling and matches `wanted`.
bool IsPickableCharacter(const AiCharacterRecord* record, uint32_t wanted)
{
  const uint32_t flags = record->m_flags.m_value;
  return (flags & 0x20000000) != 0 && (flags & 4) != 0 && (wanted & flags) != 0 && (flags & 0x20) == 0;
}

void AddPickableCharacters(AiSideController* owner, ObjectKeySet* set, GameObjNotifier* holder, uint32_t wanted)
{
  RoleIterator roles(holder, 3);
  for (roles.Begin(); roles.m_current; roles.Advance())
  {
    AiCharacterRecord* record = FindCharacter(owner, KeyOfObject(roles.m_current));
    if (record && IsPickableCharacter(record, wanted))
    {
      ObjectKey key;
      record->GetKey(&key);
      AddKeyNode(set, key);
    }
  }
}

} // namespace

int AiPosture::Virtual15()
{
  return 0;
}
int AiPosture::Virtual16()
{
  return 0;
}
int AiPosture::Virtual17()
{
  return 0;
}
int AiPosture::Virtual18()
{
  return 0;
}

void AiPosture::PickRandomCharacter(const ObjectKey* system, ObjectKey* out, uint32_t kinds, uint32_t characterFlags)
{
  ObjectKeySet candidates;
  out->Reset();
  if (!IsSystemKey(*system))
    return;
  GameObjNotifier* systemObject = GetObjectInFamilies_90_98(m_view, system);
  AiSystemEval* eval = FindSystem(m_owner, *system);
  if (systemObject && eval)
  {
    if ((kinds & 0x400) != 0)
      AddPickableCharacters(m_owner, &candidates, systemObject, characterFlags);
    if ((eval->m_flags2c.m_value & 0x800) != 0)
    {
      FleetIterator fleets(systemObject, 1);
      for (fleets.Begin(); fleets.m_current; fleets.Advance())
      {
        AiFleetEval* fleet = FindFleet(m_owner, KeyOfObject(fleets.m_current));
        if (fleet && (fleet->m_flags.m_value & kFleetOurs) == 0 && (fleet->m_flags.m_value & 0x600000) != 0)
        {
          for (auto position = fleet->m_shipKeys.Entries().begin(); position != fleet->m_shipKeys.Entries().end();
               ++position)
          {
            ObjectKeyNode* ship = static_cast<ObjectKeyNode*>((*position).get());
            ObjectKey shipKey = KeyOfNode(ship);
            GameObjNotifier* shipObject = GetObjectInFamilies_14_1c(m_view, &shipKey);
            if (shipObject)
              AddPickableCharacters(m_owner, &candidates, shipObject, characterFlags);
          }
        }
      }
    }
  }
  PickRandomKey(&candidates, out);
}

void AiPosture::MarkFleetTargets()
{
  AiSectorEval* sector = FindSector(m_owner, m_record->m_sectorKey);
  if (!sector)
    return;
  for (AiSystemEval* system : sector->m_systems)
  {
    if (system)
      system->m_flags30.m_value &= 0xfe0fffff;
  }
  for (auto position = m_record->m_fleets.Entries().begin(); position != m_record->m_fleets.Entries().end(); ++position)
  {
    ObjectKeyNode* node = static_cast<ObjectKeyNode*>((*position).get());
    AiFleetEval* fleet = FindFleet(m_owner, KeyOfNode(node));
    if (!fleet)
      continue;
    const ObjectKey target = fleet->m_x2c;
    if (!IsSystemKey(target))
      continue;
    AiSystemEval* system = FindSystem(m_owner, target);
    if (!system)
      continue;
    const uint32_t flags = fleet->m_flags.m_value;
    if ((flags & kFleetAssignedTroops) != 0)
      system->m_flags30.m_value |= 0x1000000;
    else if ((flags & kFleetAssignedCombat) != 0)
      system->m_flags30.m_value |= 0x100000;
    else if ((flags & kFleetAssignedReinforce) != 0)
      system->m_flags30.m_value |= 0x200000;
    else if ((flags & kFleetAssignedSupport) != 0)
      system->m_flags30.m_value |= 0x400000;
  }
}

int AiPosture::Virtual13()
{
  int result = 0;
  ObjectKey chosen;
  ObjectKey scratch;
  ObjectKey& target = m_record->m_systemKey;
  if (IsSystemKey(target))
  {
    AiSystemEval* eval = FindSystem(m_owner, target);
    if (eval)
    {
      bool keep = true;
      AiSectorEval* sector = FindSector(m_owner, m_record->m_sectorKey);
      if (sector && (sector->m_flags24.m_value & 0x40000000) != 0 && (eval->m_flags28.m_value & 0x80000000) == 0)
        keep = false;
      if (keep && (eval->m_flags30.m_value & kSystemOurs) != 0 && (eval->m_flags28.m_value & 2) == 0)
      {
        if ((eval->m_flags30.m_value & 0x30000) != 0)
        {
          eval->m_flags30.m_value &= 0xfffcffff;
          eval->MarkChanged();
        }
        result = 1;
      }
      else
      {
        eval->m_flags30.m_value &= 0xfff8ffff;
        eval->MarkChanged();
        target.Reset();
      }
    }
  }
  if (IsSystemKey(target))
    return result;
  m_selector.Add(m_owner->RankSystemsOfSector(&m_record->m_sectorKey, 0x80000000, 0, 1, 2, 0, 0x40000, 0x26, 1));
  chosen = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  if (IsSystemKey(chosen))
    target = chosen;
  if (!IsSystemKey(target))
  {
    m_selector.Add(m_owner->RankSystemsOfSector(&m_record->m_sectorKey, 0, 0, 1, 2, 0, 0x40000, 0x26, 1));
    chosen = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    if (IsSystemKey(chosen))
      target = chosen;
  }
  if (!IsSystemKey(target))
  {
    chosen = *m_owner->FindNearestSystemWithFleets(&scratch, &m_record->m_sectorKey, 0, 0, 1, 2, 0, 0x40000);
    if (IsSystemKey(chosen))
      target = chosen;
  }
  if (IsSystemKey(target))
  {
    AiSystemEval* eval = FindSystem(m_owner, target);
    if (eval)
    {
      eval->m_flags30.m_value = (eval->m_flags30.m_value & 0xfffcffff) | 0x40000;
      eval->MarkChanged();
      result = 1;
    }
  }
  return result;
}

OrderBase* AiPosture::Virtual14()
{
  OrderBase* order = nullptr;
  int found = 0;
  ObjectKey fleetKey;
  ObjectKey shipKey;
  ObjectKey systemKey;
  ObjectKey scratch;
  AiRankedSelector shipSelector;
  ObjectKeyValueSet team;
  m_selector.Add(m_owner->RankFleetsByTarget(m_record->m_id, 0x40001, 0xf0000002, 0xc, 1));
  fleetKey = *m_selector.PickBest(&scratch);
  while (IsFleetKey(fleetKey) && found == 0)
  {
    AiFleetEval* fleet = FindFleet(m_owner, fleetKey);
    if (fleet)
    {
      AiSystemEval* system = FindSystem(m_owner, fleet->m_systemKey);
      if (system && (system->m_flags30.m_value & kSystemOurs) != 0)
      {
        systemKey = fleet->m_systemKey;
        shipSelector.Add(m_owner->RankShipsOfFleet(&fleetKey, 0x4000, 0x20800, 9, 1));
        shipKey = *shipSelector.PickBest(&scratch);
        while (IsShipKey(shipKey))
        {
          GameObjNotifier* ship = GetObjectInFamilies_14_1c(m_view, &shipKey);
          if (ship)
          {
            TroopRegIterator troops(ship, 1);
            troops.Begin();
            if (troops.m_current)
            {
              found = 1;
              do
              {
                AddToTeam(&team, KeyOfObject(troops.m_current));
                troops.Advance();
              } while (troops.m_current);
            }
          }
          shipKey = *shipSelector.PickBest(&scratch);
        }
        shipSelector.Reset();
      }
    }
    fleetKey = *m_selector.PickBest(&scratch);
  }
  m_selector.Reset();
  if (team.First())
    order = ai::NewOrder(ai::kMoveOrder, m_view, &team, &systemKey);
  return order;
}

OrderBase* AiPosture::OrderFleetToTarget()
{
  OrderBase* order = nullptr;
  bool depleted = false;
  bool found = false;
  ObjectKey fleetKey;
  ObjectKey scratch;
  ObjectKeyValueSet team;
  AiSystemEval* system = FindSystem(m_owner, m_record->m_systemKey);
  if (!system || system->Stat(0xac) <= 0)
    return order;
  m_selector.Add(m_owner->RankFleetsByTarget(m_record->m_id, 1, 2, 0x17, 1));
  fleetKey = *m_selector.PickBest(&scratch);
  while (IsFleetKey(fleetKey) && !found && !depleted)
  {
    AiFleetEval* fleet = FindFleet(m_owner, fleetKey);
    if (fleet && fleet->m_systemKey.m_value == m_record->m_systemKey.m_value)
    {
      if (fleet->Stat(0xac) < 1)
      {
        fleetKey.Reset();
        depleted = true;
      }
      else
      {
        found = true;
      }
      continue;
    }
    fleetKey = *m_selector.PickBest(&scratch);
  }
  m_selector.Reset();
  if (!IsFleetKey(fleetKey))
    return order;
  AiFleetEval* fleet = FindFleet(m_owner, fleetKey);
  if (fleet)
  {
    int count = fleet->Stat(0xac) < system->Stat(0xac) ? fleet->Stat(0xac) : system->Stat(0xac);
    ObjectKey systemKey;
    system->GetKey(&systemKey);
    GameObjNotifier* systemObject = GetObjectInFamilies_90_98(m_view, &systemKey);
    if (systemObject)
    {
      FightSquadIterator squadrons(systemObject, 1);
      squadrons.Begin();
      while (count > 0 && squadrons.m_current)
      {
        AddToTeam(&team, KeyOfObject(squadrons.m_current));
        squadrons.Advance();
        --count;
      }
    }
  }
  if (team.First())
    order = ai::NewOrder(ai::kMoveOrder, m_view, &team, &fleetKey);
  return order;
}

int AiPosture::FindIdleFleet()
{
  int found = 0;
  ObjectKey firstKey;
  ObjectKeyNode* node = static_cast<ObjectKeyNode*>(m_record->m_fleets.Find(&m_cursorA));
  if (!node)
    node = m_record->m_fleets.First();
  if (node)
  {
    auto position = m_record->m_fleets.PositionOf(node);
    firstKey = KeyOfNode(node);
    while (node)
    {
      if (found != 0)
        break;
      AiFleetEval* fleet = FindFleet(m_owner, KeyOfNode(node));
      if (fleet && (fleet->m_flags.m_value & kFleetStale) == 0)
      {
        fleet->GetKey(&m_targetFleet);
        found = 1;
      }
      ++position;
      if (position == m_record->m_fleets.Entries().end())
        position = m_record->m_fleets.Entries().begin();
      node = position == m_record->m_fleets.Entries().end() ? nullptr : static_cast<ObjectKeyNode*>((*position).get());
      if (node && KeyOfNode(node).m_value == firstKey.m_value)
        node = nullptr;
    }
  }
  if (found != 0 && node)
    m_cursorA = KeyOfNode(node);
  else
    m_cursorA.Reset();
  return found;
}

OrderBase* AiPosture::OrderCharacterFleet(int* found)
{
  OrderBase* order = nullptr;
  ObjectKey reserveShip;
  ObjectKey reserveSystem;
  ObjectKeyValueSet team;
  *found = 0;
  reserveShip = m_record->m_reserveShip;
  AiFleetEval* fleet = FindFleet(m_owner, m_targetFleet);
  if (!fleet)
    return order;
  if (IsShipKey(reserveShip) && fleet->HasShip(&reserveShip) != 0)
  {
    const uint32_t recordFlags = m_record->m_flagsA.m_value;
    if ((recordFlags & 0x1000000) != 0)
    {
      *found = 1;
    }
    else if ((recordFlags & 0x4000000) != 0)
    {
      AiSystemEval* system = FindSystem(m_owner, m_record->m_reserveSystem);
      if (system && (system->m_flags28.m_value & 2) == 0)
      {
        reserveSystem = m_record->m_reserveSystem;
        if (IsSystemKey(reserveSystem))
        {
          order = static_cast<OrderBase*>(CreateMessageByKind(ai::kMoveOrder));
          if (order)
          {
            ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(m_targetFleet, 0);
            if (node)
            {
              team.Add(node);
              order->m_side = m_view;
              order->SetTeam(&team);
              order->SetTarget(&reserveSystem);
              *found = 1;
            }
          }
        }
      }
    }
  }
  if (order)
    return order;
  const uint32_t fleetFlags = fleet->m_flags.m_value;
  if ((fleetFlags & 0x4000000) == 0 || (fleetFlags & kFleetBusy) != 0)
    return order;
  AiSystemEval* system = FindSystem(m_owner, fleet->m_systemKey);
  if (!system || (system->m_flags30.m_value & kSystemOurs) == 0)
    return order;
  bool anyAssigned = false;
  team.Clear();
  for (auto position = fleet->m_shipKeys.Entries().begin(); position != fleet->m_shipKeys.Entries().end(); ++position)
  {
    ObjectKeyNode* shipNode = static_cast<ObjectKeyNode*>((*position).get());
    const ObjectKey shipKey = KeyOfNode(shipNode);
    AiShipRecord* shipRecord = FindShip(m_owner, shipKey);
    GameObjNotifier* shipObject = GetObjectInFamilies_14_1c(m_view, &shipKey);
    if (!shipRecord || !shipObject || (shipRecord->m_flags.m_value & 0x800) != 0 ||
        (shipRecord->m_flags.m_value & 0x600000) == 0)
      continue;
    RoleIterator roles(shipObject, 1);
    for (roles.Begin(); roles.m_current; roles.Advance())
    {
      AiCharacterRecord* character = FindCharacter(m_owner, KeyOfObject(roles.m_current));
      if (!character)
        continue;
      ObjectKey characterKey;
      if ((character->m_flags.m_value & 2) == 0)
      {
        character->GetKey(&characterKey);
        AddToTeam(&team, characterKey);
        continue;
      }
      if (character->m_agentId != 0 && m_record->HasAgent(character->m_agentId) != 0)
        continue;
      character->GetKey(&characterKey);
      ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(characterKey, 0);
      if (node)
      {
        team.Add(node);
        anyAssigned = true;
      }
    }
  }
  if (team.First() && anyAssigned)
    order = ai::NewOrder(ai::kMoveOrder, m_view, &team, &fleet->m_systemKey);
  return order;
}

OrderBase* AiPosture::OrderFleetsAway()
{
  OrderBase* order = nullptr;
  bool found = false;
  ObjectKey origin;
  ObjectKey candidate;
  ObjectKey scratch;
  ObjectKeyValueSet team;
  if (!IsFleetKey(m_targetFleet))
    return order;
  if (static_cast<uint16_t>(m_record->m_fleets.Count()) > 1)
  {
    m_selector.Add(m_owner->RankFleetsByTargetAndSystemDistance(&m_targetFleet, m_record->m_id, 1, 0xf0000002, 2));
    candidate = *m_selector.PickBest(&scratch);
    while (IsFleetKey(candidate) && !found)
    {
      if (candidate.m_value != m_targetFleet.m_value)
      {
        AiFleetEval* fleet = FindFleet(m_owner, candidate);
        if (fleet)
        {
          AiSystemEval* system = FindSystem(m_owner, fleet->m_systemKey);
          if ((system->m_flags30.m_value & 0x200) == 0)
          {
            origin = candidate;
            found = true;
          }
        }
      }
      candidate = *m_selector.PickBest(&scratch);
    }
    m_selector.Reset();
  }
  if (!found)
  {
    origin = *Virtual21(&scratch);
    if (IsSystemKey(origin))
      found = true;
    if (!found)
      return order;
  }
  ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(m_targetFleet, 0);
  if (node)
  {
    team.Add(node);
    order = ai::NewOrder(ai::kMoveOrder, m_view, &team, &origin);
  }
  return order;
}

OrderBase* AiPosture::OrderFleetHome()
{
  OrderBase* order = nullptr;
  ObjectKeyValueSet team;
  AiFleetEval* fleet = FindFleet(m_owner, m_targetFleet);
  if (fleet)
  {
    ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(m_targetFleet, 0);
    if (node)
    {
      team.Add(node);
      order = ai::NewOrder(ai::kMoveOrder, m_view, &team, &fleet->m_x2c);
    }
  }
  return order;
}

OrderBase* AiPosture::OrderAssignedFleets(int* done)
{
  OrderBase* order = nullptr;
  bool stop = false;
  *done = 0;
  ObjectKeyNode* node = static_cast<ObjectKeyNode*>(m_record->m_fleets.Find(&m_cursorB));
  if (!node)
    node = m_record->m_fleets.First();
  if (node)
  {
    auto position = m_record->m_fleets.PositionOf(node);
    do
    {
      if (stop)
        break;
      AiFleetEval* fleet = FindFleet(m_owner, KeyOfNode(node));
      if (fleet)
      {
        const uint32_t flags = fleet->m_flags.m_value;
        if ((flags & kFleetAssignmentMask) != 0 && (flags & kFleetBusy) == 0 &&
            fleet->m_systemKey.m_value == fleet->m_x2c.m_value)
        {
          AiSystemEval* system = FindSystem(m_owner, fleet->m_x2c);
          bool clear = true;
          if (system)
          {
            int orderKind = 0;
            if ((flags & kFleetAssignedCombat) != 0)
            {
              clear = CanTakeSystem(fleet, system) == 0;
            }
            else if ((flags & kFleetAssignedReinforce) != 0)
            {
              clear = false;
              if ((flags & 0x10) != 0 && system->Stat(0x110) >= 1)
              {
                order = MakeFleetOrder(fleet, kOrderHold);
              }
              else if (CanUseFleet(fleet, system) != 0)
              {
                order = MakeFleetOrder(fleet, kOrderReinforce);
              }
              else if (CanOrderFleet(fleet, system, &orderKind) != 0)
              {
                order = MakeFleetOrder(fleet, orderKind);
              }
              else
              {
                clear = true;
              }
            }
            else if ((flags & kFleetAssignedSupport) != 0)
            {
              if ((flags & 0x10) != 0 && system->Stat(0x110) >= 1)
              {
                order = MakeFleetOrder(fleet, kOrderHold);
              }
              else if (CanOrderFleet(fleet, system, &orderKind) != 0)
              {
                order = MakeFleetOrder(fleet, orderKind);
              }
            }
            else if ((flags & kFleetAssignedTroops) != 0)
            {
              clear = CanSupportSystem(fleet, system) == 0;
              if (!clear)
                order = MakeTroopOrder(fleet);
            }
            else
            {
              clear = false;
            }
          }
          if (clear)
          {
            fleet->m_flags.m_value &= kFleetKeepMask;
            fleet->m_x2c.Reset();
          }
        }
      }
      ++position;
      node = position == m_record->m_fleets.Entries().end() ? nullptr : static_cast<ObjectKeyNode*>((*position).get());
      if (order != nullptr)
      {
        stop = true;
        if (!node)
          break;
        m_cursorB = KeyOfNode(node);
      }
    } while (node);
    if (node)
      return order;
  }
  *done = 1;
  m_cursorB.Reset();
  return order;
}

OrderBase* AiPosture::MakeFleetOrder(AiFleetEval* fleet, int orderKind)
{
  OrderBase* order = nullptr;
  ObjectKeyValueSet team;
  if (fleet)
  {
    order = static_cast<OrderBase*>(CreateMessageByKind(orderKind));
    if (order)
    {
      ObjectKey fleetKey;
      fleet->GetKey(&fleetKey);
      ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(fleetKey, 0);
      if (node)
      {
        team.Add(node);
        order->m_side = m_view;
        order->SetTeam(&team);
        order->SetTarget(&fleet->m_x2c);
      }
    }
  }
  return order;
}

OrderBase* AiPosture::MakeTroopOrder(AiFleetEval* fleet)
{
  OrderBase* order = nullptr;
  bool done = false;
  ObjectKeyValueSet team;
  if (!fleet)
    return order;
  for (auto position = fleet->m_shipKeys.Entries().begin(); position != fleet->m_shipKeys.Entries().end() && !done;
       ++position)
  {
    ObjectKeyNode* shipNode = static_cast<ObjectKeyNode*>((*position).get());
    AiShipRecord* ship = FindShip(m_owner, KeyOfNode(shipNode));
    if (!ship || ship->Stat(0x68) <= 0)
      continue;
    ObjectKey shipKey;
    ship->GetKey(&shipKey);
    GameObjNotifier* shipObject = GetObjectInFamilies_14_1c(m_view, &shipKey);
    if (!shipObject)
      continue;
    TroopRegIterator troops(shipObject, 1);
    troops.Begin();
    if (!troops.m_current)
      continue;
    order = static_cast<OrderBase*>(CreateMessageByKind(ai::kMoveOrder));
    if (!order)
      continue;
    done = true;
    ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(KeyOfObject(troops.m_current), 0);
    if (node)
    {
      team.Add(node);
      order->m_side = m_view;
      order->SetTeam(&team);
      order->SetTarget(&fleet->m_x2c);
    }
  }
  return order;
}
