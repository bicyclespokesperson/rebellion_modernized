#include "character/character.h"
#include "data/game_math.h"
#include "events/event_post.h"

// Validated setters of the Character properties: master view only; each stores the value, has both side views copy it
// (ApplyX) and finally raises the virtual OnXChanged event. The 13 value setters other than the range-checked ones call
// Clamp() on the value and ignore its result.

namespace
{

constexpr int kAllianceView = 1;
constexpr int kEmpireView = 2;
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

} // namespace

int Character::SetEnhancedDiplomacy(int value, NotifyContext* ctx)
{
  int result = IsMasterView();
  if (result)
    Clamp(value, 0, 0x7fff);
  if (result && m_enhancedDiplomacy != value)
  {
    const int oldValue = m_enhancedDiplomacy;
    m_enhancedDiplomacy = static_cast<int16_t>(value);
    ViewAsCharacter(kAllianceView)->ApplyEnhancedDiplomacy(ctx);
    ViewAsCharacter(kEmpireView)->ApplyEnhancedDiplomacy(ctx);
    OnEnhancedDiplomacyChanged(oldValue, m_enhancedDiplomacy, ctx);
  }
  return result;
}

int Character::SetEnhancedEspionage(int value, NotifyContext* ctx)
{
  int result = IsMasterView();
  if (result)
    Clamp(value, 0, 0x7fff);
  if (result && m_enhancedEspionage != value)
  {
    const int oldValue = m_enhancedEspionage;
    m_enhancedEspionage = static_cast<int16_t>(value);
    ViewAsCharacter(kAllianceView)->ApplyEnhancedEspionage(ctx);
    ViewAsCharacter(kEmpireView)->ApplyEnhancedEspionage(ctx);
    OnEnhancedEspionageChanged(oldValue, m_enhancedEspionage, ctx);
  }
  return result;
}

int Character::SetEnhancedShipyardRD(int value, NotifyContext* ctx)
{
  int result = IsMasterView();
  if (result)
    Clamp(value, 0, 0x7fff);
  if (result && m_enhancedShipyardRD != value)
  {
    const int oldValue = m_enhancedShipyardRD;
    m_enhancedShipyardRD = static_cast<int16_t>(value);
    ViewAsCharacter(kAllianceView)->ApplyEnhancedShipyardRD(ctx);
    ViewAsCharacter(kEmpireView)->ApplyEnhancedShipyardRD(ctx);
    OnEnhancedShipyardRDChanged(oldValue, m_enhancedShipyardRD, ctx);
  }
  return result;
}

int Character::SetEnhancedTrainingFacilRD(int value, NotifyContext* ctx)
{
  int result = IsMasterView();
  if (result)
    Clamp(value, 0, 0x7fff);
  if (result && m_enhancedTrainingFacilRD != value)
  {
    const int oldValue = m_enhancedTrainingFacilRD;
    m_enhancedTrainingFacilRD = static_cast<int16_t>(value);
    ViewAsCharacter(kAllianceView)->ApplyEnhancedTrainingFacilRD(ctx);
    ViewAsCharacter(kEmpireView)->ApplyEnhancedTrainingFacilRD(ctx);
    OnEnhancedTrainingFacilRDChanged(oldValue, m_enhancedTrainingFacilRD, ctx);
  }
  return result;
}

int Character::SetEnhancedConstructionYardRD(int value, NotifyContext* ctx)
{
  int result = IsMasterView();
  if (result)
    Clamp(value, 0, 0x7fff);
  if (result && m_enhancedConstructionYardRD != value)
  {
    const int oldValue = m_enhancedConstructionYardRD;
    m_enhancedConstructionYardRD = static_cast<int16_t>(value);
    ViewAsCharacter(kAllianceView)->ApplyEnhancedConstructionYardRD(ctx);
    ViewAsCharacter(kEmpireView)->ApplyEnhancedConstructionYardRD(ctx);
    OnEnhancedConstructionYardRDChanged(oldValue, m_enhancedConstructionYardRD, ctx);
  }
  return result;
}

int Character::SetEnhancedCombat(int value, NotifyContext* ctx)
{
  int result = IsMasterView();
  if (result)
    Clamp(value, 0, 0x7fff);
  if (result && m_enhancedCombat != value)
  {
    const int oldValue = m_enhancedCombat;
    m_enhancedCombat = static_cast<int16_t>(value);
    ViewAsCharacter(kAllianceView)->ApplyEnhancedCombat(ctx);
    ViewAsCharacter(kEmpireView)->ApplyEnhancedCombat(ctx);
    OnEnhancedCombatChanged(oldValue, m_enhancedCombat, ctx);
  }
  return result;
}

int Character::SetEnhancedLeadership(int value, NotifyContext* ctx)
{
  int result = IsMasterView();
  if (result)
    Clamp(value, 0, 0x7fff);
  if (result && m_enhancedLeadership != value)
  {
    const int oldValue = m_enhancedLeadership;
    m_enhancedLeadership = static_cast<int16_t>(value);
    ViewAsCharacter(kAllianceView)->ApplyEnhancedLeadership(ctx);
    ViewAsCharacter(kEmpireView)->ApplyEnhancedLeadership(ctx);
    OnEnhancedLeadershipChanged(oldValue, m_enhancedLeadership, ctx);
  }
  return result;
}

int Character::SetEnhancedLoyalty(int value, NotifyContext* ctx)
{
  int result = IsMasterView();
  int valid = 0;
  if (result)
  {
    valid = IsInRange(value, 0, 100, "charac.cpp: Invalid EnhancedLoyalty value!");
    Clamp(value, 0, 0x7fff);
  }
  if (valid && m_enhancedLoyalty != value)
  {
    const int oldValue = m_enhancedLoyalty;
    m_enhancedLoyalty = static_cast<int16_t>(value);
    ViewAsCharacter(kAllianceView)->ApplyEnhancedLoyalty(ctx);
    ViewAsCharacter(kEmpireView)->ApplyEnhancedLoyalty(ctx);
    OnEnhancedLoyaltyChanged(oldValue, m_enhancedLoyalty, ctx);
  }
  return valid;
}

int Character::SetForce(int value, NotifyContext* ctx)
{
  int result = IsMasterView();
  if (result)
    Clamp(value, 0, 0x7fff);
  if (result && m_force != value)
  {
    const int oldValue = m_force;
    m_force = static_cast<int16_t>(value);
    ViewAsCharacter(kAllianceView)->ApplyForce(ctx);
    ViewAsCharacter(kEmpireView)->ApplyForce(ctx);
    OnForceChanged(oldValue, m_force, ctx);
  }
  return result;
}

int Character::SetForceExperience(int value, NotifyContext* ctx)
{
  int result = IsMasterView();
  if (result)
    Clamp(value, 0, 0x7fff);
  if (result && m_forceExperience != value)
  {
    const int oldValue = m_forceExperience;
    m_forceExperience = static_cast<int16_t>(value);
    ViewAsCharacter(kAllianceView)->ApplyForceExperience(ctx);
    ViewAsCharacter(kEmpireView)->ApplyForceExperience(ctx);
    OnForceExperienceChanged(oldValue, m_forceExperience, ctx);
  }
  return result;
}

int Character::SetForceTraining(int value, NotifyContext* ctx)
{
  int result = IsMasterView();
  if (result)
    Clamp(value, 0, 0x7fff);
  if (result && m_forceTraining != value)
  {
    const int oldValue = m_forceTraining;
    m_forceTraining = static_cast<int16_t>(value);
    ViewAsCharacter(kAllianceView)->ApplyForceTraining(ctx);
    ViewAsCharacter(kEmpireView)->ApplyForceTraining(ctx);
    OnForceTrainingChanged(oldValue, m_forceTraining, ctx);
  }
  return result;
}

int Character::SetLeadershipAdjustment(int value, NotifyContext* ctx)
{
  int result = IsMasterView();
  if (result)
    Clamp(value, 0, 0x7fff);
  if (result && m_leadershipAdjustment != value)
  {
    const int oldValue = m_leadershipAdjustment;
    m_leadershipAdjustment = static_cast<int16_t>(value);
    ViewAsCharacter(kAllianceView)->ApplyLeadershipAdjustment(ctx);
    ViewAsCharacter(kEmpireView)->ApplyLeadershipAdjustment(ctx);
    OnLeadershipAdjustmentChanged(oldValue, m_leadershipAdjustment, ctx);
  }
  return result;
}

int Character::SetInjury(int value, NotifyContext* ctx)
{
  int result = IsMasterView();
  if (result)
    Clamp(value, 0, 0x7fff);
  if (result && m_injury != value)
  {
    const int oldValue = m_injury;
    m_injury = static_cast<int16_t>(value);
    ViewAsCharacter(kAllianceView)->ApplyInjury(ctx);
    ViewAsCharacter(kEmpireView)->ApplyInjury(ctx);
    OnInjuryChanged(oldValue, m_injury, ctx);
  }
  return result;
}

int Character::SetCommandKind(int value, NotifyContext* ctx)
{
  int result = IsMasterView();
  int valid = 0;
  if (result)
    valid = IsInRange(value, 0, value, "Invalid CommandKind value!");
  if (valid && m_commandKind != value)
  {
    const int oldValue = m_commandKind;
    m_commandKind = static_cast<int16_t>(value);
    ViewAsCharacter(kAllianceView)->ApplyCommandKind(ctx);
    ViewAsCharacter(kEmpireView)->ApplyCommandKind(ctx);
    OnCommandKindChanged(oldValue, m_commandKind, ctx);
  }
  return valid;
}

int Character::SetState(int value, NotifyContext* ctx)
{
  int result = IsMasterView();
  int valid = 0;
  if (result)
    valid = IsInRange(value, 0, 4, "Invalid State value!");
  if (valid && m_state != value)
  {
    const int oldValue = m_state;
    m_state = static_cast<int16_t>(value);
    ViewAsCharacter(kAllianceView)->ApplyState(ctx);
    ViewAsCharacter(kEmpireView)->ApplyState(ctx);
    OnStateChanged(oldValue, m_state, ctx);
  }
  return valid;
}

int Character::SetMissionHyperdriveModifier(int value, NotifyContext* ctx)
{
  int result = IsMasterView();
  int valid = 0;
  if (result)
    valid = IsInRange(value, 0, value, "Invalid MissionHyperdriveModifier value!");
  if (valid && m_missionHyperdriveModifier != value)
  {
    const int oldValue = m_missionHyperdriveModifier;
    m_missionHyperdriveModifier = static_cast<int16_t>(value);
    ViewAsCharacter(kAllianceView)->ApplyMissionHyperdriveModifier(ctx);
    ViewAsCharacter(kEmpireView)->ApplyMissionHyperdriveModifier(ctx);
    OnMissionHyperdriveModifierChanged(oldValue, m_missionHyperdriveModifier, ctx);
  }
  return valid;
}

int Character::SetEncounterKey(ObjectKey* value, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && m_encounterKey.m_value != value->m_value)
  {
    ObjectKey oldKey(m_encounterKey);
    m_encounterKey = *value;
    ViewAsCharacter(kAllianceView)->ApplyEncounterKey(ctx);
    ViewAsCharacter(kEmpireView)->ApplyEncounterKey(ctx);
    OnEncounterChanged(&oldKey, &m_encounterKey, ctx);
  }
  return result;
}

int Character::SetTraitorDiscoveredKey(ObjectKey* value, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && m_traitorDiscoveredKey.m_value != value->m_value)
  {
    ObjectKey oldKey(m_traitorDiscoveredKey);
    m_traitorDiscoveredKey = *value;
    ViewAsCharacter(kAllianceView)->ApplyTraitorDiscoveredKey(ctx);
    ViewAsCharacter(kEmpireView)->ApplyTraitorDiscoveredKey(ctx);
    OnTraitorDiscoveredChanged(&oldKey, &m_traitorDiscoveredKey, ctx);
  }
  return result;
}

int Character::SetForceUserDiscoveredKey(ObjectKey* value, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && m_forceUserDiscoveredKey.m_value != value->m_value)
  {
    ObjectKey oldKey(m_forceUserDiscoveredKey);
    m_forceUserDiscoveredKey = *value;
    ViewAsCharacter(kAllianceView)->ApplyForceUserDiscoveredKey(ctx);
    ViewAsCharacter(kEmpireView)->ApplyForceUserDiscoveredKey(ctx);
    OnForceUserDiscoveredChanged(&oldKey, &m_forceUserDiscoveredKey, ctx);
  }
  return result;
}

int Character::SetCommandingKey(ObjectKey* value, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && m_commandingKey.m_value != value->m_value)
  {
    ObjectKey oldKey(m_commandingKey);
    m_commandingKey = *value;
    ViewAsCharacter(kAllianceView)->ApplyCommandingKey(ctx);
    ViewAsCharacter(kEmpireView)->ApplyCommandingKey(ctx);
    OnCommandingChanged(&oldKey, &m_commandingKey, ctx);
  }
  return result;
}

int Character::SetCaptured(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(kCaptured, on, &m_flagsAc))
  {
    ViewAsCharacter(kAllianceView)->ApplyCaptured(ctx);
    ViewAsCharacter(kEmpireView)->ApplyCaptured(ctx);
    OnCapturedChanged(on, ctx);
  }
  return result;
}

int Character::SetCanHeal(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(kCanHeal, on, &m_flagsAc))
  {
    ViewAsCharacter(kAllianceView)->ApplyCanHeal(ctx);
    ViewAsCharacter(kEmpireView)->ApplyCanHeal(ctx);
    OnCanHealChanged(on, ctx);
  }
  return result;
}

int Character::SetFastHeal(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(kFastHeal, on, &m_flagsAc))
  {
    ViewAsCharacter(kAllianceView)->ApplyFastHeal(ctx);
    ViewAsCharacter(kEmpireView)->ApplyFastHeal(ctx);
    OnFastHealChanged(on, ctx);
  }
  return result;
}

int Character::SetForceAware(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(kForceAware, on, &m_flagsAc))
  {
    ViewAsCharacter(kAllianceView)->ApplyForceAware(ctx);
    ViewAsCharacter(kEmpireView)->ApplyForceAware(ctx);
    OnForceAwareChanged(on, ctx);
  }
  return result;
}

int Character::SetForcePotential(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(kForcePotential, on, &m_flagsAc))
  {
    ViewAsCharacter(kAllianceView)->ApplyForcePotential(ctx);
    ViewAsCharacter(kEmpireView)->ApplyForcePotential(ctx);
    OnForcePotentialChanged(on, ctx);
  }
  return result;
}

int Character::SetHealing(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(kHealing, on, &m_flagsAc))
  {
    ViewAsCharacter(kAllianceView)->ApplyHealing(ctx);
    ViewAsCharacter(kEmpireView)->ApplyHealing(ctx);
    OnHealingChanged(on, ctx);
  }
  return result;
}

int Character::SetDiscoveringForceUser(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(kDiscoveringForceUser, on, &m_flagsAc))
  {
    ViewAsCharacter(kAllianceView)->ApplyDiscoveringForceUser(ctx);
    ViewAsCharacter(kEmpireView)->ApplyDiscoveringForceUser(ctx);
    OnDiscoveringForceUserChanged(on, ctx);
  }
  return result;
}

int Character::SetCanEscape(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(kCanEscape, on, &m_flagsAc))
  {
    ViewAsCharacter(kAllianceView)->ApplyCanEscape(ctx);
    ViewAsCharacter(kEmpireView)->ApplyCanEscape(ctx);
    OnCanEscapeChanged(on, ctx);
  }
  return result;
}

int Character::SetEscapeRequest(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(kEscapeRequest, on, &m_flagsAc))
  {
    ViewAsCharacter(kAllianceView)->ApplyEscapeRequest(ctx);
    ViewAsCharacter(kEmpireView)->ApplyEscapeRequest(ctx);
    OnEscapeRequestChanged(on, ctx);
  }
  return result;
}

int Character::SetEscapeAttempt(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(kEscapeAttempt, on, &m_flagsAc))
  {
    ViewAsCharacter(kAllianceView)->ApplyEscapeAttempt(ctx);
    ViewAsCharacter(kEmpireView)->ApplyEscapeAttempt(ctx);
    OnEscapeAttemptChanged(on, ctx);
  }
  return result;
}
