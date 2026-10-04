#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "gameobj/object_queries.h"
#include "mission/mission.h"
#include "mission/mission_events.h"

namespace {

constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusWrongObject = 0x12;

constexpr int kMissionMgrStatAOffset = 0x5c;
constexpr int kMissionMgrStatBOffset = 0x64;

constexpr FamilyRange kMissionFamilies(0x40, 0x80);
constexpr FamilyRange kMissionMgrFamilies(0xf8, 0xf9);

bool succeeded(const StatusCode& status) { return status.m_second == static_cast<uint32_t>(kStatusOk); }

// Slots 11 and 12 of every timer here start the same way: the timer's own checks, then the lookup of the object the
// timer acts on (the stat's owner), which both must find alive. Returns the object, or null with *ok cleared.
template <typename Resolve>
GameObj* findOwner(TimerBase* timer, Resolve resolve, int* ok) {
    StatusCode status(kStatusCategory, kStatusOk);
    GameObjNotifier* owner = nullptr;
    *ok = timer->CheckValid(&status);
    if (*ok) {
        if (succeeded(status)) {
            const StatusCode missing(kStatusCategory, kStatusWrongObject);
            *ok = resolve(&timer->m_key, &owner, &missing, &status);
        }
        if (!*ok || !succeeded(status)) return nullptr;
    }
    return static_cast<GameObj*>(owner);
}

template <typename Resolve, typename Method>
TrackedStat* statOfOwner(TimerBase* timer, Resolve resolve, Method method) {
    int ok = 0;
    GameObj* owner = findOwner(timer, resolve, &ok);
    return ok && owner ? method(owner) : nullptr;
}

template <typename Resolve, typename Method>
int fireOnOwner(TimerBase* timer, Resolve resolve, Method method) {
    int ok = 0;
    GameObj* owner = findOwner(timer, resolve, &ok);
    if (ok && owner) {
        NotifyContext ctx(timer->m_context);
        ok = method(owner, &ctx);
    }
    return ok;
}

TrackedStat* missionStatA(GameObj* mission) { return static_cast<Mission*>(mission)->GetStatA(); }
TrackedStat* missionStatB(GameObj* mission) { return static_cast<Mission*>(mission)->GetStatB(); }
TrackedStat* managerStatA(GameObj* manager) {
    return reinterpret_cast<TrackedStat*>(reinterpret_cast<uint8_t*>(manager) + kMissionMgrStatAOffset);
}
TrackedStat* managerStatB(GameObj* manager) {
    return reinterpret_cast<TrackedStat*>(reinterpret_cast<uint8_t*>(manager) + kMissionMgrStatBOffset);
}

}  // namespace

// ---- shared slot 10 --------------------------------------------------------------------------------------------------

FamilyRange* __stdcall GetMissionFamilyRange(FamilyRange* out) {
    *out = kMissionFamilies;
    return out;
}

FamilyRange* __stdcall GetMissionMgrFamilyRange(FamilyRange* out) {
    *out = kMissionMgrFamilies;
    return out;
}

// ---- MissionStatATimer (kind 0x38b) ----------------------------------------------------------------------------------

MissionStatATimer::MissionStatATimer() : TimerBase() {}

MissionStatATimer::MissionStatATimer(const MissionStatATimer& other) : TimerBase(other) {}

MissionStatATimer::~MissionStatATimer() { REB_KEEP_VPTR_STORE(); }

TimerBase* MissionStatATimer::Clone() { return NewOrNull<MissionStatATimer>(*this); }

FamilyRange* MissionStatATimer::GetFamilyRange(FamilyRange* out) { return GetMissionFamilyRange(out); }

TrackedStat* MissionStatATimer::GetStat() {
    return statOfOwner(this, ResolveMasterInFamilies_40_80, missionStatA);
}

int MissionStatATimer::Fire() {
    return fireOnOwner(this, ResolveMasterInFamilies_40_80, [](GameObj* mission, NotifyContext* ctx) {
        return FUN_00522a10(static_cast<Mission*>(mission), ctx);
    });
}

MissionStatATimer* CreateMissionStatATimer() { return NewOrNull<MissionStatATimer>(); }

// ---- MissionStatBTimer (kind 0x38c) ----------------------------------------------------------------------------------

MissionStatBTimer::MissionStatBTimer() : TimerBase() {}

MissionStatBTimer::MissionStatBTimer(const MissionStatBTimer& other) : TimerBase(other) {}

MissionStatBTimer::~MissionStatBTimer() { REB_KEEP_VPTR_STORE(); }

TimerBase* MissionStatBTimer::Clone() { return NewOrNull<MissionStatBTimer>(*this); }

FamilyRange* MissionStatBTimer::GetFamilyRange(FamilyRange* out) { return GetMissionFamilyRange(out); }

TrackedStat* MissionStatBTimer::GetStat() {
    return statOfOwner(this, ResolveMasterInFamilies_40_80, missionStatB);
}

int MissionStatBTimer::Fire() {
    return fireOnOwner(this, ResolveMasterInFamilies_40_80, [](GameObj* mission, NotifyContext* ctx) {
        return FUN_00522a30(static_cast<Mission*>(mission), ctx);
    });
}

MissionStatBTimer* CreateMissionStatBTimer() { return NewOrNull<MissionStatBTimer>(); }

// ---- MissionMgrStatATimer (kind 0x392) -------------------------------------------------------------------------------

MissionMgrStatATimer::MissionMgrStatATimer() : TimerBase() {}

MissionMgrStatATimer::MissionMgrStatATimer(const MissionMgrStatATimer& other) : TimerBase(other) {}

MissionMgrStatATimer::~MissionMgrStatATimer() { REB_KEEP_VPTR_STORE(); }

TimerBase* MissionMgrStatATimer::Clone() { return NewOrNull<MissionMgrStatATimer>(*this); }

FamilyRange* MissionMgrStatATimer::GetFamilyRange(FamilyRange* out) { return GetMissionMgrFamilyRange(out); }

TrackedStat* MissionMgrStatATimer::GetStat() {
    return statOfOwner(this, ResolveMasterInFamilies_f8_f9, managerStatA);
}

int MissionMgrStatATimer::Fire() {
    return fireOnOwner(this, ResolveMasterInFamilies_f8_f9, [](GameObj* manager, NotifyContext* ctx) {
        return FUN_00546f80(reinterpret_cast<MissionMgr*>(manager), ctx);
    });
}

MissionMgrStatATimer* CreateMissionMgrStatATimer() { return NewOrNull<MissionMgrStatATimer>(); }

// ---- MissionMgrStatBTimer (kind 0x393) -------------------------------------------------------------------------------

MissionMgrStatBTimer::MissionMgrStatBTimer() : TimerBase() {}

MissionMgrStatBTimer::MissionMgrStatBTimer(const MissionMgrStatBTimer& other) : TimerBase(other) {}

MissionMgrStatBTimer::~MissionMgrStatBTimer() { REB_KEEP_VPTR_STORE(); }

TimerBase* MissionMgrStatBTimer::Clone() { return NewOrNull<MissionMgrStatBTimer>(*this); }

FamilyRange* MissionMgrStatBTimer::GetFamilyRange(FamilyRange* out) { return GetMissionMgrFamilyRange(out); }

TrackedStat* MissionMgrStatBTimer::GetStat() {
    return statOfOwner(this, ResolveMasterInFamilies_f8_f9, managerStatB);
}

int MissionMgrStatBTimer::Fire() {
    return fireOnOwner(this, ResolveMasterInFamilies_f8_f9, [](GameObj* manager, NotifyContext* ctx) {
        return FUN_00546fa0(reinterpret_cast<MissionMgr*>(manager), ctx);
    });
}

MissionMgrStatBTimer* CreateMissionMgrStatBTimer() { return NewOrNull<MissionMgrStatBTimer>(); }
