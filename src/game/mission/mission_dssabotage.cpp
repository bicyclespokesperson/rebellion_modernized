#include "mission/mission_dssabotage.h"
#include "data/random.h"
#include "mission/mission_rules.h"

extern "C" void __cdecl LAB_005747d0();

extern int g_missionDSSabotageEventKindsReady;      // 0x6bbaa8
extern EventKindSet g_missionDSSabotageEventKinds;  // 0x6bbaa0


namespace {
constexpr uint32_t kFamilyMissionDSSabotage = 0x6a;
constexpr int kTaskSucceeded = 3;
constexpr FamilyRange kDeathStars(0x18, 0x1c);
constexpr int kStatusTargetNotDeathStar = 0x29;
constexpr int kStatusCategoryOrder = 0x40;
}  // namespace

MissionDSSabotage::MissionDSSabotage() {}

MissionDSSabotage::~MissionDSSabotage() { REB_KEEP_VPTR_STORE(); }

REB_MISSION_SHARED_PERSISTENCE(MissionDSSabotage)

uint32_t MissionDSSabotage::GetFamily() { return kFamilyMissionDSSabotage; }

int MissionDSSabotage::GetEventKinds(int unused, EventKindSet* out) {
    return CopyEventKinds(BuildEventKinds, &g_missionDSSabotageEventKinds, out);
}

int MissionDSSabotage::CheckOrder(int a, int orderKind, StatusCode* out) {
    return CheckOrderAgainstKinds(BuildEventKinds, &g_missionDSSabotageEventKinds, orderKind, out);
}

int MissionDSSabotage::GetCheckFunctions(MissionCheckFunctions* out) {
    out->m_unk00 = reinterpret_cast<void*>(MissionRuleBase::CheckMember);
    out->m_unk04 = reinterpret_cast<void*>(CheckTarget);
    return 1;
}

GameObj* MissionDSSabotage::CreateViewObject() { return NewOrNull<MissionDSSabotage>(); }

GameObjNotifier* MissionDSSabotage::Create() { return NewOrNull<MissionDSSabotage>(); }

int MissionDSSabotage::CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out) {
    *status = MakeStatus(mission_status::kOk);
    *out = 0;
    const int result = MissionObjectTargetBase::CheckTarget(check, status, out);
    const bool targetKnown = check->m_flag04 && check->m_flag0c;
    if (targetKnown && result && status->m_second == static_cast<uint32_t>(mission_status::kOk)) {
        if (!FamilyInRange(check->m_target->GetFamily(), kDeathStars)) {
            *status = StatusCode(kStatusCategoryOrder, kStatusTargetNotDeathStar);
            *out = -1;
        }
    }
    return result;
}

int MissionDSSabotage::Virtual157(GameObj* person, int* out) {
    Role* role = static_cast<Role*>(person);
    const int combat = role->GetCombat();
    const int espionage = role->GetEspionage();
    return LookupDSSabotageMissionTable(espionage, combat, out);
}

int MissionDSSabotage::Virtual158(NotifyContext* ctx) { return 1; }

int MissionDSSabotage::Virtual160(NotifyContext* ctx) { return MissionDestroyOriginOnSuccess(this, ctx); }

int MissionDSSabotage::Virtual159(GameObj* person, int roll, int* out, NotifyContext* ctx) {
    int ok = 1;
    *out = 0;
    if (!PercentChance(roll)) return ok;
    *out = 1;
    ok = SetTaskStatus(kTaskSucceeded, ctx) != 0;
    Role* role = static_cast<Role*>(person);
    if (!role->Virtual118()) return ok;
    ok = role->SetBaseEspionage(role->m_baseEspionage + g_gnprtb1817, ctx) && ok;
    ok = role->SetBaseCombat(role->m_baseCombat + g_gnprtb1818, ctx) && ok;
    return ok;
}

int MissionDSSabotage::BuildEventKinds() {
    if (!g_missionDSSabotageEventKindsReady) {
        g_missionDSSabotageEventKindsReady = 1;
        return MissionObjectTargetBase::InitEventKinds(0, &g_missionDSSabotageEventKinds);
    }
    return 1;
}

void ConstructMissionDSSabotageEventKinds() { new (&g_missionDSSabotageEventKinds) EventKindSet(); }

void RegisterMissionDSSabotageEventKindsAtExit() { atexit(LAB_005747d0); }

void MissionDSSabotage::InitStatics() {
    ConstructMissionDSSabotageEventKinds();
    RegisterMissionDSSabotageEventKindsAtExit();
}
