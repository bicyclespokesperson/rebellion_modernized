#include "core/reb_abi.h"
#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "events/event_helpers.h"
#include "gameobj/object_queries.h"
#include "mission/mission.h"
#include "mission/mission_events.h"
#include "story/mission_mgr.h"

using events::fireOnTimerOwner;
using events::timerStatOfOwner;

namespace {

constexpr int kMissionMgrStatAOffset = 0x5c;
constexpr int kMissionMgrStatBOffset = 0x64;

constexpr FamilyRange kMissionFamilies(0x40, 0x80);
constexpr FamilyRange kMissionMgrFamilies(0xf8, 0xf9);

TrackedStat* managerStatA(GameObj* manager) {
    return reinterpret_cast<TrackedStat*>(reinterpret_cast<uint8_t*>(manager) + kMissionMgrStatAOffset);
}
TrackedStat* managerStatB(GameObj* manager) {
    return reinterpret_cast<TrackedStat*>(reinterpret_cast<uint8_t*>(manager) + kMissionMgrStatBOffset);
}

}  // namespace

// ---- shared slot 10 --------------------------------------------------------------------------------------------------

FamilyRange* REB_STDCALL GetMissionFamilyRange(FamilyRange* out) {
    *out = kMissionFamilies;
    return out;
}

FamilyRange* REB_STDCALL GetMissionMgrFamilyRange(FamilyRange* out) {
    *out = kMissionMgrFamilies;
    return out;
}

// ---- MissionStatATimer (kind 0x38b) ----------------------------------------------------------------------------------

MissionStatATimer::MissionStatATimer() : TimerEvent() {}

MissionStatATimer::MissionStatATimer(const MissionStatATimer& other) : TimerEvent(other) {}

MissionStatATimer::~MissionStatATimer() { REB_KEEP_VPTR_STORE(); }

MissionStatATimer* MissionStatATimer::Clone() { return NewOrNull<MissionStatATimer>(*this); }

TrackedStat* MissionStatATimer::GetTargetSpec() {
    return timerStatOfOwner<Mission>(this, ResolveMasterInFamilies_40_80, [](Mission* mission) { return mission->GetStatA(); });
}

void MissionStatATimer::Fire() {
    fireOnTimerOwner<Mission>(this, ResolveMasterInFamilies_40_80, [](Mission* mission, NotifyContext* ctx) {
        return mission->Mission::ReadyIfPhase8(ctx);
    });
}

MissionStatATimer* CreateMissionStatATimer() { return NewOrNull<MissionStatATimer>(); }

// ---- MissionStatBTimer (kind 0x38c) ----------------------------------------------------------------------------------

MissionStatBTimer::MissionStatBTimer() : TimerEvent() {}

MissionStatBTimer::MissionStatBTimer(const MissionStatBTimer& other) : TimerEvent(other) {}

MissionStatBTimer::~MissionStatBTimer() { REB_KEEP_VPTR_STORE(); }

MissionStatBTimer* MissionStatBTimer::Clone() { return NewOrNull<MissionStatBTimer>(*this); }

TrackedStat* MissionStatBTimer::GetTargetSpec() {
    return timerStatOfOwner<Mission>(this, ResolveMasterInFamilies_40_80, [](Mission* mission) { return mission->GetStatB(); });
}

void MissionStatBTimer::Fire() {
    fireOnTimerOwner<Mission>(this, ResolveMasterInFamilies_40_80, [](Mission* mission, NotifyContext* ctx) {
        return mission->Mission::DestroyIfPhase11(ctx);
    });
}

MissionStatBTimer* CreateMissionStatBTimer() { return NewOrNull<MissionStatBTimer>(); }

// ---- MissionMgrStatATimer (kind 0x392) -------------------------------------------------------------------------------

MissionMgrStatATimer::MissionMgrStatATimer() : TimerEvent() {}

MissionMgrStatATimer::MissionMgrStatATimer(const MissionMgrStatATimer& other) : TimerEvent(other) {}

MissionMgrStatATimer::~MissionMgrStatATimer() { REB_KEEP_VPTR_STORE(); }

MissionMgrStatATimer* MissionMgrStatATimer::Clone() { return NewOrNull<MissionMgrStatATimer>(*this); }

TrackedStat* MissionMgrStatATimer::GetTargetSpec() {
    return timerStatOfOwner<GameObj>(this, ResolveMasterInFamilies_f8_f9, managerStatA);
}

void MissionMgrStatATimer::Fire() {
    fireOnTimerOwner<GameObj>(this, ResolveMasterInFamilies_f8_f9, [](GameObj* manager, NotifyContext* ctx) {
        return (reinterpret_cast<MissionMgr*>(manager))->MissionMgr::UpdateStoryFlags(ctx);
    });
}

MissionMgrStatATimer* CreateMissionMgrStatATimer() { return NewOrNull<MissionMgrStatATimer>(); }

// ---- MissionMgrStatBTimer (kind 0x393) -------------------------------------------------------------------------------

MissionMgrStatBTimer::MissionMgrStatBTimer() : TimerEvent() {}

MissionMgrStatBTimer::MissionMgrStatBTimer(const MissionMgrStatBTimer& other) : TimerEvent(other) {}

MissionMgrStatBTimer::~MissionMgrStatBTimer() { REB_KEEP_VPTR_STORE(); }

MissionMgrStatBTimer* MissionMgrStatBTimer::Clone() { return NewOrNull<MissionMgrStatBTimer>(*this); }

TrackedStat* MissionMgrStatBTimer::GetTargetSpec() {
    return timerStatOfOwner<GameObj>(this, ResolveMasterInFamilies_f8_f9, managerStatB);
}

void MissionMgrStatBTimer::Fire() {
    fireOnTimerOwner<GameObj>(this, ResolveMasterInFamilies_f8_f9, [](GameObj* manager, NotifyContext* ctx) {
        return (reinterpret_cast<MissionMgr*>(manager))->MissionMgr::CreateStoryMissionAtRandom(ctx);
    });
}

MissionMgrStatBTimer* CreateMissionMgrStatBTimer() { return NewOrNull<MissionMgrStatBTimer>(); }
