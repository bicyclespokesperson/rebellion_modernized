#include "gameobj/game_obj.h"
#include "gameobj/game_obj_iterator.h"
#include "gameobj/object_database.h"
#include "gameobj/object_family.h"

// Apply<Property>(ctx): copies the master's value into this side view and raises the OnXNotified event on it.
// Only side views are touched, only when the context's side filter (m_flag08) matches the view and, for the event,
// when the strategic layer's hook and the context both ask for notifications on this view.

namespace
{

constexpr int kMasterView = 0;
constexpr uint32_t kControlKindMask = 0xc0;

bool wantsNotification(GameObj* view, NotifyContext* ctx)
{
  return CallGameHook2AF0() && ctx->GetViewFlag(static_cast<int>(view->ViewSide()));
}

template <typename Notify>
int applyStateFlag(GameObj* self, uint32_t mask, NotifyContext* ctx, Notify notify)
{
  int result = self->IsSideView();
  if (result)
  {
    GameObj* master = self->ViewAs(kMasterView);
    if (self->MatchesSideMask(ctx->m_flag08))
    {
      if (UpdateFlagBits(mask, master->m_stateFlags, &self->m_stateFlags))
      {
        if (wantsNotification(self, ctx))
          result = notify((self->m_stateFlags & mask) != 0 ? 1 : 0);
      }
    }
  }
  return result;
}

} // namespace

int GameObj::ApplyName(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    GameObj* master = ViewAs(kMasterView);
    if (MatchesSideMask(ctx->m_flag08))
    {
      if (CompareStrings(*GetName(), *master->GetName()) != 0)
      {
        std::string oldName(*GetName());
        result = InstallName();
        if (result)
        {
          *m_name = *master->GetName();
          if (wantsNotification(this, ctx))
            result = OnNameNotified(&oldName, GetName(), ctx);
        }
      }
    }
  }
  return result;
}

int GameObj::ApplyControlKind(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    GameObj* master = ViewAs(kMasterView);
    if (MatchesSideMask(ctx->m_flag08))
    {
      const uint32_t oldFlags = m_flags;
      if (((master->m_flags ^ oldFlags) & kControlKindMask) != 0)
      {
        SetControlSide(master->ControlSide());
        if (wantsNotification(this, ctx))
        {
          result = OnControlKindNotified((oldFlags >> 6) & 3, ControlSide(), ctx);
        }
      }
    }
  }
  return result;
}

int GameObj::ApplyParent(NotifyContext* ctx)
{
  bool changed = false;
  int result = IsSideView();
  if (IsNotGameRoot())
  {
    GameObjNotifier* currentParent = nullptr;
    GameObjNotifier* newParent = nullptr;
    int hasViews;
    if (result == 0)
    {
      hasViews = 0;
    }
    else
    {
      currentParent = ViewAs(kMasterView)->m_parent;
      hasViews = currentParent->HasAllViews();
    }
    if (hasViews)
    {
      newParent = currentParent->GetViewObject(static_cast<int>(ViewSide()));
      if (newParent->IsSideView() && IsSideView())
      {
        currentParent = m_parent;
        hasViews = currentParent->HasAllViews();
      }
      else
      {
        hasViews = 0;
      }
    }
    if (!hasViews)
      return 0;
    ObjectKey currentKey;
    ObjectKey newKey;
    currentParent->GetKey(&currentKey);
    newParent->GetKey(&newKey);
    if (newKey.m_value != currentKey.m_value)
    {
      if (!MatchesSideMask(ctx->m_flag08))
      {
        if (!currentParent->MatchesSideMask(ctx->m_flag08))
          return result;
        result = RefreshParentView(ctx);
      }
      else
      {
        changed = true;
        result = static_cast<GameObj*>(newParent)->AdoptChild(this, ctx);
        if (result == 0)
          return 0;
        if (wantsNotification(this, ctx))
          result = OnParentNotified(currentParent, newParent, ctx);
      }
      changed = true;
    }
  }
  if (result != 0 && changed)
    result = ReleaseDestroyedSubtree(ctx);
  return result;
}

int GameObj::ApplyBuilderKey(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    GameObj* master = ViewAs(kMasterView);
    if (MatchesSideMask(ctx->m_flag08))
    {
      ObjectKey masterKey;
      if (m_builderKey.m_value != master->GetBuilderKey(&masterKey)->m_value)
      {
        ObjectKey oldKey(m_builderKey);
        m_builderKey = *master->GetBuilderKey(&masterKey);
        if (wantsNotification(this, ctx))
          result = OnBuilderKeyNotified(&oldKey, &m_builderKey, ctx);
      }
    }
  }
  return result;
}

int GameObj::ApplyDepartureKey(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    GameObj* master = ViewAs(kMasterView);
    if (MatchesSideMask(ctx->m_flag08))
    {
      ObjectKey masterKey;
      if (m_departureKey.m_value != master->GetDepartureKey(&masterKey)->m_value)
      {
        ObjectKey oldKey(m_departureKey);
        m_departureKey = *master->GetDepartureKey(&masterKey);
        if (wantsNotification(this, ctx))
          result = OnDepartureKeyNotified(&oldKey, &m_departureKey, ctx);
      }
    }
  }
  return result;
}

int GameObj::ApplyDestroyedReason(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    GameObj* master = ViewAs(kMasterView);
    if (MatchesSideMask(ctx->m_flag08))
    {
      const uint32_t oldPacked = m_packed;
      if (static_cast<char>(oldPacked) != static_cast<char>(master->m_packed))
      {
        m_packed = (((master->m_packed & 0xff) ^ oldPacked) & 0xff) ^ oldPacked;
        if (wantsNotification(this, ctx))
        {
          result = OnDestroyedReasonNotified(oldPacked & 0xff, m_packed & 0xff, ctx);
        }
      }
    }
  }
  return result;
}

int GameObj::ApplyDeploymentCount(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    GameObj* master = ViewAs(kMasterView);
    if (MatchesSideMask(ctx->m_flag08))
    {
      const uint32_t oldPacked = m_packed;
      if (static_cast<char>(oldPacked >> 8) != static_cast<char>(master->m_packed >> 8))
      {
        m_packed = (oldPacked & 0xffff00ff) | (((master->m_packed >> 8) & 0xff) << 8);
        if (wantsNotification(this, ctx))
        {
          result = OnDeploymentCountNotified((oldPacked >> 8) & 0xff, (m_packed >> 8) & 0xff, ctx);
        }
      }
    }
  }
  return result;
}

int GameObj::ApplyDestinationCount(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    GameObj* master = ViewAs(kMasterView);
    if (MatchesSideMask(ctx->m_flag08))
    {
      const uint32_t oldPacked = m_packed;
      if (((oldPacked ^ master->m_packed) & 0xff0000) != 0)
      {
        m_packed = (oldPacked & 0xff00ffff) | (((master->m_packed >> 16) & 0xff) << 16);
        if (wantsNotification(this, ctx))
        {
          result = OnDestinationCountNotified((oldPacked >> 16) & 0xff, (m_packed >> 16) & 0xff, ctx);
        }
      }
    }
  }
  return result;
}

int GameObj::ApplyEta(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    GameObj* master = ViewAs(kMasterView);
    if (MatchesSideMask(ctx->m_flag08))
    {
      const int oldEta = m_eta;
      if (oldEta != master->m_eta)
      {
        m_eta = master->m_eta;
        if (wantsNotification(this, ctx))
          result = OnEtaNotified(oldEta, m_eta, ctx);
      }
    }
  }
  return result;
}

int GameObj::ApplyUsable(NotifyContext* ctx)
{
  return applyStateFlag(this, 0x1, ctx,
                        [&](int value)
                        {
                          return OnUsableNotified(value, ctx);
                        });
}

int GameObj::ApplyCreated(NotifyContext* ctx)
{
  return applyStateFlag(this, 0x2, ctx,
                        [&](int value)
                        {
                          return OnCreatedNotified(value, ctx);
                        });
}

int GameObj::ApplyCompleted(NotifyContext* ctx)
{
  return applyStateFlag(this, 0x4, ctx,
                        [&](int value)
                        {
                          return OnCompletedNotified(value, ctx);
                        });
}

int GameObj::ApplyDestroyed(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    GameObj* master = ViewAs(kMasterView);
    if (MatchesSideMask(ctx->m_flag08))
    {
      if (UpdateFlagBits(0x8, master->m_stateFlags, &m_stateFlags))
      {
        if (wantsNotification(this, ctx))
          OnDestroyedNotified((m_stateFlags >> 3) & 1, ctx);
        result = RequestDeleteIfReady(ctx);
      }
    }
  }
  return result;
}

int GameObj::ApplyEnroute(NotifyContext* ctx)
{
  return applyStateFlag(this, 0x10, ctx,
                        [&](int value)
                        {
                          return OnEnrouteNotified(value, ctx);
                        });
}

int GameObj::ApplyEnrouteActive(NotifyContext* ctx)
{
  return applyStateFlag(this, 0x20, ctx,
                        [&](int value)
                        {
                          return OnEnrouteActiveNotified(value, ctx);
                        });
}

int GameObj::ApplyExisting(NotifyContext* ctx)
{
  return applyStateFlag(this, 0x40, ctx,
                        [&](int value)
                        {
                          return OnExistingNotified(value, ctx);
                        });
}

int GameObj::ApplyObservedByAlliance(NotifyContext* ctx)
{
  return applyStateFlag(this, 0x80, ctx,
                        [&](int value)
                        {
                          return OnObservedByAllianceNotified(value, ctx);
                        });
}

int GameObj::ApplyObservedByEmpire(NotifyContext* ctx)
{
  return applyStateFlag(this, 0x100, ctx,
                        [&](int value)
                        {
                          return OnObservedByEmpireNotified(value, ctx);
                        });
}

int GameObj::ApplyDamaged(NotifyContext* ctx)
{
  return applyStateFlag(this, 0x200, ctx,
                        [&](int value)
                        {
                          return OnDamagedNotified(value, ctx);
                        });
}

int GameObj::ApplyHyperdriveActive(NotifyContext* ctx)
{
  return applyStateFlag(this, 0x800, ctx,
                        [&](int value)
                        {
                          return OnHyperdriveActiveNotified(value, ctx);
                        });
}

int GameObj::ApplyAutorouting(NotifyContext* ctx)
{
  return applyStateFlag(this, 0x1000, ctx,
                        [&](int value)
                        {
                          return OnAutoroutingNotified(value, ctx);
                        });
}

int GameObj::ApplyAutoscrapRequest(NotifyContext* ctx)
{
  return applyStateFlag(this, 0x2000, ctx,
                        [&](int value)
                        {
                          return OnAutoscrapRequestNotified(value, ctx);
                        });
}

int GameObj::ApplyLocked(NotifyContext* ctx)
{
  return applyStateFlag(this, 0x4000, ctx,
                        [&](int value)
                        {
                          return OnLockedNotified(value, ctx);
                        });
}

int GameObj::ApplyReadyForDelete(NotifyContext* ctx)
{
  return applyStateFlag(this, 0x8000, ctx,
                        [&](int value)
                        {
                          return OnReadyForDeleteNotified(value, ctx);
                        });
}

int GameObj::ApplyConstructed(NotifyContext* ctx)
{
  return applyStateFlag(this, 0x10000, ctx,
                        [&](int value)
                        {
                          return OnConstructedNotified(value, ctx);
                        });
}

int GameObj::ApplyDeployed(NotifyContext* ctx)
{
  return applyStateFlag(this, 0x20000, ctx,
                        [&](int value)
                        {
                          return OnDeployedNotified(value, ctx);
                        });
}

int GameObj::ApplyAll(NotifyContext* ctx)
{
  const int result = IsSideView();
  if (ctx->m_flag04 == 1)
  {
    ApplyParent(ctx);
    ApplyControlKind(ctx);
    ApplyName(ctx);
    ApplyBuilderKey(ctx);
    ApplyDepartureKey(ctx);
    ApplyDestroyedReason(ctx);
    ApplyDeploymentCount(ctx);
    ApplyDestinationCount(ctx);
    ApplyEta(ctx);
    ApplyUsable(ctx);
    ApplyCreated(ctx);
    ApplyCompleted(ctx);
    ApplyDestroyed(ctx);
    ApplyEnroute(ctx);
    ApplyEnrouteActive(ctx);
    ApplyExisting(ctx);
    ApplyObservedByAlliance(ctx);
    ApplyObservedByEmpire(ctx);
    ApplyDamaged(ctx);
    ApplyHyperdriveActive(ctx);
    ApplyAutorouting(ctx);
    ApplyAutoscrapRequest(ctx);
    ApplyLocked(ctx);
    ApplyReadyForDelete(ctx);
    ApplyConstructed(ctx);
    ApplyDeployed(ctx);
  }
  else if (ctx->m_flag04 == 2)
  {
    ApplyName(ctx);
  }
  return result;
}

int GameObj::ApplyAllToView(int view, NotifyContext* ctx)
{
  GameObj* object = ViewAs(view);
  if (object)
    return object->ApplyAll(ctx);
  return 1;
}

ObjectKey* GameObj::GetBuilderKey(ObjectKey* out)
{
  *out = m_builderKey;
  return out;
}

ObjectKey* GameObj::GetDepartureKey(ObjectKey* out)
{
  *out = m_departureKey;
  return out;
}

// ---- helpers shared by the events -----------------------------------------------------------------------

int GameObj::UpdateViewVisibility(NotifyContext* ctx)
{
  int result = 1;
  for (int view = 1; view <= 2 && result; ++view)
  {
    GameObj* object = ViewAs(view);
    uint32_t viewSide = object->ViewSide();
    const uint32_t oldMask = object->m_flags & 0xf;
    if (viewSide != 1 && viewSide != 2)
      viewSide = 0;
    const uint32_t stateFlags = m_stateFlags;
    const int observedBit = viewSide == 1 ? 7 : 8;
    uint32_t newMask = 2;
    if (ControlSide() == viewSide || (((stateFlags >> observedBit) & 1) && (stateFlags & 4) && !(stateFlags & 0x10)))
    {
      newMask = 1;
    }
    if (newMask != oldMask)
    {
      object->SetSideMask(newMask);
      if (newMask == 1 && oldMask == 2)
      {
        NotifyContext context(*ctx);
        context.m_flag08 = static_cast<int>(newMask);
        result = object->ApplyAll(&context);
        UpdateAllChildren(object, &context);
      }
    }
  }
  return result;
}

int GameObj::AdoptChild(GameObjNotifier* child, NotifyContext* ctx)
{
  auto* const oldParent = static_cast<GameObj*>(child->m_parent);
  GameObjNotifier* previous = child;
  bool ok = AddChild(child, &previous) != 0;
  if (m_stateFlags & 8)
    ok = RequestDeleteIfReady(ctx) && ok;
  if (oldParent->m_stateFlags & 8)
  {
    const int released = oldParent->RequestDeleteIfReady(ctx);
    if (released && ok)
      return true;
    ok = false;
  }
  return ok;
}

int GameObj::InstallName()
{
  int result = 1;
  if (!m_name)
  {
    m_name = new std::string();
    result = 0;
    if (m_name)
    {
      result = 1;
      if (result == 0)
      {
        delete m_name;
        m_name = nullptr;
      }
    }
  }
  return result;
}

int GameObj::RefreshParentView(NotifyContext* ctx)
{
  int result = IsSideView();
  if (!IsNotGameRoot())
    return result;
  GameObjNotifier* currentParent = nullptr;
  int hasViews;
  if (result == 0)
  {
    hasViews = 0;
  }
  else
  {
    currentParent = ViewAs(kMasterView)->m_parent;
    hasViews = currentParent->HasAllViews();
  }
  GameObjNotifier* newParent = nullptr;
  if (hasViews)
  {
    newParent = currentParent->GetViewObject(static_cast<int>(ViewSide()));
    if (newParent->IsSideView() && IsSideView())
    {
      currentParent = m_parent;
      hasViews = currentParent->HasAllViews();
    }
    else
    {
      hasViews = 0;
    }
  }
  result = hasViews;
  if (result)
  {
    ObjectKey currentKey;
    ObjectKey newKey;
    currentParent->GetKey(&currentKey);
    newParent->GetKey(&newKey);
    if (newKey.m_value != currentKey.m_value)
      result = MoveToSideRoot(ctx);
  }
  return result;
}

int GameObj::MoveToSideRoot(NotifyContext* ctx)
{
  int result = IsSideView();
  if (IsNotGameRoot() && result)
  {
    GameObjNotifier* const oldParent = m_parent;
    result = oldParent->HasAllViews();
    if (result)
    {
      const FamilyRange sideRoots(family::kSideRoot, family::kSide);
      GameObjNotifier* newParent = FindObjectInRange(&sideRoots, static_cast<int>(ViewSide()), 1);
      result = 0;
      if (newParent)
      {
        result = static_cast<GameObj*>(newParent)->AdoptChild(this, ctx) != 0;
        if (wantsNotification(this, ctx))
        {
          const int notified = OnParentNotified(oldParent, newParent, ctx);
          result = notified && result ? 1 : 0;
        }
      }
    }
  }
  return result;
}

int GameObj::ReleaseDestroyedSubtree(NotifyContext* ctx)
{
  GameObjNotifier* ancestor = m_parent;
  bool underSideRoot = false;
  while (ancestor && !underSideRoot)
  {
    underSideRoot = family::kSideRoot == ancestor->GetFamily();
    ancestor = ancestor->m_parent;
  }
  int ok = 1;
  if (underSideRoot)
  {
    GameObjIterator children(this, 4);
    children.Begin();
    while (children.Current())
    {
      auto* child = static_cast<GameObj*>(children.Current());
      children.Advance();
      ok = child->ReleaseDestroyedSubtree(ctx) && ok ? 1 : 0;
    }
    ok = ReleaseDestroyed(ctx) && ok ? 1 : 0;
  }
  return ok;
}

int GameObj::ReleaseDestroyed(NotifyContext* ctx)
{
  const int sideView = IsSideView();
  GameObj* master = nullptr;
  if (sideView)
    master = ViewAs(kMasterView);
  int result = 0;
  GameObjNotifier* masterParent = nullptr;
  if (master && (result = master->GetParent(&masterParent)) != 0 && (master->m_stateFlags & 8) &&
      !(master->m_stateFlags & 0x4000))
  {
    const uint32_t parentFamily = masterParent->GetFamily();
    if (parentFamily >= family::kSideRoot && parentFamily < family::kSide)
    {
      GameObjIterator children(this, 4);
      children.Begin();
      while (children.Current())
      {
        auto* child = static_cast<GameObj*>(children.Current());
        children.Advance();
        result = child->MoveToSideRoot(ctx) && result ? 1 : 0;
      }
      NotifyContext context(*ctx);
      context.m_flag04 = 1;
      context.m_flag08 = 3;
      if (!(m_stateFlags & 2))
        context.SetViewFlag(static_cast<int>(ViewSide()), 0);
      bool ok = PrepareForDestroyedRelease(reinterpret_cast<intptr_t>(&context)) && result;
      ok = ApplyDestroyed(&context) && ok;
      result = RefreshParentView(&context) && ok ? 1 : 0;
    }
  }
  return result;
}
