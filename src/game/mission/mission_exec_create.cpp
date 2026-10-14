#include "galaxy/system_rules.h"
#include "mission/mission_exec_decl.h"
#include "mission/mission_rules.h"

using namespace mission_exec;

// Creation of missions by the mission manager (0x546fa0-0x547ba0). CreateMission (0x547ba0) makes the mission object
// as a child of the manager and fills in its properties; the CreateMissionTypeNN helpers fix the mission type key
// (0x41000001-0x44000004) and the defaults of the optional properties.

namespace
{

constexpr uint32_t kMissionType41 = 0x41000001;
constexpr uint32_t kMissionType42 = 0x42000002;
constexpr uint32_t kMissionType43 = 0x43000003;
constexpr uint32_t kMissionType44 = 0x44000004;
constexpr uint32_t kStoryPersonKey = 0x3c000009;
constexpr uint32_t kStoryTargetKey = 0x33000243;
constexpr uint32_t kStoryMissionTypeKey = 0x65000083;

ObjectKey keyOf(uint32_t value)
{
  ObjectKey key;
  key.m_value = value;
  return key;
}

} // namespace

int MissionMgr::CreateStoryMissionAtRandom(NotifyContext* ctx)
{
  int result = 1;
  if (!PercentChance(g_gnprtb1806))
    return result;

  ObjectKey parentKey;
  GameObjNotifier* first = nullptr;
  GameObjNotifier* parent = nullptr;
  result = FindHan(&first, &parent, &parentKey);
  if (!first || !parent)
    return result;
  auto* story = static_cast<Character*>(first);
  if (parentKey.OrNull() && !(story->m_flagsAc & kCharacterCaptured) && (story->m_stateFlags & 1))
  {
    auto* manager = static_cast<GameObj*>(GetGameRootMember_dc());
    GameObj* person = nullptr;
    if (manager)
    {
      ObjectKey personType = keyOf(kStoryPersonKey);
      person = manager->CreateChild(&personType, manager->ControlSide(), ctx, 1, 1);
    }
    GameObjNotifier* rules = GetGameRootMember_bc();
    result = rules != nullptr;
    if (rules)
    {
      ObjectKey target = keyOf(kStoryTargetKey);
      result = static_cast<RulesMgr*>(rules)->RulesMgr::SyncUnitView(&target, 2, ctx) && result;
    }
    if (person)
    {
      ObjectKey personKey;
      ObjectKey missionType = keyOf(kStoryMissionTypeKey);
      ObjectKey target = keyOf(kStoryTargetKey);
      person->GetKey(&personKey);
      result = CreateMissionForCharacter(&target, &missionType, &personKey, ctx) && result;
    }
  }
  return result;
}

int MissionMgr::CreateMissionType41ForCharacter(Character* person, NotifyContext* ctx)
{
  int ok = 1;
  ObjectKey parentKey;
  ObjectKey builderOwnerKey;
  ObjectKeyValueSet team;
  if (person->m_builderKey.IsSet())
  {
    person->GetValidParentKey(&parentKey);
    auto* builder = static_cast<GameObj*>(GetMasterInFamilies_01_ff(&person->m_builderKey));
    int found = builder != nullptr;
    if (builder)
      found = builder->GetOwnerKey(&builderOwnerKey) && found;
    ok = 0;
    if (found)
    {
      ObjectKey personKey;
      ok = team.AddKeyValue(person->GetKey(&personKey), 0) != 0;
    }
    if (parentKey.OrNull() && builderOwnerKey.OrNull())
    {
      ObjectKeyValueSet decoys;
      ObjectKey missionType = keyOf(kMissionType41);
      const int created = CreateMission(0, 0, person->ControlSide(), &parentKey, &missionType, &parentKey,
                                        &builderOwnerKey, &team, &decoys, nullptr, 0, 1, ctx);
      ok = created && ok;
    }
  }
  return ok;
}

int MissionMgr::CreateMissionType41(int side, ObjectKey* targetKey, ObjectKeyValueSet* team, NotifyContext* ctx)
{
  ObjectKey origin;
  ObjectKey target(*targetKey);
  StatusCode status(1, -1);
  int result = 0;
  const int found = FindCommonOrigin(team, &origin, &status);
  if (status.m_second == static_cast<uint32_t>(-1))
  {
    ObjectKeyValueSet decoys;
    ObjectKey missionType = keyOf(kMissionType41);
    const int created =
      CreateMission(0, 0, side, targetKey, &missionType, &target, &origin, team, &decoys, nullptr, 0, 1, ctx);
    result = created && found;
  }
  return result;
}

int MissionMgr::CreateMissionType41WithLevels(int side,
                                              ObjectKey* targetKey,
                                              ObjectKeyValueSet* team,
                                              NotifyContext* ctx)
{
  ObjectKey origin;
  ObjectKey target(*targetKey);
  MissionPhaseLevels levels;
  levels.m_levels &= 0xffff0000u;
  StatusCode status(1, -1);
  int result = 0;
  const int found = FindCommonOrigin(team, &origin, &status);
  if (status.m_second == static_cast<uint32_t>(-1))
  {
    ObjectKeyValueSet decoys;
    ObjectKey missionType = keyOf(kMissionType41);
    const int created =
      CreateMission(0, 0, side, targetKey, &missionType, &target, &origin, team, &decoys, &levels, 0, 1, ctx);
    result = created && found;
  }
  return result;
}

int MissionMgr::CreateMissionType42(int side,
                                    MissionPhaseLevels* levels,
                                    ObjectKey* targetKey,
                                    ObjectKeyValueSet* team,
                                    int mandatory,
                                    NotifyContext* ctx)
{
  ObjectKey origin;
  ObjectKey target(*targetKey);
  StatusCode status(1, -1);
  int result = 0;
  const int found = FindCommonOrigin(team, &origin, &status);
  if (status.m_second == static_cast<uint32_t>(-1))
  {
    ObjectKeyValueSet decoys;
    ObjectKey missionType = keyOf(kMissionType42);
    const int created =
      CreateMission(0, 0, side, targetKey, &missionType, &target, &origin, team, &decoys, levels, 0, mandatory, ctx);
    result = created && found;
  }
  return result;
}

int MissionMgr::CreateMissionType43(int side,
                                    ObjectKey* targetKey,
                                    ObjectKeyValueSet* team,
                                    int impliedTeam,
                                    NotifyContext* ctx)
{
  ObjectKey origin;
  ObjectKey target(*targetKey);
  StatusCode status(1, -1);
  int result = 0;
  const int found = FindCommonOrigin(team, &origin, &status);
  if (status.m_second == static_cast<uint32_t>(-1))
  {
    ObjectKeyValueSet decoys;
    ObjectKey missionType = keyOf(kMissionType43);
    const int created =
      CreateMission(0, 0, side, targetKey, &missionType, &target, &origin, team, &decoys, nullptr, impliedTeam, 1, ctx);
    result = created && found;
  }
  return result;
}

int MissionMgr::CreateMissionType44(int side, ObjectKey* targetKey, ObjectKeyValueSet* team, NotifyContext* ctx)
{
  ObjectKey origin;
  ObjectKey target(*targetKey);
  StatusCode status(1, -1);
  int result = 0;
  const int found = FindCommonOrigin(team, &origin, &status);
  if (status.m_second == static_cast<uint32_t>(-1))
  {
    ObjectKeyValueSet decoys;
    ObjectKey missionType = keyOf(kMissionType44);
    const int created =
      CreateMission(0, 0, side, targetKey, &missionType, &target, &origin, team, &decoys, nullptr, 0, 1, ctx);
    result = created && found;
  }
  return result;
}

int MissionMgr::CreateMissionForCharacter(ObjectKey* objectiveKey,
                                          ObjectKey* typeKey,
                                          ObjectKey* personKey,
                                          NotifyContext* ctx)
{
  Role* person = static_cast<Role*>(GetMasterInFamilies_30_40(personKey));
  int ok = person != nullptr;
  ObjectKey personLocation;
  if (person)
  {
    ObjectKey scratch;
    personLocation = *GetOwnerKeyValue(person, &scratch);
  }
  ObjectKey objectiveLocation;
  if (person)
  {
    auto* objective =
      static_cast<GameObj*>(GetObjectInFamilies_01_ff(ViewOfSide(person->ControlSide()), objectiveKey));
    ok = objective != nullptr;
    if (objective)
    {
      ObjectKey scratch;
      objectiveLocation = *GetOwnerKeyValue(objective, &scratch);
    }
  }
  ObjectKeyValueSet team;
  int added = 0;
  if (ok)
    added = team.AddKeyValue(personKey, 0) != 0;
  if (objectiveLocation.OrNull() && personLocation.OrNull())
  {
    ObjectKeyValueSet decoys;
    const int created = CreateMission(0, 0, person->ControlSide(), objectiveKey, typeKey, &objectiveLocation,
                                      &personLocation, &team, &decoys, nullptr, 0, 1, ctx);
    added = created && added;
  }
  return added;
}

int MissionMgr::CreateMission(int userId,
                              int userId2,
                              int side,
                              ObjectKey* objectiveKey,
                              ObjectKey* typeKey,
                              ObjectKey* targetKey,
                              ObjectKey* originKey,
                              ObjectKeyValueSet* team,
                              ObjectKeyValueSet* decoys,
                              MissionPhaseLevels* levels,
                              int impliedTeam,
                              int mandatory,
                              NotifyContext* ctx)
{
  ObjectKey targetLocationKey;
  NotifyContext missionContext(*ctx);
  missionContext.m_reserved14 = CallGameHook2AFC();

  int sideOk = GetGameRootMember_bc() != nullptr && (side == kSideAlliance || side == kSideEmpire);
  auto* mission = static_cast<Mission*>(CreateChild(typeKey, side, &missionContext, 1, 1));
  int ok = mission != nullptr && sideOk;
  if (ok)
    ok = team->Count() != 0;
  auto* host = static_cast<GameObj*>(GetObjectInFamilies_01_ff(ViewOfSide(side), objectiveKey));
  ok = host != nullptr && ok;
  if (ok)
    ok = InFamilies(originKey, kFamilySystemBegin, kFamilySystemOrNextEnd);
  if (host)
    ok = host->GetOwnerKey(&targetLocationKey) && ok;
  int result = 0;
  if (ok)
    result = InFamilies(&targetLocationKey, kFamilySystemBegin, kFamilySystemOrNextEnd);

  if (mission && targetLocationKey.OrNull())
  {
    mission->SetPhase(1, &missionContext);
    mission->SetUserId(userId, &missionContext);
    mission->SetUserId2(userId2, &missionContext);
    mission->SetOriginLocationKey(originKey, &missionContext);
    mission->SetObjectiveKey(objectiveKey, &missionContext);
    const int targetSet = mission->SetTargetKey(targetKey, &missionContext);
    int chain = mission->SetTargetLocationKey(&targetLocationKey, &missionContext) && targetSet;
    chain = mission->SetImpliedTeam(impliedTeam, &missionContext) && chain;
    chain = mission->SetMandatory(mandatory, &missionContext) && chain;
    if (levels)
      chain = mission->Mission::AssignPhaseLevels(levels) && chain;
    result = mission->AddInitialMembers(team, decoys, &missionContext) && chain;
  }
  return result;
}
