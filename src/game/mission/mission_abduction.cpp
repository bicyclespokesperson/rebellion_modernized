#include "mission/mission_abduction.h"

#include "character/character.h"

extern "C" void __cdecl LAB_00577030();

extern int g_missionAbductionEventKindsReady;      // 0x6bbb30
extern EventKindSet g_missionAbductionEventKinds;  // 0x6bbb28

int __cdecl FUN_0055c810(int personCombat, int targetCombat, int* out);  // LookupMissionTable19
int FUN_0053e2f0(int threshold);
extern int DAT_006bb574;

namespace {
constexpr uint32_t kFamilyMissionAbduction = 0x62;
constexpr int kTaskSucceeded = 3;
}  // namespace

MissionAbduction::MissionAbduction() {}

MissionAbduction::~MissionAbduction() { REB_KEEP_VPTR_STORE(); }

REB_MISSION_SHARED_PERSISTENCE(MissionAbduction)

uint32_t MissionAbduction::GetFamily() { return kFamilyMissionAbduction; }

int MissionAbduction::GetEventKinds(int unused, EventKindSet* out) {
    return CopyEventKinds(BuildEventKinds, &g_missionAbductionEventKinds, out);
}

int MissionAbduction::CheckOrder(int a, int orderKind, StatusCode* out) {
    return CheckOrderAgainstKinds(BuildEventKinds, &g_missionAbductionEventKinds, orderKind, out);
}

int MissionAbduction::GetCheckFunctions(MissionCheckFunctions* out) {
    out->m_unk00 = reinterpret_cast<void*>(FUN_00592aa0);
    out->m_unk04 = reinterpret_cast<void*>(CheckTarget);
    return 1;
}

GameObj* MissionAbduction::CreateViewObject() { return NewOrNull<MissionAbduction>(); }

GameObjNotifier* MissionAbduction::Create() { return NewOrNull<MissionAbduction>(); }

int MissionAbduction::CheckTarget(MissionOrderCheck* check, StatusCode* status, int* out) {
    *status = MakeStatus(mission_status::kOk);
    *out = 0;
    return FUN_00586e20(check, status, out);
}

int MissionAbduction::Virtual157(GameObj* person, int* out) {
    Character* target = nullptr;
    const int found = FUN_00586c80(this, &target);
    bool ok = found != 0;
    if (!target) return ok;
    const int targetCombat = target->GetCombat();
    const int personCombat = static_cast<Role*>(person)->GetCombat();
    return FUN_0055c810(personCombat, targetCombat, out) != 0 && ok ? 1 : 0;
}

int MissionAbduction::Virtual158(NotifyContext* ctx) { return 1; }

int MissionAbduction::Virtual160(NotifyContext* ctx) { return MissionStartIfIdle(this, ctx); }

int MissionAbduction::Virtual159(GameObj* personObject, int roll, int* out, NotifyContext* ctx) {
    *out = 0;
    if (!FUN_0053e2f0(roll)) return 1;
    *out = 1;
    Role* person = static_cast<Role*>(personObject);
    bool ok = SetTaskStatus(kTaskSucceeded, ctx) != 0;
    if (person->Virtual118()) ok = person->SetBaseCombat(person->m_baseCombat + DAT_006bb574, ctx) && ok;
    Character* target = nullptr;
    ok = FUN_00586c80(this, &target) && ok;
    if (target) {
        ObjectKey key;
        person->GetKey(&key);
        ok = target->Virtual131(&key, ctx) && ok;
    }
    return ok;
}

int MissionAbduction::BuildEventKinds() {
    int result = 1;
    if (!g_missionAbductionEventKindsReady) {
        g_missionAbductionEventKindsReady = 1;
        result = FUN_00587090(0, &g_missionAbductionEventKinds) != 0;
    }
    return result;
}

void ConstructMissionAbductionEventKinds() { new (&g_missionAbductionEventKinds) EventKindSet(); }

void RegisterMissionAbductionEventKindsAtExit() { atexit(LAB_00577030); }

void MissionAbduction::InitStatics() {
    ConstructMissionAbductionEventKinds();
    RegisterMissionAbductionEventKindsAtExit();
}
