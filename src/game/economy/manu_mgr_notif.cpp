#include "economy/manu_mgr.h"
#include "events/event_post.h"

// OnXNotified: events raised on a side view (the "ManuMgr*Notif" strings). The log calls are dead in the shipped game;
// the hub calls forward the change to the UI: 0x160-0x163 for the counts and the flag, 0x320/0x321 for the product and
// deployment keys, 0x400 for the seed key and 0x520 for the product name.

namespace
{

constexpr int kEventRemainingCount = 0x160;
constexpr int kEventCompletedPoints = 0x161;
constexpr int kEventRequiredPoints = 0x162;
constexpr int kEventReserved = 0x163;
constexpr int kEventProductKey = 0x320;
constexpr int kEventDeploymentKey = 0x321;
constexpr int kEventSeedKey = 0x400;
constexpr int kEventProductName = 0x520;

} // namespace

int ManuMgr::OnRemainingGameObjCountNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    LogInt("ManuMgrRemainingGameObjCountNotif", "RemainingGameObjCount", newValue, oldValue, 0, ctx);
    result = PostPayloadInts(kEventRemainingCount, this, oldValue, newValue, ctx);
  }
  return result;
}

int ManuMgr::OnCompletedPointCountNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    LogInt("ManuMgrCompletedPointCountNotif", "CompletedPointCount", newValue, oldValue, 0, ctx);
    result = PostPayloadInts(kEventCompletedPoints, this, oldValue, newValue, ctx);
  }
  return result;
}

int ManuMgr::OnOverflowPointCountNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogInt("(ManuMgrOverflowPointCountNotif)", "OverflowPointCount", newValue, oldValue, 0, ctx);
  }
  return result;
}

int ManuMgr::OnSeedKeyNotified(NullableKey* oldKey, NullableKey* newKey, NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    ObjectKey oldObjectKey;
    ObjectKey newObjectKey;
    oldObjectKey.m_value = oldKey->m_value;
    newObjectKey.m_value = newKey->m_value;
    LogTypeKey("ManuMgrSeedKeyNotif", "SeedKey", &newObjectKey, &oldObjectKey, 0, ctx);
    result = PostPayloadKeysB(kEventSeedKey, this, &oldObjectKey, &newObjectKey, ctx);
  }
  return result;
}

int ManuMgr::OnRequiredPointCountNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    LogInt("ManuMgrRequiredPointCountNotif", "RequiredPointCount", newValue, oldValue, 0, ctx);
    result = PostPayloadInts(kEventRequiredPoints, this, oldValue, newValue, ctx);
  }
  return result;
}

int ManuMgr::OnTotalRequiredPointCountNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogInt("(ManuMgrTotalRequiredPointCountNotif)", "TotalRequiredPointCount", newValue, oldValue, 0, ctx);
  }
  return result;
}

int ManuMgr::OnReservedNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    LogFlag("ManuMgrReservedNotif", "Reserved", newValue, 0, ctx);
    result = PostPayloadFlag(kEventReserved, this, newValue, ctx);
  }
  return result;
}

int ManuMgr::OnProductKeyNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    LogKey("ManuMgrProductKeyNotif", "ProductKey", newKey, oldKey, 0, ctx);
    result = PostPayloadKeysA(kEventProductKey, this, oldKey, newKey, ctx);
  }
  return result;
}

int ManuMgr::OnDeploymentKeyNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    LogKey("ManuMgrDeploymentKeyNotif", "DeploymentKey", newKey, oldKey, 0, ctx);
    result = PostPayloadKeysA(kEventDeploymentKey, this, oldKey, newKey, ctx);
  }
  return result;
}

int ManuMgr::OnTargetKeyNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogKey("ManuMgrTargetKeyNotif", "TargetKey", newKey, oldKey, 0, ctx);
  }
  return result;
}

int ManuMgr::OnProductNameNotified(std::string* oldName, std::string* newName, NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    LogString("ManuMgrProductNameNotif", "ProductName", newName, oldName, 0, ctx);
    result = PostPayloadStrings(kEventProductName, this, oldName, newName, ctx);
  }
  return result;
}
