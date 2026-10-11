#include "events/event_post.h"
#include "mission/mission.h"
#include "mission/mission_hub.h"

namespace
{

constexpr int kAllianceView = 1;
constexpr int kEmpireView = 2;
constexpr int kTaskStatusMax = 3;
constexpr int kCompletionStatusMin = -1;
constexpr int kCompletionStatusMax = 0x10;

} // namespace

int Mission::SetUserId(int value, NotifyContext* ctx)
{
  const int master = IsMasterView();
  int valid = 0;
  if (master)
    valid = IsInRange(value, 0, value, "Invalid UserID value!");
  if (valid && m_userId != value)
  {
    const int oldValue = m_userId;
    m_userId = value;
    ViewAsMission(kAllianceView)->ApplyUserId(ctx);
    ViewAsMission(kEmpireView)->ApplyUserId(ctx);
    OnUserIdChanged(oldValue, m_userId, ctx);
  }
  return valid;
}

int Mission::SetUserId2(int value, NotifyContext* ctx)
{
  const int master = IsMasterView();
  int valid = 0;
  if (master)
    valid = IsInRange(value, 0, value, "Invalid UserID2 value!");
  if (valid && m_userId2 != value)
  {
    const int oldValue = m_userId2;
    m_userId2 = value;
    ViewAsMission(kAllianceView)->ApplyUserId2(ctx);
    ViewAsMission(kEmpireView)->ApplyUserId2(ctx);
    OnUserId2Changed(oldValue, m_userId2, ctx);
  }
  return valid;
}

int Mission::SetTaskStatus(int value, NotifyContext* ctx)
{
  const int master = IsMasterView();
  int valid = 0;
  if (master)
    valid = IsInRange(value, 0, kTaskStatusMax, "Invalid TaskStatus value!");
  if (valid && m_taskStatus != value)
  {
    const int oldValue = m_taskStatus;
    m_taskStatus = value;
    ViewAsMission(kAllianceView)->ApplyTaskStatus(ctx);
    ViewAsMission(kEmpireView)->ApplyTaskStatus(ctx);
    OnTaskStatusChanged(oldValue, m_taskStatus, ctx);
  }
  return valid;
}

int Mission::SetCompletionStatus(int value, NotifyContext* ctx)
{
  const int master = IsMasterView();
  int valid = 0;
  if (master)
    valid = IsInRange(value, kCompletionStatusMin, kCompletionStatusMax, "Invalid CompletionStatus value!");
  if (valid && m_completionStatus != value)
  {
    const int oldValue = m_completionStatus;
    m_completionStatus = value;
    ViewAsMission(kAllianceView)->ApplyCompletionStatus(ctx);
    ViewAsMission(kEmpireView)->ApplyCompletionStatus(ctx);
    OnCompletionStatusChanged(oldValue, m_completionStatus, ctx);
  }
  return valid;
}

int Mission::SetPhase(int value, NotifyContext* ctx)
{
  const int master = IsMasterView();
  int valid = 0;
  if (master)
    valid = IsInRange(value, mission_phase::kMin, mission_phase::kMax, "Invalid Phase value!");
  if (valid && m_phase != value)
  {
    const int oldValue = m_phase;
    m_phase = value;
    ViewAsMission(kAllianceView)->ApplyPhase(ctx);
    ViewAsMission(kEmpireView)->ApplyPhase(ctx);
    OnPhaseChanged(oldValue, m_phase, ctx);
  }
  return valid;
}

#define REB_MISSION_KEY_SETTER(Name, field)                  \
  int Mission::Set##Name(ObjectKey* key, NotifyContext* ctx) \
  {                                                          \
    const int master = IsMasterView();                       \
    if (master && field.m_value != key->m_value)             \
    {                                                        \
      ObjectKey oldKey(field);                               \
      field = *key;                                          \
      ViewAsMission(kAllianceView)->Apply##Name(ctx);        \
      ViewAsMission(kEmpireView)->Apply##Name(ctx);          \
      On##Name##Changed(&oldKey, &field, ctx);               \
    }                                                        \
    return master;                                           \
  }

REB_MISSION_KEY_SETTER(OriginLocationKey, m_originLocationKey)
REB_MISSION_KEY_SETTER(ObjectiveKey, m_objectiveKey)
REB_MISSION_KEY_SETTER(TargetKey, m_targetKey)
REB_MISSION_KEY_SETTER(TargetLocationKey, m_targetLocationKey)
REB_MISSION_KEY_SETTER(LeaderKey, m_leaderKey)
#undef REB_MISSION_KEY_SETTER

int Mission::SetLeaderSeedKey(NullableKey* key, NotifyContext* ctx)
{
  const int master = IsMasterView();
  if (master && m_leaderSeedKey.m_value != key->m_value)
  {
    NullableKey oldKey = m_leaderSeedKey;
    m_leaderSeedKey = *key;
    ViewAsMission(kAllianceView)->ApplyLeaderSeedKey(ctx);
    ViewAsMission(kEmpireView)->ApplyLeaderSeedKey(ctx);
    OnLeaderSeedKeyChanged(&oldKey, &m_leaderSeedKey, ctx);
  }
  return master;
}

#define REB_MISSION_SET_SETTER(Name, field)                          \
  int Mission::Set##Name(ObjectKeyValueSet* set, NotifyContext* ctx) \
  {                                                                  \
    const int master = IsMasterView();                               \
    if (master && field != *set)                                     \
    {                                                                \
      ObjectKeyValueSet oldSet(field);                               \
      field = *set;                                                  \
      ViewAsMission(kAllianceView)->Apply##Name(ctx);                \
      ViewAsMission(kEmpireView)->Apply##Name(ctx);                  \
      On##Name##Changed(&oldSet, &field, ctx);                       \
    }                                                                \
    return master;                                                   \
  }

REB_MISSION_SET_SETTER(Team, m_team)
REB_MISSION_SET_SETTER(Decoys, m_decoys)
REB_MISSION_SET_SETTER(Captured, m_captured)
REB_MISSION_SET_SETTER(FinishedMembers, m_finishedMembers)
#undef REB_MISSION_SET_SETTER

#define REB_MISSION_FLAG_SETTER(Name, mask)               \
  int Mission::Set##Name(int value, NotifyContext* ctx)   \
  {                                                       \
    const int master = IsMasterView();                    \
    if (master && UpdateFlagBit(mask, value, &m_flagsA4)) \
    {                                                     \
      ViewAsMission(kAllianceView)->Apply##Name(ctx);     \
      ViewAsMission(kEmpireView)->Apply##Name(ctx);       \
      On##Name##Changed(value, ctx);                      \
    }                                                     \
    return master;                                        \
  }

REB_MISSION_FLAG_SETTER(ReadyForNextPhase, mission_flag::kReadyForNextPhase)
REB_MISSION_FLAG_SETTER(ImpliedTeam, mission_flag::kImpliedTeam)
REB_MISSION_FLAG_SETTER(Mandatory, mission_flag::kMandatory)
#undef REB_MISSION_FLAG_SETTER
