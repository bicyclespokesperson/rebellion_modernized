#include "mission/mission_diplomacy.h"
#include "data/random.h"
#include "mission/mission_rules.h"


extern int g_missionDiplomacyEventKindsReady;      // 0x6bba90
extern EventKindSet g_missionDiplomacyEventKinds;  // 0x6bba88


namespace {
constexpr uint32_t kFamilyMissionDiplomacy = 0x51;
constexpr int kTaskIdle = 0;
constexpr int kTaskStarted = 2;
constexpr int kTaskSucceeded = 3;
constexpr int kLoyaltyFullyWon = 100;
constexpr int kStatusCodeLoyaltyMaxed = 0x33;
constexpr int kStatusCategoryBlocked = 0x40;
constexpr int kOutBlockedByLoyalty = 0xf;

int ControlSideIndex(uint32_t controlSide) {
    if (controlSide == 3) return 3;
    return controlSide == 1 ? 2 : 1;
}

int CheckedSideOf(int kind) { return kind == 1 ? 1 : (kind == 2 ? 2 : 0); }
}  // namespace

MissionDiplomacy::MissionDiplomacy() {}

MissionDiplomacy::~MissionDiplomacy() { REB_KEEP_VPTR_STORE(); }

REB_MISSION_SHARED_PERSISTENCE(MissionDiplomacy)

uint32_t MissionDiplomacy::GetFamily() { return kFamilyMissionDiplomacy; }

int MissionDiplomacy::GetEventKinds(int unused, EventKindSet* out) {
    return CopyEventKinds(BuildEventKinds, &g_missionDiplomacyEventKinds, out);
}

int MissionDiplomacy::CheckOrder(int a, int orderKind, StatusCode* out) {
    return CheckOrderAgainstKinds(BuildEventKinds, &g_missionDiplomacyEventKinds, orderKind, out);
}

int MissionDiplomacy::GetCheckFunctions(MissionCheckFunctions* out) {
    out->m_unk00 = reinterpret_cast<void*>(MissionRuleBase::CheckMember);
    out->m_unk04 = reinterpret_cast<void*>(CheckTarget);
    return 1;
}

GameObj* MissionDiplomacy::CreateViewObject() { return NewOrNull<MissionDiplomacy>(); }

GameObjNotifier* MissionDiplomacy::Create() { return NewOrNull<MissionDiplomacy>(); }

int MissionDiplomacy::CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out) {
    *status = MakeStatus(mission_status::kOk);
    *out = 0;
    int result = MissionSystemTargetBase::CheckTarget(check, status, out);
    if (check->m_flag0c == 0 || result == 0 || status->m_second != static_cast<uint32_t>(mission_status::kOk))
        return result;
    const int side = CheckedSideOf(check->m_controlSide);
    result = side != 0;
    if (result) {
        if (static_cast<System*>(check->m_objective)->GetLoyaltyFor(side) == kLoyaltyFullyWon) {
            *status = StatusCode(kStatusCategoryBlocked, kStatusCodeLoyaltyMaxed);
            *out = kOutBlockedByLoyalty;
        }
    }
    return result;
}

int MissionDiplomacy::Virtual157(GameObj* person, int* out) {
    System* system = nullptr;
    const int found = static_cast<MissionSystemTargetBase*>(this)->MissionSystemTargetBase::GetObjectiveSystem(reinterpret_cast<GameObj**>(&system));
    const bool ok = found != 0;
    if (!system) return ok;
    const int side = ControlSideIndex(ControlSide());
    const int regiments = system->CountEmpireRegimentsOfKey6();
    const int loyalty = system->GetLoyaltyFor(side);
    const int diplomacy = static_cast<Role*>(person)->GetDiplomacy();
    return LookupDiplomacyMissionTable(diplomacy, loyalty, regiments, out) != 0 && ok ? 1 : 0;
}

int MissionDiplomacy::Virtual158(NotifyContext* ctx) { return 1; }

int MissionDiplomacy::Virtual159(GameObj* person, int roll, int* out, NotifyContext* ctx) {
    if (!PercentChance(roll)) return 1;
    *out = 1;
    const bool ok = SetTaskStatus(kTaskSucceeded, ctx) != 0;
    Role* role = static_cast<Role*>(person);
    if (!role->Virtual118()) return ok;
    const int updated = role->SetBaseDiplomacy(role->m_baseDiplomacy + g_gnprtb180C, ctx);
    return updated != 0 && ok ? 1 : 0;
}

int MissionDiplomacy::Virtual160(NotifyContext* ctx) {
    bool ok = true;
    if (m_taskStatus == kTaskIdle) ok = SetTaskStatus(kTaskStarted, ctx) != 0;
    if (m_taskStatus != kTaskSucceeded) return ok;
    System* system = nullptr;
    const int found = static_cast<MissionSystemTargetBase*>(this)->MissionSystemTargetBase::GetObjectiveSystem(reinterpret_cast<GameObj**>(&system));
    ok = found != 0 && ok;
    if (!system) return ok;
    const int bonus = ComputeDiplomacyBonus(ControlSide(), system->ControlSide());
    const int adjusted = system->AdjustLoyalty(bonus, ControlSide(), ctx);
    return adjusted != 0 && ok ? 1 : 0;
}

int MissionDiplomacy::BuildEventKinds() {
    if (!g_missionDiplomacyEventKindsReady) {
        g_missionDiplomacyEventKindsReady = 1;
        return MissionSystemTargetBase::InitEventKinds(0, &g_missionDiplomacyEventKinds);
    }
    return 1;
}
EventKindSet g_missionDiplomacyEventKinds{};
int g_missionDiplomacyEventKindsReady{};
