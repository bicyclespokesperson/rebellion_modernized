#include "mission/mission_espionage.h"

#include "data/random.h"
#include "economy/defense_facility.h"
#include "galaxy/system.h"
#include "gameobj/object_queries.h"

extern "C" void __cdecl LAB_005739a0();

extern int g_missionEspionageEventKindsReady;      // 0x6bba64
extern EventKindSet g_missionEspionageEventKinds;  // 0x6bba68

int __cdecl FUN_0055c6c0(int espionage, int* out);  // LookupEspionageMissionTable
int __cdecl FUN_0055c940(int isCoreSystem, int isHomeSystem, int hasFacility, int* count, int* includeType90,
                         int* includeType92);  // ComputeEspionageOutcome, always 1
int FUN_0053e2f0(int threshold);
int FUN_0053fe40(int eventId, GameObj* object, ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx);
extern int DAT_006bb534;  // g_gnprtb1810

namespace {
constexpr uint32_t kFamilyMissionEspionage = 0x52;
constexpr uint32_t kHomeSystemKey = 0x90000109;
constexpr int kEventExtraSystemChanged = 0x370;
constexpr int kEventSyncMode = 10;
constexpr int kTaskStarted = 2;
constexpr int kTaskSucceeded = 3;
constexpr uint32_t kSideBitsControl = 0xc0;
constexpr uint32_t kControlSideSecond = 0x80;

bool InFamilyRange(const FamilyRange& range, uint32_t family) { return family >= range.m_first && family < range.m_second; }

int OpposingSideIndex(uint32_t controlSide) {
    if (controlSide == 3) return 3;
    return controlSide == 1 ? 2 : 1;
}
}  // namespace

MissionEspionage::MissionEspionage() {}

MissionEspionage::~MissionEspionage() { REB_KEEP_VPTR_STORE(); }

uint32_t MissionEspionage::GetFamily() { return kFamilyMissionEspionage; }

int MissionEspionage::ApplyAll(NotifyContext* ctx) { return IsSideView() ? ApplyAllBody(ctx) : 0; }

int MissionEspionage::ApplyAllBody(NotifyContext* ctx) { return ApplyAllWithExtraKey(this, ctx); }

int MissionEspionage::GetEventKinds(int unused, EventKindSet* out) {
    return CopyEventKinds(BuildEventKinds, &g_missionEspionageEventKinds, out);
}

int MissionEspionage::CheckOrder(int a, int orderKind, StatusCode* out) {
    return CheckOrderAgainstKinds(BuildEventKinds, &g_missionEspionageEventKinds, orderKind, out);
}

int MissionEspionage::GetCheckFunctions(MissionCheckFunctions* out) {
    out->m_unk00 = reinterpret_cast<void*>(FUN_00592aa0);
    out->m_unk04 = reinterpret_cast<void*>(CheckTarget);
    return 1;
}

GameObj* MissionEspionage::CreateViewObject() { return NewOrNull<MissionEspionage>(); }

GameObjNotifier* MissionEspionage::Create() { return NewOrNull<MissionEspionage>(); }

int MissionEspionage::CheckTarget(MissionOrderCheck* check, StatusCode* status, int* out) {
    *status = MakeStatus(mission_status::kOk);
    *out = 0;
    return FUN_005868c0(check, status, out);
}

int MissionEspionage::SetExtraSystemKey(ObjectKey* value, NotifyContext* ctx) {
    return SetExtraKeyProperty(this, value, ctx);
}

int MissionEspionage::ApplyExtraKey(NotifyContext* ctx) { return ApplyExtraKeyProperty(this, ctx); }

int MissionEspionage::OnExtraKeyChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx) {
    System* system = static_cast<System*>(GetMasterInFamilies_90_98(newKey));
    int result = system != nullptr;
    if (!system) return result;
    if (!system->SyncViewForEvent(kEventSyncMode, static_cast<int>(ControlSide()), ctx)) return 0;
    return result;
}

int MissionEspionage::OnExtraKeyNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx) {
    const int result = IsSideView();
    if (!result) return result;
    LogKey("MissionEspionageExtraSystemKeyNotif", "ExtraSystem", newKey, oldKey, 0, ctx);
    return FUN_0053fe40(kEventExtraSystemChanged, this, oldKey, newKey, ctx);
}

int MissionEspionage::Virtual157(GameObj* person, int* out) {
    return FUN_0055c6c0(static_cast<Role*>(person)->GetEspionage(), out);
}

int MissionEspionage::Virtual158(NotifyContext* ctx) { return 1; }

int MissionEspionage::Virtual159(GameObj* person, int roll, int* out, NotifyContext* ctx) {
    *out = 0;
    if (!FUN_0053e2f0(roll)) return 1;
    *out = 1;
    int result = SetTaskStatus(kTaskSucceeded, ctx) != 0;
    GameObj* leader = nullptr;
    result = GetTargetLocationSystem(&leader) && result;
    if (!leader || ControlSide() == leader->ControlSide()) return result;
    Role* role = static_cast<Role*>(person);
    if (!role->Virtual118()) return result;
    const int raised = role->SetBaseEspionage(role->m_baseEspionage + DAT_006bb534, ctx);
    return raised && result ? 1 : 0;
}

int MissionEspionage::Virtual160(NotifyContext* ctx) {
    int result = 1;
    if (m_taskStatus == 0) result = SetTaskStatus(kTaskStarted, ctx) != 0;
    if (m_taskStatus != kTaskSucceeded) return result;

    GameObj* leader = nullptr;
    result = GetTargetLocationSystem(&leader) && result;
    GameObj* target = nullptr;
    if (leader) {
        const FamilyRange systems(0x90, 0x98);
        if (InFamilyRange(systems, leader->GetFamily()))
            target = leader;
        else
            result = 0;
    }
    if (!target) return result;

    const uint32_t side = ControlSide();
    result = static_cast<System*>(target)->SyncViewForEvent(kEventSyncMode, static_cast<int>(side), ctx) && result;
    if (OpposingSideIndex(side) != target->ControlSide()) return result;

    const FamilyRange firstType(0x90, 0x92);
    const int isCoreSystem = InFamilyRange(firstType, target->GetFamily());
    ObjectKey targetKey;
    target->GetKey(&targetKey);
    const int isHomeSystem = targetKey.m_value == kHomeSystemKey && (target->m_flags & kSideBitsControl) == kControlSideSecond;
    AllFacilityIterator facilities(target, 1);
    const int hasFacility = facilities.HasCurrent();

    int count = 0;
    int includeType90 = 0;
    int includeType92 = 0;
    const int computed = FUN_0055c940(isCoreSystem, isHomeSystem, hasFacility, &count, &includeType90, &includeType92);
    result = computed && result ? 1 : 0;
    if (!count) return result;
    return PickExtraSystems(target, count, includeType90, includeType92, ctx) && result ? 1 : 0;
}

int MissionEspionage::PickExtraSystems(GameObj* target, int count, int includeType90, int includeType92,
                                       NotifyContext* ctx) {
    int result = 1;
    FamilyRange wanted;
    if (includeType90 && includeType92)
        wanted = FamilyRange(0x90, 0x98);
    else if (includeType90)
        wanted = FamilyRange(0x90, 0x92);
    else if (includeType92)
        wanted = FamilyRange(0x92, 0x94);

    ObjectKeyValueSet candidates;
    int available = 0;
    const FamilyRange allSystems(0x90, 0x98);
    for (GameObj* system = static_cast<GameObj*>(GetFirstMasterInFamilies_90_98()); system;
         system = static_cast<GameObj*>(system->FindNextSibling(&allSystems, 1))) {
        if (system->ControlSide() != target->ControlSide()) continue;
        ObjectKey targetKey;
        ObjectKey systemKey;
        target->GetKey(&targetKey);
        system->GetKey(&systemKey);
        if (systemKey.m_value == targetKey.m_value || !InFamilyRange(wanted, system->GetFamily())) continue;
        ++available;
        ObjectKey key;
        system->GetKey(&key);
        result = candidates.AddKeyValue(&key, 0) && result;
    }

    if (available < count) count = available;
    while (count != 0) {
        const int last = available - 1;
        const int skip = GameRandom(last);
        AvlNode* node = candidates.First();
        if (!node) continue;
        int step = 0;
        do {
            if (skip <= step) break;
            node = node->m_successor;
            ++step;
        } while (node);
        if (!node) continue;
        ObjectKey key;
        static_cast<ObjectKeyNode*>(node)->GetKey(&key);
        result = SetExtraSystemKey(&key, ctx) && result;
        candidates.Remove(node);
        delete node;
        --count;
        available = last;
    }
    return result;
}

int MissionEspionage::BuildEventKinds() {
    if (!g_missionEspionageEventKindsReady) {
        g_missionEspionageEventKindsReady = 1;
        return FUN_00586b00(0, &g_missionEspionageEventKinds);
    }
    return 1;
}

void ConstructMissionEspionageEventKinds() { new (&g_missionEspionageEventKinds) EventKindSet(); }

void RegisterMissionEspionageEventKindsAtExit() { atexit(LAB_005739a0); }

void MissionEspionage::InitStatics() {
    ConstructMissionEspionageEventKinds();
    RegisterMissionEspionageEventKindsAtExit();
}
