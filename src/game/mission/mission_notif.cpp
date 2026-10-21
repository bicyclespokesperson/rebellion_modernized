#include "core/game_string.h"
#include "gameobj/object_database.h"
#include "mission/mission.h"
#include "mission/mission_hub.h"

namespace
{

constexpr int kEventTaskStatus = 0x1f0;
constexpr int kEventCompletionStatus = 0x1f1;

int logSetChange(Mission* self, const char* tag, const char* property, ObjectKeyValueSet* oldSet,
                 ObjectKeyValueSet* newSet, NotifyContext* ctx)
{
  const int result = self->IsSideView();
  if (result) {
    std::string newText;
    std::string oldText;
    DescribeKeyValueSet(newSet, &newText);
    DescribeKeyValueSet(oldSet, &oldText);
    self->LogChange(tag, property, newText.c_str(), oldText.c_str(), nullptr, ctx);
  }
  return result;
}

} // namespace

int Mission::OnUserIdNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogInt("(MissionUserIDNotif)", "UserID", newValue, oldValue, 0, ctx);
  }
  return result;
}

int Mission::OnUserId2Notified(int oldValue, int newValue, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogInt("(MissionUserID2Notif)", "UserID2", newValue, oldValue, 0, ctx);
  }
  return result;
}

int Mission::OnTaskStatusNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  if (!IsSideView()) {
    return 0;
  }
  return RaiseValueChangeEvent(kEventTaskStatus, this, oldValue, newValue, ctx);
}

int Mission::OnCompletionStatusNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  if (!IsSideView()) {
    return 0;
  }
  return RaiseValueChangeEvent(kEventCompletionStatus, this, oldValue, newValue, ctx);
}

int Mission::OnOriginLocationKeyNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogKey("(MissionOriginLocationKeyNotif)", "OriginLocation", newKey, oldKey, 0, ctx);
  }
  return result;
}

int Mission::OnObjectiveKeyNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogKey("(MissionObjectiveKeyNotif)", "Objective", newKey, oldKey, 0, ctx);
  }
  return result;
}

int Mission::OnTargetKeyNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogKey("(MissionTargetKeyNotif)", "Target (parent)", newKey, oldKey, 0, ctx);
  }
  return result;
}

int Mission::OnTargetLocationKeyNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogKey("(MissionTargetLocationKeyNotif)", "TargetLocation", newKey, oldKey, 0, ctx);
  }
  return result;
}

int Mission::OnLeaderKeyNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogKey("(MissionLeaderKeyNotif)", "Leader", newKey, oldKey, 0, ctx);
  }
  return result;
}

int Mission::OnLeaderSeedKeyNotified(NullableKey* oldKey, NullableKey* newKey, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    ObjectKey newObjectKey;
    ObjectKey oldObjectKey;
    newObjectKey.m_value = newKey->m_value;
    oldObjectKey.m_value = oldKey->m_value;
    LogTypeKey("(MissionLeaderSeedKeyNotif)", "Leader", &newObjectKey, &oldObjectKey, 0, ctx);
  }
  return result;
}

int Mission::OnTeamNotified(ObjectKeyValueSet* oldSet, ObjectKeyValueSet* newSet, NotifyContext* ctx)
{
  return logSetChange(this, "(MissionTeamSelectListNotif)", "Team", oldSet, newSet, ctx);
}

int Mission::OnDecoysNotified(ObjectKeyValueSet* oldSet, ObjectKeyValueSet* newSet, NotifyContext* ctx)
{
  return logSetChange(this, "(MissionDecoySelectListNotif)", "Decoys", oldSet, newSet, ctx);
}

int Mission::OnCapturedNotified(ObjectKeyValueSet* oldSet, ObjectKeyValueSet* newSet, NotifyContext* ctx)
{
  return logSetChange(this, "(MissionCapturedSelectListNotif)", "Captives", oldSet, newSet, ctx);
}

int Mission::OnFinishedMembersNotified(ObjectKeyValueSet* oldSet, ObjectKeyValueSet* newSet, NotifyContext* ctx)
{
  return logSetChange(this, "(MissionMissionMemberFinishedMissionSelectListNotif)", "Members", oldSet, newSet, ctx);
}

int Mission::OnReadyForNextPhaseNotified(int newValue, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogFlag("(MissionReadyForNextPhaseNotif)", "ReadyForNextPhase", newValue, 0, ctx);
  }
  return result;
}

int Mission::OnImpliedTeamNotified(int newValue, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogFlag("(MissionImpliedTeamNotif)", "ImpliedTeam", newValue, 0, ctx);
  }
  return result;
}

int Mission::OnMandatoryNotified(int newValue, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogFlag("(MissionMandatoryNotif)", "Mandatory", newValue, 0, ctx);
  }
  return result;
}
