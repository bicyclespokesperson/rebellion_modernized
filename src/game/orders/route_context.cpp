#include "orders/route_context.h"

#include "core/checked_new.h"
#include "economy/manu_mgr.h"
#include "fleet/capship.h"
#include "fleet/fleet_iterator.h"
#include "galaxy/system.h"
#include "galaxy/system_rules.h"
#include "gameobj/object_lookup.h"

namespace
{

constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusRefused = 1;
constexpr int kStatusMissingUnit = 0x12;
constexpr int kStatusMissingDestination = 0x22;
constexpr int kStatusMissingContainer = 0x23;
constexpr int kStatusBadObjectKind = 0x25;
constexpr int kKindCharacterMove = 0x241;
constexpr int kKindMove = 0x204;
constexpr int kKindFacilityMove = 0x270;
constexpr uint32_t kStateFilterAll = 4;
constexpr uint32_t kStateFilterUsable = 1;

bool isOk(const StatusCode& status)
{
  return status.m_second == static_cast<uint32_t>(kStatusOk);
}

StatusCode okStatus()
{
  return {kStatusCategory, kStatusOk};
}

void setStatus(StatusCode* status, int code)
{
  *status = StatusCode(kStatusCategory, code);
}

bool inFamilies(uint32_t family, uint32_t first, uint32_t last)
{
  return family >= first && family < last;
}

bool isSystemFamily(uint32_t family)
{
  return inFamilies(family, 0x90, 0x98);
}
bool isFleetFamily(uint32_t family)
{
  return inFamilies(family, 0x08, 0x10);
}
bool isCapShipFamily(uint32_t family)
{
  return inFamilies(family, 0x14, 0x1c);
}
bool isCharacterFamily(uint32_t family)
{
  return inFamilies(family, 0x30, 0x40);
}

GameObj* asObject(GameObjNotifier* object)
{
  return static_cast<GameObj*>(object);
}

} // namespace

// ---- construction ---------------------------------------------------------------------------------------------

RouteContext::RouteContext()
    : ObjectKeySet()
    , m_flag(1)
    , m_status(kStatusCategory, kStatusRefused)
    , m_side(0)
    , m_arg(0)
    , m_kind(0)
    , m_filter(0)
{
}

RouteContext::~RouteContext() = default;

int RouteContext::Init(int side, int arg, int kind, const int* filter)
{
  m_side = side;
  m_arg = arg;
  m_kind = kind;
  m_status = okStatus();
  m_filter = *filter;
  return 1;
}

int RouteContext::InitFromRequest(const ActivityRequest* request, int arg, const int* filter)
{
  const int side = request->m_side == 1 ? 1 : request->m_side == 2 ? 2 : 0;
  return Init(side, arg, request->m_kind, filter);
}

int RouteContext::InitForObject(GameObj* object, int arg, const int* filter)
{
  const int kind = isCharacterFamily(object->GetFamily()) ? kKindCharacterMove : kKindMove;
  return Init(static_cast<int>(object->ControlSide()), arg, kind, filter);
}

// ---- lookups --------------------------------------------------------------------------------------------------

int RouteContext::ResolveInSideView(const ObjectKey* key, GameObjNotifier** out)
{
  *out = nullptr;
  int view = 0;
  int ok = 0;
  if (isOk(m_status)) {
    if (m_side == 1) {
      view = 1;
      ok = 1;
    } else if (m_side == 2) {
      view = 2;
      ok = 1;
    }
  }
  if (ok) {
    *out = GetObjectInFamilies_01_ff(view, key);
  }
  return ok;
}

int RouteContext::ResolveObject(const ObjectKey* key, StatusCode* status, GameObjNotifier** out)
{
  *status = okStatus();
  *out = nullptr;
  const int ok = ResolveInSideView(key, out);
  if (ok && *out == nullptr) {
    setStatus(status, kStatusMissingDestination);
  }
  return ok;
}

int RouteContext::ResolveUnit(const ObjectKey* key, StatusCode* status, GameObjNotifier** out)
{
  int ok = 1;
  *status = m_status;
  *out = nullptr;
  if (isOk(*status)) {
    ok = ResolveInSideView(key, out);
  }
  if (ok && isOk(*status) && *out == nullptr) {
    setStatus(status, kStatusMissingUnit);
  }
  return ok;
}

int RouteContext::ResolveContainer(GameObj* object, StatusCode* status, GameObjNotifier** out)
{
  *status = okStatus();
  *out = nullptr;
  ObjectKey ownerKey;
  const int ok = ResolveInSideView(GetOwnerKeyValue(object, &ownerKey), out);
  if (ok && *out == nullptr) {
    setStatus(status, kStatusMissingContainer);
  }
  return ok;
}

int RouteContext::FindLegAndParent(const ObjectKey* key, CapacityLeg** leg, CapacityLeg** parent)
{
  *leg = nullptr;
  int found = 0;
  if (isOk(m_status)) {
    *leg = static_cast<CapacityLeg*>(Find(key));
    found = *leg != nullptr;
  }
  *parent = nullptr;
  int result = 0;
  if (found) {
    *parent = static_cast<CapacityLeg*>(Find(&(*leg)->m_parentKey));
    result = *parent != nullptr;
  }
  return result;
}

int RouteContext::CheckObjectKind(GameObj* object, StatusCode* status)
{
  *status = okStatus();
  const uint32_t family = object->GetFamily();
  const bool accepted
      = isSystemFamily(family) || inFamilies(family, 0x98, 0xa0) || isFleetFamily(family) || isCapShipFamily(family);
  if (!accepted) {
    setStatus(status, kStatusBadObjectKind);
  }
  return 1;
}

// ---- legs -----------------------------------------------------------------------------------------------------

int RouteContext::AddLeg(GameObj* object)
{
  auto* leg = NewOrNull<CapacityLeg>();
  int ok = 0;
  if (leg == nullptr) {
    return ok;
  }
  if (leg->m_flag == 0) {
    delete leg;
    return 0;
  }
  ok = leg->Build(m_side, m_arg, m_kind, &m_filter, object);
  if (ok) {
    Add(leg);
  } else {
    delete leg;
  }
  return ok;
}

int RouteContext::AddUnitLegs(const ObjectKey* key)
{
  int ok = 1;
  GameObjNotifier* unit = nullptr;
  if (isOk(m_status)) {
    ok = ResolveObject(key, &m_status, &unit);
  }
  GameObjNotifier* container = nullptr;
  if (!ok) {
    return ok;
  }
  if (isOk(m_status)) {
    ok = ResolveContainer(asObject(unit), &m_status, &container);
  }
  if (!ok) {
    return ok;
  }
  if (isOk(m_status)) {
    ok = CheckObjectKind(asObject(unit), &m_status);
  }
  if (!ok || !isOk(m_status)) {
    return ok;
  }
  if (Find(key)) {
    return ok;
  }
  const int added = AddLeg(asObject(unit));
  ok = 0;
  if (added) {
    GameObjNotifier* parent = nullptr;
    ok = asObject(unit)->GetParent(&parent);
    if (ok) {
      ObjectKey parentKey;
      if (!Find(parent->GetKey(&parentKey))) {
        ok = AddLeg(asObject(parent));
      }
    }
  }
  return ok;
}

int RouteContext::AddContainerLegs(const ObjectKey* key)
{
  int ok = 1;
  GameObjNotifier* unit = nullptr;
  if (isOk(m_status)) {
    ok = ResolveObject(key, &m_status, &unit);
  }
  GameObjNotifier* container = nullptr;
  if (!ok) {
    return ok;
  }
  if (isOk(m_status)) {
    ok = ResolveContainer(asObject(unit), &m_status, &container);
  }
  if (!ok) {
    return ok;
  }
  if (isOk(m_status)) {
    ok = CheckObjectKind(asObject(unit), &m_status);
  }
  if (!ok || !isOk(m_status)) {
    return ok;
  }
  ObjectKey containerKey;
  if (Find(container->GetKey(&containerKey))) {
    return ok;
  }
  const int added = AddLeg(asObject(container));
  ok = 0;
  if (added) {
    GameObjNotifier* parent = nullptr;
    ok = asObject(container)->GetParent(&parent);
    if (ok) {
      ObjectKey parentKey;
      if (!Find(parent->GetKey(&parentKey))) {
        ok = AddLeg(asObject(parent));
      }
    }
  }
  FleetIterator fleets(container, kStateFilterAll);
  fleets.Begin();
  while (ok && fleets.Current()) {
    ok = AddLeg(asObject(fleets.Current()));
    if (ok) {
      CapShipIterator ships(fleets.Current(), kStateFilterAll);
      ships.Begin();
      do {
        if (!ships.Current()) {
          break;
        }
        ok = AddLeg(asObject(ships.Current()));
        ships.Advance();
      } while (ok);
    }
    fleets.Advance();
  }
  return ok;
}

int RouteContext::MarkOverCapacity(const ObjectKey* key)
{
  int result = 1;
  if (Find(key)) {
    CapacityLeg* leg = nullptr;
    CapacityLeg* parent = nullptr;
    result = 0;
    if (FindLegAndParent(key, &leg, &parent)) {
      result = leg->MarkAllIncomingOverCapacity();
    }
  }
  return result;
}

// ---- counting -------------------------------------------------------------------------------------------------

int RouteContext::CountUnit(const ObjectKey* key, StatusCode* status, UnitCountTable* counts)
{
  int ok = 1;
  *status = okStatus();
  GameObjNotifier* unit = nullptr;
  if (isOk(*status)) {
    ok = ResolveUnit(key, status, &unit);
  }
  if (ok && isOk(*status)) {
    ok = counts->CountObject(asObject(unit), &m_filter, 0, status);
  }
  return ok;
}

int RouteContext::CountUnits(const ObjectKeySet* keys, StatusCode* status, UnitCountTable* counts)
{
  int ok = 1;
  *status = okStatus();
  for (auto nodePosition = keys->Entries().begin(); nodePosition != keys->Entries().end();) {
    ObjectKeyNode* node = static_cast<ObjectKeyNode*>((*(nodePosition++)).get());
    if (!(isOk(*status) && true)) {
      break;
    }

    ObjectKey key;
    ok = CountUnit(node->GetKey(&key), status, counts);
    if (!ok) {
      return 0;
    }
  }
  return ok;
}

int RouteContext::FreeParent(const ObjectKey* key, StatusCode* status)
{
  int ok = 1;
  *status = okStatus();
  UnitCountTable counts;
  GameObjNotifier* unit = nullptr;
  if (isOk(*status)) {
    ok = ResolveUnit(key, status, &unit);
  }
  if (ok && isOk(*status)) {
    ok = counts.CountObject(asObject(unit), &m_filter, 0, status);
  }
  if (ok && isOk(*status)) {
    ObjectKey parentKey;
    ok = asObject(unit)->GetValidParentKey(&parentKey);
    if (ok && Find(&parentKey)) {
      ok = Commit(&counts, 0, &parentKey);
    }
  }
  return ok;
}

int RouteContext::FreeParentsOf(const ObjectKeySet* units, StatusCode* status)
{
  int ok = 1;
  *status = okStatus();
  for (auto nodePosition = units->Entries().begin(); nodePosition != units->Entries().end();) {
    ObjectKeyNode* node = static_cast<ObjectKeyNode*>((*(nodePosition++)).get());
    if (!(isOk(*status) && true)) {
      break;
    }

    ObjectKey key;
    ok = FreeParent(node->GetKey(&key), status);
    if (!ok) {
      return 0;
    }
  }
  return ok;
}

int RouteContext::FreeCarriedParents(GameObj* object, StatusCode* status)
{
  *status = okStatus();
  UnitCountTable unusedCounts;
  ObjectKeyValueSet carried;
  int result = 0;
  if (static_cast<ManuMgr*>(object)->ManuMgr::GetQueueCopy(&carried)) {
    result = FreeParentsOf(&carried, status);
  }
  return result;
}

// ---- room at the destination ----------------------------------------------------------------------------------

int RouteContext::MinRoomAt(GameObj* object, UnitCountTable* counts, int consume, CapacityLeg** best,
                            CapacitySlot* slot)
{
  ObjectKey key;
  object->GetKey(&key);
  CapacityLeg* leg = nullptr;
  CapacityLeg* parent = nullptr;
  const int found = FindLegAndParent(&key, &leg, &parent);
  CapacityScratch legRoom;
  CapacityScratch arrivalRoom;
  CapacityScratch parentRoom;
  CapacityScratch chosen;
  int ok = 0;
  if (found) {
    ok = leg->MinRemainingBoth(counts, consume, &legRoom, &arrivalRoom);
  }
  int result = 0;
  if (!ok) {
    return result;
  }
  result = parent->MinRemainingOutgoing(counts, consume, &parentRoom);
  if (!result) {
    return result;
  }
  chosen = legRoom.IsReplacedBy(parentRoom) ? parentRoom : legRoom;
  StatusCode error;
  const bool fits = chosen.GetError(&error)->m_second == static_cast<uint32_t>(kStatusOk);
  if (fits) {
    chosen = arrivalRoom;
  }
  if (*best != nullptr && !chosen.IsReplacedBy(*slot)) {
    return result;
  }
  *best = leg;
  *slot = chosen;
  return result;
}

int RouteContext::CheckRoom(UnitCountTable* counts, int consume, const ObjectKey* destination, StatusCode* status,
                            CapacitySlot* slot, ObjectKey* tightest)
{
  tightest->Reset();
  *status = okStatus();
  slot->Reset();
  CapacityScratch unused;
  CapacityLeg* best = nullptr;
  int ok = counts->AllNonNegative();
  GameObjNotifier* object = nullptr;
  if (ok) {
    if (isOk(*status)) {
      ok = ResolveObject(destination, status, &object);
    }
    if (ok && isOk(*status)) {
      ok = MinRoomAt(asObject(object), counts, consume, &best, slot);
      if (ok) {
        const uint32_t family = object->GetFamily();
        if (isSystemFamily(family)) {
          GameObjNotifier* fleet = nullptr;
          auto* system = static_cast<System*>(asObject(object));
          ok = m_kind == kKindFacilityMove ? system->FindFleetWithFlag04(m_arg, &fleet)
                                           : system->FindFleetWithFlag02(m_arg, &fleet);
          if (ok) {
            if (fleet != nullptr) {
              ok = MinRoomAt(asObject(fleet), counts, consume, &best, slot);
            }
            if (ok) {
              StatusCode error;
              const bool fullyFits = slot->GetError(&error)->m_second == static_cast<uint32_t>(kStatusOk);
              if (!fullyFits) {
                FleetIterator fleets(system, kStateFilterUsable, static_cast<uint32_t>(m_arg));
                fleets.Begin();
                while (fleets.Current() && ok) {
                  CapShipIterator ships(fleets.Current(), kStateFilterUsable, static_cast<uint32_t>(m_arg));
                  ships.Begin();
                  while (ships.Current() && ok) {
                    ok = MinRoomAt(asObject(ships.Current()), counts, consume, &best, slot);
                    ships.Advance();
                  }
                  fleets.Advance();
                }
              }
            }
          }
        } else if (isFleetFamily(family)) {
          CapShipIterator ships(object, kStateFilterUsable, static_cast<uint32_t>(m_arg));
          ships.Begin();
          while (ships.Current() && ok) {
            ok = MinRoomAt(asObject(ships.Current()), counts, consume, &best, slot);
            ships.Advance();
          }
        } else if (!isCapShipFamily(family) && !inFamilies(family, 0x98, 0xa0)) {
          ok = 0;
        }
      }
    }
    if (ok) {
      if (isOk(*status)) {
        StatusCode error;
        *status = *slot->GetError(&error);
      }
      if (ok && isOk(*status)) {
        ObjectKey key;
        *tightest = *best->GetKey(&key);
      }
    }
  }
  if (!ok) {
    setStatus(status, kStatusRefused);
  }
  return ok;
}

int RouteContext::CheckRoomCounts(UnitCountTable* counts, int consume, const ObjectKey* destination, StatusCode* status,
                                  CapacitySlot* slot)
{
  *status = okStatus();
  slot->Reset();
  GameObjNotifier* object = nullptr;
  CapacityLeg* best = nullptr;
  int ok = counts->AllNonNegative();
  if (ok) {
    if (isOk(*status)) {
      ok = ResolveObject(destination, status, &object);
    }
    if (ok && isOk(*status)) {
      ok = MinRoomAt(asObject(object), counts, consume, &best, slot);
    }
    if (ok && isOk(*status)) {
      StatusCode error;
      *status = *slot->GetError(&error);
    }
  }
  if (!ok) {
    setStatus(status, kStatusRefused);
  }
  return ok;
}

int RouteContext::Commit(UnitCountTable* counts, int consume, const ObjectKey* key)
{
  CapacityLeg* leg = nullptr;
  CapacityLeg* parent = nullptr;
  int ok = FindLegAndParent(key, &leg, &parent);
  int committed = 0;
  if (ok) {
    committed = leg->CommitBoth(counts, consume);
  }
  int result = 0;
  if (committed) {
    result = parent->CommitOutgoing(counts, consume);
  }
  return result;
}

// ---- whole checks ---------------------------------------------------------------------------------------------

int RouteContext::CheckMove(const ObjectKey* unit, const ObjectKey* destination, StatusCode* status, CapacitySlot* slot,
                            ObjectKey* tightest)
{
  int ok = 1;
  *status = okStatus();
  slot->Reset();
  tightest->Reset();
  UnitCountTable counts;
  if (isOk(*status)) {
    ok = CountUnit(unit, status, &counts);
  }
  if (ok) {
    if (isOk(*status)) {
      ok = FreeParent(unit, status);
    }
    if (ok) {
      if (isOk(*status)) {
        ok = CheckRoom(&counts, 1, destination, status, slot, tightest);
      }
      if (ok && isOk(*status)) {
        ok = Commit(&counts, 1, tightest);
      }
    }
  }
  return ok;
}

int RouteContext::CheckMoveSets(const ObjectKeySet* units, const ObjectKeySet* others, const ObjectKey* destination,
                                StatusCode* status, CapacitySlot* slot, ObjectKey* tightest)
{
  int ok = 1;
  *status = okStatus();
  slot->Reset();
  tightest->Reset();
  UnitCountTable counts;
  if (isOk(*status)) {
    ok = CountUnits(units, status, &counts);
  }
  if (ok) {
    if (isOk(*status)) {
      ok = CountUnits(others, status, &counts);
    }
    if (ok) {
      if (isOk(*status)) {
        ok = FreeParentsOf(units, status);
      }
      if (ok) {
        if (isOk(*status)) {
          ok = FreeParentsOf(others, status);
        }
        if (ok) {
          if (isOk(*status)) {
            ok = CheckRoom(&counts, 1, destination, status, slot, tightest);
          }
          if (ok && isOk(*status)) {
            ok = Commit(&counts, 1, tightest);
          }
        }
      }
    }
  }
  return ok;
}

int RouteContext::CheckArrival(const ObjectKey* unit, const ObjectKey* destination, StatusCode* status,
                               CapacitySlot* slot, ObjectKey* tightest)
{
  int ok = 1;
  *status = okStatus();
  slot->Reset();
  tightest->Reset();
  UnitCountTable counts;
  if (isOk(*status)) {
    ok = CountUnit(unit, status, &counts);
  }
  if (ok) {
    if (isOk(*status)) {
      ok = CheckRoom(&counts, 1, destination, status, slot, tightest);
    }
    if (ok && isOk(*status)) {
      ok = Commit(&counts, 1, tightest);
    }
  }
  return ok;
}

int RouteContext::CheckArrivalOfFamily(const ObjectKey* unitKey, const ObjectKey* destination, StatusCode* status,
                                       CapacitySlot* slot, ObjectKey* tightest)
{
  int ok = 1;
  *status = okStatus();
  slot->Reset();
  tightest->Reset();
  UnitCountTable counts;
  if (isOk(*status)) {
    ok = counts.AddFamily(unitKey->Family(), 1, status);
  }
  if (ok) {
    if (isOk(*status)) {
      ok = CheckRoom(&counts, 1, destination, status, slot, tightest);
    }
    if (ok && isOk(*status)) {
      ok = Commit(&counts, 1, tightest);
    }
  }
  return ok;
}

int RouteContext::CheckMoveSimple(const ObjectKey* unit, const ObjectKey* destination, StatusCode* status,
                                  CapacitySlot* slot)
{
  int ok = 1;
  *status = okStatus();
  slot->Reset();
  UnitCountTable counts;
  if (isOk(*status)) {
    ok = CountUnit(unit, status, &counts);
  }
  if (ok) {
    if (isOk(*status)) {
      ok = FreeParent(unit, status);
    }
    if (ok && isOk(*status)) {
      ok = CheckRoomCounts(&counts, 1, destination, status, slot);
    }
  }
  if (ok && isOk(*status)) {
    ok = Commit(&counts, 1, destination);
  }
  return ok;
}

int RouteContext::CheckMoveInverted(const ObjectKey* unit, const ObjectKey* destination, StatusCode* status,
                                    CapacitySlot* slot)
{
  int ok = 1;
  *status = okStatus();
  slot->Reset();
  UnitCountTable counts;
  GameObjNotifier* object = nullptr;
  counts.ResetAll();
  if (isOk(*status)) {
    ok = ResolveUnit(unit, status, &object);
  }
  if (ok && isOk(*status)) {
    ok = counts.CountObject(asObject(object), &m_filter, 1, status);
  }
  if (ok && isOk(*status)) {
    ok = CheckRoomCounts(&counts, 1, destination, status, slot);
  }
  if (ok && isOk(*status)) {
    ok = Commit(&counts, 1, destination);
  }
  return ok;
}

int RouteContext::CheckFamilyArrival(const ObjectKey* unitKey, const ObjectKey* destination, StatusCode* status,
                                     CapacitySlot* slot)
{
  int ok = 1;
  *status = okStatus();
  slot->Reset();
  UnitCountTable counts;
  if (isOk(*status)) {
    ok = counts.AddFamily(unitKey->Family(), 1, status);
  }
  if (ok) {
    if (isOk(*status)) {
      ok = CheckRoomCounts(&counts, 1, destination, status, slot);
    }
    if (ok && isOk(*status)) {
      ok = Commit(&counts, 1, destination);
    }
  }
  return ok;
}

int RouteContext::CheckUnitArrival(const ObjectKey* unit, const ObjectKey* destination, StatusCode* status,
                                   CapacitySlot* slot)
{
  int ok = 1;
  *status = okStatus();
  slot->Reset();
  UnitCountTable counts;
  if (isOk(*status)) {
    ok = CountUnit(unit, status, &counts);
  }
  if (ok) {
    if (isOk(*status)) {
      ok = CheckRoomCounts(&counts, 1, destination, status, slot);
    }
    if (ok && isOk(*status)) {
      ok = Commit(&counts, 1, destination);
    }
  }
  return ok;
}
