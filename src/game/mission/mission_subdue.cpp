#include "mission/mission_subdue.h"
#include "data/random.h"
#include "mission/mission_rules.h"

extern "C" void __cdecl LAB_00569df0();

extern int g_missionSubdueEventKindsReady;      // 0x6bb918
extern EventKindSet g_missionSubdueEventKinds;  // 0x6bb910

extern int DAT_006bb538;  // g_gnprtb1811

namespace {
constexpr uint32_t kFamilyMissionSubdue = 0x57;
constexpr int kTaskStarted = 2;
constexpr int kTaskSucceeded = 3;

int ControlSideIndex(uint32_t controlSide) {
    if (controlSide == 3) return 3;
    return controlSide == 1 ? 2 : 1;
}
}  // namespace

MissionSubdue::MissionSubdue() {}

MissionSubdue::~MissionSubdue() { REB_KEEP_VPTR_STORE(); }

REB_MISSION_SHARED_PERSISTENCE(MissionSubdue)

uint32_t MissionSubdue::GetFamily() { return kFamilyMissionSubdue; }

int MissionSubdue::GetEventKinds(int unused, EventKindSet* out) {
    return CopyEventKinds(BuildEventKinds, &g_missionSubdueEventKinds, out);
}

int MissionSubdue::CheckOrder(int a, int orderKind, StatusCode* out) {
    return CheckOrderAgainstKinds(BuildEventKinds, &g_missionSubdueEventKinds, orderKind, out);
}

int MissionSubdue::GetCheckFunctions(MissionCheckFunctions* out) {
    out->m_unk00 = reinterpret_cast<void*>(MissionRuleBase::CheckMember);
    out->m_unk04 = reinterpret_cast<void*>(CheckTarget);
    return 1;
}

GameObj* MissionSubdue::CreateViewObject() { return NewOrNull<MissionSubdue>(); }

GameObjNotifier* MissionSubdue::Create() { return NewOrNull<MissionSubdue>(); }

int MissionSubdue::CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out) {
    *status = MakeStatus(mission_status::kOk);
    *out = 0;
    return MissionSystemTargetBase::CheckTarget(check, status, out);
}

int MissionSubdue::Virtual157(GameObj* person, int* out) {
    System* system = nullptr;
    const int found = static_cast<MissionSystemTargetBase*>(this)->GetObjectiveSystem(reinterpret_cast<GameObj**>(&system));
    bool ok = found != 0;
    if (!system) return ok;
    const int side = ControlSideIndex(ControlSide());
    const int regiments = system->CountEmpireRegimentsOfKey6();
    const int loyalty = system->GetLoyaltyFor(side);
    const int leadership = static_cast<Role*>(person)->GetLeadership();
    LookupSubdueMissionTable(leadership, loyalty, regiments, out);
    return ok && *out != 0 ? 1 : 0;
}

int MissionSubdue::Virtual158(NotifyContext* ctx) { return 1; }

int MissionSubdue::Virtual160(NotifyContext* ctx) { return MissionStartIfIdle(this, ctx); }

int MissionSubdue::Virtual159(GameObj* person, int roll, int* out, NotifyContext* ctx) {
    System* system = nullptr;
    const int found = static_cast<MissionSystemTargetBase*>(this)->GetObjectiveSystem(reinterpret_cast<GameObj**>(&system));
    if (!system) return found != 0;
    const bool inUprising = (system->m_flags & 4) != 0;
    bool ok = inUprising && found;
    if (!inUprising) return ok;
    if (!PercentChance(roll)) return ok;
    *out = 1;
    const uint32_t side = ControlSide();
    const int bonus = ComputeSubdueBonus(side, system->ControlSide());
    ok = system->AdjustLoyalty(bonus, side, ctx) && ok;
    if (system->m_flags & 4) ok = system->EndUprisingIfQuelled(ctx) && ok;
    ok = SetTaskStatus((system->m_flags & 4) ? kTaskStarted : kTaskSucceeded, ctx) && ok;
    if (m_taskStatus == kTaskSucceeded && static_cast<Role*>(person)->Virtual118()) {
        Role* role = static_cast<Role*>(person);
        ok = role->SetBaseLeadership(role->m_baseLeadership + DAT_006bb538, ctx) && ok;
    }
    return ok;
}

int MissionSubdue::BuildEventKinds() {
    if (!g_missionSubdueEventKindsReady) {
        g_missionSubdueEventKindsReady = 1;
        return MissionSystemTargetBase::InitEventKinds(0, &g_missionSubdueEventKinds);
    }
    return 1;
}

void ConstructMissionSubdueEventKinds() { new (&g_missionSubdueEventKinds) EventKindSet(); }

void RegisterMissionSubdueEventKindsAtExit() { atexit(LAB_00569df0); }

void MissionSubdue::InitStatics() {
    ConstructMissionSubdueEventKinds();
    RegisterMissionSubdueEventKindsAtExit();
}
