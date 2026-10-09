#include "mission/mission_abduction.h"

#include "character/character.h"
#include "data/random.h"
#include "mission/mission_rules.h"

extern "C" void __cdecl LAB_00577030();

extern int g_missionAbductionEventKindsReady;      // 0x6bbb30
extern EventKindSet g_missionAbductionEventKinds;  // 0x6bbb28


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
    out->m_unk00 = reinterpret_cast<void*>(MissionRuleBase::CheckMember);
    out->m_unk04 = reinterpret_cast<void*>(CheckTarget);
    return 1;
}

GameObj* MissionAbduction::CreateViewObject() { return NewOrNull<MissionAbduction>(); }

GameObjNotifier* MissionAbduction::Create() { return NewOrNull<MissionAbduction>(); }

int MissionAbduction::CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out) {
    *status = MakeStatus(mission_status::kOk);
    *out = 0;
    return MissionCharacterTargetBase::CheckTarget(check, status, out);
}

int MissionAbduction::Virtual157(GameObj* person, int* out) {
    Character* target = nullptr;
    const int found = static_cast<MissionCharacterTargetBase*>(this)->GetObjectiveCharacter(reinterpret_cast<GameObj**>(&target));
    bool ok = found != 0;
    if (!target) return ok;
    const int targetCombat = target->GetCombat();
    const int personCombat = static_cast<Role*>(person)->GetCombat();
    return LookupMissionTable19(personCombat, targetCombat, out) != 0 && ok ? 1 : 0;
}

int MissionAbduction::Virtual158(NotifyContext* ctx) { return 1; }

int MissionAbduction::Virtual160(NotifyContext* ctx) { return MissionStartIfIdle(this, ctx); }

int MissionAbduction::Virtual159(GameObj* personObject, int roll, int* out, NotifyContext* ctx) {
    *out = 0;
    if (!PercentChance(roll)) return 1;
    *out = 1;
    Role* person = static_cast<Role*>(personObject);
    bool ok = SetTaskStatus(kTaskSucceeded, ctx) != 0;
    if (person->Virtual118()) ok = person->SetBaseCombat(person->m_baseCombat + g_gnprtb1813, ctx) && ok;
    Character* target = nullptr;
    ok = static_cast<MissionCharacterTargetBase*>(this)->GetObjectiveCharacter(reinterpret_cast<GameObj**>(&target)) && ok;
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
        result = MissionCharacterTargetBase::InitEventKinds(0, &g_missionAbductionEventKinds) != 0;
    }
    return result;
}

void ConstructMissionAbductionEventKinds() { new (&g_missionAbductionEventKinds) EventKindSet(); }

void RegisterMissionAbductionEventKindsAtExit() { atexit(LAB_00577030); }

void MissionAbduction::InitStatics() {
    ConstructMissionAbductionEventKinds();
    RegisterMissionAbductionEventKindsAtExit();
}
