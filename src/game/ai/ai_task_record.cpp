#include "ai/ai_common.h"
#include "ai/ai_records.h"
#include "ai/ai_side_controller.h"
#include "character/role.h"
#include "core/checked_new.h"
#include "fleet/troop_reg.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "orders/order_base.h"

using ai::KeyInFamilies;

namespace
{

constexpr int kMoveOrder = 0x201;
constexpr int kAssignOrder = 0x270;

constexpr uint32_t kSystemFamilyFirst = 0x90;
constexpr uint32_t kSystemFamilyLast = 0x98;
constexpr uint32_t kFleetFamilyFirst = 8;
constexpr uint32_t kFleetFamilyLast = 0x10;
constexpr uint32_t kShipFamilyFirst = 0x14;
constexpr uint32_t kShipFamilyLast = 0x1c;

// AiShipRecord::m_flags
constexpr uint32_t kShipFree = 0x20000;
constexpr uint32_t kShipTaken = 0x10000000;
constexpr uint32_t kShipInFleet = 0x4000;
// AiFleetEval::m_flags
constexpr uint32_t kFleetOurs = 1;
constexpr uint32_t kFleetBusy = 2;
constexpr uint32_t kFleetFree = 4;
constexpr uint32_t kFleetTaken = 8;
// AiSystemEval::m_flags30
constexpr uint32_t kSystemTaskTarget = 0x80000;

bool IsSystemKey(const ObjectKey& key)
{
  return KeyInFamilies(key, kSystemFamilyFirst, kSystemFamilyLast);
}
bool IsFleetKey(const ObjectKey& key)
{
  return KeyInFamilies(key, kFleetFamilyFirst, kFleetFamilyLast);
}

ObjectKey KeyOfNode(ObjectKeyNode* node)
{
  ObjectKey key;
  node->GetKey(&key);
  return key;
}

void AddToTeam(ObjectKeyValueSet* team, const ObjectKey& key)
{
  auto* node = NewOrNull<ObjectKeyValueNode>(key, 0);
  if (node) {
    team->Add(node);
  }
}

OrderBase* NewOrder(int kind, int side, ObjectKeyValueSet* team, const ObjectKey* target)
{
  auto* order = static_cast<OrderBase*>(CreateMessageByKind(kind));
  if (!order) {
    return nullptr;
  }
  order->m_side = side;
  order->SetTeam(team);
  order->SetTarget(target);
  return order;
}

} // namespace

AiTaskRecord::AiTaskRecord()
    : IdNode()
    , m_ships()
    , m_fleetKey()
    , m_systemKey()
    , m_selector()
{
  m_state = 0;
  m_view = 0;
  m_owner = nullptr;
  m_demand = 0;
  m_restart = 0;
}

AiTaskRecord::AiTaskRecord(int view, AiSideController* owner)
    : IdNode()
    , m_ships()
    , m_fleetKey()
    , m_systemKey()
    , m_selector()
{
  m_owner = owner;
  m_state = 0;
  m_view = view;
  m_demand = 0;
  m_restart = 0;
}

AiTaskRecord::~AiTaskRecord()
{
  m_owner = nullptr;
}

void AiTaskRecord::Read(istream* in)
{
  ReadId(in);
  ReadValue32(in, &m_state);
  ReadValue32(in, &m_view);
  m_systemKey.Read(in);
  ReadValue32(in, &m_demand);
  ReadValue32(in, &m_restart);
  m_ships.Load(in);
  m_fleetKey.Read(in);
}

void AiTaskRecord::Write(ostream* out)
{
  WriteId(out);
  WriteValue32(out, &m_state);
  WriteValue32(out, &m_view);
  m_systemKey.Write(out);
  WriteValue32(out, &m_demand);
  WriteValue32(out, &m_restart);
  m_ships.Save(out);
  m_fleetKey.Write(out);
}

int AiTaskRecord::IsFinished()
{
  return IsSystemKey(m_systemKey);
}

void AiTaskRecord::Release()
{
  auto position = m_ships.Entries().begin();
  while (position != m_ships.Entries().end()) {
    auto* node = static_cast<ObjectKeyNode*>((*position).get());
    ++position;
    ObjectKey key = KeyOfNode(node);
    RemoveShip(&key);
  }
  ClearFleet();
  if (!IsSystemKey(m_systemKey)) {
    return;
  }
  AiSystemEval* system = m_owner->m_systems.FindEval(m_systemKey);
  if (system) {
    m_systemKey.Reset();
    system->m_flags30.m_value &= ~kSystemTaskTarget;
  }
}

OrderBase* AiTaskRecord::Step(int* done)
{
  OrderBase* order = nullptr;
  *done = 0;
  if (m_restart != 0) {
    ReleaseShips();
    m_restart = 0;
  }
  switch (m_state) {
    case 1:
      m_state = PickTarget() != 0 ? 2 : 8;
      break;
    case 2:
      order = SendIdleMessage();
      m_state = 3;
      break;
    case 3:
      ComputeBudget();
      m_state = 4;
      break;
    case 4:
      {
        int chosen = done != nullptr;
        order = ChooseAction(&chosen);
        if (chosen != 0) {
          m_state = 5;
        }
        break;
      }
    case 5:
      order = MoveToSystem();
      m_state = 6;
      break;
    case 6:
      order = MoveCharacters();
      m_state = 7;
      break;
    case 7:
      order = RecallFleet();
      m_state = 0;
      *done = 1;
      break;
    case 8:
      Release();
      m_state = 0;
      *done = 1;
      break;
    default:
      m_state = 1;
      m_restart = 1;
  }
  if (order) {
    m_restart = 1;
  }
  return order;
}

int AiTaskRecord::PickTarget()
{
  AiRankedSelector systemSelector;
  ObjectKey sectorKey;
  ObjectKey systemKey;
  ObjectKey scratch;
  int picked = 0;
  if (IsSystemKey(m_systemKey)) {
    AiSystemEval* system = m_owner->m_systems.FindEval(m_systemKey);
    if (system) {
      if ((system->m_flags30.m_value & 1) == 0 || (static_cast<uint32_t>(system->m_flags28.m_value) & 0x80000000) == 0
          || (system->m_flags28.m_value & 2) != 0) {
        m_systemKey.Reset();
        system->m_flags30.m_value &= ~kSystemTaskTarget;
      } else {
        picked = 1;
        system->m_flags30.m_value |= kSystemTaskTarget;
      }
    }
  }
  if (!IsSystemKey(m_systemKey)) {
    m_selector.Add(m_owner->RankSectorsRandomly(0x40000000, 0, 0, 0, 0, 0, 1));
    sectorKey = *m_selector.PickBest(&scratch);
    while (KeyInFamilies(sectorKey, 0x80, 0x90) && picked == 0) {
      systemSelector.Add(m_owner->RankSystemsOfSectorRandomly(&sectorKey, 0x80000000, 0, 1, 2, 0, 0x80000, 1));
      systemKey = *systemSelector.PickBest(&scratch);
      systemSelector.Reset();
      if (IsSystemKey(systemKey)) {
        AiSystemEval* system = m_owner->m_systems.FindEval(systemKey);
        if (system) {
          m_systemKey = systemKey;
          picked = 1;
          system->m_flags30.m_value |= kSystemTaskTarget;
        }
      }
      sectorKey = *m_selector.PickBest(&scratch);
    }
    m_selector.Reset();
  }
  return picked;
}

void AiTaskRecord::ReleaseShips()
{
  ObjectKey shipFleetKey;
  bool keepFleet = false;
  AiFleetEval* fleet = m_owner->m_fleets.FindEval(m_fleetKey);
  if (fleet && (fleet->m_flags.m_value & kFleetOurs) != 0 && (fleet->m_flags.m_value & kFleetTaken) != 0) {
    if (fleet->m_shipKeys.Count() != 0) {
      keepFleet = true;
    }
  }
  if (!keepFleet) {
    ClearFleet();
  }
  auto position = m_ships.Entries().begin();
  while (position != m_ships.Entries().end()) {
    auto* node = static_cast<ObjectKeyNode*>((*position).get());
    ++position;
    bool keep = false;
    ObjectKey key = KeyOfNode(node);
    auto* ship = static_cast<AiShipRecord*>(m_owner->m_ships.Find(&key));
    if (ship && (ship->m_flags.m_value & kShipInFleet) != 0) {
      shipFleetKey = ship->m_fleetKey;
      if (IsFleetKey(m_fleetKey) && m_fleetKey.m_value == shipFleetKey.m_value) {
        keep = true;
      } else if (!IsFleetKey(m_fleetKey)) {
        AiFleetEval* shipFleet = m_owner->m_fleets.FindEval(shipFleetKey);
        if (shipFleet) {
          keep = true;
          if ((shipFleet->m_flags.m_value & kFleetFree) != 0) {
            SetFleet(&shipFleetKey);
          }
        }
      }
    }
    if (!keep) {
      ObjectKey releasedKey = KeyOfNode(node);
      RemoveShip(&releasedKey);
    }
  }
}

OrderBase* AiTaskRecord::SendIdleMessage()
{
  OrderBase* order = nullptr;
  ObjectKeyValueSet team;
  ObjectKey systemKey;
  AiFleetEval* fleet = m_owner->m_fleets.FindEval(m_fleetKey);
  if (fleet && (fleet->m_flags.m_value & 0x3000) != 0
      && (static_cast<uint32_t>(fleet->m_flags.m_value) & 0xf0000802) == 0) {
    systemKey = fleet->m_systemKey;
    for (auto position = fleet->m_shipKeys.Entries().begin(); position != fleet->m_shipKeys.Entries().end();
         ++position) {
      auto* node = static_cast<ObjectKeyNode*>((*position).get());
      ObjectKey key = KeyOfNode(node);
      auto* ship = static_cast<AiShipRecord*>(m_owner->m_ships.Find(&key));
      if (ship && (ship->m_flags.m_value & 0x801000) != 0 && (ship->m_flags.m_value & 0x800) == 0) {
        AddToTeam(&team, KeyOfNode(node));
      }
    }
  }
  if (team.First()) {
    order = NewOrder(kAssignOrder, m_view, &team, &systemKey);
  }
  return order;
}

void AiTaskRecord::ComputeBudget()
{
  m_demand = 0;
  AiFleetEval* fleet = m_owner->GetFleetRecord(&m_fleetKey);
  if (fleet) {
    m_demand += fleet->Stat(0xb4);
  }
  if ((m_owner->m_flags.Get() & 1) == 0) {
    m_demand -= m_owner->m_classes.m_x2c;
  } else {
    m_demand += m_owner->m_classes.m_x2c * -2;
  }
}

OrderBase* AiTaskRecord::ChooseAction(int* done)
{
  OrderBase* order = nullptr;
  bool fleetIdle = true;
  bool proceed = true;
  if (!IsFleetKey(m_fleetKey)) {
    fleetIdle = false;
  }
  if (fleetIdle) {
    AiFleetEval* fleet = m_owner->m_fleets.FindEval(m_fleetKey);
    if (fleet && (fleet->m_flags.m_value & kFleetBusy) != 0) {
      proceed = false;
    }
  }
  if (!proceed) {
    return order;
  }
  if (m_demand < 0) {
    order = ReinforceFleet();
    if (order) {
      return order;
    }
    order = ReinforceBySize();
  }
  if (order == nullptr) {
    if (0 < m_demand) {
      order = Reinforce();
      *done = 1;
      return order;
    }
    *done = 1;
  }
  return order;
}

OrderBase* AiTaskRecord::MoveToSystem()
{
  AiRankedSelector shipSelector;
  ObjectKey fleetSystemKey;
  ObjectKey picked;
  ObjectKey scratch;
  ObjectKeyValueSet team;
  OrderBase* order = nullptr;
  AiFleetEval* fleet = m_owner->m_fleets.FindEval(m_fleetKey);
  if (fleet && 0 < fleet->Stat(0x80)) {
    AiSystemEval* system = m_owner->m_systems.FindEval(fleet->m_systemKey);
    if (system && (system->m_flags30.m_value & 1) != 0) {
      fleetSystemKey = fleet->m_systemKey;
      shipSelector.Add(m_owner->RankShipsOfFleet(&m_fleetKey, 0x4000, 0x20800, 9, 1));
      picked = *shipSelector.PickBest(&scratch);
      while (KeyInFamilies(picked, kShipFamilyFirst, kShipFamilyLast)) {
        GameObjNotifier* ship = GetObjectInFamilies_14_1c(m_view, &picked);
        if (ship) {
          TroopRegIterator troops(ship, 1);
          troops.Begin();
          while (troops.m_current) {
            ObjectKey troopKey;
            AddToTeam(&team, *GetObjectKey(troops.m_current, &troopKey));
            troops.Advance();
          }
        }
        picked = *shipSelector.PickBest(&scratch);
      }
      shipSelector.Reset();
    }
  }
  if (team.First()) {
    order = NewOrder(kMoveOrder, m_view, &team, &fleetSystemKey);
  }
  return order;
}

OrderBase* AiTaskRecord::MoveCharacters()
{
  OrderBase* order = nullptr;
  ObjectKeyValueSet team;
  bool anyBusy = false;
  AiFleetEval* fleet = m_owner->m_fleets.FindEval(m_fleetKey);
  if (fleet && (static_cast<uint32_t>(fleet->m_flags.m_value) & 0x4000000) != 0
      && (fleet->m_flags.m_value & kFleetBusy) == 0) {
    ObjectKey fleetSystemKey = fleet->m_systemKey;
    AiSystemEval* system = m_owner->m_systems.FindEval(fleet->m_systemKey);
    if (system && (system->m_flags30.m_value & 1) != 0) {
      for (auto position = fleet->m_shipKeys.Entries().begin(); position != fleet->m_shipKeys.Entries().end();
           ++position) {
        auto* node = static_cast<ObjectKeyNode*>((*position).get());
        ObjectKey recordKey = KeyOfNode(node);
        auto* ship = static_cast<AiShipRecord*>(m_owner->m_ships.Find(&recordKey));
        ObjectKey objectKey = KeyOfNode(node);
        GameObjNotifier* shipObject = GetObjectInFamilies_14_1c(m_view, &objectKey);
        if (ship && shipObject && (ship->m_flags.m_value & 0x800) == 0 && (ship->m_flags.m_value & 0x600000) != 0) {
          RoleIterator roles(shipObject, 1);
          roles.Begin();
          while (roles.m_current) {
            ObjectKey roleKey;
            auto* character
                = static_cast<AiCharacterRecord*>(m_owner->m_characters.Find(GetObjectKey(roles.m_current, &roleKey)));
            if (character) {
              ObjectKey characterKey;
              character->GetKey(&characterKey);
              auto* teamNode = NewOrNull<ObjectKeyValueNode>(characterKey, 0);
              if (teamNode) {
                team.Add(teamNode);
                if ((character->m_flags.m_value & 2) != 0) {
                  anyBusy = true;
                }
              }
            }
            roles.Advance();
          }
        }
      }
      if (team.First() && anyBusy) {
        order = NewOrder(kMoveOrder, m_view, &team, &fleetSystemKey);
      }
    }
  }
  return order;
}

OrderBase* AiTaskRecord::RecallFleet()
{
  OrderBase* order = nullptr;
  ObjectKeyValueSet team;
  if (!IsSystemKey(m_systemKey)) {
    return order;
  }
  AiSystemEval* system = m_owner->m_systems.FindEval(m_systemKey);
  if (!system || (system->m_flags28.m_value & 2) != 0) {
    return order;
  }
  AiFleetEval* fleet = m_owner->m_fleets.FindEval(m_fleetKey);
  if (fleet && fleet->m_systemKey.m_value != m_systemKey.m_value && (fleet->m_flags.m_value & kFleetBusy) == 0) {
    auto* node = NewOrNull<ObjectKeyValueNode>(m_fleetKey, 0);
    if (node) {
      team.Add(node);
      order = NewOrder(kMoveOrder, m_view, &team, &m_systemKey);
    }
  }
  return order;
}

namespace
{

struct ShipHarvest {
  AiTaskRecord* task;
  ObjectKeyValueSet* team;
  ObjectKey picked;
  int taken = 0;
  int found = 0;

  void FromFleetsPickedBy(AiRankedSelector* selector)
  {
    ObjectKey scratch;
    picked = *selector->PickBest(&scratch);
    for (;;) {
      if (!IsFleetKey(picked) || taken != 0) {
        break;
      }
      AiFleetEval* fleet = task->m_owner->m_fleets.FindEval(picked);
      if (fleet) {
        for (auto position = fleet->m_shipKeys.Entries().begin();
             position != fleet->m_shipKeys.Entries().end() && found == 0; ++position) {
          auto* node = static_cast<ObjectKeyNode*>((*position).get());
          ObjectKey key = KeyOfNode(node);
          auto* ship = static_cast<AiShipRecord*>(task->m_owner->m_ships.Find(&key));
          if (!ship) {
            continue;
          }
          const uint32_t flags = ship->m_flags.m_value;
          if ((flags & kShipFree) == 0 || (flags & 0x803800) != 0) {
            continue;
          }
          bool take = false;
          found = 1;
          if (task->m_demand < 0) {
            found = 0;
            if ((flags & 1) != 0) {
              take = true;
            }
          }
          if (!take) {
            continue;
          }
          taken = 1;
          auto* teamNode = NewOrNull<ObjectKeyValueNode>(KeyOfNode(node), 0);
          if (teamNode) {
            team->Add(teamNode);
            ObjectKey shipKey = KeyOfNode(node);
            task->AddShip(&shipKey);
          }
        }
      }
      if (taken != 0) {
        continue;
      }
      picked = *selector->PickBest(&scratch);
    }
    selector->Reset();
  }
};

} // namespace

OrderBase* AiTaskRecord::ReinforceFleet()
{
  OrderBase* order = nullptr;
  ObjectKeyValueSet team;
  ShipHarvest harvest{this, &team};
  if (IsSystemKey(m_systemKey)) {
    m_selector.Add(m_owner->RankFleetsBySystemDistance(&m_systemKey, 0x8005, 10, 2));
    harvest.FromFleetsPickedBy(&m_selector);
  }
  if (!team.First()) {
    return order;
  }
  const ObjectKey* target;
  int kind;
  if (IsFleetKey(m_fleetKey)) {
    kind = kMoveOrder;
    target = &m_fleetKey;
  } else if (IsSystemKey(m_systemKey)) {
    kind = kAssignOrder;
    target = &m_systemKey;
  } else {
    return order;
  }
  order = NewOrder(kind, m_view, &team, target);
  return order;
}

OrderBase* AiTaskRecord::ReinforceBySize()
{
  OrderBase* order = nullptr;
  ObjectKeyValueSet team;
  ShipHarvest harvest{this, &team};
  if (IsSystemKey(m_systemKey)) {
    m_selector.Add(m_owner->RankFleetsByX34(0, 0x8001, 0xf000000e, 1));
    harvest.FromFleetsPickedBy(&m_selector);
  }
  if (!team.First()) {
    return order;
  }
  int kind;
  if (IsFleetKey(m_fleetKey)) {
    kind = kMoveOrder;
  } else if (IsSystemKey(m_systemKey)) {
    kind = kAssignOrder;
  } else {
    return order;
  }
  order = NewOrder(kind, m_view, &team, &m_fleetKey);
  return order;
}

OrderBase* AiTaskRecord::Reinforce()
{
  OrderBase* order = nullptr;
  ObjectKeyValueSet team;
  bool found = false;
  auto position = m_ships.Entries().begin();
  while (position != m_ships.Entries().end() && !found) {
    auto* node = static_cast<ObjectKeyNode*>((*position).get());
    ++position;
    ObjectKey key = KeyOfNode(node);
    auto* ship = static_cast<AiShipRecord*>(m_owner->m_ships.Find(&key));
    if (ship && (ship->m_flags.m_value & 0x2800) == 0 && ship->IsUnusable() == 0) {
      found = true;
      if (m_demand < ship->Stat(0x7c)) {
        found = false;
      }
    }
    if (found) {
      auto* teamNode = NewOrNull<ObjectKeyValueNode>(KeyOfNode(node), 0);
      if (teamNode) {
        team.Add(teamNode);
        ObjectKey shipKey = KeyOfNode(node);
        RemoveShip(&shipKey);
      }
    }
  }
  if (team.First()) {
    order = NewOrder(kAssignOrder, m_view, &team, &m_systemKey);
  }
  return order;
}

int AiTaskRecord::AddShip(const ObjectKey* key)
{
  int added = 0;
  if (!KeyInFamilies(*key, kShipFamilyFirst, kShipFamilyLast)) {
    return added;
  }
  auto* node = NewOrNull<ObjectKeyNode>(*key);
  if (!node) {
    return added;
  }
  auto* ship = static_cast<AiShipRecord*>(m_owner->m_ships.Find(key));
  if (!ship) {
    delete node;
    return added;
  }
  ship->m_flags.m_value &= ~kShipFree;
  ship->m_flags.m_value |= kShipTaken;
  ship->m_x30 = 0;
  ship->m_x34 = 0;
  if (0 < ship->Stat(0x7c)) {
    m_demand += ship->Stat(0x7c);
  }
  m_ships.Add(node);
  return 1;
}

int AiTaskRecord::RemoveShip(const ObjectKey* key)
{
  if (!m_ships.Find(key)) {
    return 0;
  }
  auto* ship = static_cast<AiShipRecord*>(m_owner->m_ships.Find(key));
  if (ship) {
    if (ship->m_x30 == 0) {
      ship->m_flags.m_value = (ship->m_flags.m_value & 0xecffffff) | kShipFree;
      ship->m_x30 = 0;
      ship->m_x34 = 0;
    }
    m_demand -= ship->Stat(0x7c);
  }
  m_ships.RemoveKey(key);
  return 1;
}

int AiTaskRecord::SetFleet(const ObjectKey* key)
{
  if (!IsFleetKey(*key)) {
    return 0;
  }
  AiFleetEval* fleet = m_owner->m_fleets.FindEval(*key);
  if (!fleet) {
    return 0;
  }
  m_fleetKey = *key;
  fleet->m_flags.m_value = (fleet->m_flags.m_value & ~kFleetFree) | kFleetTaken;
  fleet->m_x30 = 0;
  fleet->m_x34 = 0;
  return 1;
}

int AiTaskRecord::ClearFleet()
{
  if (!IsFleetKey(m_fleetKey)) {
    return 0;
  }
  AiFleetEval* fleet = m_owner->m_fleets.FindEval(m_fleetKey);
  if (fleet) {
    fleet->m_flags.m_value = (fleet->m_flags.m_value | kFleetFree) & ~kFleetTaken;
    fleet->m_x30 = 0;
    fleet->m_x34 = 0;
  }
  m_fleetKey.Reset();
  return 1;
}
