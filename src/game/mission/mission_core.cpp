#include "mission/mission.h"

#include <new>

#include "core/vptr_store.h"
#include "gameobj/object_database.h"
#include "gameobj/object_queries.h"

extern "C" int __cdecl atexit(void(__cdecl* function)(void));
extern "C" void __cdecl LAB_00525730();

extern int g_missionEventKindsBuilt;     // 0x6b8fe0
extern EventKindSet g_missionEventKinds;  // 0x6b8fe8

namespace {

constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusNotSupported = 0x17;
constexpr int kKindMissionOrder = 0x250;
constexpr int kDefaultEventKindValue = 1;
constexpr int kPhaseReadyToStart = 4;
constexpr int kPhaseOutcome = 8;

}  // namespace

// ---- construction -----------------------------------------------------------------------------------------

Mission::Mission()
    : m_userId(0), m_userId2(0), m_taskStatus(0), m_completionStatus(0), m_phase(0), m_flagsA4(0) {
    m_leaderSeedKey.m_value = 0;
}

Mission::~Mission() { REB_KEEP_VPTR_STORE(); }

// ---- overrides of GameObj slots ---------------------------------------------------------------------------

int Mission::GetOwnerKey(ObjectKey* out) {
    out->Reset();
    return 1;
}

int Mission::BuildEventKinds() {
    if (g_missionEventKindsBuilt) return 1;
    g_missionEventKindsBuilt = 1;
    const int baseOk = InitEventKinds(0, &g_missionEventKinds);
    const int addOk = g_missionEventKinds.Add(kKindMissionOrder, kDefaultEventKindValue) != nullptr;
    return addOk && baseOk;
}

int Mission::GetBaseEventKinds(int unused, EventKindSet* out) {
    const int result = BuildEventKinds();
    *out = g_missionEventKinds;
    return result;
}

int Mission::GetEventKinds(int unused, EventKindSet* out) { return GetBaseEventKinds(unused, out); }

int Mission::CheckOrder(int a, int orderKind, StatusCode* out) {
    *out = StatusCode(kStatusCategory, kStatusNotSupported);
    const int result = BuildEventKinds();
    if (result && g_missionEventKinds.Contains(orderKind)) *out = StatusCode(kStatusCategory, kStatusOk);
    return result;
}

void ConstructMissionEventKinds() { new (&g_missionEventKinds) EventKindSet(); }  // 0x525710

void RegisterMissionEventKindsAtExit() { atexit(LAB_00525730); }  // 0x525720

void Mission::InitStatics() {
    ConstructMissionEventKinds();
    RegisterMissionEventKindsAtExit();
}

// ---- simple accessors -------------------------------------------------------------------------------------

namespace {
MissionData* dataOf(Mission* mission) { return static_cast<MissionData*>(mission->m_data); }
}  // namespace

TrackedStat* Mission::GetStatA() { return m_data ? &dataOf(this)->m_statA : nullptr; }

TrackedStat* Mission::GetStatB() { return m_data ? &dataOf(this)->m_statB : nullptr; }

int Mission::GetDataField1c() { return m_data ? dataOf(this)->m_unk1c : 0; }

int Mission::IsDataField1cAbove4() { return GetDataField1c() > 4; }

int Mission::IsDataField1cAbove6() { return GetDataField1c() > 6; }

int Mission::IsPhase4() { return m_phase == kPhaseReadyToStart; }

int Mission::IsPhase8() { return m_phase == kPhaseOutcome; }

int Mission::GetRecordTimerMinDays() { return m_record ? GetRecordSD()->m_timerMinDays : 0; }

int Mission::GetRecordTimerSpreadDays() { return m_record ? GetRecordSD()->m_timerSpreadDays : 0; }

int Mission::GetRecordRepeats() { return m_record ? GetRecordSD()->m_repeats : 0; }

int Mission::GetRecordHidden() { return m_record ? GetRecordSD()->m_hidden : 0; }

int Mission::GetRecordDetectionPhases() { return m_record ? GetRecordSD()->m_detectionPhases : 0; }

int Mission::GetRecordCanResign() { return m_record ? GetRecordSD()->m_canResign : 0; }

int Mission::GetRecordField68() { return m_record ? GetRecordSD()->m_field68 : 0; }

int Mission::TargetLocationDiffersFromOrigin() { return m_targetLocationKey.m_value != m_originLocationKey.m_value; }

int Mission::IsInTeam(ObjectKey* key) { return m_team.Find(key) != nullptr; }

int Mission::IsInDecoys(ObjectKey* key) { return m_decoys.Find(key) != nullptr; }

int Mission::IsInCaptured(ObjectKey* key) { return m_captured.Find(key) != nullptr; }

int Mission::IsMember(ObjectKey* key) { return IsInTeam(key) || IsInDecoys(key) || IsInCaptured(key); }

int Mission::TargetKeyIs(ObjectKey* key) { return m_targetKey.m_value == key->m_value; }

int Mission::TargetLocationKeyIs(ObjectKey* key) { return m_targetLocationKey.m_value == key->m_value; }

int Mission::ObjectiveKeyIs(ObjectKey* key) { return m_objectiveKey.m_value == key->m_value; }

int Mission::HasCompletionStatus() { return m_completionStatus != 0; }

// ---- mission iterator -------------------------------------------------------------------------------------

MissionIterator::MissionIterator(GameObjNotifier* parent)
    : ChildIterator(parent, &family::kMissions, 1) {}

MissionIterator::~MissionIterator() { REB_KEEP_VPTR_STORE(); }
