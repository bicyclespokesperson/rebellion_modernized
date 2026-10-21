#include "ai/ai_common.h"
#include "core/checked_new.h"
#include "data/gnprtb.h"
#include "data/sd_records.h"
#include "economy/side.h"
#include "gameobj/object_database.h"
#include "gameobj/object_lookup.h"
#include "gameobj/object_name_node.h"
#include "gameobj/object_queries.h"
#include "gameobj/object_type_db.h"
#include "mission/mission_external.h"
#include "story/mission_mgr.h"
#include "story/story_characters.h"
#include "story/story_common.h"

// The mission manager's checks for the orders that start or join missions, and the helpers that gather the people of
// such an order (0x5420d0-0x543040 and 0x54bac0-0x54c720). A "request" is the ActivityRequest of the order: its target
// (+0x08) is the objective and its kind (+0x0c) the order (0x240 / 0x241). The people of an order are split into four
// key sets: the able members, the decoys, the captured ones and the union of all.

// The rules object (game root member +0xbc, family 0xf9).

namespace
{

constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kMissionStatus = 0x40;
constexpr int kStatusNoMemberLocation = 0x16;
constexpr int kReasonLocationsDiffer = 3;
constexpr int kReasonNoAbleMember = 0x91;
constexpr int kReasonTargetNotFound = 0x21;
constexpr uint32_t kOrderStartMission = 0x240;
constexpr uint32_t kOrderJoinMission = 0x241;
constexpr uint32_t kKeyHan = 0x33000243;
constexpr uint32_t kAllianceHomeSystem = 0x92000121;
constexpr uint32_t kEmpireHomeSystem = 0x90000109;
constexpr uint32_t kPalaceSystemKey = 0x98000481;
constexpr uint32_t kFamilyBountyMission = 0x65;
constexpr uint32_t kFamilySystemBegin = 0x90;
constexpr uint32_t kFamilySystemEnd = 0x98;
constexpr uint32_t kFamilySpecialForcesFirst = 0x3c;
constexpr uint32_t kFamilySpecialForcesEnd = 0x40;
constexpr int kRecordFamilyMissions = 0x40;
constexpr int kRulesMode = 0;
constexpr uint32_t kFamilyMissionsBegin = 0x40;
constexpr uint32_t kFamilyMissionsMid = 0x50;
constexpr uint32_t kFamilyMissionsEnd = 0x80;

using MemberCheck = int (*)(int side, GameObj* object, GameObj** out, StatusCode* status);
using TargetCheck = int (*)(MissionCheckArgs* args, StatusCode* status, int* code);

bool statusOk(const StatusCode* status)
{
  return status->m_second == static_cast<uint32_t>(kStatusOk);
}

void setStatus(StatusCode* status, int category, int code)
{
  *status = StatusCode(category, code);
}

int viewOfSummary(const MissionMemberSummary& summary)
{
  if (summary.m_kind == 1) {
    return 1;
  }
  return summary.m_kind == 2 ? 2 : 0;
}

intptr_t asArgument(const void* pointer)
{
  return reinterpret_cast<intptr_t>(pointer);
}

} // namespace

// ---- gathering the people of an order
// ---------------------------------------------------------------------------------

int MissionMgr::GatherMissionMembers(const ActivityRequest* request, ObjectKeyValueSet* team,
                                     ObjectKeyValueSet* characters, ObjectKeyValueSet* members,
                                     ObjectKeyValueSet* decoys, ObjectKeyValueSet* captured,
                                     ObjectKeyValueSet* everyone, StatusCode* status)
{
  int ok = 1;
  members->Clear();
  decoys->Clear();
  captured->Clear();
  everyone->Clear();
  setStatus(status, kStatusCategory, kStatusOk);

  if (statusOk(status)) {
    ok = CollectKeyDependencies(const_cast<ActivityRequest*>(request), team, members, status) ? 1 : 0;
  }
  if (ok) {
    if (statusOk(status)) {
      ok = CollectKeyDependencies(const_cast<ActivityRequest*>(request), characters, decoys, status) ? 1 : 0;
    }
    if (ok) {
      if (!statusOk(status)) {
        return ok;
      }
      for (auto position = members->Entries().begin(); position != members->Entries().end();) {
        auto current = position++;
        auto* node = static_cast<ObjectKeyNode*>((*current).get());
        GameObj* role = nullptr;
        ObjectKey key;
        node->GetKey(&key);
        ok = ResolveRoleFromKey(&key, &role) && ok ? 1 : 0;
        if (role && static_cast<Role*>(role)->IsCaptured()) {
          members->Remove(node);
          captured->Add(node);
        }
      }
      for (auto position = decoys->Entries().begin(); position != decoys->Entries().end() && ok;) {
        auto current = position++;
        auto* node = static_cast<ObjectKeyNode*>((*current).get());
        GameObj* role = nullptr;
        ObjectKey key;
        node->GetKey(&key);
        ok = ResolveRoleFromKey(&key, &role) ? 1 : 0;
        if (role && static_cast<Role*>(role)->IsCaptured()) {
          decoys->Remove(node);
          captured->Add(node);
        }
      }
      if (members->Count() == 0) {
        setStatus(status, kMissionStatus, kReasonNoAbleMember);
      }
    }
  }

  if (statusOk(status)) {
    ObjectKeyValueSet* groups[] = {members, decoys, captured};
    for (ObjectKeyValueSet* set : groups) {
      for (auto nodePosition = set->Entries().begin(); nodePosition != set->Entries().end();) {
        ObjectKeyNode* node = static_cast<ObjectKeyNode*>((*(nodePosition++)).get());

        ObjectKey key;
        ok = everyone->AddKeyValue(node->GetKey(&key), 0) && ok ? 1 : 0;
      }
    }
  }
  return ok;
}

int MissionMgr::FindCommonOrigin(ObjectKeyValueSet* team, ObjectKey* out, StatusCode* status)
{
  int result = 1;
  setStatus(status, kStatusCategory, kStatusOk);
  out->Reset();
  ObjectKey common;
  bool firstMember = true;
  bool aborted = false;
  for (auto nodePosition = team->Entries().begin(); nodePosition != team->Entries().end();) {
    ObjectKeyNode* node = static_cast<ObjectKeyNode*>((*(nodePosition++)).get());

    if (!statusOk(status)) {
      aborted = true;
      break;
    }
    GameObj* person = nullptr;
    ObjectKey key;
    node->GetKey(&key);
    result = ResolveRoleFromKey(&key, &person) && result ? 1 : 0;
    if (!person) {
      continue;
    }
    ObjectKey location;
    result = person->GetOwnerKey(&location) && result ? 1 : 0;
    if (!location.OrNull()) {
      continue;
    }
    if (firstMember) {
      firstMember = false;
      common = location;
    } else if (common.m_value != location.m_value) {
      setStatus(status, kMissionStatus, kReasonLocationsDiffer);
    }
  }
  if (!aborted && statusOk(status) && firstMember) {
    setStatus(status, kStatusCategory, kStatusNoMemberLocation);
  }
  if (common.OrNull()) {
    *out = common;
  }
  return result;
}

int MissionMgr::CheckTeamActivity(const ActivityRequest* request, ObjectKeyValueSet* team, StatusCode* status)
{
  int result = 1;
  setStatus(status, kStatusCategory, kStatusOk);
  for (auto nodePosition = team->Entries().begin(); nodePosition != team->Entries().end();) {
    ObjectKeyNode* node = static_cast<ObjectKeyNode*>((*(nodePosition++)).get());
    if (!(true && statusOk(status))) {
      break;
    }

    GameObj* person = nullptr;
    ObjectKey key;
    node->GetKey(&key);
    result = ResolveRoleFromKey(&key, &person) && result ? 1 : 0;
    if (person) {
      result = static_cast<Role*>(person)->CheckActivity2(request, status);
    }
  }
  return result;
}

int MissionMgr::CheckTeamActivities(const ActivityRequest* request, ObjectKeyValueSet* members,
                                    ObjectKeyValueSet* decoys, ObjectKeyValueSet* captured, StatusCode* status)
{
  int result = 1;
  setStatus(status, kStatusCategory, kStatusOk);
  const struct {
    ObjectKeyValueSet* set;
    int asDecoy;
  } groups[] = {{members, 0}, {decoys, 1}, {captured, 0}};
  for (const auto& group : groups) {
    for (auto nodePosition = group.set->Entries().begin(); nodePosition != group.set->Entries().end();) {
      ObjectKeyNode* node = static_cast<ObjectKeyNode*>((*(nodePosition++)).get());
      if (!(true && statusOk(status))) {
        break;
      }

      GameObj* person = nullptr;
      ObjectKey key;
      node->GetKey(&key);
      result = ResolveRoleFromKey(&key, &person) && result ? 1 : 0;
      if (person) {
        result = static_cast<Role*>(person)->CheckActivityStatus2(request, group.asDecoy, status) ? 1 : 0;
      }
    }
  }
  return result;
}

int MissionMgr::AddRecordName(DatRecord* record, ObjectNameList* out)
{
  auto* node = NewOrNull<ObjectNameNode>();
  int ok = node != nullptr;
  if (node) {
    ok = node->m_nameValid != 0 && ok ? 1 : 0;
    ok = node->Load(record) && ok ? 1 : 0;
    ok = out->AddWithId(node) && ok ? 1 : 0;
  }
  return ok;
}

// ---- the mission-type checks
// ------------------------------------------------------------------------------------------

int MissionMgr::CheckMissionTypeForTeam(int side, ObjectKeyValueSet* members, ObjectKeyValueSet* decoys,
                                        ObjectKeyValueSet* captured, MissionMemberSummary* summary, DatRecord* record,
                                        StatusCode* status)
{
  setStatus(status, kStatusCategory, kStatusOk);
  ObjectKey recordKey;
  GetAiRecordKey(record, &recordKey);
  MissionCheckFunctions checks;
  int result = 0;
  if (FindTypeChecks(&recordKey, &checks)) {
    ObjectRefList refs;
    MissionCheckArgs args(1, 1, 0, side, record, &refs, &refs, &refs, members, decoys, captured, summary, nullptr,
                          nullptr);
    int code;
    result = reinterpret_cast<TargetCheck>(checks.m_unk04)(&args, status, &code);
  }
  return result;
}

int MissionMgr::CheckMissionTypeAtTarget(int side, ObjectKeyValueSet* members, ObjectKeyValueSet* decoys,
                                         ObjectKeyValueSet* captured, MissionMemberSummary* summary,
                                         GameObjNotifier* target, DatRecord* record, StatusCode* status)
{
  setStatus(status, kStatusCategory, kStatusOk);
  ObjectKey recordKey;
  GetAiRecordKey(record, &recordKey);
  MissionCheckFunctions checks;
  GameObj* resolved = nullptr;
  int result = 0;
  if (FindTypeChecks(&recordKey, &checks)) {
    result = reinterpret_cast<MemberCheck>(checks.m_unk00)(side, static_cast<GameObj*>(target), &resolved, status);
    if (result && statusOk(status)) {
      ObjectRefList refs;
      MissionCheckArgs args(1, 1, 1, side, record, &refs, &refs, &refs, members, decoys, captured, summary, resolved,
                            static_cast<GameObj*>(target));
      int code;
      result = reinterpret_cast<TargetCheck>(checks.m_unk04)(&args, status, &code);
    }
  }
  return result;
}

int MissionMgr::FindMissionForTarget(int side, NullableKey* missionKey, GameObjNotifier* target,
                                     GameObjNotifier** mission, StatusCode* status)
{
  setStatus(status, kStatusCategory, kStatusOk);
  *mission = nullptr;
  MissionCheckFunctions checks;
  ObjectKey missionTypeKey;
  missionTypeKey.m_value = missionKey->m_value;
  if (!FindTypeChecks(&missionTypeKey, &checks)) {
    return 0;
  }
  GameObj* foundMission = nullptr;
  const int found = reinterpret_cast<MemberCheck>(checks.m_unk00)(side, static_cast<GameObj*>(target), &foundMission,
                                                                 status);
  *mission = static_cast<GameObjNotifier*>(foundMission);
  return found;
}

// ---- the candidate lists of the mission orders
// --------------------------------------------------------------------------

int MissionMgr::CollectMissionTypesForTeam(const ActivityRequest* request, ObjectKeyValueSet* team,
                                           ObjectKeyValueSet* characters, ObjectRefList* out, StatusCode* status)
{
  out->Clear();
  setStatus(status, kStatusCategory, kStatusOk);
  ObjectKeyValueSet members;
  ObjectKeyValueSet decoys;
  ObjectKeyValueSet captured;
  ObjectKeyValueSet everyone;
  int ok = GatherMissionMembers(request, team, characters, &members, &decoys, &captured, &everyone, status) ? 1 : 0;
  MissionMemberSummary summary = {};
  if (statusOk(status)) {
    ok = FillMemberSummaryForMission(&members, &decoys, &captured, &summary, status) && ok ? 1 : 0;
    if (statusOk(status)) {
      for (DatRecord* record : GetRecordsOfType(kRecordFamilyMissions)) {
        if (static_cast<MissionSD*>(record)->m_hidden != 0) {
          continue;
        }
        StatusCode recordStatus(kStatusCategory, kStatusOk);
        ok = CheckMissionTypeForTeam(summary.m_kind, &members, &decoys, &captured, &summary, record, &recordStatus)
                     && ok
                 ? 1
                 : 0;
        if (recordStatus.m_second == static_cast<uint32_t>(kStatusOk)) {
          ObjectKey recordKey;
          GetAiRecordKey(record, &recordKey);
          ok = out->AddKey(recordKey) != nullptr;
        }
      }
    }
  }
  return ok;
}

int MissionMgr::CollectMissionTypeNamesAtTarget(const ActivityRequest* request, ObjectKeyValueSet* team,
                                                ObjectKeyValueSet* characters, ObjectNameList* out, StatusCode* status)
{
  out->Clear();
  setStatus(status, kStatusCategory, kStatusOk);
  ObjectKeyValueSet members;
  ObjectKeyValueSet decoys;
  ObjectKeyValueSet captured;
  ObjectKeyValueSet everyone;
  int ok = GatherMissionMembers(request, team, characters, &members, &decoys, &captured, &everyone, status) ? 1 : 0;
  ObjectKey origin;
  if (statusOk(status)) {
    ok = FindCommonOrigin(&everyone, &origin, status) && ok ? 1 : 0;
    if (statusOk(status)) {
      ok = CheckTeamActivities(request, &members, &decoys, &captured, status) && ok ? 1 : 0;
    }
  }
  MissionMemberSummary summary = {};
  if (statusOk(status)) {
    ok = FillMemberSummaryForMission(&members, &decoys, &captured, &summary, status) && ok ? 1 : 0;
  }
  if (statusOk(status)) {
    GameObjNotifier* target = GetObjectInFamilies_01_ff(viewOfSummary(summary), &request->m_target);
    if (!target) {
      setStatus(status, kMissionStatus, kReasonTargetNotFound);
    }
    if (statusOk(status)) {
      for (DatRecord* record : GetRecordsOfType(kRecordFamilyMissions)) {
        if (static_cast<MissionSD*>(record)->m_hidden != 0) {
          continue;
        }
        StatusCode recordStatus(kStatusCategory, kStatusOk);
        ok = CheckMissionTypeAtTarget(summary.m_kind, &members, &decoys, &captured, &summary, target, record,
                                      &recordStatus)
                     && ok
                 ? 1
                 : 0;
        if (recordStatus.m_second == static_cast<uint32_t>(kStatusOk)) {
          ok = AddRecordName(record, out);
        }
      }
    }
  }
  return ok;
}

int MissionMgr::CheckMissionOrder(const ActivityRequest* request, int a, int b, ObjectKey* missionKey,
                                  ObjectKey* secondKey, ObjectKeyValueSet* team, ObjectKeyValueSet* characters,
                                  StatusCode* status)
{
  setStatus(status, kStatusCategory, kStatusOk);
  bool ok;
  if (request->m_kind == kOrderStartMission) {
    ok = mission_exec::InFamilies(missionKey, kFamilyMissionsMid, kFamilyMissionsEnd);
  } else if (request->m_kind == kOrderJoinMission) {
    ok = mission_exec::InFamilies(missionKey, kFamilyMissionsBegin, kFamilyMissionsMid);
  } else {
    ok = false;
  }
  if (!ok) {
    setStatus(status, kStatusCategory, 1);
  }

  ObjectKeyValueSet members;
  ObjectKeyValueSet decoys;
  ObjectKeyValueSet captured;
  ObjectKeyValueSet everyone;
  if (statusOk(status)) {
    ok = GatherMissionMembers(request, team, characters, &members, &decoys, &captured, &everyone, status) && ok;
  }
  ObjectKey origin;
  if (statusOk(status)) {
    ok = FindCommonOrigin(&everyone, &origin, status) && ok;
    if (statusOk(status)) {
      ok = CheckTeamActivities(request, &members, &decoys, &captured, status) && ok;
    }
  }
  MissionMemberSummary summary = {};
  if (statusOk(status)) {
    ok = FillMemberSummaryForMission(&members, &decoys, &captured, &summary, status) && ok;
  }
  if (statusOk(status)) {
    GameObjNotifier* target = GetObjectInFamilies_01_ff(viewOfSummary(summary), &request->m_target);
    if (!target) {
      setStatus(status, kMissionStatus, kReasonTargetNotFound);
    }
    if (statusOk(status)) {
      DatRecord* record = FindDataRecord(AsLookupKey(missionKey));
      ok = record != nullptr;
      if (record) {
        ok = CheckMissionTypeAtTarget(summary.m_kind, &members, &decoys, &captured, &summary, target, record, status)
             && ok;
      }
    }
  }
  return ok;
}

int MissionMgr::CheckMissionOrderMembers(const ActivityRequest* request, int a, int b, ObjectKeyValueSet* team,
                                         ObjectKeyValueSet* characters, StatusCode* status)
{
  setStatus(status, kStatusCategory, kStatusOk);
  ObjectKeyValueSet members;
  ObjectKeyValueSet decoys;
  ObjectKeyValueSet captured;
  ObjectKeyValueSet everyone;
  bool ok = GatherMissionMembers(request, team, characters, &members, &decoys, &captured, &everyone, status) != 0;
  ObjectKey origin;
  if (statusOk(status)) {
    ok = FindCommonOrigin(&everyone, &origin, status) && ok;
    if (statusOk(status)) {
      ok = CheckTeamActivity(request, &everyone, status) && ok;
    }
  }
  return ok;
}

int MissionMgr::GetMissionHyperdriveModifier(int linked, int hasForces, int* modifier)
{
  bool ok = true;
  *modifier = g_gnprtbDefault;
  if (linked != 0 && hasForces == 0) {
    GameObjNotifier* han = GetMaster_33000243();
    ok = han != nullptr;
    if (han) {
      *modifier = GetHanHyperdriveModifier();
    }
  }
  return ok;
}

int MissionMgr::ResolveMissionTravel(const ActivityRequest* request, ObjectKey* resultKey, ObjectKeyValueSet* team,
                                     ObjectKeyValueSet* characters, ObjectKeyValueSet* everyone, StatusCode* status)
{
  setStatus(status, kStatusCategory, kStatusOk);
  ObjectKeyValueSet members;
  ObjectKeyValueSet decoys;
  ObjectKeyValueSet captured;
  int ok = GatherMissionMembers(request, team, characters, &members, &decoys, &captured, everyone, status) ? 1 : 0;
  ObjectKey origin;
  if (statusOk(status)) {
    ok = FindCommonOrigin(everyone, &origin, status) && ok ? 1 : 0;
  }

  int travelMode = 0;
  if (statusOk(status)) {
    ObjectKey han;
    han.m_value = kKeyHan;
    const int hasHan = members.Find(&han) != nullptr || decoys.Find(&han) != nullptr ? 1 : 0;
    int hasForces = 0;
    ObjectKeyValueSet* groups[] = {&members, &decoys, &captured};
    for (ObjectKeyValueSet* set : groups) {
      for (auto nodePosition = set->Entries().begin(); nodePosition != set->Entries().end();) {
        ObjectKeyNode* node = static_cast<ObjectKeyNode*>((*(nodePosition++)).get());
        if (!(true && !hasForces)) {
          break;
        }

        ObjectKey key;
        node->GetKey(&key);
        hasForces = mission_exec::InFamilies(&key, kFamilySpecialForcesFirst, kFamilySpecialForcesEnd);
      }
    }
    int modifier = g_gnprtbDefault;
    const int modified = GetMissionHyperdriveModifier(hasHan, hasForces, &modifier) && ok ? 1 : 0;
    GameObjNotifier* rules = GetGameRootMember_bc();
    ok = rules && modified ? 1 : 0;
    if (rules) {
      ok = static_cast<RulesMgr*>(rules)->RulesMgr::ComputeTravelDays(modifier, kRulesMode, &origin, resultKey,
                                                                      &travelMode)
                   && modified
               ? 1
               : 0;
    }
  }
  for (auto nodePosition = everyone->Entries().begin(); nodePosition != everyone->Entries().end();) {
    ObjectKeyNode* node = static_cast<ObjectKeyNode*>((*(nodePosition++)).get());
    static_cast<ObjectKeyValueNode*>(node)->m_value = travelMode;
  }
  return ok;
}

// ---- where a released person goes
// -------------------------------------------------------------------------------------

int MissionMgr::FindPersonSourceKey(GameObj* person, ObjectKey* out)
{
  out->Reset();
  ObjectKey owner;
  const int result = person->GetOwnerKey(&owner);
  if (!result) {
    return result;
  }
  if (owner.Family() >= kFamilySystemBegin && owner.Family() < kFamilySystemEnd) {
    *out = owner;
    return result;
  }
  Role* role = static_cast<Role*>(person);
  if (role->m_locationAtMissionCompletionKey.Family() >= kFamilySystemBegin
      && role->m_locationAtMissionCompletionKey.Family() < kFamilySystemEnd) {
    *out = role->m_locationAtMissionCompletionKey;
    return result;
  }
  out->m_value = (role->ControlSide() == 1) ? kAllianceHomeSystem : kEmpireHomeSystem;
  return result;
}

int MissionMgr::FindPersonOriginKey(GameObj* person, ObjectKey* out)
{
  out->Reset();
  Role* role = static_cast<Role*>(person);
  if (role->m_locationAtMissionCompletionKey.Family() >= kFamilySystemBegin
      && role->m_locationAtMissionCompletionKey.Family() < kFamilySystemEnd) {
    *out = role->m_parentAtMissionCompletionKey;
  }
  return 1;
}

int MissionMgr::FindPersonDestination(GameObj* person, int flag, ObjectKey* out)
{
  StatusCode status(kStatusCategory, kStatusOk);
  out->Reset();
  GameObjNotifier* rules = GetGameRootMember_bc();
  ObjectKey source;
  ObjectKey origin;
  int ok = rules != nullptr;
  if (ok) {
    ok = FindPersonSourceKey(person, &source);
  }
  if (ok) {
    ok = FindPersonOriginKey(person, &origin);
    if (ok) {
      ok = static_cast<RulesMgr*>(rules)->RulesMgr::FindDestination(person, &source, &origin, flag, 1, out, &status);
    }
  } else {
    ok = 0;
  }
  if (ok && !statusOk(&status)) {
    out->Reset();
  }
  ObjectKey missionKey;
  if (static_cast<Role*>(person)->GetMissionKey(&missionKey)->Family() == kFamilyBountyMission) {
    ok = 1;
    out->m_value = kPalaceSystemKey;
  }
  return ok;
}

int MissionMgr::FindPersonMissionPoint(Role* person, Mission* mission, int flag, ObjectKey* out)
{
  int ok = 1;
  out->Reset();
  ObjectKey missionKey;
  if (mission_exec::InFamilies(mission->GetKey(&missionKey), kFamilyMissionsMid, kFamilyMissionsEnd)) {
    GameObjNotifier* rules = GetGameRootMember_bc();
    ok = rules != nullptr;
    StatusCode status(1, 1);
    if (rules) {
      if (flag != 0) {
        ok = static_cast<RulesMgr*>(rules)->RulesMgr::CheckParentCapacity(person, &status);
        if (ok && statusOk(&status)) {
          ok = person->GetValidParentKey(out);
        }
      }
      if (ok && !statusOk(&status)) {
        ok = static_cast<RulesMgr*>(rules)->RulesMgr::CheckUnitMove(person, &person->m_parentAtMissionCompletionKey,
                                                                    &status);
        if (ok && statusOk(&status)) {
          *out = person->m_parentAtMissionCompletionKey;
        }
      }
    }
  }
  if (!out->IsSet()) {
    ok = FindPersonDestination(person, flag, out);
  }
  return ok;
}
