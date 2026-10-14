#include "character/character.h"
#include "gameobj/game_hooks.h"

// Apply<Property>(ctx): copies the master's value into this side view and raises the OnXNotified event on it. Only side
// views are touched, only when the context's side filter (m_flag08) matches the view and, for the event, when the
// strategic layer's hook and the context both ask for notifications on this view.

namespace
{

constexpr int kMasterView = 0;
constexpr uint32_t kCaptured = 0x1;
constexpr uint32_t kCanHeal = 0x2;
constexpr uint32_t kFastHeal = 0x4;
constexpr uint32_t kForceAware = 0x10;
constexpr uint32_t kForcePotential = 0x20;
constexpr uint32_t kHealing = 0x40;
constexpr uint32_t kDiscoveringForceUser = 0x80;
constexpr uint32_t kCanEscape = 0x100;
constexpr uint32_t kEscapeRequest = 0x200;
constexpr uint32_t kEscapeAttempt = 0x400;

bool wantsNotification(Character* view, NotifyContext* ctx)
{
  return CallGameHook2AF0() && ctx->GetViewFlag(static_cast<int>(view->ViewSide()));
}

template <typename Notify>
int applyFlag(Character* self, uint32_t mask, NotifyContext* ctx, Notify notify)
{
  int result = self->IsSideView();
  if (result)
  {
    Character* master = self->ViewAsCharacter(kMasterView);
    if (self->MatchesSideMask(ctx->m_flag08))
    {
      if (UpdateFlagBits(mask, master->m_flagsAc, &self->m_flagsAc))
      {
        if (wantsNotification(self, ctx))
          result = notify((self->m_flagsAc & mask) != 0 ? 1 : 0);
      }
    }
  }
  return result;
}

} // namespace

int Character::ApplyEnhancedDiplomacy(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    Character* master = ViewAsCharacter(kMasterView);
    if (MatchesSideMask(ctx->m_flag08))
    {
      const int oldValue = m_enhancedDiplomacy;
      if (oldValue != master->m_enhancedDiplomacy)
      {
        m_enhancedDiplomacy = master->m_enhancedDiplomacy;
        if (wantsNotification(this, ctx))
          result = OnEnhancedDiplomacyNotified(oldValue, m_enhancedDiplomacy, ctx);
      }
    }
  }
  return result;
}

int Character::ApplyEnhancedEspionage(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    Character* master = ViewAsCharacter(kMasterView);
    if (MatchesSideMask(ctx->m_flag08))
    {
      const int oldValue = m_enhancedEspionage;
      if (oldValue != master->m_enhancedEspionage)
      {
        m_enhancedEspionage = master->m_enhancedEspionage;
        if (wantsNotification(this, ctx))
          result = OnEnhancedEspionageNotified(oldValue, m_enhancedEspionage, ctx);
      }
    }
  }
  return result;
}

int Character::ApplyEnhancedShipyardRD(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    Character* master = ViewAsCharacter(kMasterView);
    if (MatchesSideMask(ctx->m_flag08))
    {
      const int oldValue = m_enhancedShipyardRD;
      if (oldValue != master->m_enhancedShipyardRD)
      {
        m_enhancedShipyardRD = master->m_enhancedShipyardRD;
        if (wantsNotification(this, ctx))
          result = OnEnhancedShipyardRDNotified(oldValue, m_enhancedShipyardRD, ctx);
      }
    }
  }
  return result;
}

int Character::ApplyEnhancedTrainingFacilRD(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    Character* master = ViewAsCharacter(kMasterView);
    if (MatchesSideMask(ctx->m_flag08))
    {
      const int oldValue = m_enhancedTrainingFacilRD;
      if (oldValue != master->m_enhancedTrainingFacilRD)
      {
        m_enhancedTrainingFacilRD = master->m_enhancedTrainingFacilRD;
        if (wantsNotification(this, ctx))
          result = OnEnhancedTrainingFacilRDNotified(oldValue, m_enhancedTrainingFacilRD, ctx);
      }
    }
  }
  return result;
}

int Character::ApplyEnhancedConstructionYardRD(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    Character* master = ViewAsCharacter(kMasterView);
    if (MatchesSideMask(ctx->m_flag08))
    {
      const int oldValue = m_enhancedConstructionYardRD;
      if (oldValue != master->m_enhancedConstructionYardRD)
      {
        m_enhancedConstructionYardRD = master->m_enhancedConstructionYardRD;
        if (wantsNotification(this, ctx))
          result = OnEnhancedConstructionYardRDNotified(oldValue, m_enhancedConstructionYardRD, ctx);
      }
    }
  }
  return result;
}

int Character::ApplyEnhancedCombat(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    Character* master = ViewAsCharacter(kMasterView);
    if (MatchesSideMask(ctx->m_flag08))
    {
      const int oldValue = m_enhancedCombat;
      if (oldValue != master->m_enhancedCombat)
      {
        m_enhancedCombat = master->m_enhancedCombat;
        if (wantsNotification(this, ctx))
          result = OnEnhancedCombatNotified(oldValue, m_enhancedCombat, ctx);
      }
    }
  }
  return result;
}

int Character::ApplyEnhancedLeadership(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    Character* master = ViewAsCharacter(kMasterView);
    if (MatchesSideMask(ctx->m_flag08))
    {
      const int oldValue = m_enhancedLeadership;
      if (oldValue != master->m_enhancedLeadership)
      {
        m_enhancedLeadership = master->m_enhancedLeadership;
        if (wantsNotification(this, ctx))
          result = OnEnhancedLeadershipNotified(oldValue, m_enhancedLeadership, ctx);
      }
    }
  }
  return result;
}

int Character::ApplyEnhancedLoyalty(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    Character* master = ViewAsCharacter(kMasterView);
    if (MatchesSideMask(ctx->m_flag08))
    {
      const int oldValue = m_enhancedLoyalty;
      if (oldValue != master->m_enhancedLoyalty)
      {
        m_enhancedLoyalty = master->m_enhancedLoyalty;
        if (wantsNotification(this, ctx))
          result = OnEnhancedLoyaltyNotified(oldValue, m_enhancedLoyalty, ctx);
      }
    }
  }
  return result;
}

int Character::ApplyForce(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    Character* master = ViewAsCharacter(kMasterView);
    if (MatchesSideMask(ctx->m_flag08))
    {
      const int oldValue = m_force;
      if (oldValue != master->m_force)
      {
        m_force = master->m_force;
        if (wantsNotification(this, ctx))
          result = OnForceNotified(oldValue, m_force, ctx);
      }
    }
  }
  return result;
}

int Character::ApplyForceExperience(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    Character* master = ViewAsCharacter(kMasterView);
    if (MatchesSideMask(ctx->m_flag08))
    {
      const int oldValue = m_forceExperience;
      if (oldValue != master->m_forceExperience)
      {
        m_forceExperience = master->m_forceExperience;
        if (wantsNotification(this, ctx))
          result = OnForceExperienceNotified(oldValue, m_forceExperience, ctx);
      }
    }
  }
  return result;
}

int Character::ApplyForceTraining(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    Character* master = ViewAsCharacter(kMasterView);
    if (MatchesSideMask(ctx->m_flag08))
    {
      const int oldValue = m_forceTraining;
      if (oldValue != master->m_forceTraining)
      {
        m_forceTraining = master->m_forceTraining;
        if (wantsNotification(this, ctx))
          result = OnForceTrainingNotified(oldValue, m_forceTraining, ctx);
      }
    }
  }
  return result;
}

int Character::ApplyLeadershipAdjustment(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    Character* master = ViewAsCharacter(kMasterView);
    if (MatchesSideMask(ctx->m_flag08))
    {
      const int oldValue = m_leadershipAdjustment;
      if (oldValue != master->m_leadershipAdjustment)
      {
        m_leadershipAdjustment = master->m_leadershipAdjustment;
        if (wantsNotification(this, ctx))
          result = OnLeadershipAdjustmentNotified(oldValue, m_leadershipAdjustment, ctx);
      }
    }
  }
  return result;
}

int Character::ApplyInjury(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    Character* master = ViewAsCharacter(kMasterView);
    if (MatchesSideMask(ctx->m_flag08))
    {
      const int oldValue = m_injury;
      if (oldValue != master->m_injury)
      {
        m_injury = master->m_injury;
        if (wantsNotification(this, ctx))
          result = OnInjuryNotified(oldValue, m_injury, ctx);
      }
    }
  }
  return result;
}

int Character::ApplyCommandKind(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    Character* master = ViewAsCharacter(kMasterView);
    if (MatchesSideMask(ctx->m_flag08))
    {
      const int oldValue = m_commandKind;
      if (oldValue != master->m_commandKind)
      {
        m_commandKind = master->m_commandKind;
        if (wantsNotification(this, ctx))
          result = OnCommandKindNotified(oldValue, m_commandKind, ctx);
      }
    }
  }
  return result;
}

int Character::ApplyState(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    Character* master = ViewAsCharacter(kMasterView);
    if (MatchesSideMask(ctx->m_flag08))
    {
      const int oldValue = m_state;
      if (oldValue != master->m_state)
      {
        m_state = master->m_state;
        if (wantsNotification(this, ctx))
          result = OnStateNotified(oldValue, m_state, ctx);
      }
    }
  }
  return result;
}

int Character::ApplyMissionHyperdriveModifier(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    Character* master = ViewAsCharacter(kMasterView);
    if (MatchesSideMask(ctx->m_flag08))
    {
      const int oldValue = m_missionHyperdriveModifier;
      if (oldValue != master->m_missionHyperdriveModifier)
      {
        m_missionHyperdriveModifier = master->m_missionHyperdriveModifier;
        if (wantsNotification(this, ctx))
          result = OnMissionHyperdriveModifierNotified(oldValue, m_missionHyperdriveModifier, ctx);
      }
    }
  }
  return result;
}

int Character::ApplyEncounterKey(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    Character* master = ViewAsCharacter(kMasterView);
    if (MatchesSideMask(ctx->m_flag08))
    {
      ObjectKey masterKey;
      if (m_encounterKey.m_value != master->GetEncounterKey(&masterKey)->m_value)
      {
        ObjectKey oldKey(m_encounterKey);
        m_encounterKey = *master->GetEncounterKey(&masterKey);
        if (wantsNotification(this, ctx))
          result = OnEncounterNotified(&oldKey, &m_encounterKey, ctx);
      }
    }
  }
  return result;
}

int Character::ApplyTraitorDiscoveredKey(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    Character* master = ViewAsCharacter(kMasterView);
    if (MatchesSideMask(ctx->m_flag08))
    {
      ObjectKey masterKey;
      if (m_traitorDiscoveredKey.m_value != master->GetTraitorDiscoveredKey(&masterKey)->m_value)
      {
        ObjectKey oldKey(m_traitorDiscoveredKey);
        m_traitorDiscoveredKey = *master->GetTraitorDiscoveredKey(&masterKey);
        if (wantsNotification(this, ctx))
          result = OnTraitorDiscoveredNotified(&oldKey, &m_traitorDiscoveredKey, ctx);
      }
    }
  }
  return result;
}

int Character::ApplyForceUserDiscoveredKey(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    Character* master = ViewAsCharacter(kMasterView);
    if (MatchesSideMask(ctx->m_flag08))
    {
      ObjectKey masterKey;
      if (m_forceUserDiscoveredKey.m_value != master->GetForceUserDiscoveredKey(&masterKey)->m_value)
      {
        ObjectKey oldKey(m_forceUserDiscoveredKey);
        m_forceUserDiscoveredKey = *master->GetForceUserDiscoveredKey(&masterKey);
        if (wantsNotification(this, ctx))
          result = OnForceUserDiscoveredNotified(&oldKey, &m_forceUserDiscoveredKey, ctx);
      }
    }
  }
  return result;
}

int Character::ApplyCommandingKey(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    Character* master = ViewAsCharacter(kMasterView);
    if (MatchesSideMask(ctx->m_flag08))
    {
      ObjectKey masterKey;
      if (m_commandingKey.m_value != master->GetCommandingKey(&masterKey)->m_value)
      {
        ObjectKey oldKey(m_commandingKey);
        m_commandingKey = *master->GetCommandingKey(&masterKey);
        if (wantsNotification(this, ctx))
          result = OnCommandingNotified(&oldKey, &m_commandingKey, ctx);
      }
    }
  }
  return result;
}

int Character::ApplyCaptured(NotifyContext* ctx)
{
  return applyFlag(this, kCaptured, ctx,
                   [&](int on)
                   {
                     return OnCapturedNotified(on, ctx);
                   });
}

int Character::ApplyCanHeal(NotifyContext* ctx)
{
  return applyFlag(this, kCanHeal, ctx,
                   [&](int on)
                   {
                     return OnCanHealNotified(on, ctx);
                   });
}

int Character::ApplyFastHeal(NotifyContext* ctx)
{
  return applyFlag(this, kFastHeal, ctx,
                   [&](int on)
                   {
                     return OnFastHealNotified(on, ctx);
                   });
}

int Character::ApplyForceAware(NotifyContext* ctx)
{
  return applyFlag(this, kForceAware, ctx,
                   [&](int on)
                   {
                     return OnForceAwareNotified(on, ctx);
                   });
}

int Character::ApplyForcePotential(NotifyContext* ctx)
{
  return applyFlag(this, kForcePotential, ctx,
                   [&](int on)
                   {
                     return OnForcePotentialNotified(on, ctx);
                   });
}

int Character::ApplyHealing(NotifyContext* ctx)
{
  return applyFlag(this, kHealing, ctx,
                   [&](int on)
                   {
                     return OnHealingNotified(on, ctx);
                   });
}

int Character::ApplyDiscoveringForceUser(NotifyContext* ctx)
{
  return applyFlag(this, kDiscoveringForceUser, ctx,
                   [&](int on)
                   {
                     return OnDiscoveringForceUserNotified(on, ctx);
                   });
}

int Character::ApplyCanEscape(NotifyContext* ctx)
{
  return applyFlag(this, kCanEscape, ctx,
                   [&](int on)
                   {
                     return OnCanEscapeNotified(on, ctx);
                   });
}

int Character::ApplyEscapeRequest(NotifyContext* ctx)
{
  return applyFlag(this, kEscapeRequest, ctx,
                   [&](int on)
                   {
                     return OnEscapeRequestNotified(on, ctx);
                   });
}

int Character::ApplyEscapeAttempt(NotifyContext* ctx)
{
  return applyFlag(this, kEscapeAttempt, ctx,
                   [&](int on)
                   {
                     return OnEscapeAttemptNotified(on, ctx);
                   });
}
