#include "events/event_post.h"
#include "gameobj/game_obj.h"
#include "gameobj/game_obj_iterator.h"
#include "gameobj/object_database.h"

namespace
{

constexpr uint32_t kEnroute = 0x10;
constexpr uint32_t kEnrouteActive = 0x20;
constexpr uint32_t kDestroyed = 0x8;
constexpr uint32_t kMaxCount = 0xff;
constexpr uint32_t kReasonSabotage = 6;
constexpr uint32_t kReasonAssassination = 7;
constexpr uint32_t kReasonAutoscrap = 0x15;
constexpr int kExistingFilter = 3;
constexpr int kAlwaysFilter = 4;

template <typename Visit>
int forEachChild(GameObj* self, uint32_t filter, Visit visit)
{
  int ok = 1;
  GameObjIterator children(self, filter);
  children.Begin();
  while (children.Current())
  {
    const int visited = visit(static_cast<GameObj*>(children.Current()));
    ok = visited && ok ? 1 : 0;
    children.Advance();
  }
  return ok;
}

} // namespace

// ---- OnXChanged: events on the master -------------------------------------------------------------------

int GameObj::OnControlKindChanged(uint32_t oldKind, uint32_t newKind, NotifyContext* ctx)
{
  int ok = UpdateViewVisibility(ctx) != 0;
  ok = forEachChild(this, kExistingFilter,
                    [&](GameObj* child)
                    {
                      return child->Virtual44(ctx);
                    }) &&
       ok;
  ok = PostSourceEvent(0x302, this, oldKind, newKind, ctx) && ok;
  return PostSourceEvent(0x318, this, oldKind, newKind, ctx) && ok ? 1 : 0;
}

int GameObj::OnParentChanged(GameObjNotifier* oldParent, GameObjNotifier* newParent, NotifyContext* ctx)
{
  if (!oldParent || !newParent)
    return 0;
  bool ok = SetAutorouting(0, ctx) != 0;
  if (m_stateFlags & kEnrouteActive)
  {
    const uint32_t count = (m_packed >> 16) & 0xff;
    if (count < kMaxCount)
      ok = SetDestinationCount(count + 1, ctx) && ok;
  }
  ok = UpdateObservedByAlliance(ctx) && ok;
  ok = UpdateObservedByEmpire(ctx) && ok;
  ok = UpdateEnroute(ctx) && ok;
  ok = UpdateEta(ctx) && ok;
  int result = UpdateDepartureKey(ctx) && ok ? 1 : 0;
  if (m_stateFlags & kDestroyed)
  {
    const uint32_t family = newParent->GetFamily();
    if (family > 0xf1 && family < 0xf3)
    {
      ViewAs(1)->ReleaseDestroyedSubtree(ctx);
      result = ViewAs(2)->ReleaseDestroyedSubtree(ctx);
    }
  }
  ObjectKey newKey;
  ObjectKey oldKey;
  newParent->GetKey(&newKey);
  oldParent->GetKey(&oldKey);
  const int hub = PostTwoKeyEvent(0x320, this, &oldKey, &newKey, ctx);
  return hub && result ? 1 : 0;
}

int GameObj::OnDepartureKeyChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx)
{
  return forEachChild(this, kAlwaysFilter,
                      [&](GameObj* child)
                      {
                        return child->UpdateDepartureKey(ctx);
                      });
}

int GameObj::OnDestroyedReasonChanged(uint32_t oldValue, uint32_t newValue, NotifyContext* ctx)
{
  return SetDestroyed(newValue != 0, ctx);
}

int GameObj::OnDeploymentCountChanged(uint32_t oldValue, uint32_t newValue, NotifyContext* ctx)
{
  int result = 1;
  if ((oldValue != 0) != (newValue != 0))
    result = SetDeployed(1, ctx) != 0;
  return result;
}

int GameObj::OnEtaChanged(int oldValue, int newValue, NotifyContext* ctx)
{
  return forEachChild(this, kAlwaysFilter,
                      [&](GameObj* child)
                      {
                        return child->UpdateEta(ctx);
                      });
}

int GameObj::OnUsableChanged(int newValue, NotifyContext* ctx)
{
  bool ok = UpdateViewVisibility(ctx) != 0;
  if (IsNotGameRoot())
  {
    ObjectKey parentKey;
    ObjectKey dataKey;
    ok = GetValidParentKey(&parentKey) && ok;
    ok = GetDataKey(&dataKey) && ok;
    if (ok && parentKey.IsSet() && parentKey.m_value != dataKey.m_value)
    {
      const uint32_t count = (m_packed >> 8) & 0xff;
      bool counted = true;
      if (count < kMaxCount)
        counted = SetDeploymentCount(count + 1, ctx) != 0;
      ok = SetDataKey(&parentKey) && counted;
    }
  }
  ok = AutoscrapIfRequested(ctx) && ok;
  return PostSourceEventFlag(0x301, this, newValue, ctx) && ok ? 1 : 0;
}

int GameObj::OnCreatedChanged(int newValue, NotifyContext* ctx)
{
  return UpdateExisting(ctx) != 0;
}

int GameObj::OnCompletedChanged(int newValue, NotifyContext* ctx)
{
  const int constructed = SetConstructed(1, ctx);
  const bool ok = UpdateUsable(ctx) && constructed;
  return PostSourceEventFlag(0x308, this, newValue, ctx) && ok ? 1 : 0;
}

int GameObj::OnDestroyedChanged(int newValue, NotifyContext* ctx)
{
  bool ok = UpdateExisting(ctx) != 0;
  ok = forEachChild(this, kExistingFilter,
                    [&](GameObj* child)
                    {
                      return child->Virtual45(1, ctx);
                    }) &&
       ok;
  if (newValue != 0)
  {
    ok = SetEnrouteActive(0, ctx) && ok;
    ok = SetHyperdriveActive(0, ctx) && ok;
    ok = SetAutorouting(0, ctx) && ok;
  }
  ok = UpdateEnroute(ctx) && ok;
  ok = UpdateEta(ctx) && ok;
  ok = UpdateDepartureKey(ctx) && ok;
  return RequestDeleteIfReady(ctx) && ok ? 1 : 0;
}

int GameObj::OnEnrouteChanged(int newValue, NotifyContext* ctx)
{
  bool ok = UpdateUsable(ctx) != 0;
  return forEachChild(this, kAlwaysFilter,
                      [&](GameObj* child)
                      {
                        return child->UpdateEnroute(ctx);
                      }) &&
         ok;
}

int GameObj::OnEnrouteActiveChanged(int newValue, NotifyContext* ctx)
{
  const int destinations = SetDestinationCount(0, ctx);
  bool ok = UpdateObservedByAlliance(ctx) && destinations;
  ok = UpdateObservedByEmpire(ctx) && ok;
  ok = UpdateEnroute(ctx) && ok;
  ok = UpdateEta(ctx) && ok;
  return UpdateDepartureKey(ctx) && ok ? 1 : 0;
}

int GameObj::OnExistingChanged(int newValue, NotifyContext* ctx)
{
  const int usable = UpdateUsable(ctx);
  return PostSourceEventFlag(0x300, this, newValue, ctx) && usable ? 1 : 0;
}

int GameObj::OnObservedByAllianceChanged(int newValue, NotifyContext* ctx)
{
  bool ok = UpdateViewVisibility(ctx) != 0;
  return forEachChild(this, kAlwaysFilter,
                      [&](GameObj* child)
                      {
                        return child->UpdateObservedByAlliance(ctx);
                      }) &&
         ok;
}

int GameObj::OnObservedByEmpireChanged(int newValue, NotifyContext* ctx)
{
  bool ok = UpdateViewVisibility(ctx) != 0;
  return forEachChild(this, kAlwaysFilter,
                      [&](GameObj* child)
                      {
                        return child->UpdateObservedByEmpire(ctx);
                      }) &&
         ok;
}

int GameObj::OnDamagedChanged(int newValue, NotifyContext* ctx)
{
  return PostSourceEventFlag(0x31d, this, newValue, ctx) != 0;
}

int GameObj::OnHyperdriveActiveChanged(int newValue, NotifyContext* ctx)
{
  const bool first = ArmTimerAfterSetFlag(0x387, newValue, this, &m_data->m_stat, ctx) != 0;
  return PostSourceEventFlag(0x306, this, newValue, ctx) && first ? 1 : 0;
}

int GameObj::OnAutoroutingChanged(int newValue, NotifyContext* ctx)
{
  return PostSourceEventFlag(0x307, this, newValue, ctx) != 0;
}

int GameObj::OnAutoscrapRequestChanged(int newValue, NotifyContext* ctx)
{
  return AutoscrapIfRequested(ctx) != 0;
}

int GameObj::OnLockedChanged(int newValue, NotifyContext* ctx)
{
  return PostSourceEventFlag(0x316, this, newValue, ctx) != 0;
}

int GameObj::OnReadyForDeleteChanged(int newValue, NotifyContext* ctx)
{
  return PostSourceEventFlag(0x31a, this, newValue, ctx) != 0;
}

int GameObj::OnConstructedChanged(int newValue, NotifyContext* ctx)
{
  const int enroute = UpdateEnroute(ctx);
  const int eta = UpdateEta(ctx);
  const bool ok = eta && enroute;
  return UpdateDepartureKey(ctx) && ok ? 1 : 0;
}

// ---- OnXNotified: events on a side view ---------------------------------------------------------------------

int GameObj::OnNameNotified(std::string* oldName, std::string* newName, NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    LogString("GameObjNameNotif", "Name", newName, oldName, 0, ctx);
    result = PostPayloadStrings(0x500, this, oldName, newName, ctx);
  }
  return result;
}

int GameObj::OnControlKindNotified(uint32_t oldKind, uint32_t newKind, NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    LogSide("GameObjControlKindNotif", "ControlKind", newKind, oldKind, 0, ctx);
    result = PostPayloadInts(0x100, this, oldKind, newKind, ctx);
  }
  return result;
}

int GameObj::OnParentNotified(GameObjNotifier* oldParent, GameObjNotifier* newParent, NotifyContext* ctx)
{
  int result = 0;
  if (IsSideView())
  {
    ObjectKey oldKey;
    ObjectKey newKey;
    oldParent->GetKey(&oldKey);
    newParent->GetKey(&newKey);
    LogKey("GameObjParentNotif", "Parent", &newKey, &oldKey, 0, ctx);
    ObjectKey hubNewKey;
    ObjectKey hubOldKey;
    newParent->GetKey(&hubNewKey);
    oldParent->GetKey(&hubOldKey);
    result = PostPayloadKeysA(0x300, this, &hubOldKey, &hubNewKey, ctx);
  }
  if (result && (m_stateFlags & kEnrouteActive) && (m_packed & 0xff0000))
  {
    ObjectKey oldKey;
    ObjectKey newKey;
    oldParent->GetKey(&oldKey);
    newParent->GetKey(&newKey);
    LogKey("GameObjParentReroutedNotif", "ParentRerouted", &newKey, &oldKey, 0, ctx);
    ObjectKey hubNewKey;
    ObjectKey hubOldKey;
    newParent->GetKey(&hubNewKey);
    oldParent->GetKey(&hubOldKey);
    result = PostPayloadKeysA(0x301, this, &hubOldKey, &hubNewKey, ctx);
  }
  return result;
}

int GameObj::OnBuilderKeyNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result)
    LogKey("(GameObjBuilderKeyNotif)", "Builder", newKey, oldKey, 0, ctx);
  return result;
}

int GameObj::OnDepartureKeyNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result)
  {
    LogKey("(GameObjDestinationLocationAtDepartureKeyNotif)", "DestinationLocationAtDeparture", newKey, oldKey, 0, ctx);
  }
  return result;
}

int GameObj::OnDestroyedReasonNotified(uint32_t oldValue, uint32_t newValue, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result)
    LogChange("(GameObjDestroyedReasonNotif)", "DestroyedReason", "", "", nullptr, ctx);
  return result;
}

int GameObj::OnDeploymentCountNotified(uint32_t oldValue, uint32_t newValue, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result)
    LogInt("(GameObjDeploymentCountNotif)", "DeploymentCount", newValue, oldValue, 0, ctx);
  return result;
}

int GameObj::OnDestinationCountNotified(uint32_t oldValue, uint32_t newValue, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result)
    LogInt("(GameObjDestinationCountNotif)", "DestinationCount", newValue, oldValue, 0, ctx);
  return result;
}

int GameObj::OnEtaNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result)
    LogInt("(GameObjETANotif)", "ETA", newValue, oldValue, 0, ctx);
  return result;
}

int GameObj::OnUsableNotified(int value, NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    LogFlag("GameObjUsableNotif", "Usable", value, 0, ctx);
    result = PostPayloadFlag(0x101, this, value, ctx);
  }
  return result;
}

int GameObj::OnCreatedNotified(int value, NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    LogFlag("GameObjCreatedNotif", "Created", value, 0, ctx);
    result = PostPayloadFlag(0x102, this, value, ctx);
  }
  return result;
}

int GameObj::OnCompletedNotified(int value, NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    LogFlag("GameObjCompletedNotif", "Completed", value, 0, ctx);
    result = PostPayloadFlag(0x103, this, value, ctx);
  }
  return result;
}

int GameObj::OnDestroyedNotified(int value, NotifyContext* ctx)
{
  int result = IsSideView();
  ObjectKey parentKey;
  if (result)
    result = GetValidParentKey(&parentKey);
  if (result)
  {
    LogFlag("GameObjDestroyedNotif", "Destroyed", value, 0, ctx);
    ObjectKey unset;
    result = PostPayloadKeysA(0x302, this, &parentKey, &unset, ctx);
  }
  if (result && (m_stateFlags & kEnrouteActive) && (m_packed & 0xff0000))
  {
    LogFlag("GameObjDestroyedOnArrivalNotif", "DestroyedOnArrival", value, 0, ctx);
    ObjectKey unset;
    result = PostPayloadKeysA(0x303, this, &parentKey, &unset, ctx);
  }
  if (result)
  {
    int eventId = 0;
    const char* tag = nullptr;
    const char* property = nullptr;
    switch (m_packed & 0xff)
    {
      case kReasonSabotage:
        eventId = 0x305;
        tag = "GameObjDestroyedSabotageNotif";
        property = "DestroyedSabotage";
        break;
      case kReasonAssassination:
        eventId = 0x306;
        tag = "GameObjDestroyedAssassinationNotif";
        property = "DestroyedAssassination";
        break;
      case kReasonAutoscrap:
        eventId = 0x304;
        tag = "GameObjDestroyedAutoscrapNotif";
        property = "DestroyedAutoscrap";
        break;
      default:
        break;
    }
    if (eventId != 0)
    {
      LogFlag(tag, property, value, 0, ctx);
      ObjectKey unset;
      result = PostPayloadKeysA(eventId, this, &parentKey, &unset, ctx);
    }
  }
  return result;
}

int GameObj::OnEnrouteNotified(int value, NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    LogFlag("GameObjEnrouteNotif", "Enroute", value, 0, ctx);
    result = PostPayloadFlag(0x104, this, value, ctx);
  }
  return result;
}

int GameObj::OnEnrouteActiveNotified(int value, NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    LogFlag("GameObjEnrouteActiveNotif", "EnrouteActive", value, 0, ctx);
    result = PostPayloadFlag(0x105, this, value, ctx);
  }
  return result;
}

int GameObj::OnExistingNotified(int value, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result)
    LogFlag("(GameObjExistingNotif)", "Existing", value, 0, ctx);
  return result;
}

int GameObj::OnObservedByAllianceNotified(int value, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result)
    LogFlag("(GameObjObservedByAlliance)", "Observed", value, 0, ctx);
  return result;
}

int GameObj::OnObservedByEmpireNotified(int value, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result)
    LogFlag("(GameObjObservedByEmpire)", "Observed", value, 0, ctx);
  return result;
}

int GameObj::OnDamagedNotified(int value, NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    LogFlag("GameObjDamagedNotif", "Damaged", value, 0, ctx);
    result = PostPayloadFlag(0x106, this, value, ctx);
  }
  return result;
}

int GameObj::OnHyperdriveActiveNotified(int value, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result)
    LogFlag("(GameObjHyperdriveActiveNotif)", "HyperdriveActive", value, 0, ctx);
  return result;
}

int GameObj::OnAutoroutingNotified(int value, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result)
    LogFlag("(GameObjAutoroutingNotif)", "Autorouting", value, 0, ctx);
  return result;
}

int GameObj::OnAutoscrapRequestNotified(int value, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result)
    LogFlag("(GameObjAutoscrapRequestNotif)", "AutoscrapRequest", value, 0, ctx);
  return result;
}

int GameObj::OnLockedNotified(int value, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result)
    LogFlag("(GameObjLockedNotif)", "Locked", value, 0, ctx);
  return result;
}

int GameObj::OnReadyForDeleteNotified(int value, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result)
    LogFlag("(GameObjReadyForDeleteNotif)", "ReadyForDelete", value, 0, ctx);
  return result;
}

int GameObj::OnConstructedNotified(int value, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result)
    LogFlag("(GameObjConstructedNotif)", "Constructed", value, 0, ctx);
  return result;
}

int GameObj::OnDeployedNotified(int value, NotifyContext* ctx)
{
  int result = 0;
  if (IsSideView())
  {
    LogFlag("GameObjDeployedNotif", "Deployed", value, 0, ctx);
    result = PostPayloadFlag(0x107, this, value, ctx);
  }
  return result;
}
