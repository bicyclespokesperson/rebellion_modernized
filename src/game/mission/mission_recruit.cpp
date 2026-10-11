#include "mission/mission_recruit.h"
#include "character/character_mgr.h"

#include "data/random.h"
#include "economy/side.h"
#include "galaxy/mission_decl.h"
#include "gameobj/game_hooks.h"
#include "gameobj/object_queries.h"
#include "mission/mission_rules.h"

extern int g_missionRecruitEventKindsReady; // 0x6bb968
extern EventKindSet g_missionRecruitEventKinds; // 0x6bb960

namespace
{
constexpr uint32_t kFamilyMissionRecruit = 0x55;
constexpr int kTaskSucceeded = 3;
constexpr uint32_t kFirstCharacterFamily = 0x30;
constexpr uint32_t kEndCharacterFamily = 0x38;
constexpr int kStatusCategoryBlocked = 0x40;
constexpr int kStatusNotACharacter = 6;
constexpr int kStatusRecruitmentDone = 0x35;
constexpr int kOutBlockedByRecruitmentDone = 0x10;

int ControlSideIndex(uint32_t controlSide)
{
  if (controlSide == 3)
    return 3;
  return controlSide == 1 ? 2 : 1;
}

template <class Set>
struct ThreeSets
{
  Set* m_set[3];
};

ThreeSets<ObjectRefList> RefSetsOf(MissionCheckArgs* check)
{
  return {{static_cast<ObjectRefList*>(check->m_refs[0]), static_cast<ObjectRefList*>(check->m_refs[1]),
           static_cast<ObjectRefList*>(check->m_refs[2])}};
}

ThreeSets<ObjectKeySet> KeySetsOf(MissionCheckArgs* check)
{
  return {{check->m_team, check->m_decoys, check->m_captured}};
}

int CheckedSideOf(int kind)
{
  return kind == 1 ? 1 : (kind == 2 ? 2 : 0);
}

bool IsCharacterFamily(uint32_t family)
{
  return family >= kFirstCharacterFamily && family < kEndCharacterFamily;
}

void RejectNonCharacter(StatusCode* status, int* out)
{
  *status = StatusCode(kStatusCategoryBlocked, kStatusNotACharacter);
  *out = -1;
}

bool StatusIsOk(const StatusCode* status)
{
  return status->m_second == static_cast<uint32_t>(mission_status::kOk);
}

// Runs the three set checks of one target flavour in order; each runs only while the status is still ok.
template <class Set, int (*Check)(Set*, MissionCheckArgs*, StatusCode*, int*)>
int CheckThreeSets(MissionCheckArgs* check, StatusCode* status, int* out, ThreeSets<Set> sets, int result)
{
  if (!result)
    return result;
  if (StatusIsOk(status))
    result = Check(sets.m_set[0], check, status, out);
  if (!result)
    return result;
  if (StatusIsOk(status))
    result = Check(sets.m_set[1], check, status, out);
  if (result && StatusIsOk(status))
    result = Check(sets.m_set[2], check, status, out);
  return result;
}
} // namespace

MissionRecruit::MissionRecruit() = default;

MissionRecruit::~MissionRecruit() = default;

int MissionRecruit::Read(istream* in)
{
  if (!this->Mission::ReadImpl(in))
    return 0;
  return m_recruitedCharacter.Read(in);
}

int MissionRecruit::Write(ostream* out)
{
  if (!WriteImpl(out))
    return 0;
  return m_recruitedCharacter.Write(out);
}

int MissionRecruit::Checksum(Crc32* crc)
{
  if (!this->Mission::ChecksumImpl(crc))
    return 0;
  return reinterpret_cast<SideBits*>(&m_recruitedCharacter)->Checksum(crc);
}

int MissionRecruit::Validate()
{
  return this->Mission::ValidateImpl();
}

int MissionRecruit::ApplyAll(NotifyContext* ctx)
{
  return IsSideView() ? ApplyAllBody(ctx) : 0;
}

int MissionRecruit::CreateViews(int controlKind, NotifyContext* ctx)
{
  return this->Mission::CreateViewsImpl(controlKind, ctx);
}

int MissionRecruit::ApplyAllBody(NotifyContext* ctx)
{
  const int result = ApplyAllImpl(ctx);
  if (result && ctx->m_flag04 == 1)
    ApplyRecruitedCharacter(ctx);
  return result;
}

uint32_t MissionRecruit::GetFamily()
{
  return kFamilyMissionRecruit;
}

int MissionRecruit::GetEventKinds(int unused, EventKindSet* out)
{
  return CopyEventKinds(BuildEventKinds, &g_missionRecruitEventKinds, out);
}

int MissionRecruit::CheckOrder(int a, int orderKind, StatusCode* out)
{
  return CheckOrderAgainstKinds(BuildEventKinds, &g_missionRecruitEventKinds, orderKind, out);
}

int MissionRecruit::GetCheckFunctions(MissionCheckFunctions* out)
{
  out->m_unk00 = reinterpret_cast<void*>(MissionRuleBase::CheckMember);
  out->m_unk04 = reinterpret_cast<void*>(CheckTarget);
  return 1;
}

GameObj* MissionRecruit::CreateViewObject()
{
  return NewOrNull<MissionRecruit>();
}

GameObjNotifier* MissionRecruit::Create()
{
  return NewOrNull<MissionRecruit>();
}

int MissionRecruit::SetRecruitedCharacter(ObjectKey* key, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && m_recruitedCharacter.m_value != key->m_value)
  {
    ObjectKey oldKey(m_recruitedCharacter);
    m_recruitedCharacter = *key;
    static_cast<MissionRecruit*>(ViewAs(1))->ApplyRecruitedCharacter(ctx);
    static_cast<MissionRecruit*>(ViewAs(2))->ApplyRecruitedCharacter(ctx);
    OnRecruitedCharacterChanged(&oldKey, &m_recruitedCharacter, ctx);
  }
  return result;
}

int MissionRecruit::ApplyRecruitedCharacter(NotifyContext* ctx)
{
  int result = IsSideView();
  if (!result)
    return result;
  MissionRecruit* master = static_cast<MissionRecruit*>(GetViewObject(0));
  if (!MatchesSideMask(ctx->m_flag08))
    return result;
  if (m_recruitedCharacter.m_value == master->m_recruitedCharacter.m_value)
    return result;
  ObjectKey oldKey(m_recruitedCharacter);
  m_recruitedCharacter = master->m_recruitedCharacter;
  if (CallGameHook2AF0() && ctx->GetViewFlag(static_cast<int>(ViewSide())))
    result = OnRecruitedCharacterNotified(&oldKey, &m_recruitedCharacter, ctx);
  return result;
}

int MissionRecruit::OnRecruitedCharacterNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result)
    LogKey("(MissionRecruitRecruitedCharacterKeyNotif)", "RecruitedCharacter", newKey, oldKey, 0, ctx);
  return result;
}

int MissionRecruit::CheckCandidateKeys(ObjectKeySet* keys, MissionCheckArgs* check, StatusCode* status, int* out)
{
  *status = MakeStatus(mission_status::kOk);
  *out = 0;
  for (const auto& nodeEntry : keys->Entries())
  {
    ObjectKeyNode* node = static_cast<ObjectKeyNode*>(nodeEntry.get());
    if (!(StatusIsOk(status)))
      break;

    ObjectKey key;
    node->GetKey(&key);
    if (!IsCharacterFamily(key.Family()))
      RejectNonCharacter(status, out);
  }
  return 1;
}

int MissionRecruit::CheckCandidateRefs(ObjectRefList* refs, MissionCheckArgs* check, StatusCode* status, int* out)
{
  *status = MakeStatus(mission_status::kOk);
  *out = 0;
  for (const auto& nodeEntry : refs->Entries())
  {
    AvlNode* node = static_cast<AvlNode*>(nodeEntry.get());
    if (!(StatusIsOk(status)))
      break;

    ObjectKey key;
    static_cast<ObjectRefNode*>(node)->GetKey(&key);
    if (!IsCharacterFamily(key.Family()))
      RejectNonCharacter(status, out);
  }
  return 1;
}

int MissionRecruit::CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out)
{
  *status = MakeStatus(mission_status::kOk);
  *out = 0;
  int result = MissionSystemTargetBase::CheckTarget(check, status, out);
  if (check->m_flag04 != 0)
  {
    if (check->m_flag08 == 0)
      result = CheckThreeSets<ObjectRefList, CheckCandidateRefs>(check, status, out, RefSetsOf(check), result);
    else
      result = CheckThreeSets<ObjectKeySet, CheckCandidateKeys>(check, status, out, KeySetsOf(check), result);
  }
  if (check->m_flag0c != 0 && result && StatusIsOk(status))
  {
    const int side = CheckedSideOf(check->m_controlSide);
    result = 0;
    if (side != 0)
    {
      Side* sideObject = static_cast<Side*>(GetGameRootSideMemberThunk(side));
      result = sideObject != nullptr;
      if (result && sideObject->m_recruitmentDone != 0)
      {
        *status = StatusCode(kStatusCategoryBlocked, kStatusRecruitmentDone);
        *out = kOutBlockedByRecruitmentDone;
      }
    }
  }
  return result;
}

int MissionRecruit::Virtual157(GameObj* person, int* out)
{
  System* system = nullptr;
  const int found = static_cast<MissionSystemTargetBase*>(this)->MissionSystemTargetBase::GetObjectiveSystem(
    reinterpret_cast<GameObj**>(&system));
  const bool ok = found != 0;
  if (!system)
    return ok;
  const int side = ControlSideIndex(ControlSide());
  const int loyalty = system->GetLoyaltyFor(side);
  const int leadership = static_cast<Role*>(person)->GetLeadership();
  return LookupRecruitMissionTable(leadership, loyalty, out) != 0 && ok ? 1 : 0;
}

int MissionRecruit::Virtual158(NotifyContext* ctx)
{
  ObjectKey unset;
  SetRecruitedCharacter(&unset, ctx);
  return 1;
}

int MissionRecruit::Virtual159(GameObj* person, int roll, int* out, NotifyContext* ctx)
{
  *out = 0;
  if (!PercentChance(roll))
    return 1;
  *out = 1;
  System* system = nullptr;
  const int found = static_cast<MissionSystemTargetBase*>(this)->MissionSystemTargetBase::GetObjectiveSystem(
    reinterpret_cast<GameObj**>(&system));
  CharacterMgr* root = GetCharacterManager();
  int ok = root != nullptr && found != 0;
  if (!system || !root)
    return ok;
  ObjectKey recruit;
  ok = root->RecruitRandomMinor(system, ControlSide(), &recruit, ctx) != 0 && ok;
  if (!recruit.OrNull())
    return ok;
  ok = SetRecruitedCharacter(&recruit, ctx) != 0 && ok;
  ok = SetTaskStatus(kTaskSucceeded, ctx) != 0 && ok;
  Role* role = static_cast<Role*>(person);
  if (role->Virtual118())
    ok = role->SetBaseLeadership(role->m_baseLeadership + g_gnprtb180F, ctx) != 0 && ok;
  return ok;
}

int MissionRecruit::Virtual160(NotifyContext* ctx)
{
  return MissionStartIfIdle(this, ctx);
}

int MissionRecruit::BuildEventKinds()
{
  if (!g_missionRecruitEventKindsReady)
  {
    g_missionRecruitEventKindsReady = 1;
    return MissionSystemTargetBase::InitEventKinds(0, &g_missionRecruitEventKinds);
  }
  return 1;
}
EventKindSet g_missionRecruitEventKinds{};
int g_missionRecruitEventKindsReady{};
