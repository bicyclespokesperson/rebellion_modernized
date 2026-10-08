#include "character/character_mgr.h"
#include "mission/mission_recruit.h"

#include "economy/side.h"
#include "gameobj/game_hooks.h"
#include "gameobj/object_queries.h"

extern "C" void __cdecl LAB_0056bbb0();

extern int g_missionRecruitEventKindsReady;      // 0x6bb968
extern EventKindSet g_missionRecruitEventKinds;  // 0x6bb960

int __cdecl FUN_0055c700(int leadership, int loyalty, int* out);  // LookupRecruitMissionTable
int FUN_0053e2f0(int threshold);
int __thiscall FUN_005589b0(ObjectKey* self, Crc32* crc);
extern int DAT_006bb5b0;  // g_gnprtb1824

namespace {
constexpr uint32_t kFamilyMissionRecruit = 0x55;
constexpr int kTaskSucceeded = 3;
constexpr uint32_t kFirstCharacterFamily = 0x30;
constexpr uint32_t kEndCharacterFamily = 0x38;
constexpr int kStatusCategoryBlocked = 0x40;
constexpr int kStatusNotACharacter = 6;
constexpr int kStatusRecruitmentDone = 0x35;
constexpr int kOutBlockedByRecruitmentDone = 0x10;

constexpr int kRefSetOffsets[] = {0x18, 0x1c, 0x20};
constexpr int kKeySetOffsets[] = {0x24, 0x28, 0x2c};

int ControlSideIndex(uint32_t controlSide) {
    if (controlSide == 3) return 3;
    return controlSide == 1 ? 2 : 1;
}

template <class T>
T& FieldAt(MissionOrderCheck* check, int offset) {
    return *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(check) + offset);
}

int CheckedSideOf(int kind) { return kind == 1 ? 1 : (kind == 2 ? 2 : 0); }

bool IsCharacterFamily(uint32_t family) { return family >= kFirstCharacterFamily && family < kEndCharacterFamily; }

void RejectNonCharacter(StatusCode* status, int* out) {
    *status = StatusCode(kStatusCategoryBlocked, kStatusNotACharacter);
    *out = -1;
}

bool StatusIsOk(const StatusCode* status) { return status->m_second == static_cast<uint32_t>(mission_status::kOk); }

// Runs the three set checks of one target flavour in order; each runs only while the status is still ok.
template <class Set, int (*Check)(Set*, MissionOrderCheck*, StatusCode*, int*)>
int CheckThreeSets(MissionOrderCheck* check, StatusCode* status, int* out, const int (&offsets)[3], int result) {
    if (!result) return result;
    if (StatusIsOk(status)) result = Check(FieldAt<Set*>(check, offsets[0]), check, status, out);
    if (!result) return result;
    if (StatusIsOk(status)) result = Check(FieldAt<Set*>(check, offsets[1]), check, status, out);
    if (result && StatusIsOk(status)) result = Check(FieldAt<Set*>(check, offsets[2]), check, status, out);
    return result;
}
}  // namespace

MissionRecruit::MissionRecruit() {}

MissionRecruit::~MissionRecruit() { REB_KEEP_VPTR_STORE(); }

int MissionRecruit::Read(istream* in) {
    if (!FUN_00592c50(this, in)) return 0;
    return m_recruitedCharacter.Read(in);
}

int MissionRecruit::Write(ostream* out) {
    if (!WriteImpl(out)) return 0;
    return m_recruitedCharacter.Write(out);
}

int MissionRecruit::Checksum(Crc32* crc) {
    if (!FUN_00593ad0(this, crc)) return 0;
    return FUN_005589b0(&m_recruitedCharacter, crc);
}

int MissionRecruit::Validate() { return FUN_00523c60(this); }

int MissionRecruit::ApplyAll(NotifyContext* ctx) { return IsSideView() ? ApplyAllBody(ctx) : 0; }

int MissionRecruit::CreateViews(int controlKind, NotifyContext* ctx) { return FUN_005236e0(this, controlKind, ctx); }

int MissionRecruit::ApplyAllBody(NotifyContext* ctx) {
    const int result = ApplyAllImpl(ctx);
    if (result && ctx->m_flag04 == 1) ApplyRecruitedCharacter(ctx);
    return result;
}

uint32_t MissionRecruit::GetFamily() { return kFamilyMissionRecruit; }

int MissionRecruit::GetEventKinds(int unused, EventKindSet* out) {
    return CopyEventKinds(BuildEventKinds, &g_missionRecruitEventKinds, out);
}

int MissionRecruit::CheckOrder(int a, int orderKind, StatusCode* out) {
    return CheckOrderAgainstKinds(BuildEventKinds, &g_missionRecruitEventKinds, orderKind, out);
}

int MissionRecruit::GetCheckFunctions(MissionCheckFunctions* out) {
    out->m_unk00 = reinterpret_cast<void*>(FUN_00592aa0);
    out->m_unk04 = reinterpret_cast<void*>(CheckTarget);
    return 1;
}

GameObj* MissionRecruit::CreateViewObject() { return NewOrNull<MissionRecruit>(); }

GameObjNotifier* MissionRecruit::Create() { return NewOrNull<MissionRecruit>(); }

int MissionRecruit::SetRecruitedCharacter(ObjectKey* key, NotifyContext* ctx) {
    const int result = IsMasterView();
    if (result && m_recruitedCharacter.m_value != key->m_value) {
        ObjectKey oldKey(m_recruitedCharacter);
        m_recruitedCharacter = *key;
        static_cast<MissionRecruit*>(ViewAs(1))->ApplyRecruitedCharacter(ctx);
        static_cast<MissionRecruit*>(ViewAs(2))->ApplyRecruitedCharacter(ctx);
        OnRecruitedCharacterChanged(&oldKey, &m_recruitedCharacter, ctx);
    }
    return result;
}

int MissionRecruit::ApplyRecruitedCharacter(NotifyContext* ctx) {
    int result = IsSideView();
    if (!result) return result;
    MissionRecruit* master = static_cast<MissionRecruit*>(GetViewObject(0));
    if (!MatchesSideMask(ctx->m_flag08)) return result;
    if (m_recruitedCharacter.m_value == master->m_recruitedCharacter.m_value) return result;
    ObjectKey oldKey(m_recruitedCharacter);
    m_recruitedCharacter = master->m_recruitedCharacter;
    if (CallGameHook2AF0() && ctx->GetViewFlag(static_cast<int>(ViewSide())))
        result = OnRecruitedCharacterNotified(&oldKey, &m_recruitedCharacter, ctx);
    return result;
}

int MissionRecruit::OnRecruitedCharacterNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogKey("(MissionRecruitRecruitedCharacterKeyNotif)", "RecruitedCharacter", newKey, oldKey, 0, ctx);
    return result;
}

int MissionRecruit::CheckCandidateKeys(ObjectKeySet* keys, MissionOrderCheck* check, StatusCode* status, int* out) {
    *status = MakeStatus(mission_status::kOk);
    *out = 0;
    for (ObjectKeyNode* node = keys->First(); node && StatusIsOk(status);
         node = static_cast<ObjectKeyNode*>(node->m_successor)) {
        ObjectKey key;
        node->GetKey(&key);
        if (!IsCharacterFamily(key.Family())) RejectNonCharacter(status, out);
    }
    return 1;
}

int MissionRecruit::CheckCandidateRefs(ObjectRefList* refs, MissionOrderCheck* check, StatusCode* status, int* out) {
    *status = MakeStatus(mission_status::kOk);
    *out = 0;
    for (AvlNode* node = refs->First(); node && StatusIsOk(status); node = node->m_successor) {
        ObjectKey key;
        static_cast<ObjectRefNode*>(node)->GetKey(&key);
        if (!IsCharacterFamily(key.Family())) RejectNonCharacter(status, out);
    }
    return 1;
}

int MissionRecruit::CheckTarget(MissionOrderCheck* check, StatusCode* status, int* out) {
    *status = MakeStatus(mission_status::kOk);
    *out = 0;
    int result = FUN_005868c0(check, status, out);
    if (FieldAt<int>(check, 4) != 0) {
        if (FieldAt<int>(check, 8) == 0)
            result = CheckThreeSets<ObjectRefList, CheckCandidateRefs>(check, status, out, kRefSetOffsets, result);
        else
            result = CheckThreeSets<ObjectKeySet, CheckCandidateKeys>(check, status, out, kKeySetOffsets, result);
    }
    if (FieldAt<int>(check, 0xc) != 0 && result && StatusIsOk(status)) {
        const int side = CheckedSideOf(FieldAt<int>(check, 0x10));
        result = 0;
        if (side != 0) {
            Side* sideObject = static_cast<Side*>(GetGameRootSideMemberThunk(side));
            result = sideObject != nullptr;
            if (result && sideObject->m_recruitmentDone != 0) {
                *status = StatusCode(kStatusCategoryBlocked, kStatusRecruitmentDone);
                *out = kOutBlockedByRecruitmentDone;
            }
        }
    }
    return result;
}

int MissionRecruit::Virtual157(GameObj* person, int* out) {
    System* system = nullptr;
    const int found = FUN_00586720(this, &system);
    const bool ok = found != 0;
    if (!system) return ok;
    const int side = ControlSideIndex(ControlSide());
    const int loyalty = system->GetLoyaltyFor(side);
    const int leadership = static_cast<Role*>(person)->GetLeadership();
    return FUN_0055c700(leadership, loyalty, out) != 0 && ok ? 1 : 0;
}

int MissionRecruit::Virtual158(NotifyContext* ctx) {
    ObjectKey unset;
    SetRecruitedCharacter(&unset, ctx);
    return 1;
}

int MissionRecruit::Virtual159(GameObj* person, int roll, int* out, NotifyContext* ctx) {
    *out = 0;
    if (!FUN_0053e2f0(roll)) return 1;
    *out = 1;
    System* system = nullptr;
    const int found = FUN_00586720(this, &system);
    CharacterMgr* root = GetCharacterManager();
    int ok = root != nullptr && found != 0;
    if (!system || !root) return ok;
    ObjectKey recruit;
    ok = root->RecruitRandomMinor(system, ControlSide(), &recruit, ctx) != 0 && ok;
    if (!recruit.OrNull()) return ok;
    ok = SetRecruitedCharacter(&recruit, ctx) != 0 && ok;
    ok = SetTaskStatus(kTaskSucceeded, ctx) != 0 && ok;
    Role* role = static_cast<Role*>(person);
    if (role->Virtual118()) ok = role->SetBaseLeadership(role->m_baseLeadership + DAT_006bb5b0, ctx) != 0 && ok;
    return ok;
}

int MissionRecruit::Virtual160(NotifyContext* ctx) { return MissionStartIfIdle(this, ctx); }

int MissionRecruit::BuildEventKinds() {
    if (!g_missionRecruitEventKindsReady) {
        g_missionRecruitEventKindsReady = 1;
        return FUN_00586b00(0, &g_missionRecruitEventKinds);
    }
    return 1;
}

void ConstructMissionRecruitEventKinds() { new (&g_missionRecruitEventKinds) EventKindSet(); }

void RegisterMissionRecruitEventKindsAtExit() { atexit(LAB_0056bbb0); }

void MissionRecruit::InitStatics() {
    ConstructMissionRecruitEventKinds();
    RegisterMissionRecruitEventKindsAtExit();
}
