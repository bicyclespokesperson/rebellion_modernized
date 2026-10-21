#include "strategy_ui/advisor_sides.h"
#include "economy/manu_mgr.h"
#include "orders/mission_orders.h"

#include <new>

#include "fleet/capship.h"
#include "gameobj/object_lookup.h"
#include "orders/order_base.h"

using namespace advisor_sides;

namespace
{

constexpr uint32_t kFleetFamilies[2] = {8, 0x10};
constexpr uint32_t kSystemFamilies[2] = {0x90, 0x98};
constexpr uint32_t kManagerFamilies[2] = {0xa0, 0xb0};
constexpr uint32_t kShipFamilies[2] = {0x14, 0x1c};
constexpr int kOrderKindMission = 0x240;
constexpr uint8_t kObjectFlagSelected = 8;

bool TargetIn(OrderBase* order, uint32_t first, uint32_t last)
{
  ObjectKey key;
  return KeyIn(*order->GetTarget(&key), first, last);
}

} // namespace

void advisor_sides::RefusedGeneric(AdvisorBase* advisor, OrderBase* order, IntPair* status,
                                   const RefusalOffsets& offsets, ScheduleFallbackFn fallback)
{
  if (Is(status, 1, 0x16) || Is(status, 1, 0x12)) {
    return;
  }
  if (Is(status, 1, 0x26)) {
    return;
  }
  ObjectKey target;
  if (Is(status, 0x40, 0x92)) {
    if (order->GetTarget(&target)->IsSet() != 0) {
      Raise(advisor, 0xc, kShortCue);
      return;
    }
    fallback(advisor);
    return;
  }
  if (Is(status, 0x40, 0x91)) {
    bool missionOrder = false;
    if (order->GetTarget(&target)->IsSet() != 0 && order->GetKind() == kOrderKindMission) {
      ObjectKeyValueSet* secondTeam = &static_cast<MissionOrder*>(order)->m_characters;
      missionOrder = order->GetTeam()->First() == nullptr && secondTeam->First() != nullptr;
    }
    if (missionOrder) {
      Raise(advisor, offsets.missionOrder, kShortCue);
      return;
    }
    fallback(advisor);
    return;
  }
  if (Is(status, 1, 0x18)) {
    Raise(advisor, 0x14, kShortCue);
    return;
  }
  if (Is(status, 1, 0x22)) {
    bool targetsSystem = order->GetTarget(&target)->IsSet() != 0 && TargetIn(order, 0x90, 0x98);
    if (!targetsSystem) {
      return;
    }
    GameObjNotifier* system = GetObjectInFamilies_90_98(advisor->m_side, order->GetTarget(&target));
    if (system && (static_cast<GameObj*>(system)->m_stateFlags & kObjectFlagSelected) != 0) {
      Raise(advisor, 0x18, kShortCue);
      return;
    }
    fallback(advisor);
    return;
  }
  if (Is(status, 1, 0x27) || Is(status, 1, 0x25)) {
    bool targetsFleet = order->GetTarget(&target)->IsSet() != 0 && TargetIn(order, 8, 0x10);
    Raise(advisor, targetsFleet ? 0x1c : offsets.otherTarget, kShortCue);
    return;
  }
  if (Is(status, 1, 0x20)) {
    Raise(advisor, offsets.fleetWork, kShortCue);
    return;
  }
  if (Is(status, 1, 0x11) || Is(status, 1, 0x21)) {
    Raise(advisor, 4, kShortCue);
    return;
  }
  if (Is(status, 1, 0x17)) {
    fallback(advisor);
    return;
  }
  if (Is(status, 1, 0x24) || Is(status, 1, 0x28)) {
    GameObjNotifier* manager = nullptr;
    ObjectKeyNode* first = order->GetTeam()->First();
    if (first) {
      ObjectKey memberKey;
      if (KeyIn(*first->GetKey(&memberKey), 0xa0, 0xb0)) {
        manager = GetObjectInFamilies_01_ff(1, first->GetKey(&memberKey));
      }
    }
    if (TargetIn(order, 0x90, 0x98)) {
      Raise(advisor, 8, kShortCue);
      return;
    }
    if (manager) {
      ObjectKey location;
      ManagerGetLocationKey(manager, &location);
      if (KeyIn(location, 0x90, 0x98)) {
        Raise(advisor, 8, kShortCue);
        return;
      }
    }
    Raise(advisor, offsets.otherTarget, kShortCue);
    return;
  }
  fallback(advisor);
}

void advisor_sides::RefusedUnits(AdvisorBase* advisor, OrderBase* order, IntPair* status, int view,
                                 ScheduleFallbackFn fallback)
{
  ObjectKey target;
  GameObjNotifier* ship = GetObjectInFamilies_14_1c(view, order->GetTarget(&target));
  if (Is(status, 0x14, 1)) {
    if (ship) {
      if (static_cast<CapShip*>(ship)->GetFighterCapacity() != 0) {
        Raise(advisor, 0x20, kQuickCue);
      } else {
        Raise(advisor, 0x1c, kQuickCue);
      }
      return;
    }
    if (KeyIn(*order->GetTarget(&target), 8, 0x10)) {
      Raise(advisor, 0x24, kQuickCue);
      return;
    }
  } else if (Is(status, 0x14, 2)) {
    if (ship) {
      if (static_cast<CapShip*>(ship)->GetTroopCapacity() != 0) {
        Raise(advisor, 0x20, kQuickCue);
      } else {
        Raise(advisor, 0x1c, kQuickCue);
      }
      return;
    }
    if (KeyIn(*order->GetTarget(&target), 8, 0x10)) {
      Raise(advisor, 0x24, kQuickCue);
      return;
    }
  }
  fallback(advisor);
}

ObjectKey* ManagerGetLocationKey(GameObjNotifier* manager, ObjectKey* out)
{
  return new (out) ObjectKey(static_cast<ManuMgr*>(manager)->m_targetKey);
}
