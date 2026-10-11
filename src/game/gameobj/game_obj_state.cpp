#include "events/event_post.h"
#include "gameobj/game_obj.h"
#include "gameobj/game_obj_iterator.h"
#include "gameobj/object_database.h"

namespace
{

constexpr int kViewCount = 3;
constexpr uint32_t kCreated = 0x2;
constexpr uint32_t kCompleted = 0x4;
constexpr uint32_t kDestroyed = 0x8;
constexpr uint32_t kEnroute = 0x10;
constexpr uint32_t kEnrouteActive = 0x20;
constexpr uint32_t kHyperdriveActive = 0x800;
constexpr uint32_t kReadyForDelete = 0x8000;
constexpr uint32_t kConstructed = 0x10000;
constexpr uint32_t kReasonDestroyed = 0x14;
constexpr uint32_t kReasonCompleted3 = 3;
constexpr uint32_t kReasonCompleted4 = 4;
constexpr uint32_t kSideRootFamily = 0xf2;
constexpr int kAllocationEventId = 0x387;

bool isValidControlKind(uint32_t kind)
{
  return static_cast<int>(kind) >= 1 && static_cast<int>(kind) <= 3;
}

} // namespace

// ---- bookkeeping --------------------------------------------------------------------------------------------

int GameObj::CountChildren(const FamilyRange* range, uint32_t side, uint32_t filter)
{
  ChildIterator iterator(this, side, range, filter);
  return iterator.Count();
}

int GameObj::IsDestructionComplete()
{
  int ok = 1;
  for (int view = 0; view < kViewCount; ++view)
  {
    if (!ok)
      return 0;
    GameObj* object = ViewAs(view);
    if (!(object->m_stateFlags & kDestroyed))
      ok = 0;
    if (ok && !object->Virtual17())
      ok = 0;
    if (ok)
    {
      GameObjIterator children(object, 4);
      if (children.HasCurrent())
        ok = 0;
    }
    if (ok && IsNotGameRoot())
    {
      if (!m_parent || m_parent->GetFamily() != kSideRootFamily)
        ok = 0;
    }
  }
  return ok;
}

int GameObj::CanBeDeleted()
{
  int ok = IsDestructionComplete();
  if (ok)
  {
    for (int view = 0; view < kViewCount; ++view)
    {
      GameObj* object = ViewAs(view);
      if ((object->m_stateFlags >> 1) & 1)
      {
        if (!ViewAs(view)->GetFlag8())
          ok = 0;
      }
    }
  }
  return ok;
}

// ---- data block ---------------------------------------------------------------------------------------------

int GameObj::GetDataField04(int* out)
{
  *out = m_data->m_field04;
  return 1;
}

int GameObj::GetLockCount(int* out)
{
  *out = m_data->m_lockCount;
  return 1;
}

int GameObj::GetDataKey(ObjectKey* out)
{
  *out = m_data->m_key;
  return 1;
}

void* GameObj::GetDataExtra()
{
  if (m_data)
    return &m_data->m_stat;
  return nullptr;
}

int GameObj::SetDataField04(int value)
{
  m_data->m_field04 = value;
  return 1;
}

int GameObj::SetLockCount(int value)
{
  m_data->m_lockCount = value;
  return 1;
}

int GameObj::SetDataKey(const ObjectKey* key)
{
  m_data->m_key = *key;
  return 1;
}

int GameObj::IncrementLock(NotifyContext* ctx)
{
  int count = 0;
  GetLockCount(&count);
  ++count;
  SetLockCount(count);
  return SetLocked(count != 0, ctx);
}

int GameObj::DecrementLock(NotifyContext* ctx)
{
  int count = 0;
  GetLockCount(&count);
  if (count != 0)
    --count;
  SetLockCount(count);
  SetLocked(count != 0, ctx);
  return 1;
}

int GameObj::NotifyDataEvent387(int level, NotifyContext* ctx)
{
  return ArmTimerAfterSetLevel(kAllocationEventId, level, this, &m_data->m_stat, ctx);
}

// ---- creating and updating children -------------------------------------------------------------------------

GameObj* GameObj::CreateChild(const ObjectKey* key, int initArg, NotifyContext* ctx, int created, int completed)
{
  GameObj* child = static_cast<GameObj*>(CreateObjectForKey(key));
  if (child)
  {
    child->CreateViews(initArg, ctx);
    ObjectKey parentKey;
    GetKey(&parentKey);
    child->Reparent(&parentKey, ctx);
    child->SetCreated(created, ctx);
    child->SetCompleted(completed, ctx);
  }
  return child;
}

int GameObj::CreateChildren(int count,
                            const ObjectKey* key,
                            int initArg,
                            NotifyContext* ctx,
                            int created,
                            int completed)
{
  int ok = 1;
  for (int i = count; i > 0; --i)
  {
    ok = CreateChild(key, initArg, ctx, created, completed) != nullptr && ok;
  }
  return ok;
}

int GameObj::UpdateChildren(const FamilyRange* range, NotifyContext* ctx)
{
  int ok = 1;
  GameObjNotifier* master = GetViewObject(0);
  if (master)
  {
    for (GameObjNotifier* child = master->FindFirstChildInRange(range); child; child = child->NextSiblingInRange(range))
    {
      static_cast<GameObj*>(child)->ApplyAllToView(ViewSide(), ctx);
    }
  }
  GameObjNotifier* child = FindFirstChildInRange(range);
  while (child)
  {
    GameObjNotifier* next = child->NextSiblingInRange(range);
    ok = static_cast<GameObj*>(child)->UpdateSelfAndDescendants(ctx) && ok;
    child = next;
  }
  return ok;
}

int GameObj::UpdateSelfAndDescendants(NotifyContext* ctx)
{
  const int first = RefreshParentView(ctx) != 0;
  return ReleaseDestroyedSubtree(ctx) && first ? 1 : 0;
}

int GameObj::InheritDestroyedReason(NotifyContext* ctx)
{
  GameObjNotifier* parent = nullptr;
  int parentDestroyed = 0;
  if (GetParent(&parent))
    parentDestroyed = (static_cast<GameObj*>(parent)->m_stateFlags >> 3) & 1;
  if (parentDestroyed)
    return ChangeDestroyedReason(static_cast<GameObj*>(parent)->m_packed & 0xff, ctx);
  return 0;
}

int GameObj::UpdateHyperdriveAndEta(NotifyContext* ctx)
{
  if ((m_stateFlags >> 11) & 1)
  {
    const int hyperdriveOk = SetHyperdriveActive(0, ctx) != 0;
    return SetEta(0, ctx) && hyperdriveOk ? 1 : 0;
  }
  return 0;
}

int GameObj::UpdateDepartureKey(NotifyContext* ctx)
{
  int ok = 1;
  ObjectKey key;
  if (IsNotGameRoot() && (m_stateFlags & kConstructed) && !(m_stateFlags & kDestroyed))
  {
    GameObjNotifier* parent = nullptr;
    ok = GetParent(&parent) != 0;
    const ObjectKey* source = nullptr;
    if (m_stateFlags & kEnrouteActive)
    {
      source = &m_departureKey;
    }
    else if (parent)
    {
      source = &static_cast<GameObj*>(parent)->m_departureKey;
    }
    if (source)
      key = *source;
  }
  return SetDepartureKey(&key, ctx) && ok ? 1 : 0;
}

int GameObj::UpdateEta(NotifyContext* ctx)
{
  int ok = 1;
  int eta = 0;
  if (IsNotGameRoot() && (m_stateFlags & kConstructed) && !(m_stateFlags & kDestroyed))
  {
    GameObjNotifier* parent = nullptr;
    ok = GetParent(&parent) != 0;
    if (m_stateFlags & kEnrouteActive)
    {
      eta = m_eta;
    }
    else if (parent)
    {
      eta = static_cast<GameObj*>(parent)->m_eta;
    }
  }
  return SetEta(eta, ctx) && ok ? 1 : 0;
}

int GameObj::UpdateEnroute(NotifyContext* ctx)
{
  int ok = 1;
  int enroute = 0;
  if (IsNotGameRoot() && (m_stateFlags & kConstructed) && !(m_stateFlags & kDestroyed))
  {
    GameObjNotifier* parent = nullptr;
    ok = GetParent(&parent) != 0;
    if (m_stateFlags & kEnrouteActive)
    {
      enroute = 1;
    }
    else if (parent)
    {
      enroute = (static_cast<GameObj*>(parent)->m_stateFlags >> 4) & 1;
    }
  }
  return SetEnroute(enroute, ctx) && ok ? 1 : 0;
}

int GameObj::RequestDeleteIfReady(NotifyContext* ctx)
{
  GameObj* master = ViewAs(0);
  if (master && !(master->m_stateFlags & kReadyForDelete) && master->CanBeDeleted())
  {
    master->SetReadyForDelete(1, ctx);
  }
  return 1;
}

int GameObj::ReleaseIfDestroyed(NotifyContext* ctx)
{
  if (!GetFlag8() && (m_stateFlags & kDestroyed))
  {
    SetFlag8(1);
    RequestDeleteIfReady(ctx);
  }
  return 1;
}

int UpdateAllChildren(GameObj* object, NotifyContext* ctx)
{
  const FamilyRange everything(1, 0xff);
  return object->UpdateChildren(&everything, ctx);
}

// Frees the three views of an object that is ready for deletion. Returns 1 if every unregistration succeeded.
int DeleteObjectViews(GameObj* object)
{
  int result = 1;
  if (object->m_stateFlags & kReadyForDelete)
  {
    GameObjNotifier* master = object->GetViewObject(0);
    GameObjNotifier* alliance = object->GetViewObject(1);
    GameObjNotifier* empire = object->GetViewObject(2);
    result = object->HasAllViews();
    if (result)
    {
      bool ok = UnregisterObject(master) != 0;
      ok = UnregisterObject(alliance) && ok;
      ok = UnregisterObject(empire) && ok;
      ok = NoteUnresolvedKeyOf(master) && ok;
      ok = DetachObject(master) && ok;
      ok = DetachObject(alliance) && ok;
      result = DetachObject(empire) && ok;
      if (master)
        delete master;
      if (alliance)
        delete alliance;
      if (empire)
        delete empire;
    }
  }
  return result;
}

// ---- virtual defaults ---------------------------------------------------------------------------------------

int GameObj::DestroyIfAllowed(const ActivityRequest* request, NotifyContext* ctx)
{
  StatusCode status(1, -1);
  int result = CheckRequest(request, &status);
  if (result && status.m_second == static_cast<uint32_t>(-1))
    result = Destroy(ctx);
  return result;
}

int GameObj::TryRename(const ActivityRequest* request, intptr_t name, NotifyContext* ctx)
{
  StatusCode status(1, -1);
  int result = CheckRename(request, name, &status);
  if (result && status.m_second == static_cast<uint32_t>(-1))
    result = Rename(reinterpret_cast<std::string*>(name), ctx);
  return result;
}

int GameObj::Destroy(NotifyContext* ctx)
{
  return ChangeDestroyedReason(kReasonDestroyed, ctx);
}

int GameObj::Rename(std::string* name, NotifyContext* ctx)
{
  return SetName(name, ctx);
}

int GameObj::Reparent(ObjectKey* key, NotifyContext* ctx)
{
  GameObjNotifier* parent = GetMasterObject(key);
  if (parent)
    return SetParent(parent, ctx);
  return 0;
}

int GameObj::ChangeDestroyedReason(int reason, NotifyContext* ctx)
{
  return SetDestroyedReason(reason, ctx);
}

int GameObj::Virtual44(NotifyContext* ctx)
{
  GameObjNotifier* parent = nullptr;
  int result = GetParent(&parent);
  if ((m_stateFlags & kCompleted) && !(m_stateFlags & kEnrouteActive))
  {
    const uint32_t side = (parent->m_flags >> 6) & 3;
    if (!isValidControlKind(side))
      return 0;
    if (CanBeControlledBy(side))
      return SetControlKind(side, ctx);
    return ChangeDestroyedReason(kReasonCompleted3, ctx);
  }
  if (!(m_stateFlags & kDestroyed))
    result = SetAutorouting(1, ctx);
  return result;
}

int GameObj::Virtual45(int a, NotifyContext* ctx)
{
  int result = 0;
  GameObjNotifier* parent = nullptr;
  result = GetParent(&parent);
  if ((m_stateFlags & kCompleted) && !(m_stateFlags & kEnrouteActive))
    return InheritDestroyedReason(ctx);
  if (!(m_stateFlags & kDestroyed))
    result = SetAutorouting(1, ctx);
  return result;
}

int GameObj::Virtual46(NotifyContext* ctx)
{
  GameObjNotifier* parent = nullptr;
  int result = GetParent(&parent);
  if ((m_stateFlags & kCompleted) && !(m_stateFlags & kEnrouteActive))
  {
    return ChangeDestroyedReason(kReasonCompleted4, ctx);
  }
  if (!(m_stateFlags & kDestroyed))
    result = SetAutorouting(1, ctx);
  return result;
}

int GameObj::ApplyAllToViewOf(int view, int unused, NotifyContext* ctx)
{
  NotifyContext context(*ctx);
  context.m_flag04 = 1;
  context.m_flag08 = 3;
  int ok = 1;
  if (view == 1 || view == 2)
    ok = ApplyAllToView(view, &context) != 0;
  return ok;
}

int GameObj::AutoscrapIfRequested(NotifyContext* ctx)
{
  int result = 1;
  if ((m_stateFlags & 1) && (m_stateFlags & 0x2000))
    result = ChangeDestroyedReason(0x15, ctx);
  return result;
}
