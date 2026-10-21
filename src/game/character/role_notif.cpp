#include "character/role.h"
#include "events/event_post.h"

int Role::OnEnrouteActiveNotified(int value, NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    LogFlag("RoleEnrouteActiveNotif", "EnrouteActive", value, 0, ctx);
    result = PostPayloadFlag(0x200, this, value, ctx);
  }
  return result;
}

int Role::OnBaseDiplomacyNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogInt("(RoleBaseDiplomacyNotif)", "BaseDiplomacy", newValue, oldValue, 0, ctx);
  }
  return result;
}

int Role::OnBaseEspionageNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogInt("(RoleBaseEspionageNotif)", "BaseEspionage", newValue, oldValue, 0, ctx);
  }
  return result;
}

int Role::OnBaseShipyardRDNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogInt("(RoleBaseShipyardRDNotif)", "BaseShipyardRD", newValue, oldValue, 0, ctx);
  }
  return result;
}

int Role::OnBaseTrainingFacilRDNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogInt("(RoleBaseTrainingFacilRDNotif)", "BaseTrainingFacilRD", newValue, oldValue, 0, ctx);
  }
  return result;
}

int Role::OnBaseConstructionYardRDNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogInt("(RoleBaseConstructionYardRDNotif)", "BaseConstructionYardRD", newValue, oldValue, 0, ctx);
  }
  return result;
}

int Role::OnBaseCombatNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogInt("(RoleBaseCombatNotif)", "BaseCombat", newValue, oldValue, 0, ctx);
  }
  return result;
}

int Role::OnBaseLeadershipNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogInt("(RoleBaseLeadershipNotif)", "BaseLeadership", newValue, oldValue, 0, ctx);
  }
  return result;
}

int Role::OnBaseLoyaltyNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogInt("(RoleBaseLoyaltyNotif)", "BaseLoyalty", newValue, oldValue, 0, ctx);
  }
  return result;
}

int Role::OnMissionNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx)
{
  if (!IsSideView()) {
    return 0;
  }
  LogKey("RoleMissionKeyNotif", "Mission", newKey, oldKey, 0, ctx);
  const int first = PostPayloadKeysA(0x380, this, oldKey, newKey, ctx);
  const int second = PostTwoKeyEvent(0x321, this, oldKey, newKey, ctx);
  return second && first ? 1 : 0;
}

int Role::OnMissionSeedNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogTypeKey("(RoleMissionSeedKeyNotif)", "MissionSeed", newKey, oldKey, 0, ctx);
  }
  return result;
}

int Role::OnParentAtMissionCompletionNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogKey("(RoleParentAtMissionCompletionKeyNotif)", "ParentAtMissionCompletion", newKey, oldKey, 0, ctx);
  }
  return result;
}

int Role::OnLocationAtMissionCompletionNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogKey("(RoleLocationAtMissionCompletionKeyNotif)", "LocationAtMissionCompletion", newKey, oldKey, 0, ctx);
  }
  return result;
}

int Role::OnDecoyNotified(int value, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogFlag("(RoleDecoyNotif)", "Decoy", value, 0, ctx);
  }
  return result;
}

int Role::OnMovingBetweenMissionsNotified(int value, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogFlag("(RoleMovingBetweenMissionsNotif)", "MovingBetweenMissions", value, 0, ctx);
  }
  return result;
}

int Role::OnMissionRemoveRequestNotified(int value, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogFlag("(RoleMissionRemoveRequestNotif)", "MissionRemoveRequest", value, 0, ctx);
  }
  return result;
}

int Role::OnMissionResignRequestNotified(int value, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogFlag("(RoleMissionResignRequestNotif)", "MissionResignRequest", value, 0, ctx);
  }
  return result;
}

int Role::OnCanResignFromMissionNotified(int value, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogFlag("(RoleCanResignFromMissionNotif)", "CanResignFromMission", value, 0, ctx);
  }
  return result;
}

int Role::OnIsDecoyingNotified(int value, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogFlag("(RoleIsDecoyingNotif)", "IsDecoying", value, 0, ctx);
  }
  return result;
}

int Role::OnAdriftNotified(int value, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogFlag("(RoleAdriftNotif)", "Adrift", value, 0, ctx);
  }
  return result;
}

int Role::OnOnMissionNotified(int value, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogFlag("(RoleOnMissionNotif)", "OnMission", value, 0, ctx);
  }
  return result;
}

int Role::OnOnHiddenMissionNotified(int value, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogFlag("(RoleOnHiddenMissionNotif)", "OnHiddenMission", value, 0, ctx);
  }
  return result;
}

int Role::OnOnMandatoryMissionNotified(int value, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    LogFlag("(RoleOnMandatoryMissionNotif)", "OnMandatoryMission", value, 0, ctx);
  }
  return result;
}
