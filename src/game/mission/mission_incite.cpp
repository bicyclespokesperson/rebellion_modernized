#include "mission/mission_incite.h"
#include "data/random.h"
#include "mission/mission_rules.h"

void DestroyMissionInciteEventKinds();

extern int g_missionInciteEventKindsReady;      // 0x6bba30
extern EventKindSet g_missionInciteEventKinds;  // 0x6bba28


namespace {
constexpr uint32_t kFamilyMissionIncite = 0x56;
constexpr int kTaskStarted = 2;
constexpr int kTaskSucceeded = 3;

int ControlSideIndex(uint32_t controlSide) {
    if (controlSide == 3) return 3;
    return controlSide == 1 ? 2 : 1;
}
}  // namespace

MissionIncite::MissionIncite() {}

MissionIncite::~MissionIncite() { REB_KEEP_VPTR_STORE(); }

REB_MISSION_SHARED_PERSISTENCE(MissionIncite)

uint32_t MissionIncite::GetFamily() { return kFamilyMissionIncite; }

int MissionIncite::GetEventKinds(int unused, EventKindSet* out) {
    return CopyEventKinds(BuildEventKinds, &g_missionInciteEventKinds, out);
}

int MissionIncite::CheckOrder(int a, int orderKind, StatusCode* out) {
    return CheckOrderAgainstKinds(BuildEventKinds, &g_missionInciteEventKinds, orderKind, out);
}

int MissionIncite::GetCheckFunctions(MissionCheckFunctions* out) {
    out->m_unk00 = reinterpret_cast<void*>(MissionRuleBase::CheckMember);
    out->m_unk04 = reinterpret_cast<void*>(CheckTarget);
    return 1;
}

GameObj* MissionIncite::CreateViewObject() { return NewOrNull<MissionIncite>(); }

GameObjNotifier* MissionIncite::Create() { return NewOrNull<MissionIncite>(); }

int MissionIncite::CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out) {
    *status = MakeStatus(mission_status::kOk);
    *out = 0;
    return MissionSystemTargetBase::CheckTarget(check, status, out);
}

int MissionIncite::Virtual157(GameObj* person, int* out) {
    System* system = nullptr;
    const int found = static_cast<MissionSystemTargetBase*>(this)->MissionSystemTargetBase::GetObjectiveSystem(reinterpret_cast<GameObj**>(&system));
    int ok = found != 0;
    if (!system) return ok;
    const int side = ControlSideIndex(ControlSide());
    const int regiments = system->CountEmpireRegimentsOfKey6();
    const int loyalty = system->GetLoyaltyFor(side);
    const int leadership = static_cast<Role*>(person)->GetLeadership();
    return LookupInciteMissionTable(leadership, loyalty, regiments, out) && ok ? 1 : 0;
}

int MissionIncite::Virtual158(NotifyContext* ctx) { return 1; }

int MissionIncite::Virtual160(NotifyContext* ctx) { return MissionStartIfIdle(this, ctx); }

int MissionIncite::Virtual159(GameObj* person, int roll, int* out, NotifyContext* ctx) {
    System* system = nullptr;
    const int found = static_cast<MissionSystemTargetBase*>(this)->MissionSystemTargetBase::GetObjectiveSystem(reinterpret_cast<GameObj**>(&system));
    int ok = found != 0;
    if (!system) return ok;
    if (!PercentChance(roll)) return ok;
    *out = 1;
    ok = system->System::ResolveUprisingTurn(ctx) && ok ? 1 : 0;
    const uint32_t side = ControlSideIndex(ControlSide());
    const bool sideHoldsLoyalty = system->m_controlData.Loyalty() == side;
    const bool uprisingContained = sideHoldsLoyalty || system->CountTroopRegiments(system->ControlSide(), 1) != 0;
    ok = SetTaskStatus(uprisingContained ? kTaskStarted : kTaskSucceeded, ctx) && ok ? 1 : 0;
    if (m_taskStatus == kTaskSucceeded && static_cast<Role*>(person)->Virtual118()) {
        Role* role = static_cast<Role*>(person);
        ok = role->SetBaseLeadership(role->m_baseLeadership + g_gnprtb1810, ctx) && ok ? 1 : 0;
    }
    return ok;
}

int MissionIncite::BuildEventKinds() {
    if (!g_missionInciteEventKindsReady) {
        g_missionInciteEventKindsReady = 1;
        return MissionSystemTargetBase::InitEventKinds(0, &g_missionInciteEventKinds);
    }
    return 1;
}

void ConstructMissionInciteEventKinds() { new (&g_missionInciteEventKinds) EventKindSet(); }

void RegisterMissionInciteEventKindsAtExit() { atexit(DestroyMissionInciteEventKinds); }

void MissionIncite::InitStatics() {
    ConstructMissionInciteEventKinds();
    RegisterMissionInciteEventKindsAtExit();
}

// ---- destructors registered with atexit ----------------------------------------------------

extern uint8_t g_missionInciteEventKindsDestroyedGuard;  // 0x6bba20

void DestroyMissionInciteEventKinds() {
    if (!(g_missionInciteEventKindsDestroyedGuard & 0x1)) {
        g_missionInciteEventKindsDestroyedGuard |= 0x1;
        g_missionInciteEventKinds.~EventKindSet();
    }
}
