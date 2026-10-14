#include "story/story_missions.h"
#include "character/character_mgr.h"

#include "ai/ai_common.h"
#include "character/character.h"
#include "core/checked_new.h"
#include "data/random.h"
#include "galaxy/mission_decl.h"
#include "gameobj/object_family.h"
#include "gameobj/object_key.h"
#include "mission/mission_external.h"
#include "mission/mission_rules.h"
#include "story/story_common.h"

extern int g_missionPickupEventKindsReady; // 0x6bb994
extern EventKindSet g_missionPickupEventKinds; // 0x6bb998
extern int g_missionPalaceEventKindsReady; // 0x6bb9a4
extern EventKindSet g_missionPalaceEventKinds; // 0x6bb9a8
extern int g_missionVacationEventKindsReady; // 0x6bba48
extern EventKindSet g_missionVacationEventKinds; // 0x6bba40
extern int g_dagobahMissionEventKindsReady; // 0x6bbab4
extern EventKindSet g_dagobahMissionEventKinds; // 0x6bbab8
extern int g_missionBountyEventKindsReady; // 0x6bbad4
extern EventKindSet g_missionBountyEventKinds; // 0x6bbad8

GameObjNotifier* GetGameRootMember_c0();
GameObjNotifier* GetMaster_32000242();
int ComputePalaceMissionValue(int espionage, int combat, int* out);
int ComputeDagobahBonusA(int force, int* out);
int ComputeDagobahBonusB(int force, int days, int* out);

namespace
{

constexpr uint32_t kFamilyPickup = 0x73;
constexpr uint32_t kFamilyPalace = 0x64;
constexpr uint32_t kFamilyVacation = 0x72;
constexpr uint32_t kFamilyDagobah = 0x71;
constexpr uint32_t kFamilyBounty = 0x65;
constexpr uint32_t kFamilyPickupPassenger = 0x35;
constexpr uint32_t kFamilyVacationTraveller = 0x32;
constexpr uint32_t kFamilyHan = 0x33;
constexpr uint32_t kKeyPickupSystem = 0x98000481;
constexpr uint32_t kKeyVacationSystem = 0x98000442;
constexpr uint32_t kKeyDagobahSystem = 0x98000441;
constexpr uint32_t kKeyHan = 0x33000243;
constexpr uint32_t kKeyLuke = 0x32000242;
constexpr uint32_t kKeyLeia = 0x31000241;
constexpr uint32_t kKeyCharacter38 = 0x38000343;
constexpr uint32_t kKeyBountyHunters = 0x3c000009;
constexpr uint32_t kIdLuke = 0x242;
constexpr uint32_t kIdLeia = 0x241;
constexpr uint32_t kIdCharacter38 = 0x343;

constexpr int kStatusSourceOrder = 0x40;
constexpr int kReasonDecoy = 8;
constexpr int kReasonCaptured = 9;
constexpr int kReasonWrongTraveller = 7;
constexpr int kReasonWrongPassenger = 0xc;
constexpr int kReasonWrongStoryCharacter = 10;
constexpr int kReasonWrongBountyUnit = 0xb;
constexpr int kReasonPalaceOrigin = 0x15;
constexpr int kReasonPalaceHan = 0x30;
constexpr int kReasonPickupTarget = 0x31;
constexpr int kReasonVacationTarget = 0x2e;
constexpr int kReasonDagobahTarget = 0x2f;
constexpr int kReasonBountyHan = 0x30;

constexpr int kTaskStarted = 2;
constexpr int kTaskSucceeded = 3;
constexpr int kPhaseTrainingStarted = 8;
constexpr int kPhaseTrainingEnded = 0xb;
constexpr int kTrainingLevelNeeded = 8;
constexpr int kConstantMissionValue = 100;

// The record the order-check callbacks of slot 111 receive, as the story missions read it: the original names its fields by
// offset (+0x04 team flag, +0x08 key-set flag, +0x0c target flag, +0x10 side, +0x18.. reference lists, +0x24.. key sets, +0x30
// summary, +0x34 palace origin, +0x38 target); a copy of those fields of MissionCheckArgs, so that the checks do not depend on
// the record's memory layout.
struct StoryOrderCheck
{
  int m_hasTeam;
  int m_useKeySets; // 0 = the three object-reference lists, else the three key sets
  int m_hasTarget;
  int m_side;
  ObjectRefList* m_refLists[3];
  ObjectKeySet* m_keySets[3];
  GameObjRoot* m_palaceOrigin; // read by the Palace check only
  GameObjRoot* m_target;

  explicit StoryOrderCheck(const MissionCheckArgs* args)
  : m_hasTeam(args->m_flag04),
    m_useKeySets(args->m_flag08),
    m_hasTarget(args->m_flag0c),
    m_side(args->m_controlSide),
    m_refLists{static_cast<ObjectRefList*>(args->m_refs[0]), static_cast<ObjectRefList*>(args->m_refs[1]),
               static_cast<ObjectRefList*>(args->m_refs[2])},
    m_keySets{reinterpret_cast<ObjectKeySet*>(args->m_team), reinterpret_cast<ObjectKeySet*>(args->m_decoys),
              reinterpret_cast<ObjectKeySet*>(args->m_captured)},
    m_palaceOrigin(args->m_target),
    m_target(args->m_objective)
  {
  }
};

bool statusOk(const StatusCode* status)
{
  return status->m_second == static_cast<uint32_t>(story::kStatusOk);
}

void reject(StatusCode* status, int* out, int reason)
{
  *status = StatusCode(kStatusSourceOrder, reason);
  *out = -1;
}

uint32_t keyOf(GameObjRoot* object)
{
  ObjectKey key;
  return object->GetKey(&key)->m_value;
}

void rejectCapturedOrDecoy(Character* member, StatusCode* status, int* out)
{
  if (statusOk(status) && (member->m_flagsAc & 1))
    reject(status, out, kReasonCaptured);
  if (statusOk(status) && (member->m_flags78 & role_flag::kDecoy))
    reject(status, out, kReasonDecoy);
}

// Walks the candidate keys of one team set: every key must satisfy `isAllowed`, name a character that is neither captured
// nor a decoy.
template <bool (*IsAllowed)(const ObjectKey&), int Reason>
int checkMemberKeys(ObjectKeySet* keys, StatusCode* status, int* out)
{
  int result = 1;
  *status = StatusCode(story::kStatusCategory, story::kStatusOk);
  *out = 0;
  for (const auto& nodeEntry : keys->Entries())
  {
    auto* node = nodeEntry.get();
    if (!(result && statusOk(status)))
      break;

    ObjectKey key;
    static_cast<ObjectKeyNode*>(node)->GetKey(&key);
    if (!IsAllowed(key))
      reject(status, out, Reason);
    if (statusOk(status))
    {
      ObjectKey lookupKey;
      static_cast<ObjectKeyNode*>(node)->GetKey(&lookupKey);
      GameObj* any;
      GameObj* specialForces;
      GameObj* character;
      result = SplitRoleFromKey(&lookupKey, &any, &specialForces, &character);
      if (!result)
        continue;
      result = character != nullptr;
      if (result)
        rejectCapturedOrDecoy(static_cast<Character*>(character), status, out);
    }
  }
  return result;
}

template <bool (*IsAllowed)(const ObjectKey&), int Reason>
int checkMemberRefs(ObjectRefList* refs, StatusCode* status, int* out)
{
  *status = StatusCode(story::kStatusCategory, story::kStatusOk);
  *out = 0;
  for (const auto& nodeEntry : refs->Entries())
  {
    auto* node = nodeEntry.get();
    if (!(statusOk(status)))
      break;

    ObjectKey key;
    static_cast<ObjectRefNode*>(node)->GetKey(&key);
    if (!IsAllowed(key))
      reject(status, out, Reason);
  }
  return 1;
}

// The three candidate sets of a check run in order, each only while the status is still ok.
template <class Set, int (*Check)(Set*, MissionCheckArgs*, StatusCode*, int*)>
int checkThreeSets(MissionCheckArgs* check, Set* const (&sets)[3], StatusCode* status, int* out, int result)
{
  if (!result)
    return result;
  if (statusOk(status))
    result = Check(sets[0], check, status, out);
  if (!result)
    return result;
  if (statusOk(status))
    result = Check(sets[1], check, status, out);
  if (result && statusOk(status))
    result = Check(sets[2], check, status, out);
  return result;
}

template <int (*CheckRefs)(ObjectRefList*, MissionCheckArgs*, StatusCode*, int*),
          int (*CheckKeys)(ObjectKeySet*, MissionCheckArgs*, StatusCode*, int*)>
int checkCandidates(MissionCheckArgs* check, StatusCode* status, int* out, int result)
{
  StoryOrderCheck fieldsView(check);
  StoryOrderCheck* fields = &fieldsView;
  if (fields->m_useKeySets == 0)
    return checkThreeSets<ObjectRefList, CheckRefs>(check, fields->m_refLists, status, out, result);
  return checkThreeSets<ObjectKeySet, CheckKeys>(check, fields->m_keySets, status, out, result);
}

bool isPassengerKey(const ObjectKey& key)
{
  return key.Family() == kFamilyPickupPassenger;
}
bool isTravellerKey(const ObjectKey& key)
{
  return key.Family() == kFamilyVacationTraveller;
}
bool isPalaceCharacterKey(const ObjectKey& key)
{
  return key.m_value == kKeyLuke || key.m_value == kKeyLeia || key.m_value == kKeyCharacter38;
}
bool isPalaceCharacterId(const ObjectKey& key)
{
  const uint32_t id = key.Id();
  return id == kIdLuke || id == kIdLeia || id == kIdCharacter38;
}

// The status record of a target check starts as success.
void beginCheck(StatusCode* status, int* out)
{
  *status = StatusCode(story::kStatusCategory, story::kStatusOk);
  *out = 0;
}

// Slot 159 of Vacation and Dagobah: the mission succeeds as soon as it is told to resolve.
int succeedMission(Mission* mission, int* out, NotifyContext* ctx)
{
  *out = 1;
  return mission->SetTaskStatus(kTaskSucceeded, ctx) != 0;
}

// Slot 160 of Vacation and Dagobah: a mission that has not started yet starts.
int startMissionIfIdle(Mission* mission, NotifyContext* ctx)
{
  int result = 1;
  if (mission->m_taskStatus == 0)
    result = mission->SetTaskStatus(kTaskStarted, ctx);
  return result;
}

int constantMissionValue(int* out)
{
  *out = kConstantMissionValue;
  return 1;
}

void* asRef(ObjectKey* key)
{
  return key;
}

intptr_t keyAsArgument(ObjectKey* key)
{
  return reinterpret_cast<intptr_t>(asRef(key));
}

} // namespace

// ---- shared second-level forwarders of the story missions -----------------------------------------------------------

int MissionStoryReadForward(Mission* self, istream* in)
{
  return self->Mission::ReadImpl(in);
}

int MissionStoryWriteForward(Mission* self, ostream* out)
{
  return self->Mission::WriteImpl(out);
}

int MissionStoryApplyAllForward(Mission* self, NotifyContext* ctx)
{
  return self->Mission::ApplyAllImpl(ctx);
}

int MissionStoryChecksumForward(Mission* self, Crc32* crc)
{
  return self->Mission::ChecksumImpl(crc);
}

// ---- MissionPickup ----------------------------------------------------------------------------------------------

MissionPickup::MissionPickup() = default;

MissionPickup::~MissionPickup() = default;

uint32_t MissionPickup::GetFamily()
{
  return kFamilyPickup;
}

int MissionPickup::Read(istream* in)
{
  return MissionStoryReadForward(this, in);
}

int MissionPickup::Write(ostream* out)
{
  return MissionStoryWriteForward(this, out);
}

int MissionPickup::Checksum(Crc32* crc)
{
  return MissionStoryChecksumForward(this, crc);
}

int MissionPickup::Validate()
{
  return this->Mission::ValidateImpl();
}

int MissionPickup::ApplyAll(NotifyContext* ctx)
{
  return IsSideView() ? MissionStoryApplyAllForward(this, ctx) : 0;
}

int MissionPickup::CreateViews(int controlKind, NotifyContext* ctx)
{
  return this->Mission::CreateViewsImpl(controlKind, ctx);
}

int MissionPickup::GetEventKinds(int unused, EventKindSet* out)
{
  return CopyEventKinds(BuildEventKinds, &g_missionPickupEventKinds, out);
}

int MissionPickup::CheckOrder(int a, int orderKind, StatusCode* out)
{
  return CheckOrderAgainstKinds(BuildEventKinds, &g_missionPickupEventKinds, orderKind, out);
}

int MissionPickup::GetCheckFunctions(MissionCheckFunctions* out)
{
  out->m_unk00 = reinterpret_cast<void*>(MissionRuleBase::CheckMember);
  out->m_unk04 = reinterpret_cast<void*>(CheckTarget);
  return 1;
}

GameObj* MissionPickup::CreateViewObject()
{
  return NewOrNull<MissionPickup>();
}

GameObjNotifier* MissionPickup::Create()
{
  return NewOrNull<MissionPickup>();
}

int MissionPickup::GetPersonSuccessChance(GameObj* person, int* out)
{
  return constantMissionValue(out);
}

int MissionPickup::BeforeScoringResults(NotifyContext* ctx)
{
  return 1;
}

int MissionPickup::AfterScoringResults(NotifyContext* ctx)
{
  return MissionStartIfIdle(this, ctx);
}

int MissionPickup::ApplyPersonResult(GameObj* person, int roll, int* out, NotifyContext* ctx)
{
  GameObj* objective = nullptr;
  const int found = GetTargetLocationSystem(&objective);
  CharacterMgr* manager = GetCharacterManager();
  int result = (manager && found) ? 1 : 0;
  if (!manager || !objective)
    return result;

  bool ok = manager->SetPickupInProgress(1, ctx) && result;
  CharacterIterator members(objective, 1, ControlSide());
  for (members.Begin(); members.Current(); members.Advance())
  {
    auto* member = static_cast<Character*>(members.Current());
    if ((member->m_flags78 & role_flag::kOnMission) || !(member->m_flagsAc & 1))
      continue;
    *out = 1;
    ObjectKey memberKey;
    member->GetKey(&memberKey);
    ok = AddMember(&memberKey, 0, ctx) && ok;
    ObjectKey homeKey;
    ObjectKey* home = static_cast<Role*>(person)->GetParentAtMissionCompletionKey(&homeKey);
    ok = member->SetParentAtMissionCompletionKey(home, ctx) && ok;
    ok = SetTaskStatus(kTaskSucceeded, ctx) && ok;
  }
  return manager->SetPickupInProgress(0, ctx) && ok ? 1 : 0;
}

int MissionPickup::CheckCandidateKeys(ObjectKeySet* keys, MissionCheckArgs* check, StatusCode* status, int* out)
{
  return checkMemberKeys<isPassengerKey, kReasonWrongPassenger>(keys, status, out);
}

int MissionPickup::CheckCandidateRefs(ObjectRefList* refs, MissionCheckArgs* check, StatusCode* status, int* out)
{
  return checkMemberRefs<isPassengerKey, kReasonWrongPassenger>(refs, status, out);
}

int MissionPickup::CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out)
{
  beginCheck(status, out);
  int result = MissionStoryBase::CheckTarget(reinterpret_cast<MissionCheckArgs*>(check), status, out);
  StoryOrderCheck fieldsView(check);
  StoryOrderCheck* fields = &fieldsView;
  if (fields->m_hasTeam != 0)
  {
    if (fields->m_hasTarget != 0 && result && statusOk(status) && keyOf(fields->m_target) != kKeyPickupSystem)
      reject(status, out, kReasonPickupTarget);
    result = checkCandidates<CheckCandidateRefs, CheckCandidateKeys>(check, status, out, result);
  }
  return result;
}

int MissionPickup::BuildEventKinds()
{
  return story::buildKindsOnce(&g_missionPickupEventKindsReady,
                               []
                               {
                                 return MissionStoryBase::InitEventKinds(0, &g_missionPickupEventKinds);
                               });
}

// ---- MissionPalace ----------------------------------------------------------------------------------------------

MissionPalace::MissionPalace() = default;

MissionPalace::~MissionPalace() = default;

uint32_t MissionPalace::GetFamily()
{
  return kFamilyPalace;
}

int MissionPalace::Read(istream* in)
{
  return MissionStoryReadForward(this, in);
}

int MissionPalace::Write(ostream* out)
{
  return MissionStoryWriteForward(this, out);
}

int MissionPalace::Checksum(Crc32* crc)
{
  return MissionStoryChecksumForward(this, crc);
}

int MissionPalace::Validate()
{
  return this->Mission::ValidateImpl();
}

int MissionPalace::ApplyAll(NotifyContext* ctx)
{
  return IsSideView() ? MissionStoryApplyAllForward(this, ctx) : 0;
}

int MissionPalace::CreateViews(int controlKind, NotifyContext* ctx)
{
  return this->Mission::CreateViewsImpl(controlKind, ctx);
}

int MissionPalace::GetEventKinds(int unused, EventKindSet* out)
{
  return CopyEventKinds(BuildEventKinds, &g_missionPalaceEventKinds, out);
}

int MissionPalace::CheckOrder(int a, int orderKind, StatusCode* out)
{
  return CheckOrderAgainstKinds(BuildEventKinds, &g_missionPalaceEventKinds, orderKind, out);
}

int MissionPalace::GetCheckFunctions(MissionCheckFunctions* out)
{
  out->m_unk00 = reinterpret_cast<void*>(MissionRuleBase::CheckMember);
  out->m_unk04 = reinterpret_cast<void*>(CheckTarget);
  return 1;
}

GameObj* MissionPalace::CreateViewObject()
{
  return NewOrNull<MissionPalace>();
}

GameObjNotifier* MissionPalace::Create()
{
  return NewOrNull<MissionPalace>();
}

int MissionPalace::BeforeScoringResults(NotifyContext* ctx)
{
  return 1;
}

int MissionPalace::GetPersonSuccessChance(GameObj* person, int* out)
{
  Role* role = static_cast<Role*>(person);
  const int combat = role->GetCombat();
  const int espionage = role->GetEspionage();
  return ComputePalaceMissionValue(espionage, combat, out);
}

int MissionPalace::ApplyPersonResult(GameObj* person, int roll, int* out, NotifyContext* ctx)
{
  int result = 1;
  *out = 0;
  if (!PercentChance(roll))
    return result;
  *out = 1;
  int ok = SetTaskStatus(kTaskSucceeded, ctx) ? 1 : 0;
  Role* role = static_cast<Role*>(person);
  if (role->IsCharacter())
  {
    ok = role->SetBaseCombat(role->m_baseCombat + g_gnprtb181A, ctx) && ok ? 1 : 0;
    ok = role->SetBaseEspionage(role->m_baseEspionage + g_gnprtb1819, ctx) && ok ? 1 : 0;
  }
  GameObj* objectiveCharacter = nullptr;
  ok = GetObjectiveCharacter(&objectiveCharacter) && ok ? 1 : 0;
  if (objectiveCharacter)
  {
    ObjectKey personKey;
    ok = static_cast<Character*>(objectiveCharacter)->AssignHolderAndState(person->GetKey(&personKey), ctx) && ok ? 1 : 0;
  }
  GameObj* objective = nullptr;
  result = GetTargetObject(&objective) && ok ? 1 : 0;
  if (!objective)
    return result;
  CharacterIterator members(objective, 1);
  for (members.Begin(); members.Current(); members.Advance())
  {
    auto* member = static_cast<Character*>(members.Current());
    if (!(member->m_flagsAc & 1))
      continue;
    ObjectKey personKey;
    result = member->AssignHolderAndState(person->GetKey(&personKey), ctx) && result ? 1 : 0;
  }
  return result;
}

int MissionPalace::AfterScoringResults(NotifyContext* ctx)
{
  int result = 1;
  if (m_taskStatus != 0)
    return result;
  result = SetTaskStatus(kTaskStarted, ctx) ? 1 : 0;
  MissionCharacterIterator members(this);
  ObjectKey noHolder;
  for (members.Begin(); members.Current(); members.Advance())
  {
    auto* member = static_cast<Character*>(members.Current());
    result = member->HandleCaptureByOpponent(&noHolder, ctx) && result ? 1 : 0;
    result = member->SetCanEscape(0, ctx) && result ? 1 : 0;
  }
  return result;
}

int MissionPalace::CheckCandidateKeys(ObjectKeySet* keys, MissionCheckArgs* check, StatusCode* status, int* out)
{
  return checkMemberKeys<isPalaceCharacterKey, kReasonWrongStoryCharacter>(keys, status, out);
}

int MissionPalace::CheckCandidateRefs(ObjectRefList* refs, MissionCheckArgs* check, StatusCode* status, int* out)
{
  return checkMemberRefs<isPalaceCharacterId, kReasonWrongStoryCharacter>(refs, status, out);
}

int MissionPalace::CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out)
{
  beginCheck(status, out);
  int result = MissionCharacterTargetBase::CheckTarget(reinterpret_cast<MissionCheckArgs*>(check), status, out);
  StoryOrderCheck fieldsView(check);
  StoryOrderCheck* fields = &fieldsView;
  if (fields->m_hasTeam != 0)
  {
    if (fields->m_hasTarget != 0 && result)
    {
      if (statusOk(status) && keyOf(fields->m_palaceOrigin) != kKeyPickupSystem)
        reject(status, out, kReasonPalaceOrigin);
      if (result && statusOk(status) && keyOf(fields->m_target) == kKeyHan)
        reject(status, out, kReasonPalaceHan);
    }
    result = checkCandidates<CheckCandidateRefs, CheckCandidateKeys>(check, status, out, result);
  }
  return result;
}

int MissionPalace::BuildEventKinds()
{
  return story::buildKindsOnce(&g_missionPalaceEventKindsReady,
                               []
                               {
                                 return MissionCharacterTargetBase::InitEventKinds(0, &g_missionPalaceEventKinds);
                               });
}

// ---- MissionVacation --------------------------------------------------------------------------------------------

MissionVacation::MissionVacation() = default;

MissionVacation::~MissionVacation() = default;

uint32_t MissionVacation::GetFamily()
{
  return kFamilyVacation;
}

int MissionVacation::Read(istream* in)
{
  return MissionStoryReadForward(this, in);
}

int MissionVacation::Write(ostream* out)
{
  return MissionStoryWriteForward(this, out);
}

int MissionVacation::Checksum(Crc32* crc)
{
  return MissionStoryChecksumForward(this, crc);
}

int MissionVacation::Validate()
{
  return this->Mission::ValidateImpl();
}

int MissionVacation::ApplyAll(NotifyContext* ctx)
{
  return IsSideView() ? MissionStoryApplyAllForward(this, ctx) : 0;
}

int MissionVacation::CreateViews(int controlKind, NotifyContext* ctx)
{
  return this->Mission::CreateViewsImpl(controlKind, ctx);
}

int MissionVacation::GetEventKinds(int unused, EventKindSet* out)
{
  return CopyEventKinds(BuildEventKinds, &g_missionVacationEventKinds, out);
}

int MissionVacation::CheckOrder(int a, int orderKind, StatusCode* out)
{
  return CheckOrderAgainstKinds(BuildEventKinds, &g_missionVacationEventKinds, orderKind, out);
}

int MissionVacation::GetCheckFunctions(MissionCheckFunctions* out)
{
  out->m_unk00 = reinterpret_cast<void*>(MissionRuleBase::CheckMember);
  out->m_unk04 = reinterpret_cast<void*>(CheckTarget);
  return 1;
}

GameObj* MissionVacation::CreateViewObject()
{
  return NewOrNull<MissionVacation>();
}

GameObjNotifier* MissionVacation::Create()
{
  return NewOrNull<MissionVacation>();
}

int MissionVacation::GetPersonSuccessChance(GameObj* person, int* out)
{
  return constantMissionValue(out);
}

int MissionVacation::BeforeScoringResults(NotifyContext* ctx)
{
  return 1;
}

int MissionVacation::ApplyPersonResult(GameObj* person, int roll, int* out, NotifyContext* ctx)
{
  return succeedMission(this, out, ctx);
}

int MissionVacation::AfterScoringResults(NotifyContext* ctx)
{
  return startMissionIfIdle(this, ctx);
}

int MissionVacation::CheckCandidateKeys(ObjectKeySet* keys, MissionCheckArgs* check, StatusCode* status, int* out)
{
  return checkMemberKeys<isTravellerKey, kReasonWrongTraveller>(keys, status, out);
}

int MissionVacation::CheckCandidateRefs(ObjectRefList* refs, MissionCheckArgs* check, StatusCode* status, int* out)
{
  return checkMemberRefs<isTravellerKey, kReasonWrongTraveller>(refs, status, out);
}

int MissionVacation::CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out)
{
  beginCheck(status, out);
  int result = MissionStoryBase::CheckTarget(reinterpret_cast<MissionCheckArgs*>(check), status, out);
  StoryOrderCheck fieldsView(check);
  StoryOrderCheck* fields = &fieldsView;
  if (fields->m_hasTeam != 0)
  {
    if (fields->m_hasTarget != 0 && result && statusOk(status) && keyOf(fields->m_target) != kKeyVacationSystem)
      reject(status, out, kReasonVacationTarget);
    result = checkCandidates<CheckCandidateRefs, CheckCandidateKeys>(check, status, out, result);
  }
  return result;
}

int MissionVacation::BuildEventKinds()
{
  return story::buildKindsOnce(&g_missionVacationEventKindsReady,
                               []
                               {
                                 return MissionStoryBase::InitEventKinds(0, &g_missionVacationEventKinds);
                               });
}

// ---- DagobahMission ---------------------------------------------------------------------------------------------

DagobahMission::DagobahMission() : m_firstTrainingDay(0)
{
}

DagobahMission::~DagobahMission() = default;

uint32_t DagobahMission::GetFamily()
{
  return kFamilyDagobah;
}

int DagobahMission::Read(istream* in)
{
  return ReadBody(in);
}

int DagobahMission::Write(ostream* out)
{
  return WriteBody(out);
}

int DagobahMission::Checksum(Crc32* crc)
{
  return ChecksumBody(crc);
}

int DagobahMission::Validate()
{
  return this->Mission::ValidateImpl();
}

int DagobahMission::ApplyAll(NotifyContext* ctx)
{
  return IsSideView() ? ApplyAllBody(ctx) : 0;
}

int DagobahMission::CreateViews(int controlKind, NotifyContext* ctx)
{
  return this->Mission::CreateViewsImpl(controlKind, ctx);
}

int DagobahMission::ReadBody(istream* in)
{
  const int result = MissionStoryReadForward(this, in);
  if (result)
    ReadValue32(in, &m_firstTrainingDay);
  return result;
}

int DagobahMission::WriteBody(ostream* out)
{
  const int result = MissionStoryWriteForward(this, out);
  if (result)
    WriteValue32(out, &m_firstTrainingDay);
  return result;
}

int DagobahMission::ChecksumBody(Crc32* crc)
{
  const int result = MissionStoryChecksumForward(this, crc);
  if (result)
    crc->AddInt(m_firstTrainingDay);
  return result;
}

int DagobahMission::ApplyAllBody(NotifyContext* ctx)
{
  const int result = MissionStoryApplyAllForward(this, ctx);
  if (result && ctx->m_flag04 == 1)
    ApplyFirstTrainingDay(ctx);
  return result;
}

int DagobahMission::GetEventKinds(int unused, EventKindSet* out)
{
  return CopyEventKinds(BuildEventKinds, &g_dagobahMissionEventKinds, out);
}

int DagobahMission::CheckOrder(int a, int orderKind, StatusCode* out)
{
  return CheckOrderAgainstKinds(BuildEventKinds, &g_dagobahMissionEventKinds, orderKind, out);
}

int DagobahMission::GetCheckFunctions(MissionCheckFunctions* out)
{
  out->m_unk00 = reinterpret_cast<void*>(MissionRuleBase::CheckMember);
  out->m_unk04 = reinterpret_cast<void*>(CheckTarget);
  return 1;
}

GameObj* DagobahMission::CreateViewObject()
{
  return NewOrNull<DagobahMission>();
}

GameObjNotifier* DagobahMission::Create()
{
  return NewOrNull<DagobahMission>();
}

int DagobahMission::GetPersonSuccessChance(GameObj* person, int* out)
{
  return constantMissionValue(out);
}

int DagobahMission::BeforeScoringResults(NotifyContext* ctx)
{
  return 1;
}

int DagobahMission::ApplyPersonResult(GameObj* person, int roll, int* out, NotifyContext* ctx)
{
  return succeedMission(this, out, ctx);
}

int DagobahMission::AfterScoringResults(NotifyContext* ctx)
{
  return startMissionIfIdle(this, ctx);
}

int DagobahMission::SetFirstTrainingDay(int value, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result)
  {
    const int oldValue = m_firstTrainingDay;
    if (oldValue != value)
    {
      m_firstTrainingDay = value;
      static_cast<DagobahMission*>(ViewAs(story::kViewAlliance))->ApplyFirstTrainingDay(ctx);
      static_cast<DagobahMission*>(ViewAs(story::kViewEmpire))->ApplyFirstTrainingDay(ctx);
      OnFirstTrainingDayChanged(oldValue, m_firstTrainingDay, ctx);
    }
  }
  return result;
}

int DagobahMission::ApplyFirstTrainingDay(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    auto* master = static_cast<DagobahMission*>(GetViewObject(0));
    if (MatchesSideMask(ctx->m_flag08))
    {
      const int oldValue = m_firstTrainingDay;
      if (oldValue != master->m_firstTrainingDay)
      {
        m_firstTrainingDay = master->m_firstTrainingDay;
        if (CallGameHook2AF0() && ctx->GetViewFlag(static_cast<int>(ViewSide())))
          result = OnFirstTrainingDayNotified(oldValue, m_firstTrainingDay, ctx);
      }
    }
  }
  return result;
}

int DagobahMission::OnFirstTrainingDayNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result)
    LogInt("(DagobahMissionFirstTrainingDayNotif)", "FirstTrainingDay", newValue, oldValue, 0, ctx);
  return result;
}

int DagobahMission::OnPhaseChanged(int oldValue, int newValue, NotifyContext* ctx)
{
  int ok = MissionStoryBase::OnPhaseChanged(oldValue, newValue, ctx) ? 1 : 0;
  if (newValue == kPhaseTrainingStarted)
    ok = SetFirstTrainingDay(CallGameHook2B08(), ctx) && ok ? 1 : 0;
  if (newValue != kPhaseTrainingEnded)
    return ok;
  if (GetDataField1c() < kTrainingLevelNeeded)
    return ok;
  const int firstDay = m_firstTrainingDay;
  if (firstDay == 0)
    return ok;
  const int days = CallGameHook2B08() - firstDay;
  int gain = 0;
  Luke* luke = static_cast<Luke*>(GetMaster_32000242());
  ok = luke && ok ? 1 : 0;
  if (!luke)
    return ok;
  const int computed = m_taskStatus == kTaskSucceeded ? ComputeDagobahBonusA(luke->m_force, &gain) :
                                                        ComputeDagobahBonusB(luke->m_force, days, &gain);
  ok = computed && ok ? 1 : 0;
  ok = luke->SetForceExperience(luke->m_forceExperience + gain - luke->m_force, ctx) && ok ? 1 : 0;
  return luke->SetDagobahCompleted(1, ctx) && ok ? 1 : 0;
}

int DagobahMission::CheckCandidateKeys(ObjectKeySet* keys, MissionCheckArgs* check, StatusCode* status, int* out)
{
  return checkMemberKeys<isTravellerKey, kReasonWrongTraveller>(keys, status, out);
}

int DagobahMission::CheckCandidateRefs(ObjectRefList* refs, MissionCheckArgs* check, StatusCode* status, int* out)
{
  return checkMemberRefs<isTravellerKey, kReasonWrongTraveller>(refs, status, out);
}

int DagobahMission::CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out)
{
  beginCheck(status, out);
  int result = MissionStoryBase::CheckTarget(reinterpret_cast<MissionCheckArgs*>(check), status, out);
  StoryOrderCheck fieldsView(check);
  StoryOrderCheck* fields = &fieldsView;
  if (fields->m_hasTeam != 0)
  {
    if (fields->m_hasTarget != 0 && result && statusOk(status) && keyOf(fields->m_target) != kKeyDagobahSystem)
      reject(status, out, kReasonDagobahTarget);
    result = checkCandidates<CheckCandidateRefs, CheckCandidateKeys>(check, status, out, result);
  }
  return result;
}

int DagobahMission::BuildEventKinds()
{
  return story::buildKindsOnce(&g_dagobahMissionEventKindsReady,
                               []
                               {
                                 return MissionStoryBase::InitEventKinds(0, &g_dagobahMissionEventKinds);
                               });
}

// ---- MissionBounty ----------------------------------------------------------------------------------------------

MissionBounty::MissionBounty() = default;

MissionBounty::~MissionBounty() = default;

uint32_t MissionBounty::GetFamily()
{
  return kFamilyBounty;
}

int MissionBounty::Read(istream* in)
{
  return MissionStoryReadForward(this, in);
}

int MissionBounty::Write(ostream* out)
{
  return MissionStoryWriteForward(this, out);
}

int MissionBounty::Checksum(Crc32* crc)
{
  return MissionStoryChecksumForward(this, crc);
}

int MissionBounty::Validate()
{
  return this->Mission::ValidateImpl();
}

int MissionBounty::ApplyAll(NotifyContext* ctx)
{
  return IsSideView() ? MissionStoryApplyAllForward(this, ctx) : 0;
}

int MissionBounty::CreateViews(int controlKind, NotifyContext* ctx)
{
  return this->Mission::CreateViewsImpl(controlKind, ctx);
}

int MissionBounty::GetEventKinds(int unused, EventKindSet* out)
{
  return CopyEventKinds(BuildEventKinds, &g_missionBountyEventKinds, out);
}

int MissionBounty::CheckOrder(int a, int orderKind, StatusCode* out)
{
  return CheckOrderAgainstKinds(BuildEventKinds, &g_missionBountyEventKinds, orderKind, out);
}

int MissionBounty::GetCheckFunctions(MissionCheckFunctions* out)
{
  out->m_unk00 = reinterpret_cast<void*>(MissionRuleBase::CheckMember);
  out->m_unk04 = reinterpret_cast<void*>(CheckTarget);
  return 1;
}

GameObj* MissionBounty::CreateViewObject()
{
  return NewOrNull<MissionBounty>();
}

GameObjNotifier* MissionBounty::Create()
{
  return NewOrNull<MissionBounty>();
}

int MissionBounty::GetPersonSuccessChance(GameObj* person, int* out)
{
  return constantMissionValue(out);
}

int MissionBounty::BeforeScoringResults(NotifyContext* ctx)
{
  return 1;
}

int MissionBounty::AfterScoringResults(NotifyContext* ctx)
{
  return 1;
}

int MissionBounty::ApplyPersonResult(GameObj* person, int roll, int* out, NotifyContext* ctx)
{
  *out = 1;
  GameObj* target = nullptr;
  int ok = GetObjectiveCharacter(&target) ? 1 : 0;
  Han* han = nullptr;
  if (target)
  {
    ok = target->GetFamily() == kFamilyHan && ok ? 1 : 0;
    if (ok)
      han = static_cast<Han*>(target);
  }
  if (!han)
    return ok;
  ok = han->SetBountyAttack(1, ctx) && ok ? 1 : 0;
  ObjectKey personKey;
  ok = han->RollCombatCheck(person->GetKey(&personKey), 0, ctx) && ok ? 1 : 0;
  int newStatus = kTaskStarted;
  if (han->m_flagsAc & 1)
  {
    newStatus = kTaskSucceeded;
    ok = han->SetCanEscape(0, ctx) && ok ? 1 : 0;
    ok = han->SetCapturedByBountyHunters(1, ctx) && ok ? 1 : 0;
  }
  ok = SetTaskStatus(newStatus, ctx) && ok ? 1 : 0;
  return han->SetBountyAttack(0, ctx) && ok ? 1 : 0;
}

namespace
{

bool isBountyUnitKey(const ObjectKey& key)
{
  return key.m_value == kKeyBountyHunters;
}

} // namespace

int MissionBounty::CheckCandidateKeys(ObjectKeySet* keys, MissionCheckArgs* check, StatusCode* status, int* out)
{
  int result = 1;
  beginCheck(status, out);
  for (const auto& nodeEntry : keys->Entries())
  {
    auto* node = nodeEntry.get();
    if (!(result && statusOk(status)))
      break;

    ObjectKey key;
    static_cast<ObjectKeyNode*>(node)->GetKey(&key);
    GameObj* any;
    GameObj* specialForces;
    GameObj* character;
    const int found = SplitRoleFromKey(&key, &any, &specialForces, &character);
    result = found && specialForces ? 1 : 0;
    if (!result)
      continue;
    if (statusOk(status))
    {
      DatRecord* record = static_cast<GameObjNotifier*>(specialForces)->m_record;
      result = record != nullptr;
      if (!result)
        continue;
      ObjectKey recordKey;
      if (!isBountyUnitKey(*GetAiRecordKey(record, &recordKey)))
        reject(status, out, kReasonWrongBountyUnit);
    }
    if (statusOk(status) && (static_cast<Role*>(specialForces)->m_flags78 & role_flag::kDecoy))
      reject(status, out, kReasonDecoy);
  }
  return result;
}

int MissionBounty::CheckCandidateRefs(ObjectRefList* refs, MissionCheckArgs* check, StatusCode* status, int* out)
{
  return checkMemberRefs<isBountyUnitKey, kReasonWrongBountyUnit>(refs, status, out);
}

int MissionBounty::CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out)
{
  beginCheck(status, out);
  int result = MissionCharacterTargetBase::CheckTarget(reinterpret_cast<MissionCheckArgs*>(check), status, out);
  StoryOrderCheck fieldsView(check);
  StoryOrderCheck* fields = &fieldsView;
  if (fields->m_hasTeam != 0)
  {
    if (fields->m_hasTarget != 0 && result && statusOk(status) && keyOf(fields->m_target) == kKeyHan)
      reject(status, out, kReasonBountyHan);
    result = checkCandidates<CheckCandidateRefs, CheckCandidateKeys>(check, status, out, result);
  }
  return result;
}

int MissionBounty::BuildEventKinds()
{
  return story::buildKindsOnce(&g_missionBountyEventKindsReady,
                               []
                               {
                                 return MissionCharacterTargetBase::InitEventKinds(0, &g_missionBountyEventKinds);
                               });
}
EventKindSet g_dagobahMissionEventKinds{};
EventKindSet g_missionBountyEventKinds{};
EventKindSet g_missionPalaceEventKinds{};
EventKindSet g_missionPickupEventKinds{};
EventKindSet g_missionVacationEventKinds{};
int g_dagobahMissionEventKindsReady{};
int g_missionBountyEventKindsReady{};
int g_missionPalaceEventKindsReady{};
int g_missionPickupEventKindsReady{};
int g_missionVacationEventKindsReady{};
