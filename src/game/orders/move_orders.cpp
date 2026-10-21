#include "orders/move_orders.h"

#include <new>

#include "core/checked_new.h"
#include "galaxy/system.h"
#include "gameobj/object_database.h"
#include "gameobj/object_family.h"
#include "gameobj/object_queries.h"
#include "mission/mission_external.h"
#include "orders/mission_orders.h"
#include "orders/order_checks.h"
#include "orders/route_context.h"
#include "rules/rules_mgr.h"

namespace
{

constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusRefused = 1;
constexpr int kStatusObjectNotFound = 0x12;
constexpr int kStatusNoMembers = 0x16;
constexpr int kStatusSingleObjectOnly = 0x15;
constexpr int kStatusDestinationNotFound = 0x22;
constexpr int kStatusDestinationSideRefused = 0x28;

constexpr uint32_t kMissionOrderKind = 0x241;
constexpr uint32_t kObjectMoveKind = 0x204;
constexpr uint32_t kMissionKeyFamily = 0x41;
constexpr uint32_t kObjectExistsFlag = 0x40;
constexpr uint32_t kCompletedFlag = 0x4;
constexpr int kRouteMode = 3;

bool isOk(const StatusCode* status)
{
  return status->m_second == static_cast<uint32_t>(kStatusOk);
}

void setStatus(StatusCode* status, int code)
{
  *status = StatusCode(kStatusCategory, code);
}

void setOk(StatusCode* status)
{
  setStatus(status, kStatusOk);
}

bool inFamilies(const ObjectKey& key, uint32_t first, uint32_t last)
{
  return key.Family() >= first && key.Family() < last;
}

bool isFleetOrCapitalShip(const ObjectKey& key)
{
  return inFamilies(key, family::kCapitalShips.m_first, family::kCapitalShips.m_second)
         || inFamilies(key, family::kFleets.m_first, family::kFleets.m_second);
}

bool isTroopRegiment(const ObjectKey& key)
{
  return inFamilies(key, family::kTroopRegiments.m_first, family::kTroopRegiments.m_second);
}

bool isSystem(const ObjectKey& key)
{
  return inFamilies(key, family::kSystems.m_first, family::kSystems.m_second);
}

} // namespace

// ---- MoveOrderBase --------------------------------------------------------------------------------------------

MoveOrderBase::MoveOrderBase()
    : OrderBase()
{
  m_subOrders = nullptr;
  m_status = nullptr;
  CreateLists();
}

MoveOrderBase::~MoveOrderBase()
{
  DestroyLists();
}

int MoveOrderBase::CreateLists()
{
  m_subOrders = NewOrNull<MessageTree>();
  if (m_subOrders != nullptr) {
    m_status = NewOrNull<IntPair>();
    if (m_status != nullptr) {
      return 1;
    }
  }
  DestroyLists();
  return 0;
}

int MoveOrderBase::DestroyLists()
{
  if (m_subOrders != nullptr) {
    delete m_subOrders;
    m_subOrders = nullptr;
  }
  if (m_status != nullptr) {
    delete m_status;
    m_status = nullptr;
  }
  return 1;
}

int MoveOrderBase::CheckEnabled(StatusCode* status)
{
  setOk(status);
  if (Expand(status)) {
    for (auto position = m_subOrders->Entries().begin(); position != m_subOrders->Entries().end();) {
      if (!isOk(status)) {
        break;
      }
      auto current = position++;
      auto* order = static_cast<OrderBase*>((*current).get());
      order->CheckEnabled(status);
    }
  }
  return isOk(status);
}

int MoveOrderBase::CheckValid(StatusCode* status)
{
  setOk(status);
  if (Expand(status)) {
    for (auto position = m_subOrders->Entries().begin(); position != m_subOrders->Entries().end();) {
      if (!isOk(status)) {
        break;
      }
      auto current = position++;
      auto* order = static_cast<OrderBase*>((*current).get());
      order->CheckValid(status);
    }
  }
  return isOk(status);
}

int MoveOrderBase::Execute(StatusCode* status)
{
  setOk(status);
  if (Expand(status)) {
    for (auto position = m_subOrders->Entries().begin(); position != m_subOrders->Entries().end();) {
      if (!isOk(status)) {
        break;
      }
      auto current = position++;
      auto* order = static_cast<OrderBase*>((*current).get());
      if (order->Execute(status) == 0) {
        break;
      }
    }
  }
  return 1;
}

int MoveOrderBase::CollectTransitDays(StatusCode* status, ObjectKeyValueSet* out)
{
  setOk(status);
  out->Clear();
  int result = CheckValid(status);
  if (result != 0 && isOk(status)) {
    ObjectKeyValueSet days;
    for (auto position = m_subOrders->Entries().begin(); position != m_subOrders->Entries().end();) {
      auto current = position++;
      auto* order = static_cast<OrderBase*>((*current).get());
      days.Clear();
      const int kind = order->GetKind();
      const bool collectable = kind == kObjectMoveKind || kind == kMissionOrderKind;
      result = collectable && result != 0;
      if (kind == kObjectMoveKind) {
        int collected = static_cast<ObjectMoveOrder*>(order)->CollectTransitDays(status, &days);
        result = collected != 0 && result != 0;
      } else if (kind == kMissionOrderKind) {
        int collected = (static_cast<MissionOrder*>(order))->MissionOrder::CollectCandidates(status, &days);
        result = collected != 0 && result != 0;
      }
      if (isOk(status)) {
        while (ObjectKeyNode* node = days.First()) {
          ObjectKey key;
          days.Detach(node->GetKey(&key));
          out->Add(node);
        }
      }
    }
  }
  return result;
}

int MoveOrderBase::Expand(StatusCode* status)
{
  setOk(status);
  int result = m_subOrders != nullptr && m_status != nullptr;
  if (!result) {
    setStatus(status, kStatusRefused);
  } else if (isOk(status)) {
    m_subOrders->Clear();
  }

  ObjectKeyValueSet team;
  if (result != 0) {
    if (isOk(status)) {
      ActivityRequest request;
      BuildRequest(&request);
      result = CollectKeyDependencies(&request, GetTeam(), &team, status);
      if (result != 0 && isOk(status) && team.Count() == 0) {
        setStatus(status, kStatusNoMembers);
      }
    }
    if (result != 0) {
      if (isOk(status)) {
        MissionOrder* mission = nullptr;
        do {
          result = BuildMissionOrder(&team, &mission, status);
          if (result == 0) {
            goto failed;
          }
          if (isOk(status) && mission != nullptr) {
            m_subOrders->Adopt(mission);
          }
        } while (isOk(status) && mission != nullptr);
      }
      if (result != 0) {
        if (isOk(status)) {
          ObjectMoveOrder* move = nullptr;
          result = BuildMoveOrder(&team, &move, status);
          if (result == 0) {
            goto failed;
          }
          if (isOk(status) && move != nullptr) {
            m_subOrders->Adopt(move);
          }
        }
        if (result != 0) {
          goto done;
        }
      }
    }
  }
failed:
  setStatus(status, kStatusRefused);
done:
  if (m_status != nullptr) {
    *m_status = *status;
  }
  return result;
}

ObjectKey* MoveOrderBase::GetTargetInto(ObjectKey* out)
{
  GetTarget(out);
  return out;
}

int MoveOrderBase::BuildMissionOrder(ObjectKeyValueSet* team, MissionOrder** out, StatusCode* status)
{
  setOk(status);
  *out = nullptr;
  ObjectKeyValueSet characters;
  int result = TakeCharactersAtOneLocation(team, &characters, status);
  if (result != 0 && isOk(status) && characters.Count() != 0) {
    void* memory = ::operator new(sizeof(MissionOrder241));
    MissionOrder241* order = memory ? new (memory) MissionOrder241() : nullptr;
    *out = order;
    result = order != nullptr;
    if (result != 0) {
      order->SetId(m_subOrders->NextId());
      order->m_unk1c = m_unk1c;
      order->m_side = m_side;
      order->m_unk44 = 0;
      order->m_unk48 = 0;
      ObjectKey target;
      order->SetTarget(GetTargetInto(&target));
      order->m_missionKey.m_value = ObjectKey::Make(kMissionKeyFamily, 1).m_value;
      order->SetTeam(&characters);
      order->m_characters = ObjectKeyValueSet();
    }
  }
  if (result == 0) {
    setStatus(status, kStatusRefused);
  }
  if (!isOk(status) && *out != nullptr) {
    delete *out;
    *out = nullptr;
  }
  return result;
}

int MoveOrderBase::BuildMoveOrder(ObjectKeyValueSet* team, ObjectMoveOrder** out, StatusCode* status)
{
  bool built = true;
  setOk(status);
  *out = nullptr;
  if (team->Count() != 0) {
    auto* order = NewOrNull<ObjectMoveOrder>();
    built = order != nullptr;
    *out = order;
    if (built) {
      order->SetId(m_subOrders->NextId());
      order->m_unk1c = m_unk1c;
      order->m_side = m_side;
      order->SetTeam(team);
      ObjectKey target;
      order->SetTarget(GetTargetInto(&target));
    }
  }
  if (!built) {
    setStatus(status, kStatusRefused);
  }
  if (!isOk(status) && *out != nullptr) {
    delete *out;
    *out = nullptr;
  }
  return built;
}

int TakeCharactersAtOneLocation(ObjectKeyValueSet* team, ObjectKeyValueSet* out, StatusCode* status)
{
  int result = 1;
  out->Clear();
  setOk(status);
  ObjectKey firstLocation;
  ObjectKey location;
  bool haveFirstLocation = false;
  for (auto position = team->Entries().begin(); position != team->Entries().end();) {
    if (!isOk(status) || result == 0) {
      break;
    }
    auto current = position++;
    auto* node = static_cast<ObjectKeyNode*>((*current).get());
    ObjectKey key;
    node->GetKey(&key);
    if (inFamilies(key, family::kCharacters.m_first, family::kSpecialForces.m_second)) {
      ObjectKey lookupKey;
      node->GetKey(&lookupKey);
      GameObjNotifier* object = GetMasterInFamilies_01_ff(&lookupKey);
      if (object == nullptr) {
        setStatus(status, kStatusObjectNotFound);
      }
      if (result != 0) {
        if (isOk(status)) {
          const uint32_t objectFamily = object->GetFamily();
          result = objectFamily >= family::kCharacters.m_first && objectFamily < family::kSpecialForces.m_second;
        }
        if (result != 0 && (static_cast<GameObj*>(object)->m_stateFlags & kCompletedFlag) != 0) {
          if (isOk(status)) {
            result = object->GetOwnerKey(&location);
          }
          if (result != 0 && isOk(status)) {
            if (!haveFirstLocation) {
              haveFirstLocation = true;
              firstLocation = location;
            }
            if (firstLocation.m_value == location.m_value) {
              ObjectKey detachKey;
              node->GetKey(&detachKey);
              team->Detach(&detachKey);
              out->Add(node);
            }
          }
        }
      }
    }
  }
  if (result == 0) {
    setStatus(status, kStatusRefused);
  }
  return result;
}

// ---- MoveOrder, ConfirmedMoveOrder ----------------------------------------------------------------------------

MoveOrder::MoveOrder()
    : MoveOrderBase()
{
}
MoveOrder::~MoveOrder() = default;
int MoveOrder::Write(ostream* out)
{
  return OrderBase::Write(out);
}
int MoveOrder::GetKind()
{
  return 0x201;
}
void* CreateMoveOrder()
{
  return NewOrNull<MoveOrder>();
}

ConfirmedMoveOrder::ConfirmedMoveOrder()
    : MoveOrderBase()
{
}
ConfirmedMoveOrder::~ConfirmedMoveOrder() = default;
int ConfirmedMoveOrder::GetKind()
{
  return 0x202;
}
void* CreateConfirmedMoveOrder()
{
  return NewOrNull<ConfirmedMoveOrder>();
}

// ---- ObjectMoveOrder ------------------------------------------------------------------------------------------

ObjectMoveOrder::ObjectMoveOrder()
    : OrderBase()
{
}
ObjectMoveOrder::~ObjectMoveOrder() = default;
int ObjectMoveOrder::Read(istream* in)
{
  return OrderBase::Read(in);
}
int ObjectMoveOrder::Write(ostream* out)
{
  return OrderBase::Write(out);
}
int ObjectMoveOrder::GetKind()
{
  return 0x204;
}
void* CreateObjectMoveOrder()
{
  return NewOrNull<ObjectMoveOrder>();
}

Command* ObjectMoveOrder::CreateCommand()
{
  void* memory = ::operator new(sizeof(MoveCommand));
  return memory ? new (memory) MoveCommand() : nullptr;
}

int ObjectMoveOrder::CheckValid(StatusCode* status)
{
  int ownsQueue = static_cast<int>(m_unk1c);
  setOk(status);
  ActivityRequest request;
  BuildRequest(&request);
  OrderBase::CheckValid(status);
  if (ownsQueue != 0) {
    if (!isOk(status)) {
      ownsQueue = RefuseDestinationInTeam(&request, GetTeam(), status);
    }
    if (ownsQueue != 0) {
      return isOk(status);
    }
  }
  setStatus(status, kStatusRefused);
  return isOk(status);
}

int ObjectMoveOrder::CollectTransitDays(StatusCode* status, ObjectKeyValueSet* out)
{
  setOk(status);
  out->Clear();
  int valid = CheckValid(status);
  GameObjNotifier* galaxy = GetGameRootMember_bc();
  bool ok = galaxy != nullptr && valid != 0;
  if (ok && galaxy != nullptr && isOk(status)) {
    ok = CollectTeamDependencies(out) != 0;
    for (const auto& nodeEntry : out->Entries()) {
      auto* node = static_cast<ObjectKeyValueNode*>(nodeEntry.get());

      ObjectKey key;
      node->GetKey(&key);
      GameObjNotifier* object = GetMasterInFamilies_01_ff(&key);
      if (object == nullptr) {
        ok = false;
      }
      int days = 0;
      if (object != nullptr) {
        ObjectKey target;
        ObjectKey* targetKey = GetTarget(&target);
        ObjectKey memberKey;
        node->GetKey(&memberKey);
        if (GetRulesMgr()->RulesMgr::ComputeUnitTravelDays(static_cast<GameObj*>(object), &memberKey, targetKey, &days)
            == 0) {
          ok = false;
        }
      }
      node->m_value = days;
    }
  }
  return ok;
}

int ObjectMoveOrder::CheckDestination(StatusCode* status)
{
  setOk(status);
  ActivityRequest request;
  BuildRequest(&request);
  int result = OrderBase::CheckDestination(status);
  uint32_t side = 0;
  if (result != 0 && isOk(status)) {
    result = GetCommandSide(m_commands, &side);
  }

  GameObjNotifier* destination = nullptr;
  StatusCode resolveStatus(kStatusCategory, kStatusOk);
  if (result != 0) {
    if (isOk(status)) {
      const StatusCode missing(kStatusCategory, kStatusDestinationNotFound);
      result = ResolveMasterInFamilies_01_ff(&request.m_target, &destination, &missing, &resolveStatus);
      if (result != 0 && destination == nullptr) {
        *status = resolveStatus;
      }
    }
    if (result != 0 && isOk(status)) {
      int onlyFleetsAndCapitalShips = 1;
      int onlyRegiments = 1;
      for (const auto& entry : m_commands->Entries()) {
        if (!isOk(status) || onlyFleetsAndCapitalShips == 0) {
          break;
        }
        auto* command = static_cast<Command*>(entry.get());

        if (!isFleetOrCapitalShip(command->m_key)) {
          onlyFleetsAndCapitalShips = 0;
        }
        if (!isTroopRegiment(command->m_key)) {
          onlyRegiments = 0;
        }
      }
      bool unpopulatedSystem = false;
      ObjectKey destinationKey;
      destination->GetKey(&destinationKey);
      if (isSystem(destinationKey)) {
        unpopulatedSystem = (static_cast<GameObj*>(destination)->m_stateFlags & kObjectExistsFlag) != 0
                            && (static_cast<System*>(destination)->m_flags & system_flag::kPopulated) == 0;
      }
      if (destination->ControlSide() != side && onlyFleetsAndCapitalShips == 0
          && (onlyRegiments == 0 || !unpopulatedSystem)) {
        setStatus(status, kStatusDestinationSideRefused);
      }
    }
  }

  RouteContext context;
  if (result != 0) {
    if (isOk(status)) {
      int mode = kRouteMode;
      int planned = context.InitFromRequest(&request, static_cast<int>(side), &mode);
      result = 0;
      if (planned == 0) {
        goto failed;
      }
      result = context.AddContainerLegs(&request.m_target);
    }
    if (result != 0) {
      if (isOk(status)) {
        for (const auto& entry : m_commands->Entries()) {
          if (result == 0 || !isOk(status)) {
            break;
          }
          auto* command = static_cast<Command*>(entry.get());

          ObjectKey leg;
          CapacityScratch plan;
          ObjectKey object = command->m_key;
          result = context.CheckMove(&object, &request.m_target, status, &plan, &leg);
          if (result != 0 && isOk(status)) {
            command->m_request.m_target = leg;
          }
        }
      }
      if (result != 0) {
        goto done;
      }
    }
  }
failed:
  setStatus(status, kStatusRefused);
done:
  return result;
}

// ---- RenameOrder ----------------------------------------------------------------------------------------------

RenameOrder::RenameOrder()
    : TextMessage()
{
}
RenameOrder::~RenameOrder() = default;
int RenameOrder::Read(istream* in)
{
  return TextMessage::Read(in);
}
int RenameOrder::Write(ostream* out)
{
  return TextMessage::Write(out);
}
int RenameOrder::GetKind()
{
  return 0x203;
}
void* CreateRenameOrder()
{
  return NewOrNull<RenameOrder>();
}

int RenameOrder::CheckEnabled(StatusCode* status)
{
  int ownsQueue = static_cast<int>(m_unk1c);
  setOk(status);
  TextMessage::CheckEnabled(status);
  if (ownsQueue != 0 && isOk(status)) {
    if (static_cast<uint16_t>(m_commands->Count()) > 1) {
      setStatus(status, kStatusSingleObjectOnly);
    }
  }
  return isOk(status);
}

Command* RenameOrder::CreateCommand()
{
  void* memory = ::operator new(sizeof(RenameCommand));
  return memory ? new (memory) RenameCommand() : nullptr;
}

int RenameOrder::CheckDestination(StatusCode* status)
{
  setOk(status);
  ActivityRequest request;
  BuildRequest(&request);
  int result = OrderBase::CheckDestination(status);
  if (result != 0) {
    if (isOk(status)) {
      for (const auto& entry : m_commands->Entries()) {
        if (!isOk(status)) {
          break;
        }
        auto* command = static_cast<Command*>(entry.get());

        static_cast<RenameCommand*>(command)->m_name = m_text;
      }
    }
    if (result != 0) {
      return result;
    }
  }
  setStatus(status, kStatusRefused);
  return result;
}

// ---- ScrapOrder, ReadyForDeleteOrder --------------------------------------------------------------------------

ScrapOrder::ScrapOrder()
    : OrderBase()
{
}
ScrapOrder::~ScrapOrder() = default;
void* CreateScrapOrder()
{
  return NewOrNull<ScrapOrder>();
}

Command* ScrapOrder::CreateCommand()
{
  void* memory = ::operator new(sizeof(ScrapCommand));
  return memory ? new (memory) ScrapCommand() : nullptr;
}

ReadyForDeleteOrder::ReadyForDeleteOrder()
    : OrderBase()
{
}
ReadyForDeleteOrder::~ReadyForDeleteOrder() = default;
int ReadyForDeleteOrder::GetKind()
{
  return 0x205;
}
void* CreateReadyForDeleteOrder()
{
  return NewOrNull<ReadyForDeleteOrder>();
}

Command* ReadyForDeleteOrder::CreateCommand()
{
  void* memory = ::operator new(sizeof(ReadyForDeleteCommand));
  return memory ? new (memory) ReadyForDeleteCommand() : nullptr;
}

// ---- ObjectKeyIntSet ------------------------------------------------------------------------------------------

ObjectKeyIntSet::ObjectKeyIntSet()
    : ObjectKeySet()
{
}
ObjectKeyIntSet::ObjectKeyIntSet(const ObjectKeyIntSet& other) = default;
ObjectKeyIntSet::~ObjectKeyIntSet() = default;
ObjectKeyNode* ObjectKeyIntSet::NewNode()
{
  return NewOrNull<IntPairNode>();
}

int ObjectKeyIntSet::AddPair(const ObjectKey& key, const int& value)
{
  auto* node = NewOrNull<IntPairNode>(key, value);
  return node != nullptr && Add(node) != 0;
}
