#include "character/role.h"

#include "gameobj/game_hooks.h"
#include "gameobj/game_obj_root.h"

namespace
{

constexpr int kMasterView = 0;

bool wantsNotification(Role* view, NotifyContext* ctx)
{
  return CallGameHook2AF0() && ctx->GetViewFlag(static_cast<int>(view->ViewSide()));
}

template <typename Notify>
int applyMissionFlag(Role* self, uint32_t mask, NotifyContext* ctx, Notify notify)
{
  int result = self->IsSideView();
  if (result) {
    Role* master = self->ViewAsRole(kMasterView);
    if (self->MatchesSideMask(ctx->m_flag08)) {
      if (UpdateFlagBits(mask, master->m_flags78, &self->m_flags78)) {
        if (wantsNotification(self, ctx)) {
          result = notify(self->HasMissionFlag(mask));
        }
      }
    }
  }
  return result;
}

} // namespace

int Role::ApplyBaseDiplomacy(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    Role* master = ViewAsRole(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      const int oldValue = m_baseDiplomacy;
      if (oldValue != master->m_baseDiplomacy) {
        m_baseDiplomacy = master->m_baseDiplomacy;
        if (wantsNotification(this, ctx)) {
          result = OnBaseDiplomacyNotified(oldValue, m_baseDiplomacy, ctx);
        }
      }
    }
  }
  return result;
}

int Role::ApplyBaseEspionage(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    Role* master = ViewAsRole(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      const int oldValue = m_baseEspionage;
      if (oldValue != master->m_baseEspionage) {
        m_baseEspionage = master->m_baseEspionage;
        if (wantsNotification(this, ctx)) {
          result = OnBaseEspionageNotified(oldValue, m_baseEspionage, ctx);
        }
      }
    }
  }
  return result;
}

int Role::ApplyBaseShipyardRD(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    Role* master = ViewAsRole(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      const int oldValue = m_baseShipyardRD;
      if (oldValue != master->m_baseShipyardRD) {
        m_baseShipyardRD = master->m_baseShipyardRD;
        if (wantsNotification(this, ctx)) {
          result = OnBaseShipyardRDNotified(oldValue, m_baseShipyardRD, ctx);
        }
      }
    }
  }
  return result;
}

int Role::ApplyBaseTrainingFacilRD(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    Role* master = ViewAsRole(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      const int oldValue = m_baseTrainingFacilRD;
      if (oldValue != master->m_baseTrainingFacilRD) {
        m_baseTrainingFacilRD = master->m_baseTrainingFacilRD;
        if (wantsNotification(this, ctx)) {
          result = OnBaseTrainingFacilRDNotified(oldValue, m_baseTrainingFacilRD, ctx);
        }
      }
    }
  }
  return result;
}

int Role::ApplyBaseConstructionYardRD(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    Role* master = ViewAsRole(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      const int oldValue = m_baseConstructionYardRD;
      if (oldValue != master->m_baseConstructionYardRD) {
        m_baseConstructionYardRD = master->m_baseConstructionYardRD;
        if (wantsNotification(this, ctx)) {
          result = OnBaseConstructionYardRDNotified(oldValue, m_baseConstructionYardRD, ctx);
        }
      }
    }
  }
  return result;
}

int Role::ApplyBaseCombat(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    Role* master = ViewAsRole(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      const int oldValue = m_baseCombat;
      if (oldValue != master->m_baseCombat) {
        m_baseCombat = master->m_baseCombat;
        if (wantsNotification(this, ctx)) {
          result = OnBaseCombatNotified(oldValue, m_baseCombat, ctx);
        }
      }
    }
  }
  return result;
}

int Role::ApplyBaseLeadership(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    Role* master = ViewAsRole(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      const int oldValue = m_baseLeadership;
      if (oldValue != master->m_baseLeadership) {
        m_baseLeadership = master->m_baseLeadership;
        if (wantsNotification(this, ctx)) {
          result = OnBaseLeadershipNotified(oldValue, m_baseLeadership, ctx);
        }
      }
    }
  }
  return result;
}

int Role::ApplyBaseLoyalty(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    Role* master = ViewAsRole(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      const int oldValue = m_baseLoyalty;
      if (oldValue != master->m_baseLoyalty) {
        m_baseLoyalty = master->m_baseLoyalty;
        if (wantsNotification(this, ctx)) {
          result = OnBaseLoyaltyNotified(oldValue, m_baseLoyalty, ctx);
        }
      }
    }
  }
  return result;
}

int Role::ApplyMissionKey(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    Role* master = ViewAsRole(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      ObjectKey masterKey;
      if (m_missionKey.m_value != master->GetMissionKey(&masterKey)->m_value) {
        ObjectKey oldKey;
        oldKey.m_value = m_missionKey.m_value;
        m_missionKey.m_value = master->GetMissionKey(&masterKey)->m_value;
        if (wantsNotification(this, ctx)) {
          result = OnMissionNotified(&oldKey, &m_missionKey, ctx);
        }
      }
    }
  }
  return result;
}

int Role::ApplyMissionSeedKey(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    Role* master = ViewAsRole(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      ObjectKey masterKey;
      if (m_missionSeedKey.m_value != master->GetMissionSeedKey(&masterKey)->m_value) {
        ObjectKey oldKey;
        oldKey.m_value = m_missionSeedKey.m_value;
        m_missionSeedKey.m_value = master->GetMissionSeedKey(&masterKey)->m_value;
        if (wantsNotification(this, ctx)) {
          ObjectKey newKey;
          newKey.m_value = m_missionSeedKey.m_value;
          result = OnMissionSeedNotified(&oldKey, &newKey, ctx);
        }
      }
    }
  }
  return result;
}

int Role::ApplyParentAtMissionCompletionKey(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    Role* master = ViewAsRole(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      ObjectKey masterKey;
      if (m_parentAtMissionCompletionKey.m_value != master->GetParentAtMissionCompletionKey(&masterKey)->m_value) {
        ObjectKey oldKey;
        oldKey.m_value = m_parentAtMissionCompletionKey.m_value;
        m_parentAtMissionCompletionKey.m_value = master->GetParentAtMissionCompletionKey(&masterKey)->m_value;
        if (wantsNotification(this, ctx)) {
          result = OnParentAtMissionCompletionNotified(&oldKey, &m_parentAtMissionCompletionKey, ctx);
        }
      }
    }
  }
  return result;
}

int Role::ApplyLocationAtMissionCompletionKey(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    Role* master = ViewAsRole(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      ObjectKey masterKey;
      if (m_locationAtMissionCompletionKey.m_value != master->GetLocationAtMissionCompletionKey(&masterKey)->m_value) {
        ObjectKey oldKey;
        oldKey.m_value = m_locationAtMissionCompletionKey.m_value;
        m_locationAtMissionCompletionKey.m_value = master->GetLocationAtMissionCompletionKey(&masterKey)->m_value;
        if (wantsNotification(this, ctx)) {
          result = OnLocationAtMissionCompletionNotified(&oldKey, &m_locationAtMissionCompletionKey, ctx);
        }
      }
    }
  }
  return result;
}

int Role::ApplyDecoy(NotifyContext* ctx)
{
  return applyMissionFlag(this, role_flag::kDecoy, ctx, [&](int value) {
    return OnDecoyNotified(value, ctx);
  });
}

int Role::ApplyMovingBetweenMissions(NotifyContext* ctx)
{
  return applyMissionFlag(this, role_flag::kMovingBetweenMissions, ctx, [&](int value) {
    return OnMovingBetweenMissionsNotified(value, ctx);
  });
}

int Role::ApplyMissionRemoveRequest(NotifyContext* ctx)
{
  return applyMissionFlag(this, role_flag::kMissionRemoveRequest, ctx, [&](int value) {
    return OnMissionRemoveRequestNotified(value, ctx);
  });
}

int Role::ApplyMissionResignRequest(NotifyContext* ctx)
{
  return applyMissionFlag(this, role_flag::kMissionResignRequest, ctx, [&](int value) {
    return OnMissionResignRequestNotified(value, ctx);
  });
}

int Role::ApplyCanResignFromMission(NotifyContext* ctx)
{
  return applyMissionFlag(this, role_flag::kCanResignFromMission, ctx, [&](int value) {
    return OnCanResignFromMissionNotified(value, ctx);
  });
}

int Role::ApplyIsDecoying(NotifyContext* ctx)
{
  return applyMissionFlag(this, role_flag::kIsDecoying, ctx, [&](int value) {
    return OnIsDecoyingNotified(value, ctx);
  });
}

int Role::ApplyAdrift(NotifyContext* ctx)
{
  return applyMissionFlag(this, role_flag::kAdrift, ctx, [&](int value) {
    return OnAdriftNotified(value, ctx);
  });
}

int Role::ApplyOnMission(NotifyContext* ctx)
{
  return applyMissionFlag(this, role_flag::kOnMission, ctx, [&](int value) {
    return OnOnMissionNotified(value, ctx);
  });
}

int Role::ApplyOnHiddenMission(NotifyContext* ctx)
{
  return applyMissionFlag(this, role_flag::kOnHiddenMission, ctx, [&](int value) {
    return OnOnHiddenMissionNotified(value, ctx);
  });
}

int Role::ApplyOnMandatoryMission(NotifyContext* ctx)
{
  return applyMissionFlag(this, role_flag::kOnMandatoryMission, ctx, [&](int value) {
    return OnOnMandatoryMissionNotified(value, ctx);
  });
}
