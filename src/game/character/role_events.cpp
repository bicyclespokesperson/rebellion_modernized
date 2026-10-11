#include "character/role.h"
#include "data/random.h"
#include "events/event_post.h"
#include "gameobj/object_queries.h"

namespace
{

constexpr uint32_t kExistingState = 0x40;
constexpr uint32_t kCompletedState = 0x4;

} // namespace

// ---- events inherited from UnitObjBase that Role extends ---------------------------------------------------------

int Role::OnCreatedChanged(int newValue, NotifyContext* ctx)
{
  const int base = UnitObjBase::OnCreatedChanged(newValue, ctx);
  const RoleRecord* record = static_cast<RoleRecord*>(m_record);
  if (!record)
    return 0;
  int ok = base != 0 ? 1 : 0;
  ok = SetBaseDiplomacy(record->m_skills[0].m_base + GameRandom(record->m_skills[0].m_spread), ctx) && ok;
  ok = SetBaseEspionage(record->m_skills[1].m_base + GameRandom(record->m_skills[1].m_spread), ctx) && ok;
  ok = SetBaseShipyardRD(record->m_skills[2].m_base + GameRandom(record->m_skills[2].m_spread), ctx) && ok;
  ok = SetBaseTrainingFacilRD(record->m_skills[3].m_base + GameRandom(record->m_skills[3].m_spread), ctx) && ok;
  ok = SetBaseConstructionYardRD(record->m_skills[4].m_base + GameRandom(record->m_skills[4].m_spread), ctx) && ok;
  ok = SetBaseCombat(record->m_skills[5].m_base + GameRandom(record->m_skills[5].m_spread), ctx) && ok;
  ok = SetBaseLeadership(record->m_skills[6].m_base + GameRandom(record->m_skills[6].m_spread), ctx) && ok;
  ok = SetBaseLoyalty(record->m_skills[7].m_base + GameRandom(record->m_skills[7].m_spread), ctx) && ok;
  return ok;
}

int Role::OnUsableChanged(int newValue, NotifyContext* ctx)
{
  return UnitObjBase::OnUsableChanged(newValue, ctx) != 0;
}

int Role::OnParentChanged(GameObjNotifier* oldParent, GameObjNotifier* newParent, NotifyContext* ctx)
{
  int ok = UnitObjBase::OnParentChanged(oldParent, newParent, ctx) != 0;
  const bool existingAndCompleted = (m_stateFlags & kExistingState) && (m_stateFlags & kCompletedState);
  if (existingAndCompleted && !HasMissionFlag(role_flag::kAdrift))
  {
    ObjectKey missionKey;
    ok = GetMissionKey(&missionKey)->IsSet() && ok;
  }
  return ok;
}

int Role::OnDestroyedChanged(int newValue, NotifyContext* ctx)
{
  int ok = UnitObjBase::OnDestroyedChanged(newValue, ctx) != 0;
  if (newValue != 0)
  {
    ObjectKey missionKey;
    if (GetMissionKey(&missionKey)->IsSet())
      ok = SetMissionRemoveRequest(1, ctx) && ok;
    ok = SetAdrift(0, ctx) && ok;
  }
  return ok;
}

int Role::OnEnrouteChanged(int newValue, NotifyContext* ctx)
{
  return UnitObjBase::OnEnrouteChanged(newValue, ctx) != 0;
}

int Role::OnAutoroutingChanged(int newValue, NotifyContext* ctx)
{
  return UnitObjBase::OnAutoroutingChanged(newValue, ctx) != 0;
}

// ---- OnXChanged of the Role properties --------------------------------------------------------------------------------

int Role::OnMissionChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx)
{
  int ok = UpdateOnMission(ctx) != 0;
  if (!newKey->OrNull())
  {
    ok = SetDecoy(0, ctx) && ok;
    ok = SetMissionRemoveRequest(0, ctx) && ok;
    ok = SetMissionResignRequest(0, ctx) && ok;
    ok = SetCanResignFromMission(0, ctx) && ok;
    ok = SetIsDecoying(0, ctx) && ok;
    return ok;
  }
  return SetMovingBetweenMissions(0, ctx) && ok;
}

int Role::OnParentAtMissionCompletionChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx)
{
  ObjectKey location;
  int ok = 1;
  if (newKey->OrNull())
  {
    GameObjNotifier* parent = GetMasterInFamilies_01_ff(newKey);
    if (parent)
      ok = parent->GetOwnerKey(&location) != 0;
  }
  return SetLocationAtMissionCompletionKey(&location, ctx) && ok;
}

int Role::OnMovingBetweenMissionsChanged(int newValue, NotifyContext* ctx)
{
  return UpdateOnMission(ctx) != 0;
}

int Role::OnMissionRemoveRequestChanged(int newValue, NotifyContext* ctx)
{
  int ok = 1;
  if (newValue != 0)
    ok = SetMissionResignRequest(0, ctx) != 0;
  return PostSourceEventFlag(0x30b, this, newValue, ctx) && ok;
}

int Role::OnMissionResignRequestChanged(int newValue, NotifyContext* ctx)
{
  return PostSourceEventFlag(0x314, this, newValue, ctx) != 0;
}

int Role::OnAdriftChanged(int newValue, NotifyContext* ctx)
{
  return PostSourceEventFlag(0x30e, this, newValue, ctx) != 0;
}

int Role::OnOnMissionChanged(int newValue, NotifyContext* ctx)
{
  ObjectKey parentKey;
  int ok;
  if (newValue == 0)
  {
    ok = SetParentAtMissionCompletionKey(&parentKey, ctx) != 0;
  }
  else
  {
    ok = GetValidParentKey(&parentKey) != 0;
    ok = SetParentAtMissionCompletionKey(&parentKey, ctx) && ok;
  }
  ok = SetOnHiddenMission(0, ctx) && ok;
  ok = SetOnMandatoryMission(0, ctx) && ok;
  return PostSourceEventFlag(0x312, this, newValue, ctx) && ok;
}
