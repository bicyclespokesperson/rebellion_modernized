#include <new>

#include "character/character.h"
#include "data/game_math.h"
#include "data/gnprtb.h"
#include "data/random.h"
#include "data/sd_records.h"

// Derived values of the Character properties: each Update* recomputes a property from the others and runs its validated
// setter (which does nothing if the value did not change).

namespace
{

constexpr uint32_t kCapturedFlag = 0x1;
constexpr uint32_t kCanHealFlag = 0x2;
constexpr uint32_t kFastHealFlag = 0x4;
constexpr uint32_t kForceAwareFlag = 0x10;
constexpr uint32_t kForcePotentialFlag = 0x20;
constexpr uint32_t kEscapeRequestFlag = 0x200;
constexpr uint32_t kOnMissionFlag = 0x80;
constexpr uint32_t kUsableState = 0x1;
constexpr uint32_t kEnrouteState = 0x10;
constexpr uint32_t kExistingState = 0x40;
constexpr uint32_t kDestroyedState = 0x8;
constexpr uint32_t kControlKindMask = 0xc0;
constexpr int kBothSides = 3;
constexpr uint32_t kFirstMissionFamily = 0x50;
constexpr uint32_t kEndMissionFamily = 0x80;

bool isMissionFamily(const ObjectKey& key)
{
  return key.Family() >= kFirstMissionFamily && key.Family() < kEndMissionFamily;
}

} // namespace

ObjectKey* Character::GetEncounterKey(ObjectKey* out)
{
  new (out) ObjectKey(m_encounterKey);
  return out;
}

ObjectKey* Character::GetTraitorDiscoveredKey(ObjectKey* out)
{
  new (out) ObjectKey(m_traitorDiscoveredKey);
  return out;
}

ObjectKey* Character::GetForceUserDiscoveredKey(ObjectKey* out)
{
  new (out) ObjectKey(m_forceUserDiscoveredKey);
  return out;
}

ObjectKey* Character::GetCommandingKey(ObjectKey* out)
{
  new (out) ObjectKey(m_commandingKey);
  return out;
}

int Character::SetHolderKey(const ObjectKey* key)
{
  const int result = IsMasterView();
  static_cast<CharacterData*>(m_data)->m_holderKey = *key;
  return result;
}

void Character::SetDataField34(int value)
{
  IsMasterView();
  static_cast<CharacterData*>(m_data)->m_field34 = value;
}

int Character::UpdateControlKind(NotifyContext* ctx)
{
  int side = GetRecordSide();
  if ((m_flagsAc & kCapturedFlag) && side != kBothSides)
    side = side != 1 ? 1 : 2;
  return SetControlKind(side, ctx);
}

int Character::UpdateEnhancedDiplomacy(NotifyContext* ctx)
{
  int value = m_baseDiplomacy;
  int result = ComputeSkillWithForce(value, m_force, &value);
  if (result)
    result = SetEnhancedDiplomacy(value, ctx);
  return result;
}

int Character::UpdateEnhancedEspionage(NotifyContext* ctx)
{
  int value = m_baseEspionage;
  int result = ComputeSkillWithForce(value, m_force, &value);
  if (result)
    result = SetEnhancedEspionage(value, ctx);
  return result;
}

int Character::UpdateEnhancedShipyardRD(NotifyContext* ctx)
{
  return SetEnhancedShipyardRD(m_baseShipyardRD, ctx);
}

int Character::UpdateEnhancedTrainingFacilRD(NotifyContext* ctx)
{
  return SetEnhancedTrainingFacilRD(m_baseTrainingFacilRD, ctx);
}

int Character::UpdateEnhancedConstructionYardRD(NotifyContext* ctx)
{
  return SetEnhancedConstructionYardRD(m_baseConstructionYardRD, ctx);
}

int Character::UpdateEnhancedCombat(NotifyContext* ctx)
{
  int value = m_baseCombat;
  int result = ComputeSkillWithForceAndInjury(value, m_force, m_injury, &value);
  if (result)
    result = SetEnhancedCombat(value, ctx);
  return result;
}

int Character::UpdateEnhancedLeadership(NotifyContext* ctx)
{
  int value = m_baseLeadership;
  int result = ComputeSkillWithForce(value, m_leadershipAdjustment, &value);
  if (result)
    result = SetEnhancedLeadership(value, ctx);
  return result;
}

int Character::UpdateEnhancedLoyalty(NotifyContext* ctx)
{
  return SetEnhancedLoyalty(m_baseLoyalty, ctx);
}

int Character::UpdateForce(NotifyContext* ctx)
{
  return SetForce(m_forceExperience + m_forceTraining, ctx);
}

int Character::UpdateForceExperience(NotifyContext* ctx)
{
  int value = m_forceExperience;
  if (value == 0 && (m_flagsAc & kForcePotentialFlag) && (m_flagsAc & kForceAwareFlag))
  {
    SdRecordExt25* record = GetCharacterRecord();
    if (!record)
      return 0;
    const int base = record->m_moreFields[2];
    value = GameRandom(record->m_moreFields[3]) + base;
  }
  return SetForceExperience(value, ctx);
}

int Character::UpdateCommandKind(NotifyContext* ctx)
{
  int result = 1;
  if (m_commandKind == 0)
    return result;
  bool clear = true;
  if (m_injury == 0 && !(m_flagsAc & kCapturedFlag) && (m_stateFlags & kExistingState))
    clear = isMissionFamily(m_missionKey);
  if (clear)
    result = SetCommandKind(0, ctx);
  return result;
}

int Character::UpdateCaptured(NotifyContext* ctx)
{
  return SetCaptured(m_state == 0, ctx);
}

int Character::UpdateCanHeal(NotifyContext* ctx)
{
  GameObjNotifier* parent = nullptr;
  int result = GetParent(&parent);
  if (result)
  {
    const bool blocked = ((m_flags ^ parent->m_flags) & kControlKindMask) != 0 || (m_flagsAc & kCapturedFlag) ||
                         (m_stateFlags & kDestroyedState);
    result = SetCanHeal(blocked ? 0 : 1, ctx);
  }
  return result;
}

int Character::UpdateFastHeal(NotifyContext* ctx)
{
  int fastHeal = 0;
  int result = HasForcePotentialA00(m_force, &fastHeal);
  if (result)
    result = SetFastHeal(fastHeal, ctx);
  return result;
}

int Character::UpdateHealing(NotifyContext* ctx)
{
  const bool healing = (m_flagsAc & kCanHealFlag) && !(m_stateFlags & kEnrouteState) && m_injury != 0;
  return SetHealing(healing ? 1 : 0, ctx);
}

int Character::UpdateDiscoveringForceUser(NotifyContext* ctx)
{
  int discovering = 0;
  if (RecordIsJediTrainer() && HasForcePotentialA14(m_force) && !(m_flags78 & kOnMissionFlag) &&
      (m_stateFlags & kUsableState) && !(m_flagsAc & kCapturedFlag))
    discovering = 1;
  return SetDiscoveringForceUser(discovering, ctx);
}

int Character::RaiseEscapeRequest(NotifyContext* ctx)
{
  int result = 1;
  const bool ready = (m_flagsAc & kCapturedFlag) && (m_stateFlags & kUsableState) && !(m_flags78 & kOnMissionFlag) &&
                     (m_flagsAc & kEscapeRequestFlag);
  if (ready)
    result = Virtual133(ctx);
  return result;
}

int Character::RecoverFromInjury(NotifyContext* ctx)
{
  if (m_injury == 0)
    return 0;
  const int recovery = (m_flagsAc & kFastHealFlag) ? g_gnprtbA03 : g_gnprtbA04;
  return SetInjury(ClampNegativeToZero(m_injury - recovery), ctx);
}

int Character::PulseEscapeAttempt(NotifyContext* ctx)
{
  int result = (m_flagsAc >> 8) & 1;
  if (result)
    result = SetEscapeAttempt(1, ctx);
  if (result)
    result = SetEscapeAttempt(0, ctx);
  return result;
}

int Character::AdjustLoyaltyForSide(int amount, int side, NotifyContext* ctx)
{
  int result = side >= 1 && side <= kBothSides;
  if (result)
  {
    result = side != kBothSides;
    if (result && !RecordIsUnableToBetray())
    {
      if (GetRecordSide() != side)
        amount = -amount;
      result = SetBaseLoyalty(ClampPercent(m_baseLoyalty + amount), ctx);
    }
  }
  return result;
}

const int g_percentMax = 100;
const int g_percentMin{};
