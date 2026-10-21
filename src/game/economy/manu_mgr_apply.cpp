#include <new>

#include "economy/manu_mgr.h"
#include "gameobj/game_hooks.h"

// ApplyX: copies the master's value into this side view and raises the OnXNotified event on it, when the context's
// side filter matches and the strategic layer wants notifications for this view (see GameObj::ApplyBuilderKey).

namespace
{

constexpr int kMasterView = 0;

ManuMgr* masterOf(ManuMgr* self)
{
  return static_cast<ManuMgr*>(self->ViewAs(kMasterView));
}

int wantsNotification(ManuMgr* view, NotifyContext* ctx)
{
  return CallGameHook2AF0() && ctx->GetViewFlag(static_cast<int>(view->ViewSide()));
}

} // namespace

int ManuMgr::ApplyRemainingGameObjCount(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    ManuMgr* master = masterOf(this);
    if (MatchesSideMask(ctx->m_flag08)) {
      const int oldValue = m_remainingGameObjCount;
      if (oldValue != master->m_remainingGameObjCount) {
        m_remainingGameObjCount = master->m_remainingGameObjCount;
        if (wantsNotification(this, ctx)) {
          result = OnRemainingGameObjCountNotified(oldValue, m_remainingGameObjCount, ctx);
        }
      }
    }
  }
  return result;
}

int ManuMgr::ApplyCompletedPointCount(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    ManuMgr* master = masterOf(this);
    if (MatchesSideMask(ctx->m_flag08)) {
      const int oldValue = m_completedPointCount;
      if (oldValue != master->m_completedPointCount) {
        m_completedPointCount = master->m_completedPointCount;
        if (wantsNotification(this, ctx)) {
          result = OnCompletedPointCountNotified(oldValue, m_completedPointCount, ctx);
        }
      }
    }
  }
  return result;
}

int ManuMgr::ApplyOverflowPointCount(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    ManuMgr* master = masterOf(this);
    if (MatchesSideMask(ctx->m_flag08)) {
      const int oldValue = m_overflowPointCount;
      if (oldValue != master->m_overflowPointCount) {
        m_overflowPointCount = master->m_overflowPointCount;
        if (wantsNotification(this, ctx)) {
          result = OnOverflowPointCountNotified(oldValue, m_overflowPointCount, ctx);
        }
      }
    }
  }
  return result;
}

int ManuMgr::ApplySeedKey(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    ManuMgr* master = masterOf(this);
    if (MatchesSideMask(ctx->m_flag08)) {
      if (m_seedKey.m_value != master->m_seedKey.m_value) {
        NullableKey oldKey = m_seedKey;
        m_seedKey = master->m_seedKey;
        if (wantsNotification(this, ctx)) {
          result = OnSeedKeyNotified(&oldKey, &m_seedKey, ctx);
        }
      }
    }
  }
  return result;
}

int ManuMgr::ApplyRequiredPointCount(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    ManuMgr* master = masterOf(this);
    if (MatchesSideMask(ctx->m_flag08)) {
      const int oldValue = m_requiredPointCount;
      if (oldValue != master->m_requiredPointCount) {
        m_requiredPointCount = master->m_requiredPointCount;
        if (wantsNotification(this, ctx)) {
          result = OnRequiredPointCountNotified(oldValue, m_requiredPointCount, ctx);
        }
      }
    }
  }
  return result;
}

int ManuMgr::ApplyTotalRequiredPointCount(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    ManuMgr* master = masterOf(this);
    if (MatchesSideMask(ctx->m_flag08)) {
      const int oldValue = m_totalRequiredPointCount;
      if (oldValue != master->m_totalRequiredPointCount) {
        m_totalRequiredPointCount = master->m_totalRequiredPointCount;
        if (wantsNotification(this, ctx)) {
          result = OnTotalRequiredPointCountNotified(oldValue, m_totalRequiredPointCount, ctx);
        }
      }
    }
  }
  return result;
}

int ManuMgr::ApplyReserved(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    ManuMgr* master = masterOf(this);
    if (MatchesSideMask(ctx->m_flag08)) {
      const int oldValue = m_reserved;
      if (oldValue != master->m_reserved) {
        m_reserved = master->m_reserved;
        if (wantsNotification(this, ctx)) {
          result = OnReservedNotified(oldValue, m_reserved, ctx);
        }
      }
    }
  }
  return result;
}

int ManuMgr::ApplyProductKey(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    ManuMgr* master = masterOf(this);
    if (MatchesSideMask(ctx->m_flag08)) {
      if (m_productKey.m_value != master->m_productKey.m_value) {
        ObjectKey oldKey(m_productKey);
        m_productKey = master->m_productKey;
        if (wantsNotification(this, ctx)) {
          result = OnProductKeyNotified(&oldKey, &m_productKey, ctx);
        }
      }
    }
  }
  return result;
}

int ManuMgr::ApplyDeploymentKey(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    ManuMgr* master = masterOf(this);
    if (MatchesSideMask(ctx->m_flag08)) {
      if (m_deploymentKey.m_value != master->m_deploymentKey.m_value) {
        ObjectKey oldKey(m_deploymentKey);
        m_deploymentKey = master->m_deploymentKey;
        if (wantsNotification(this, ctx)) {
          result = OnDeploymentKeyNotified(&oldKey, &m_deploymentKey, ctx);
        }
      }
    }
  }
  return result;
}

int ManuMgr::ApplyTargetKey(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    ManuMgr* master = masterOf(this);
    if (MatchesSideMask(ctx->m_flag08)) {
      if (m_targetKey.m_value != master->m_targetKey.m_value) {
        ObjectKey oldKey(m_targetKey);
        m_targetKey = master->m_targetKey;
        if (wantsNotification(this, ctx)) {
          result = OnTargetKeyNotified(&oldKey, &m_targetKey, ctx);
        }
      }
    }
  }
  return result;
}

std::string* ManuMgr::GetProductNameCopy(std::string* out)
{
  new (out) std::string(m_productName);
  return out;
}

int ManuMgr::ApplyProductName(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    ManuMgr* master = masterOf(this);
    if (MatchesSideMask(ctx->m_flag08)) {
      std::string masterName(master->m_productName);
      if (CompareStrings(m_productName, masterName) != 0) {
        std::string oldName(m_productName);
        (m_productName = master->m_productName);
        if (wantsNotification(this, ctx)) {
          result = OnProductNameNotified(&oldName, &m_productName, ctx);
        }
      }
    }
  }
  return result;
}
