#include "mission/mission_assassination.h"

#include "character/character.h"
#include "data/random.h"
#include "mission/mission_rules.h"

extern "C" void __cdecl LAB_00576790();

extern int g_missionAssassinationEventKindsReady;      // 0x6bbb08
extern EventKindSet g_missionAssassinationEventKinds;  // 0x6bbb00

extern int DAT_006bb584;

namespace {
constexpr uint32_t kFamilyMissionAssassination = 0x63;
constexpr int kTaskSucceeded = 3;
constexpr uint32_t kDestroyedState = 0x8;
}  // namespace

MissionAssassination::MissionAssassination() {}

MissionAssassination::~MissionAssassination() { REB_KEEP_VPTR_STORE(); }

REB_MISSION_SHARED_PERSISTENCE(MissionAssassination)

uint32_t MissionAssassination::GetFamily() { return kFamilyMissionAssassination; }

int MissionAssassination::GetEventKinds(int unused, EventKindSet* out) {
    return CopyEventKinds(BuildEventKinds, &g_missionAssassinationEventKinds, out);
}

int MissionAssassination::CheckOrder(int a, int orderKind, StatusCode* out) {
    return CheckOrderAgainstKinds(BuildEventKinds, &g_missionAssassinationEventKinds, orderKind, out);
}

int MissionAssassination::GetCheckFunctions(MissionCheckFunctions* out) {
    out->m_unk00 = reinterpret_cast<void*>(FUN_00592aa0);
    out->m_unk04 = reinterpret_cast<void*>(CheckTarget);
    return 1;
}

GameObj* MissionAssassination::CreateViewObject() { return NewOrNull<MissionAssassination>(); }

GameObjNotifier* MissionAssassination::Create() { return NewOrNull<MissionAssassination>(); }

int MissionAssassination::CheckTarget(MissionOrderCheck* check, StatusCode* status, int* out) {
    *status = MakeStatus(mission_status::kOk);
    *out = 0;
    return FUN_00586e20(check, status, out);
}

int MissionAssassination::Virtual157(GameObj* person, int* out) {
    Character* target = nullptr;
    const int found = FUN_00586c80(this, &target);
    bool ok = found != 0;
    if (!target) return ok;
    const int targetCombat = target->GetCombat();
    const int personCombat = static_cast<Role*>(person)->GetCombat();
    return LookupMissionTable1D(personCombat, targetCombat, out) != 0 && ok ? 1 : 0;
}

int MissionAssassination::Virtual158(NotifyContext* ctx) { return 1; }

int MissionAssassination::Virtual160(NotifyContext* ctx) { return MissionStartIfIdle(this, ctx); }

int MissionAssassination::Virtual159(GameObj* personObject, int roll, int* out, NotifyContext* ctx) {
    *out = 0;
    if (!PercentChance(roll)) return 1;
    *out = 1;
    Character* target = nullptr;
    const int found = FUN_00586c80(this, &target);
    bool ok = found != 0;
    if (target) ok = target->Virtual187(ctx) && ok;
    if (!target || !(target->m_stateFlags & kDestroyedState)) return ok;
    ok = SetTaskStatus(kTaskSucceeded, ctx) && ok;
    Role* person = static_cast<Role*>(personObject);
    if (!person->Virtual118()) return ok;
    return person->SetBaseCombat(person->m_baseCombat + DAT_006bb584, ctx) && ok ? 1 : 0;
}

int MissionAssassination::BuildEventKinds() {
    if (!g_missionAssassinationEventKindsReady) {
        g_missionAssassinationEventKindsReady = 1;
        return FUN_00587090(0, &g_missionAssassinationEventKinds);
    }
    return 1;
}

void ConstructMissionAssassinationEventKinds() { new (&g_missionAssassinationEventKinds) EventKindSet(); }

void RegisterMissionAssassinationEventKindsAtExit() { atexit(LAB_00576790); }

void MissionAssassination::InitStatics() {
    ConstructMissionAssassinationEventKinds();
    RegisterMissionAssassinationEventKindsAtExit();
}
