#include "mission/mission_assassination.h"

#include "character/character.h"
#include "data/random.h"
#include "mission/mission_rules.h"


extern int g_missionAssassinationEventKindsReady;      // 0x6bbb08
extern EventKindSet g_missionAssassinationEventKinds;  // 0x6bbb00


namespace {
constexpr uint32_t kFamilyMissionAssassination = 0x63;
constexpr int kTaskSucceeded = 3;
constexpr uint32_t kDestroyedState = 0x8;
}  // namespace

MissionAssassination::MissionAssassination() = default;

MissionAssassination::~MissionAssassination() = default;

REB_MISSION_SHARED_PERSISTENCE(MissionAssassination)

uint32_t MissionAssassination::GetFamily() { return kFamilyMissionAssassination; }

int MissionAssassination::GetEventKinds(int unused, EventKindSet* out) {
    return CopyEventKinds(BuildEventKinds, &g_missionAssassinationEventKinds, out);
}

int MissionAssassination::CheckOrder(int a, int orderKind, StatusCode* out) {
    return CheckOrderAgainstKinds(BuildEventKinds, &g_missionAssassinationEventKinds, orderKind, out);
}

int MissionAssassination::GetCheckFunctions(MissionCheckFunctions* out) {
    out->m_unk00 = reinterpret_cast<void*>(MissionRuleBase::CheckMember);
    out->m_unk04 = reinterpret_cast<void*>(CheckTarget);
    return 1;
}

GameObj* MissionAssassination::CreateViewObject() { return NewOrNull<MissionAssassination>(); }

GameObjNotifier* MissionAssassination::Create() { return NewOrNull<MissionAssassination>(); }

int MissionAssassination::CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out) {
    *status = MakeStatus(mission_status::kOk);
    *out = 0;
    return MissionCharacterTargetBase::CheckTarget(check, status, out);
}

int MissionAssassination::Virtual157(GameObj* person, int* out) {
    Character* target = nullptr;
    const int found = static_cast<MissionCharacterTargetBase*>(this)->MissionCharacterTargetBase::GetObjectiveCharacter(reinterpret_cast<GameObj**>(&target));
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
    const int found = static_cast<MissionCharacterTargetBase*>(this)->MissionCharacterTargetBase::GetObjectiveCharacter(reinterpret_cast<GameObj**>(&target));
    bool ok = found != 0;
    if (target) ok = target->Virtual187(ctx) && ok;
    if (!target || !(target->m_stateFlags & kDestroyedState)) return ok;
    ok = SetTaskStatus(kTaskSucceeded, ctx) && ok;
    Role* person = static_cast<Role*>(personObject);
    if (!person->Virtual118()) return ok;
    return person->SetBaseCombat(person->m_baseCombat + g_gnprtb1814, ctx) && ok ? 1 : 0;
}

int MissionAssassination::BuildEventKinds() {
    if (!g_missionAssassinationEventKindsReady) {
        g_missionAssassinationEventKindsReady = 1;
        return MissionCharacterTargetBase::InitEventKinds(0, &g_missionAssassinationEventKinds);
    }
    return 1;
}
EventKindSet g_missionAssassinationEventKinds{};
int g_missionAssassinationEventKindsReady{};
