#include "mission/mission_rescue.h"

extern "C" void __cdecl LAB_0056afa0();

extern int g_missionRescueEventKindsReady;      // 0x6bb94c
extern EventKindSet g_missionRescueEventKinds;  // 0x6bb950

int __cdecl FUN_0055c7e0(int combat, int* out);  // LookupRescueMissionTable
int FUN_0053e2f0(int threshold);
extern int DAT_006bb55c;

namespace {
constexpr uint32_t kFamilyMissionRescue = 0x61;
constexpr int kTaskSucceeded = 3;
}  // namespace

MissionRescue::MissionRescue() {}

MissionRescue::~MissionRescue() { REB_KEEP_VPTR_STORE(); }

REB_MISSION_SHARED_PERSISTENCE(MissionRescue)

uint32_t MissionRescue::GetFamily() { return kFamilyMissionRescue; }

int MissionRescue::GetEventKinds(int unused, EventKindSet* out) {
    return CopyEventKinds(BuildEventKinds, &g_missionRescueEventKinds, out);
}

int MissionRescue::CheckOrder(int a, int orderKind, StatusCode* out) {
    return CheckOrderAgainstKinds(BuildEventKinds, &g_missionRescueEventKinds, orderKind, out);
}

int MissionRescue::GetCheckFunctions(MissionCheckFunctions* out) {
    out->m_unk00 = reinterpret_cast<void*>(FUN_00592aa0);
    out->m_unk04 = reinterpret_cast<void*>(CheckTarget);
    return 1;
}

GameObj* MissionRescue::CreateViewObject() { return NewOrNull<MissionRescue>(); }

GameObjNotifier* MissionRescue::Create() { return NewOrNull<MissionRescue>(); }

int MissionRescue::CheckTarget(MissionOrderCheck* check, StatusCode* status, int* out) {
    *status = MakeStatus(mission_status::kOk);
    *out = 0;
    return FUN_00586e20(check, status, out);
}

int MissionRescue::Virtual157(GameObj* person, int* out) {
    return FUN_0055c7e0(static_cast<Role*>(person)->GetCombat(), out);
}

int MissionRescue::Virtual158(NotifyContext* ctx) { return 1; }

int MissionRescue::Virtual160(NotifyContext* ctx) { return MissionStartIfIdle(this, ctx); }

int MissionRescue::Virtual159(GameObj* person, int roll, int* out, NotifyContext* ctx) {
    int ok = 1;
    *out = 0;
    if (!FUN_0053e2f0(roll)) return ok;
    *out = 1;
    SetTaskStatus(kTaskSucceeded, ctx);
    Role* role = static_cast<Role*>(person);
    if (role->Virtual118()) role->SetBaseCombat(role->m_baseCombat + DAT_006bb55c, ctx);
    Role* target = nullptr;
    ok = FUN_00586c80(this, &target);
    if (target) {
        ObjectKey key;
        role->GetKey(&key);
        const int moved = target->Virtual132(&key, ctx);
        ok = moved && ok ? 1 : 0;
    }
    return ok;
}

int MissionRescue::BuildEventKinds() {
    if (!g_missionRescueEventKindsReady) {
        g_missionRescueEventKindsReady = 1;
        return FUN_00587090(0, &g_missionRescueEventKinds);
    }
    return 1;
}

void ConstructMissionRescueEventKinds() { new (&g_missionRescueEventKinds) EventKindSet(); }

void RegisterMissionRescueEventKindsAtExit() { atexit(LAB_0056afa0); }

void MissionRescue::InitStatics() {
    ConstructMissionRescueEventKinds();
    RegisterMissionRescueEventKindsAtExit();
}
