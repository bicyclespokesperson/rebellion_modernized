#include "rules/rules_mgr.h"
#include "data/sd_record_bases.h"

#include <new>

#include "core/checked_new.h"
#include "data/game_math.h"
#include "data/gnprtb.h"
#include "data/random.h"
#include "economy/manu_mgr.h"
#include "events/event_helpers.h"
#include "events/event_post.h"
#include "events/game_clock_hooks.h"
#include "fleet/capship.h"
#include "fleet/fight_squad.h"
#include "fleet/fleet_iterator.h"
#include "galaxy/game_setup_seed.h"
#include "galaxy/system.h"
#include "galaxy/system_neighbours.h"
#include "galaxy/system_rules_1e.h"
#include "gameobj/object_family.h"
#include "gameobj/object_queries.h"
#include "gameobj/object_type_db.h"
#include "orders/route_capacity.h"
#include "rules/event_rules.h"
#include "rules/loyalty_params.h"

extern int g_rulesMgrEventKindsBuilt;     // 0x6bb344
extern EventKindSet g_rulesMgrEventKinds; // 0x6bb348

ObjectKey* GetAiRecordKey(DatRecord* record, ObjectKey* out); // 0x402e40

namespace
{

constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusRefused = 1;
constexpr int kStatusNotSupported = 0x17;
constexpr int kStatusNotAllowed = 0x18;
constexpr int kStatusDifferentControl = 0x28;
constexpr int kTimerDisasterKind = 0x38f;
constexpr int kTimerResourceKind = 0x390;
constexpr uint32_t kRulesMgrId = 7;
constexpr int kBothFilter = 3;
constexpr uint32_t kHyperdriveActive = 0x800;
constexpr uint32_t kAutorouting = 0x1000;
constexpr uint32_t kSideMask = 0xc0;
constexpr FamilyRange kSystemFamilies(0x90, 0x98);
constexpr FamilyRange kManufacturingFamilies(0x98, 0xa0);
constexpr FamilyRange kCapitalShipFamilies(0x14, 0x1c);
constexpr FamilyRange kFleetFamilies(0x08, 0x10);
constexpr FamilyRange kFacilityFamilies(0x20, 0x22);
constexpr FamilyRange kTroopFamilies(0x10, 0x14);
constexpr FamilyRange kShipLikeFamilies(0x14, 0x20);
constexpr FamilyRange kSectorFamilies(0x80, 0x90);
constexpr FamilyRange kUnitFamilies(0x10, 0x40);
constexpr int kMaxResidualPercentForIncident = 0x1e;

using namespace event_rules;
using events::succeeded;

int both(int result, int ok)
{
  return result != 0 && ok != 0 ? 1 : 0;
}

GameObj* asObject(GameObjNotifier* object)
{
  return static_cast<GameObj*>(object);
}

bool inFamilies(const GameObjNotifier* object, const FamilyRange& range)
{
  const uint32_t family = const_cast<GameObjNotifier*>(object)->GetFamily();
  return family >= range.m_first && family < range.m_second;
}

bool keyInFamilies(const ObjectKey& key, const FamilyRange& range)
{
  return key.Family() >= range.m_first && key.Family() < range.m_second;
}

void setStatus(StatusCode* status, uint32_t first, uint32_t second)
{
  *status = StatusCode(first, second);
}

uint32_t controlSide(const GameObjNotifier* object)
{
  return object->ControlSide();
}

// The side the other one fights: both stays both.
uint32_t opposingSide(uint32_t side)
{
  return side == 3 ? 3 : 2 - (side != 1);
}

} // namespace

// ---- object plumbing --------------------------------------------------------------------------------------------

RulesMgr::RulesMgr()
    : GameObj()
    , m_timerA()
    , m_timerB()
    , m_neighbours(nullptr)
{
  SetId(kRulesMgrId);
}

RulesMgr::~RulesMgr()
{
  if ((m_flags & 0x30) == 0 && m_neighbours != nullptr) {
    delete m_neighbours;
    m_neighbours = nullptr;
  }
}

uint32_t RulesMgr::GetFamily()
{
  return 0xf9;
}

int RulesMgr::Read(istream* in)
{
  return ReadBody(in);
}

int RulesMgr::Write(ostream* out)
{
  return WriteBody(out);
}

int RulesMgr::Checksum(Crc32* crc)
{
  return ChecksumBody(crc);
}

int RulesMgr::ApplyAll(NotifyContext* ctx)
{
  return IsSideView() ? ApplyAllBody(ctx) : 0;
}

int RulesMgr::UpdateObservedByAlliance(NotifyContext* ctx)
{
  return SetObservedByAlliance(1, ctx);
}

int RulesMgr::UpdateObservedByEmpire(NotifyContext* ctx)
{
  return SetObservedByEmpire(1, ctx);
}

int RulesMgr::CreateViews(int controlKind, NotifyContext* ctx)
{
  return CreateViewsBody(controlKind, ctx);
}

GameObj* RulesMgr::CreateViewObject()
{
  return NewOrNull<RulesMgr>();
}

GameObjNotifier* CreateRulesMgr()
{
  return NewOrNull<RulesMgr>();
}

int RulesMgr::Validate()
{
  return GameObj::Validate();
}

int RulesMgr::ReadBody(istream* in)
{
  int result = GameObj::Read(in);
  if (result != 0) {
    result = m_timerA.Read(in);
    if (result != 0) {
      result = m_timerB.Read(in);
    }
  }
  return result;
}

int RulesMgr::WriteBody(ostream* out)
{
  int result = GameObj::Write(out);
  if (result != 0) {
    result = m_timerA.Write(out);
    if (result != 0) {
      result = m_timerB.Write(out);
    }
  }
  return result;
}

int RulesMgr::ChecksumBody(Crc32* crc)
{
  int result = GameObj::Checksum(crc);
  if (result != 0) {
    result = m_timerA.Checksum(crc);
    if (result != 0) {
      result = m_timerB.Checksum(crc);
    }
  }
  return result;
}

int RulesMgr::ApplyAllBody(NotifyContext* ctx)
{
  return GameObj::ApplyAll(ctx);
}

int RulesMgr::CreateViewsBody(int controlKind, NotifyContext* ctx)
{
  int result = 0;
  if (GameObj::CreateViews(controlKind, ctx) != 0) {
    m_timerA.SetLevel(g_gnprtb1E25);
    m_timerA.SetProgress(g_gnprtb1E26);
    {
      NotifyContext timerCtx(*ctx);
      timerCtx.m_reserved14 = CallGameHook2AFC();
      result = ArmTimerAfterSetFlag(kTimerDisasterKind, 1, this, &m_timerA, &timerCtx);
    }
    if (result != 0) {
      m_timerB.SetLevel(g_gnprtb1E27);
      m_timerB.SetProgress(g_gnprtb1E28);
      NotifyContext timerCtx(*ctx);
      timerCtx.m_reserved14 = CallGameHook2AFC();
      result = ArmTimerAfterSetFlag(kTimerResourceKind, 1, this, &m_timerB, &timerCtx);
    }
  }
  return result;
}

int RulesMgr::BuildNeighbourTable(NotifyContext*)
{
  if (m_neighbours != nullptr) {
    delete m_neighbours;
  }
  m_neighbours = NewOrNull<SystemNeighbourTable>();
  int built = 0;
  if (m_neighbours != nullptr) {
    built = m_neighbours->Build();
    if (built == 0) {
      if (m_neighbours != nullptr) {
        delete m_neighbours;
      }
      m_neighbours = nullptr;
    }
  }
  return built;
}

int RulesMgr::GetNeighbours(const ObjectKey* system, SystemNeighbourCursor* out)
{
  int result = 0;
  if (m_neighbours != nullptr) {
    result = m_neighbours->GetNeighbours(system, out);
  }
  return result;
}

int BuildRulesEventKinds()
{
  int result = 1;
  if (g_rulesMgrEventKindsBuilt == 0) {
    g_rulesMgrEventKindsBuilt = 1;
    result = InitEventKinds(0, &g_rulesMgrEventKinds);
  }
  return result;
}

int RulesMgr::GetEventKinds(int, EventKindSet* out)
{
  const int result = BuildRulesEventKinds();
  if (result != 0) {
    *out = g_rulesMgrEventKinds;
  }
  return result;
}

int RulesMgr::CheckOrder(int, int orderKind, StatusCode* out)
{
  setStatus(out, kStatusCategory, kStatusNotSupported);
  const int result = BuildRulesEventKinds();
  if (result != 0 && g_rulesMgrEventKinds.Contains(orderKind) != 0) {
    setStatus(out, kStatusCategory, kStatusOk);
  }
  return result;
}

// ---- capacity checks of moves -----------------------------------------------------------------------------------

int RulesMgr::CheckParentCapacity(GameObj* unit, StatusCode* status)
{
  setStatus(status, kStatusCategory, kStatusOk);
  RouteContext route;
  const int filter = kBothFilter;
  int ok = route.InitForObject(unit, unit->ControlSide(), &filter);
  ObjectKey parentKey;
  int result = 0;
  if (ok != 0) {
    ok = unit->GetValidParentKey(&parentKey);
    result = 0;
    if (ok != 0) {
      result = route.AddUnitLegs(&parentKey);
    }
  }
  CapacityScratch slot;
  if (result != 0) {
    if (succeeded(*status)) {
      ObjectKey unitKey;
      unit->GetKey(&unitKey);
      result = route.CheckMoveInverted(&unitKey, &parentKey, status, &slot);
    }
  }
  if (result == 0) {
    setStatus(status, kStatusCategory, kStatusRefused);
  }
  return result;
}

int RulesMgr::CheckRequestMove(GameObj* unit, const ObjectKey* destination, ActivityRequest* request,
                               StatusCode* status)
{
  setStatus(status, kStatusCategory, kStatusOk);
  RouteContext route;
  const int filter = kBothFilter;
  int result = 0;
  if (route.InitFromRequest(request, unit->ControlSide(), &filter) != 0) {
    result = CheckMoveInRoute(unit, destination, &route, status);
  }
  return result;
}

int RulesMgr::CheckUnitMove(GameObj* unit, const ObjectKey* destination, StatusCode* status)
{
  setStatus(status, kStatusCategory, kStatusOk);
  RouteContext route;
  const int filter = kBothFilter;
  int result = 0;
  if (route.InitForObject(unit, unit->ControlSide(), &filter) != 0) {
    result = CheckMoveInRoute(unit, destination, &route, status);
  }
  return result;
}

int RulesMgr::CheckMoveInRoute(GameObj* unit, const ObjectKey* destination, RouteContext* route, StatusCode* status)
{
  setStatus(status, kStatusCategory, kStatusOk);
  ObjectKey parentKey;
  int result = 0;
  int ok = route->AddUnitLegs(destination);
  if (ok != 0) {
    ok = unit->GetValidParentKey(&parentKey);
    result = 0;
    if (ok != 0) {
      result = route->AddUnitLegs(&parentKey);
    }
  }
  CapacityScratch slot;
  if (result != 0) {
    if (succeeded(*status)) {
      ObjectKey unitKey;
      unit->GetKey(&unitKey);
      result = route->CheckMoveSimple(&unitKey, destination, status, &slot);
    }
    if (result != 0) {
      return result;
    }
  }
  setStatus(status, kStatusCategory, kStatusRefused);
  return result;
}

// ---- relocation -------------------------------------------------------------------------------------------------

int RulesMgr::TryPlaceUnit(GameObj* unit, GameObjNotifier* candidate, int ignoreParent, ObjectKey* tightest,
                           StatusCode* status)
{
  tightest->Reset();
  setStatus(status, kStatusCategory, kStatusOk);
  RouteContext route;
  int ok = 1;
  if (succeeded(*status)) {
    const int filter = kBothFilter;
    ok = route.InitForObject(unit, unit->ControlSide(), &filter);
    if (ok != 0) {
      ObjectKey candidateKey;
      candidate->GetKey(&candidateKey);
      ok = route.AddContainerLegs(&candidateKey);
      if (ok != 0 && ignoreParent == 0) {
        ObjectKey parentKey;
        ok = unit->GetValidParentKey(&parentKey);
        if (ok != 0) {
          ok = route.MarkOverCapacity(&parentKey);
        }
      }
    }
  }
  if (ok != 0) {
    if (succeeded(*status)) {
      CapacityScratch slot;
      ObjectKey candidateKey;
      candidate->GetKey(&candidateKey);
      ObjectKey unitKey;
      unit->GetKey(&unitKey);
      ok = route.CheckMove(&unitKey, &candidateKey, status, &slot, tightest);
    }
    if (ok != 0) {
      return ok;
    }
  }
  setStatus(status, kStatusCategory, kStatusRefused);
  return ok;
}

int RulesMgr::FindDestination(GameObj* unit, const ObjectKey* home, const ObjectKey* target, int flag, int tryAncestors,
                              ObjectKey* out, StatusCode* status)
{
  int ok = 1;
  setStatus(status, kStatusCategory, kStatusRefused);
  if (succeeded(*status) == false && tryAncestors != 0) {
    ok = TryAncestorsOfUnit(unit, flag, out, status);
  }
  if (ok != 0) {
    if (!succeeded(*status)) {
      ok = TryAncestorsOfTarget(unit, target, out, status);
    }
    if (ok != 0) {
      if (!succeeded(*status)) {
        ok = TryFriendlySystems(unit, home, out, status);
      }
      if (ok != 0) {
        if (!succeeded(*status)) {
          ok = TryFriendlyFleets(unit, home, out, status);
        }
        if (ok != 0) {
          if (!succeeded(*status)) {
            ok = TryBlockadedSystems(unit, home, out, status);
          }
          if (ok != 0) {
            if (!succeeded(*status)) {
              ok = TryNeutralSystems(unit, home, out, status);
            }
            if (ok != 0 && !succeeded(*status)) {
              ok = TryEnemySystems(unit, home, out, status);
            }
          }
        }
      }
    }
  }
  return ok;
}

int RulesMgr::TryAncestorsOfUnit(GameObj* unit, int flag, ObjectKey* out, StatusCode* status)
{
  setStatus(status, kStatusCategory, kStatusRefused);
  GameObjNotifier* parent = nullptr;
  GameObjNotifier* grandParent = nullptr;
  GameObjNotifier* greatGrandParent = nullptr;
  ObjectKey ownerKey;
  int result = unit->GetOwnerKey(&ownerKey);
  if (result == 0) {
    return 0;
  }
  if (keyInFamilies(ownerKey, kManufacturingFamilies)) {
    return result;
  }
  result = unit->GetParent(&parent);
  if (result == 0) {
    return 0;
  }
  if (!succeeded(*status)) {
    const bool tryParent = inFamilies(parent, kCapitalShipFamilies) || inFamilies(parent, kFleetFamilies)
                           || inFamilies(parent, kSystemFamilies);
    if (tryParent) {
      result = TryPlaceUnit(unit, parent, flag, out, status);
    }
  }
  if (result == 0) {
    return 0;
  }
  if (!succeeded(*status)) {
    if (inFamilies(parent, kCapitalShipFamilies) || inFamilies(parent, kFleetFamilies)) {
      result = static_cast<GameObj*>(parent)->GetParent(&grandParent);
      if (result == 0) {
        return 0;
      }
      result = TryPlaceUnit(unit, grandParent, flag, out, status);
    }
  }
  if (result == 0 || succeeded(*status)) {
    return result;
  }
  if (inFamilies(parent, kCapitalShipFamilies)) {
    result = static_cast<GameObj*>(grandParent)->GetParent(&greatGrandParent);
    if (result == 0) {
      return 0;
    }
    result = TryPlaceUnit(unit, greatGrandParent, flag, out, status);
  }
  return result;
}

int RulesMgr::TryAncestorsOfTarget(GameObj* unit, const ObjectKey* target, ObjectKey* out, StatusCode* status)
{
  setStatus(status, kStatusCategory, kStatusRefused);
  GameObjNotifier* targetObject = nullptr;
  GameObjNotifier* parent = nullptr;
  GameObjNotifier* grandParent = nullptr;
  if (const_cast<ObjectKey*>(target)->OrNull() != nullptr) {
    targetObject = GetMasterInFamilies_01_ff(target);
  }
  ObjectKey parentKey;
  int result = unit->GetValidParentKey(&parentKey);
  if (result == 0) {
    return result;
  }
  int differs = 0;
  if (targetObject != nullptr) {
    ObjectKey targetKey;
    targetObject->GetKey(&targetKey);
    differs = targetKey.m_value != parentKey.m_value;
  }
  if (targetObject == nullptr || differs == 0) {
    return result;
  }
  ObjectKey ownerKey;
  result = targetObject->GetOwnerKey(&ownerKey);
  if (result == 0) {
    return result;
  }
  if (keyInFamilies(ownerKey, kManufacturingFamilies)) {
    return result;
  }
  if (!succeeded(*status)) {
    const bool tryTarget = inFamilies(targetObject, kCapitalShipFamilies) || inFamilies(targetObject, kFleetFamilies)
                           || inFamilies(targetObject, kSystemFamilies);
    if (tryTarget) {
      result = TryPlaceUnit(unit, targetObject, 0, out, status);
    }
  }
  if (result == 0) {
    return result;
  }
  if (!succeeded(*status)) {
    if (inFamilies(targetObject, kCapitalShipFamilies) || inFamilies(targetObject, kFleetFamilies)) {
      result = static_cast<GameObj*>(targetObject)->GetParent(&parent);
      if (result == 0) {
        return result;
      }
      result = TryPlaceUnit(unit, parent, 0, out, status);
    }
  }
  if (result == 0 || succeeded(*status)) {
    return result;
  }
  if (inFamilies(targetObject, kCapitalShipFamilies)) {
    result = static_cast<GameObj*>(parent)->GetParent(&grandParent);
    if (result == 0) {
      return result;
    }
    result = TryPlaceUnit(unit, grandParent, 0, out, status);
  }
  return result;
}

namespace
{

// Walks the neighbour cursor of `home` while the status is not a success; `visit` returns the new result for a system.
template <typename Visit>
int walkNeighbours(RulesMgr* rules, const ObjectKey* home, StatusCode* status, Visit visit)
{
  setStatus(status, kStatusCategory, kStatusRefused);
  SystemNeighbourCursor cursor;
  int result = rules->GetNeighbours(home, &cursor);
  if (cursor.m_begin != nullptr) {
    cursor.m_current = cursor.m_begin;
  }
  while (result != 0 && *cursor.m_current != nullptr && !succeeded(*status)) {
    result = visit(*cursor.m_current, result);
    if (*cursor.m_current != nullptr) {
      ++cursor.m_current;
    }
  }
  return result;
}

} // namespace

int RulesMgr::TryFriendlySystems(GameObj* unit, const ObjectKey* home, ObjectKey* out, StatusCode* status)
{
  return walkNeighbours(this, home, status, [&](GameObjNotifier* system, int result) {
    if (((unit->m_flags ^ system->m_flags) & kSideMask) == 0
        && (static_cast<System*>(system)->m_flags & system_flag::kBlockade) == 0) {
      result = TryPlaceUnit(unit, system, 0, out, status);
    }
    return result;
  });
}

int RulesMgr::TryFriendlyFleets(GameObj* unit, const ObjectKey* home, ObjectKey* out, StatusCode* status)
{
  return walkNeighbours(this, home, status, [&](GameObjNotifier* system, int result) {
    FleetIterator fleets(system, 3, unit->ControlSide());
    for (fleets.Begin(); result != 0 && fleets.Current() != nullptr && !succeeded(*status); fleets.Advance()) {
      result = TryPlaceUnit(unit, fleets.Current(), 0, out, status);
    }
    return result;
  });
}

int RulesMgr::TryBlockadedSystems(GameObj* unit, const ObjectKey* home, ObjectKey* out, StatusCode* status)
{
  return walkNeighbours(this, home, status, [&](GameObjNotifier* system, int result) {
    if (((unit->m_flags ^ system->m_flags) & kSideMask) == 0
        && (static_cast<System*>(system)->m_flags & system_flag::kBlockade) != 0) {
      result = TryPlaceUnit(unit, system, 0, out, status);
    }
    return result;
  });
}

int RulesMgr::TryNeutralSystems(GameObj* unit, const ObjectKey* home, ObjectKey* out, StatusCode* status)
{
  return walkNeighbours(this, home, status, [&](GameObjNotifier* system, int result) {
    if ((system->m_flags & kSideMask) == kSideMask) {
      result = TryPlaceUnit(unit, system, 0, out, status);
    }
    return result;
  });
}

int RulesMgr::TryEnemySystems(GameObj* unit, const ObjectKey* home, ObjectKey* out, StatusCode* status)
{
  setStatus(status, kStatusCategory, kStatusRefused);
  int result = 1;
  if ((unit->m_flags & kSideMask) != kSideMask) {
    result = walkNeighbours(this, home, status, [&](GameObjNotifier* system, int result) {
      if (system->ControlSide() == opposingSide(unit->ControlSide())) {
        result = TryPlaceUnit(unit, system, 0, out, status);
      }
      return result;
    });
  }
  return result;
}

// ---- owners and locations ---------------------------------------------------------------------------------------

int RulesMgr::GetOwnerObject(GameObjNotifier* object, GameObjNotifier** out)
{
  *out = nullptr;
  ObjectKey key;
  int result = object->GetOwnerKey(&key);
  if (result != 0 && key.OrNull() != nullptr) {
    GameObjNotifier* owner = GetMasterInFamilies_01_ff(&key);
    result = owner != nullptr;
    *out = owner;
  }
  return result;
}

int RulesMgr::AreInSameOwner(GameObjNotifier* a, GameObjNotifier* b, uint32_t* same)
{
  GameObjNotifier* ownerA = nullptr;
  GameObjNotifier* ownerB = nullptr;
  *same = 1;
  const int found = GetOwnerObject(a, &ownerA);
  int result = 0;
  if (found != 0) {
    result = GetOwnerObject(b, &ownerB);
    if (result != 0 && ownerA != nullptr && ownerB != nullptr) {
      ObjectKey keyB;
      ObjectKey keyA;
      ownerB->GetKey(&keyB);
      ownerA->GetKey(&keyA);
      *same = keyA.m_value == keyB.m_value;
    }
  }
  return result;
}

int RulesMgr::AreKeysInSameOwner(const ObjectKey* a, const ObjectKey* b, uint32_t* same)
{
  GameObjNotifier* first = GetMasterInFamilies_01_ff(a);
  if (first == nullptr) {
    return 0;
  }
  GameObjNotifier* second = GetMasterInFamilies_01_ff(b);
  return AreInSameOwner(first, second, same);
}

// ---- travel -----------------------------------------------------------------------------------------------------

int RulesMgr::ComputeTravelDays(int speed, int view, const ObjectKey* from, const ObjectKey* to, int* days)
{
  *days = 0;
  GameObjNotifier* fromObject = nullptr;
  GameObjNotifier* ownerFrom = nullptr;
  GameObjNotifier* ownerTo = nullptr;
  fromObject = LookupObject(from, view);
  GameObjNotifier* toObject = LookupObject(to, view);
  int ok = toObject != nullptr && fromObject != nullptr;
  if (fromObject != nullptr) {
    ok = both(GetOwnerObject(fromObject, &ownerFrom), ok);
  }
  if (toObject != nullptr) {
    ok = both(GetOwnerObject(toObject, &ownerTo), ok);
  }
  if (ownerFrom == nullptr || ownerTo == nullptr) {
    return ok;
  }
  ObjectKey keyTo;
  ObjectKey keyFrom;
  ownerTo->GetKey(&keyTo);
  ownerFrom->GetKey(&keyFrom);
  if (keyFrom.m_value == keyTo.m_value) {
    return ok;
  }
  *days = 1;
  if (inFamilies(ownerFrom, kSystemFamilies) && inFamilies(ownerTo, kSystemFamilies)) {
    uint32_t fromPacked = 0;
    uint32_t toPacked = 0;
    static_cast<System*>(ownerFrom)->GetRecordField4c(&fromPacked);
    static_cast<System*>(ownerTo)->GetRecordField4c(&toPacked);
    const MapPosition fromPosition{static_cast<int16_t>(fromPacked), static_cast<int16_t>(fromPacked >> 16)};
    const MapPosition toPosition{static_cast<int16_t>(toPacked), static_cast<int16_t>(toPacked >> 16)};
    *days = TravelDaysAtSpeed(&fromPosition, &toPosition, speed);
  }
  return ok;
}

int RulesMgr::ComputeUnitTravelDays(GameObj* unit, const ObjectKey* from, const ObjectKey* to, int* days)
{
  *days = 0;
  int ok = 1;
  if (unit->IsConstructed() != 0) {
    const int view = unit->ViewSide();
    ok = ComputeTravelDays(unit->GetTransitSpeed(1), view, from, to, days) != 0;
  }
  return ok;
}

int RulesMgr::ComputeRecordTravelDays(DatRecord* record, uint32_t viewSide, const ObjectKey* from, const ObjectKey* to,
                                      int* days)
{
  *days = 0;
  int speed = 0;
  ObjectKey recordKey;
  GetAiRecordKey(record, &recordKey);
  if (recordKey.Family() >= kShipLikeFamilies.m_first && recordKey.Family() < kShipLikeFamilies.m_second) {
    speed = static_cast<int>(static_cast<SdRecordGrid*>(record)->m_field6c);
  }
  if (speed == 0) {
    speed = g_gnprtbDefault;
  }
  return ComputeTravelDays(speed, viewSide, from, to, days) != 0;
}

int RulesMgr::QueryLastBuildDay(GameObjNotifier* manager, StatusCode* status, int* out)
{
  setStatus(status, kStatusCategory, kStatusOk);
  *out = 0;
  int turns = 0;
  const int result = static_cast<ManuMgr*>(manager)->EstimateLastQueuedBuildTime(status, &turns);
  if (succeeded(*status)) {
    *out = CallGameHook2B08() + turns;
  }
  return result != 0;
}

int RulesMgr::EstimateBuildDays(GameObjNotifier* manager, const NullableKey* item, int count, StatusCode* status,
                                int* days)
{
  setStatus(status, kStatusCategory, kStatusOk);
  *days = 0;
  return static_cast<ManuMgr*>(manager)->EstimateBuildTimeFresh(item, count, status, days) != 0;
}

int RulesMgr::EstimateDeploymentDays(GameObjNotifier* manager, const ObjectKey* item, StatusCode* status, int* days)
{
  setStatus(status, kStatusCategory, kStatusOk);
  *days = 0;
  SeedKey lookupKey;
  lookupKey.m_key = item->m_value;
  DatRecord* record = FindDataRecord(&lookupKey);
  int ok = record != nullptr;
  if (record != nullptr) {
    ObjectKey deploymentKey;
    static_cast<ManuMgr*>(manager)->GetDeploymentKey(&deploymentKey);
    ObjectKey managerKey;
    manager->GetKey(&managerKey);
    const int days_ok = ComputeRecordTravelDays(record, manager->ViewSide(), &managerKey, &deploymentKey, days);
    ok = both(days_ok, ok);
  }
  return ok;
}

int RulesMgr::QueryArrivalDay(GameObj* unit, StatusCode* status, int* out)
{
  setStatus(status, kStatusCategory, kStatusOk);
  int ok = 1;
  *out = 0;
  if ((unit->m_stateFlags & kEnroute) != 0) {
    *out = unit->m_eta;
    int days = 0;
    ObjectKey owner;
    ObjectKey departure;
    ok = unit->GetOwnerKey(&owner) != 0;
    new (&departure) ObjectKey(unit->m_departureKey);
    if (owner.OrNull() != nullptr && departure.OrNull() != nullptr && owner.m_value != departure.m_value) {
      ok = both(ComputeUnitTravelDays(unit, &departure, &owner, &days), ok);
      *out += days;
    }
  }
  return ok;
}

// ---- movement ---------------------------------------------------------------------------------------------------

int RulesMgr::CheckMoveEnabled(const ObjectKey* key, StatusCode* status)
{
  setStatus(status, kStatusCategory, kStatusOk);
  GameObj* object = asObject(GetMasterInFamilies_01_ff(key));
  uint32_t ok = object != nullptr;
  if (ok != 0) {
    if (object->IsConstructed() != 0 && succeeded(*status)) {
      if (inFamilies(object, kFacilityFamilies)) {
        ObjectKey ownerKey;
        object->GetOwnerKey(&ownerKey);
        GameObjNotifier* system = nullptr;
        if (keyInFamilies(ownerKey, kSystemFamilies)) {
          system = GetMasterInFamilies_90_98(&ownerKey);
        }
        if (system != nullptr && (static_cast<System*>(system)->m_flags & system_flag::kBlockade) != 0) {
          int shipGroups = 0;
          int squadrons = 0;
          FleetIterator fleets(system, 1);
          for (fleets.Begin(); fleets.Current() != nullptr; fleets.Advance()) {
            CapShipIterator ships(fleets.Current(), 1);
            for (ships.Begin(); ships.Current() != nullptr; ships.Advance()) {
              ++shipGroups;
              FightSquadIterator ownSquadrons(ships.Current(), 1);
              squadrons += ownSquadrons.Count();
            }
          }
          FightSquadIterator systemSquadrons(system, 1);
          squadrons += systemSquadrons.Count();
          if (ComputeResidualPercent(shipGroups, squadrons) <= kMaxResidualPercentForIncident) {
            setStatus(status, 0x90, 4);
          }
        }
      }
    }
    if (ok != 0) {
      return ok;
    }
  }
  setStatus(status, kStatusCategory, kStatusRefused);
  return 0;
}

int RulesMgr::CheckMoveValid(const ObjectKey* key, const ObjectKey* destination, StatusCode* status)
{
  setStatus(status, kStatusCategory, kStatusOk);
  GameObj* object = asObject(GetMasterInFamilies_01_ff(key));
  uint32_t result = 0;
  if (object == nullptr) {
    setStatus(status, kStatusCategory, kStatusRefused);
    return result;
  }
  uint32_t ok = object != nullptr;
  if (object->IsConstructed() != 0) {
    uint32_t same = 0;
    result = AreKeysInSameOwner(key, destination, &same);
    if (result == 0) {
      setStatus(status, kStatusCategory, kStatusRefused);
      return 0;
    }
    if (same == 0 && object->GetTransitSpeed(1) == 0) {
      setStatus(status, kStatusCategory, kStatusNotAllowed);
    }
    ok = result;
    if (same == 0 && succeeded(*status)) {
      if (inFamilies(object, kTroopFamilies)) {
        GameObjNotifier* target = GetMasterInFamilies_01_ff(destination);
        ok = target != nullptr;
        if (ok == 0) {
          setStatus(status, kStatusCategory, kStatusRefused);
          return 0;
        }
        if (((target->m_flags ^ object->m_flags) & kSideMask) != 0) {
          setStatus(status, kStatusCategory, kStatusDifferentControl);
        }
      }
    }
  }
  result = 0;
  if (ok != 0) {
    return ok;
  }
  setStatus(status, kStatusCategory, kStatusRefused);
  return result;
}

int RulesMgr::BeginTravel(GameObj* unit, const ObjectKey* from, const ObjectKey* to, NotifyContext* ctx)
{
  int result = 1;
  if (unit->IsConstructed() == 0 || (unit->m_stateFlags & kHyperdriveActive) != 0) {
    return result;
  }
  GameObjNotifier* fromObject = GetMasterInFamilies_01_ff(from);
  GameObjNotifier* toObject = nullptr;
  bool bothExist;
  if (fromObject == nullptr) {
    bothExist = false;
  } else {
    toObject = GetMasterInFamilies_01_ff(to);
    bothExist = toObject != nullptr;
  }
  result = 0;
  GameObjNotifier* fromOwner = nullptr;
  GameObjNotifier* toOwner = nullptr;
  if (bothExist && GetOwnerObject(fromObject, &fromOwner) != 0) {
    result = GetOwnerObject(toObject, &toOwner);
    if (result != 0 && fromOwner != nullptr && toOwner != nullptr) {
      ObjectKey toKey;
      ObjectKey fromKey;
      toOwner->GetKey(&toKey);
      fromOwner->GetKey(&fromKey);
      if (fromKey.m_value != toKey.m_value) {
        int days = 0;
        result = ComputeUnitTravelDays(unit, from, to, &days);
        if (result != 0) {
          result = 0;
          if (unit->SetEnrouteActive(1, ctx) != 0 && unit->NotifyDataEvent387(days, ctx) != 0) {
            ObjectKey destination;
            toOwner->GetKey(&destination);
            if (unit->SetDepartureKey(&destination, ctx) != 0 && unit->SetHyperdriveActive(1, ctx) != 0) {
              const int day = CallGameHook2B08();
              result = unit->SetEta(day + days, ctx);
            }
          }
        }
      }
    }
  }
  return result;
}

int RulesMgr::MoveUnitTo(const ObjectKey* key, ObjectKey* location, NotifyContext* ctx)
{
  GameObj* unit = asObject(GetMasterInFamilies_01_ff(key));
  if (unit == nullptr || unit->IsConstructed() == 0) {
    return unit->Reparent(location, ctx);
  }
  uint32_t same = 0;
  int result = AreKeysInSameOwner(key, location, &same);
  if (result != 0 && (unit->m_stateFlags & kHyperdriveActive) == 0) {
    result = unit->SetEnrouteActive(same == 0, ctx);
  }
  if (result == 0 || (unit->m_stateFlags & kDestroyed) != 0) {
    return result;
  }
  return unit->Reparent(location, ctx);
}

int RulesMgr::ResumeTrip(GameObj* unit, uint32_t* notHyperdrive, NotifyContext* ctx)
{
  *notHyperdrive = 1;
  ObjectKey owner;
  ObjectKey departure;
  int ok = unit->GetOwnerKey(&owner) != 0;
  new (&departure) ObjectKey(unit->m_departureKey);
  if (departure.m_value != owner.m_value) {
    ok = both(BeginTravel(unit, &departure, &owner, ctx), ok);
  }
  *notHyperdrive = (~unit->m_stateFlags >> 11) & 1;
  return ok;
}

int RulesMgr::EnforceLocation(GameObj* unit, NotifyContext* ctx)
{
  GameObjNotifier* parent = nullptr;
  int result = unit->GetParent(&parent);
  if (result != 0 && (unit->m_stateFlags & kDestroyed) == 0 && (asObject(parent)->m_stateFlags & kDestroyed) != 0) {
    result = unit->HandleDestroyedParent(0, ctx);
  }
  if (result != 0 && (unit->m_stateFlags & kAutorouting) == 0 && (unit->m_stateFlags & kDestroyed) == 0) {
    StatusCode sameControl;
    result = asObject(parent)->CheckSameControl(unit, &sameControl);
    if (result != 0 && !succeeded(sameControl)) {
      result = unit->HandleControlMismatch(ctx);
    }
  }
  if (result != 0 && (unit->m_stateFlags & kAutorouting) == 0 && (unit->m_stateFlags & kDestroyed) == 0) {
    RouteContext route;
    StatusCode fit(1, 0xffffffff);
    const int filter = 1;
    result = route.Init(unit->ControlSide(), unit->ControlSide(), 0x3f0, &filter);
    if (result != 0) {
      ObjectKey parentKey;
      parent->GetKey(&parentKey);
      result = route.AddUnitLegs(&parentKey);
    }
    CapacityScratch slot;
    if (result != 0) {
      if (succeeded(fit)) {
        ObjectKey parentKey;
        parent->GetKey(&parentKey);
        ObjectKey unitKey;
        unit->GetKey(&unitKey);
        result = route.CheckMoveInverted(&unitKey, &parentKey, &fit, &slot);
      }
    }
    if (result != 0 && !succeeded(fit) && slot.m_limitStatus.m_second != 0xffffffff) {
      result = unit->HandleLocationConstraintFailure(ctx);
    }
  }
  if (result != 0 && (unit->m_stateFlags & kAutorouting) == 0 && (unit->m_stateFlags & kDestroyed) == 0) {
    int ownerDiffers = 0;
    if ((unit->m_stateFlags & kCompleted) == 0) {
      ObjectKey ownerKey;
      unit->GetOwnerKey(&ownerKey);
      ObjectKey builderOwnerKey;
      if (unit->m_builderKey.IsSet()) {
        GameObjNotifier* builder = GetMasterInFamilies_01_ff(&unit->m_builderKey);
        if (builder != nullptr) {
          builder->GetOwnerKey(&builderOwnerKey);
        }
      }
      ownerDiffers = ownerKey.m_value != builderOwnerKey.m_value;
    }
    if ((unit->m_stateFlags & kEnroute) != 0 || ownerDiffers != 0) {
      GameObjNotifier* owner = nullptr;
      result = GetOwnerObject(unit, &owner);
      GameObjNotifier* ownerSystem = nullptr;
      if (result != 0) {
        if (inFamilies(owner, kSystemFamilies)) {
          ownerSystem = owner;
        }
        if (ownerSystem != nullptr && (static_cast<System*>(ownerSystem)->m_flags & system_flag::kBlockade) != 0) {
          uint32_t blockadingSide = 0;
          result = static_cast<System*>(ownerSystem)->GetBlockadingSide(&blockadingSide);
          if (result != 0 && !inFamilies(unit, kFleetFamilies) && !inFamilies(unit, kCapitalShipFamilies)
              && blockadingSide != unit->ControlSide()) {
            result = unit->HandleLocationConstraintFailure(ctx);
          }
        }
      }
    }
  }
  return result;
}

int RulesMgr::FireDisasterTimer(NotifyContext* ctx)
{
  int existing = 0;
  for (GameObjNotifier* system = GetFirstMasterInFamilies_90_98(); system != nullptr;
       system = NextSystemMaster(system)) {
    if ((asObject(system)->m_stateFlags & kExisting) != 0) {
      ++existing;
    }
  }
  if (existing == 0) {
    return 1;
  }
  const int pick = GameRandom(existing - 1);
  int index = 0;
  GameObjNotifier* chosen = nullptr;
  GameObjNotifier* system = GetFirstMasterInFamilies_90_98();
  while (system != nullptr) {
    if (chosen != nullptr) {
      break;
    }
    GameObjNotifier* current = system;
    if ((asObject(current)->m_stateFlags & kExisting) == 0) {
      system = NextSystemMaster(current);
    } else if (pick != index) {
      ++index;
      system = NextSystemMaster(current);
    } else {
      system = NextSystemMaster(current);
      chosen = current;
    }
  }
  if (chosen == nullptr) {
    return 1;
  }
  return static_cast<System*>(chosen)->PulseDisasterIncident(ctx);
}

int RulesMgr::FireResourceTimer(NotifyContext* ctx)
{
  int existing = 0;
  for (GameObjNotifier* system = GetFirstMasterInFamilies_90_98(); system != nullptr;
       system = NextSystemMaster(system)) {
    if ((asObject(system)->m_stateFlags & kExisting) != 0) {
      ++existing;
    }
  }
  if (existing == 0) {
    return 1;
  }
  const int pick = GameRandom(existing - 1);
  int index = 0;
  GameObjNotifier* chosen = nullptr;
  GameObjNotifier* system = GetFirstMasterInFamilies_90_98();
  while (system != nullptr) {
    if (chosen != nullptr) {
      break;
    }
    GameObjNotifier* current = system;
    if ((asObject(current)->m_stateFlags & kExisting) == 0) {
      system = NextSystemMaster(current);
    } else if (pick != index) {
      ++index;
      system = NextSystemMaster(current);
    } else {
      system = NextSystemMaster(current);
      chosen = current;
    }
  }
  if (chosen == nullptr) {
    return 1;
  }
  return static_cast<System*>(chosen)->RollResourceIncident(ctx);
}

int RulesMgr::SyncUnitView(const ObjectKey* key, int side, NotifyContext* ctx)
{
  GameObj* unit = asObject(GetMasterInFamilies_01_ff(key));
  ObjectKey ownerKey;
  int result = 0;
  if (unit != nullptr) {
    result = unit->GetOwnerKey(&ownerKey);
  }
  bool skip = result == 0;
  GameObjNotifier* parent = nullptr;
  result = 0;
  if (!skip) {
    result = unit->GetParent(&parent);
    skip = result == 0;
  }
  if (skip) {
    return result;
  }
  const bool ownerIsSystem = keyInFamilies(ownerKey, kSystemFamilies);
  const bool parentIsSystem = ownerIsSystem && inFamilies(parent, kSystemFamilies);
  if (ownerIsSystem && !parentIsSystem) {
    GameObjNotifier* system = GetMasterInFamilies_90_98(&ownerKey);
    result = 0;
    if (system != nullptr) {
      result = static_cast<System*>(system)->SyncViewForEvent(5, side, ctx);
    }
  }
  if (result != 0) {
    NotifyContext viewCtx(*ctx);
    viewCtx.m_flag04 = 1;
    viewCtx.m_flag08 = 3;
    if (side == 1 || side == 2) {
      result = unit->ApplyAllToView(side, &viewCtx);
    }
  }
  return result;
}
EventKindSet g_rulesMgrEventKinds{};
int g_rulesMgrEventKindsBuilt{};
