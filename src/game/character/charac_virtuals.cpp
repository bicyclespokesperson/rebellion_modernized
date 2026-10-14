#include "character/character.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "data/gnprtb.h"
#include "mission/mission_forwarders.h"

namespace
{

constexpr uint32_t kOnMissionFlag = 0x80;
constexpr uint32_t kDestroyedState = 0x8;
constexpr uint32_t kControlKindMask = 0xc0;
constexpr uint32_t kCapturedFlag = 0x1;
constexpr uint32_t kFastHealFlag = 0x4;
constexpr uint32_t kReasonAdrift = 0x14;
constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kSlowInjuryDestroyedReason = 10;
constexpr int kDeathInjuryDestroyedReason = 7;
constexpr int kStateCaptive = 0;
constexpr int kStateFree = 1;
constexpr int kStateSecond = 2;
constexpr int kStateThird = 3;

} // namespace

// ---- slots 44-48, 117 ----------------------------------------------------------------------------------------

int Character::InitializeNewObject(intptr_t a)
{
  return Role::InitializeNewObject(a);
}

int Character::InitializeAfterSetup(intptr_t a)
{
  return Role::InitializeNewObject(a);
}

int Character::HandleControlMismatch(NotifyContext* ctx)
{
  int result = 1;
  if (!(m_flags78 & kOnMissionFlag) && !(m_stateFlags & kDestroyedState))
  {
    GameObjNotifier* parent = nullptr;
    result = GetParent(&parent) != 0;
    if (parent && ((parent->m_flags ^ m_flags) & kControlKindMask) != 0)
    {
      if (m_flagsAc & kCapturedFlag)
        result = SetUncapturedState(ctx);
      if (((parent->m_flags ^ m_flags) & kControlKindMask) != 0)
        result = SetAutorouting(1, ctx);
    }
  }
  return result;
}

int Character::HandleDestroyedParent(int a, NotifyContext* ctx)
{
  int result = 1;
  if (!(m_flags78 & kOnMissionFlag) && !(m_stateFlags & kDestroyedState))
  {
    GameObjNotifier* parent = nullptr;
    GetParent(&parent);
    if (parent && static_cast<char>(static_cast<GameObj*>(parent)->m_packed) == static_cast<char>(kReasonAdrift))
      return SetAutorouting(1, ctx);
    result = SetAdrift(1, ctx);
  }
  return result;
}

// ---- slots 130-134: control-kind and state transitions -----------------------------------------------------

int Character::HandleMissionResignationRequest(NotifyContext* ctx)
{
  int result = Role::HandleMissionResignationRequest(ctx);
  if (result)
    result = RollAndApplyInjury(ctx);
  return result;
}

int Character::HandleCaptureByOpponent(const ObjectKey* opponent, NotifyContext* ctx)
{
  int ok = ReturnOneTwoArgs(reinterpret_cast<intptr_t>(opponent), ctx) != 0;
  if (!(m_stateFlags & kDestroyedState))
  {
    ok = RollAndApplyInjury(ctx) && ok;
    if (!(m_stateFlags & kDestroyedState))
    {
      ok = SetState(kStateCaptive, ctx) && ok;
      ok = SetHolderKey(opponent) && ok;
    }
  }
  return ok;
}

int Character::AssignHolderAndState(const ObjectKey* holder, NotifyContext* ctx)
{
  const int stub = ReturnOneTwoArgs(reinterpret_cast<intptr_t>(holder), ctx);
  int ok = SetState(kStateThird, ctx) && stub;
  ok = SetHolderKey(holder) && ok;
  return ok;
}

int Character::EscapeCaptivity(NotifyContext* ctx)
{
  return SetState(kStateFree, ctx) ? 1 : 0;
}

int Character::SetUncapturedState(NotifyContext* ctx)
{
  return SetState(kStateSecond, ctx) ? 1 : 0;
}

// ---- slots 185-190: injuries and commands ------------------------------------------------------------------

int Character::RollAndApplyInjury(NotifyContext* ctx)
{
  int duration = 0;
  if (!RollInjuryDuration(GetCombat(), &duration))
    return 0;
  return ApplyInjury(duration, kSlowInjuryDestroyedReason, ctx);
}

int Character::RollAndApplyShipLossInjury(NotifyContext* ctx)
{
  int duration = 0;
  if (!RollInjuryDurationB(GetCombat(), &duration))
    return 0;
  return ApplyInjury(duration, kSlowInjuryDestroyedReason, ctx);
}

int Character::RollAndApplyFatalInjury(NotifyContext* ctx)
{
  int duration = 0;
  if (!RollDurationUnconditional(&duration))
    return 0;
  return ApplyInjury(duration, kDeathInjuryDestroyedReason, ctx);
}

int Character::ApplyInjury(int duration, int destroyedReason, NotifyContext* ctx)
{
  int result = duration >= 0;
  if (result && duration != 0)
  {
    result = SetInjury(m_injury + duration, ctx) != 0;
    if (ShouldDestroyFromInjury())
    {
      const int reasonChanged = ChangeDestroyedReason(destroyedReason, ctx);
      result = reasonChanged && result ? 1 : 0;
    }
  }
  return result;
}

int Character::ApplyCommandRequest(const ActivityRequest* request, const ObjectKey* holder, NotifyContext* ctx)
{
  StatusCode status(kStatusCategory, kStatusOk);
  int result = CheckActivityStatus3(request, &status);
  if (result && status.m_second == static_cast<uint32_t>(kStatusOk))
  {
    int index = 0;
    const int mapped = MapTypeCodeToIndex(request->m_kind, &index);
    result = 0;
    if (mapped)
      result = SetCommandKindForRequest(index, holder, ctx);
  }
  return result;
}

int Character::SetCommandKindForRequest(int a, const ObjectKey* holder, NotifyContext* ctx)
{
  if (m_commandKind == a)
    return SetCommandKind(0, ctx);
  int result = SetCommandKind(a, ctx);
  if (m_commandKind != 0)
    result = SetCommandingKey(const_cast<ObjectKey*>(holder), ctx);
  return result;
}

// ---- creation and persistence --------------------------------------------------------------------------------

int Character::CreateViewsImpl(int controlKind, NotifyContext* ctx)
{
  int result = GameObj::CreateViews(controlKind, ctx);
  if (result)
  {
    auto* data = static_cast<CharacterData*>(m_data);
    data->m_healingStat.SetLevel((m_flagsAc & kFastHealFlag) ? g_gnprtbA01 : g_gnprtbA02);
    data->m_escapeStat.SetLevel(g_gnprtbA18);
    data->m_escapeStat.SetProgress(g_gnprtbA19);
    data->m_forceDiscoveryStat.SetLevel(g_gnprtbA16);
    data->m_forceDiscoveryStat.SetProgress(g_gnprtbA17);
    result = UpdateDiscoveringForceUser(ctx);
  }
  return result;
}

int Character::Read(istream* in)
{
  const int result = Role::Read(in);
  if (result)
  {
    ReadValue16(in, &m_enhancedDiplomacy);
    ReadValue16(in, &m_enhancedEspionage);
    ReadValue16(in, &m_enhancedShipyardRD);
    ReadValue16(in, &m_enhancedTrainingFacilRD);
    ReadValue16(in, &m_enhancedConstructionYardRD);
    ReadValue16(in, &m_enhancedCombat);
    ReadValue16(in, &m_enhancedLeadership);
    ReadValue16(in, &m_enhancedLoyalty);
    ReadValue16(in, &m_force);
    ReadValue16(in, &m_forceExperience);
    ReadValue16(in, &m_forceTraining);
    ReadValue16(in, &m_leadershipAdjustment);
    ReadValue16(in, &m_injury);
    ReadValue16(in, &m_commandKind);
    ReadValue16(in, &m_state);
    ReadValue16(in, &m_missionHyperdriveModifier);
    m_encounterKey.Read(in);
    m_traitorDiscoveredKey.Read(in);
    m_forceUserDiscoveredKey.Read(in);
    m_commandingKey.Read(in);
    ReadValue32(in, &m_flagsAc);
  }
  return result;
}

int Character::Write(ostream* out)
{
  const int result = Role::Write(out);
  if (result)
  {
    WriteValue16(out, &m_enhancedDiplomacy);
    WriteValue16(out, &m_enhancedEspionage);
    WriteValue16(out, &m_enhancedShipyardRD);
    WriteValue16(out, &m_enhancedTrainingFacilRD);
    WriteValue16(out, &m_enhancedConstructionYardRD);
    WriteValue16(out, &m_enhancedCombat);
    WriteValue16(out, &m_enhancedLeadership);
    WriteValue16(out, &m_enhancedLoyalty);
    WriteValue16(out, &m_force);
    WriteValue16(out, &m_forceExperience);
    WriteValue16(out, &m_forceTraining);
    WriteValue16(out, &m_leadershipAdjustment);
    WriteValue16(out, &m_injury);
    WriteValue16(out, &m_commandKind);
    WriteValue16(out, &m_state);
    WriteValue16(out, &m_missionHyperdriveModifier);
    m_encounterKey.Write(out);
    m_traitorDiscoveredKey.Write(out);
    m_forceUserDiscoveredKey.Write(out);
    m_commandingKey.Write(out);
    WriteValue32(out, &m_flagsAc);
  }
  return result;
}

int Character::ApplyAll(NotifyContext* ctx)
{
  const int result = Role::ApplyAll(ctx);
  if (result && ctx->m_flag04 == 1)
  {
    ApplyEnhancedDiplomacy(ctx);
    ApplyEnhancedEspionage(ctx);
    ApplyEnhancedShipyardRD(ctx);
    ApplyEnhancedTrainingFacilRD(ctx);
    ApplyEnhancedConstructionYardRD(ctx);
    ApplyEnhancedCombat(ctx);
    ApplyEnhancedLeadership(ctx);
    ApplyEnhancedLoyalty(ctx);
    ApplyForce(ctx);
    ApplyForceExperience(ctx);
    ApplyForceTraining(ctx);
    ApplyLeadershipAdjustment(ctx);
    ApplyInjury(ctx);
    ApplyCommandKind(ctx);
    ApplyState(ctx);
    ApplyMissionHyperdriveModifier(ctx);
    ApplyEncounterKey(ctx);
    ApplyTraitorDiscoveredKey(ctx);
    ApplyForceUserDiscoveredKey(ctx);
    ApplyCommandingKey(ctx);
    ApplyCaptured(ctx);
    ApplyCanHeal(ctx);
    ApplyFastHeal(ctx);
    ApplyForceAware(ctx);
    ApplyForcePotential(ctx);
    ApplyHealing(ctx);
    ApplyDiscoveringForceUser(ctx);
    ApplyCanEscape(ctx);
    ApplyEscapeRequest(ctx);
    ApplyEscapeAttempt(ctx);
  }
  return result;
}

int Character::Checksum(Crc32* crc)
{
  int result = Role::Checksum(crc);
  if (result)
  {
    crc->AddShort(m_enhancedDiplomacy);
    crc->AddShort(m_enhancedEspionage);
    crc->AddShort(m_enhancedShipyardRD);
    crc->AddShort(m_enhancedTrainingFacilRD);
    crc->AddShort(m_enhancedConstructionYardRD);
    crc->AddShort(m_enhancedCombat);
    crc->AddShort(m_enhancedLeadership);
    crc->AddShort(m_enhancedLoyalty);
    crc->AddShort(m_force);
    crc->AddShort(m_forceExperience);
    crc->AddShort(m_forceTraining);
    crc->AddShort(m_leadershipAdjustment);
    crc->AddShort(m_injury);
    crc->AddShort(m_commandKind);
    crc->AddShort(m_state);
    crc->AddShort(m_missionHyperdriveModifier);
    result = m_encounterKey.Checksum(crc) && m_traitorDiscoveredKey.Checksum(crc) &&
             m_forceUserDiscoveredKey.Checksum(crc) && m_commandingKey.Checksum(crc);
    if (result)
      crc->AddInt(m_flagsAc);
  }
  return result;
}

int Character::Validate()
{
  bool valid = false;
  if (Role::Validate())
  {
    if (!m_encounterKey.IsSet())
    {
      ObjectKey holder;
      GetHolderKey(&holder);
      valid = !holder.IsSet();
    }
  }
  return valid ? 1 : 0;
}
