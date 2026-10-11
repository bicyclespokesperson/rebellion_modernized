#include "core/reb_abi.h"
#include "mission/mission_rules.h"
#include "data/random.h"
#include "mission/mission_sabotage.h"


extern int g_missionSabotageEventKindsReady;      // 0x6bb920
extern EventKindSet g_missionSabotageEventKinds;  // 0x6bb928


namespace {
constexpr uint32_t kFamilyMissionSabotage = 0x69;
constexpr int kTaskSucceeded = 3;
constexpr FamilyRange kDeathStars(0x18, 0x1c);
constexpr int kStatusTargetIsDeathStar = 0x28;
constexpr int kStatusTargetIsCharacter = 0x25;
constexpr int kStatusCategoryOrder = 0x40;
}  // namespace

MissionSabotage::MissionSabotage() {}

MissionSabotage::~MissionSabotage() { REB_KEEP_VPTR_STORE(); }

REB_MISSION_SHARED_PERSISTENCE(MissionSabotage)

uint32_t MissionSabotage::GetFamily() { return kFamilyMissionSabotage; }

int MissionSabotage::GetEventKinds(int unused, EventKindSet* out) {
    return CopyEventKinds(BuildEventKinds, &g_missionSabotageEventKinds, out);
}

int MissionSabotage::CheckOrder(int a, int orderKind, StatusCode* out) {
    return CheckOrderAgainstKinds(BuildEventKinds, &g_missionSabotageEventKinds, orderKind, out);
}

int MissionSabotage::GetCheckFunctions(MissionCheckFunctions* out) {
    out->m_unk00 = reinterpret_cast<void*>(MissionRuleBase::CheckMember);
    out->m_unk04 = reinterpret_cast<void*>(CheckTarget);
    return 1;
}

GameObj* MissionSabotage::CreateViewObject() { return NewOrNull<MissionSabotage>(); }

GameObjNotifier* MissionSabotage::Create() { return NewOrNull<MissionSabotage>(); }

int MissionSabotage::CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out) {
    *status = MakeStatus(mission_status::kOk);
    *out = 0;
    const int result = MissionObjectTargetBase::CheckTarget(check, status, out);
    if (check->m_flag04 && check->m_flag0c) {
        if (result && status->m_second == static_cast<uint32_t>(mission_status::kOk)) {
            if (FamilyInRange(check->m_target->GetFamily(), kDeathStars)) {
                *status = StatusCode(kStatusCategoryOrder, kStatusTargetIsDeathStar);
                *out = -1;
            }
        }
        if (result && status->m_second == static_cast<uint32_t>(mission_status::kOk)) {
            if (FamilyInRange(check->m_target->GetFamily(), family::kCharacters)) {
                *status = StatusCode(kStatusCategoryOrder, kStatusTargetIsCharacter);
                *out = -1;
            }
        }
    }
    return result;
}

int MissionSabotage::Virtual157(GameObj* person, int* out) {
    Role* role = static_cast<Role*>(person);
    const int combat = role->GetCombat();
    const int espionage = role->GetEspionage();
    return LookupSabotageMissionTable(espionage, combat, out);
}

int MissionSabotage::Virtual158(NotifyContext* ctx) { return 1; }

int MissionSabotage::Virtual160(NotifyContext* ctx) { return MissionDestroyOriginOnSuccess(this, ctx); }

int MissionSabotage::Virtual159(GameObj* person, int roll, int* out, NotifyContext* ctx) {
    int ok = 1;
    *out = 0;
    if (!PercentChance(roll)) return ok;
    *out = 1;
    ok = SetTaskStatus(kTaskSucceeded, ctx) != 0;
    Role* role = static_cast<Role*>(person);
    if (!role->Virtual118()) return ok;
    ok = role->SetBaseEspionage(role->m_baseEspionage + g_gnprtb1815, ctx) && ok;
    ok = role->SetBaseCombat(role->m_baseCombat + g_gnprtb1816, ctx) && ok;
    return ok;
}

int REB_THISCALL MissionDestroyOriginOnSuccess(Mission* mission, NotifyContext* ctx) {
    int ok = 1;
    if (mission->m_taskStatus == 0) ok = mission->SetTaskStatus(2, ctx) != 0;
    if (mission->m_taskStatus == kTaskSucceeded) {
        GameObj* origin = nullptr;
        ok = mission->GetObjectiveObject(&origin) && ok;
        if (origin) ok = origin->ChangeDestroyedReason(6, ctx) && ok;
    }
    return ok;
}

int MissionSabotage::BuildEventKinds() {
    if (!g_missionSabotageEventKindsReady) {
        g_missionSabotageEventKindsReady = 1;
        return MissionObjectTargetBase::InitEventKinds(0, &g_missionSabotageEventKinds);
    }
    return 1;
}
EventKindSet g_missionSabotageEventKinds{};
int g_missionSabotageEventKindsReady{};
