#include "events/incident_timers.h"

#include "events/event_helpers.h"
#include "events/incident_timers_decl.h"
#include "galaxy/system.h"
#include "mission/mission.h"

using events::findTimerOwner;
using events::fireOnTimerOwner;
using events::timerStatOfOwner;

namespace {

TrackedStat* statAt(GameObj* owner, int offset) {
    return reinterpret_cast<TrackedStat*>(reinterpret_cast<uint8_t*>(owner) + offset);
}

}  // namespace

FamilyRange* __stdcall GetSystemFamilyRange(FamilyRange* out) {
    *out = FamilyRange(0x90, 0x98);
    return out;
}

FamilyRange* __stdcall GetFamilyF9Range(FamilyRange* out) {
    *out = FamilyRange(0xf9, 0xfa);
    return out;
}


UprisingEndTimer::UprisingEndTimer() : TimerEvent() {}

UprisingEndTimer::UprisingEndTimer(const UprisingEndTimer& other) : TimerEvent(other) {}

UprisingEndTimer::~UprisingEndTimer() { REB_KEEP_VPTR_STORE(); }

UprisingEndTimer* UprisingEndTimer::Clone() { return NewOrNull<UprisingEndTimer>(*this); }

TrackedStat* UprisingEndTimer::GetTargetSpec() {
    return timerStatOfOwner<System>(this, ResolveMasterInFamilies_90_98, [](System* s) { return s->GetStat0(); });
}

void UprisingEndTimer::Fire() {
    fireOnTimerOwner<System>(this, ResolveMasterInFamilies_90_98, [](System* s, NotifyContext* ctx) { return s->EndUprising(ctx); });
}

UprisingEndTimer* CreateUprisingEndTimer() { return NewOrNull<UprisingEndTimer>(); }

UprisingLoyaltyTimer::UprisingLoyaltyTimer() : TimerEvent() {}

UprisingLoyaltyTimer::UprisingLoyaltyTimer(const UprisingLoyaltyTimer& other) : TimerEvent(other) {}

UprisingLoyaltyTimer::~UprisingLoyaltyTimer() { REB_KEEP_VPTR_STORE(); }

UprisingLoyaltyTimer* UprisingLoyaltyTimer::Clone() { return NewOrNull<UprisingLoyaltyTimer>(*this); }

TrackedStat* UprisingLoyaltyTimer::GetTargetSpec() {
    return timerStatOfOwner<System>(this, ResolveMasterInFamilies_90_98, [](System* s) { return reinterpret_cast<Mission*>(s)->GetStatA(); });
}

void UprisingLoyaltyTimer::Fire() {
    fireOnTimerOwner<System>(this, ResolveMasterInFamilies_90_98, [](System* s, NotifyContext* ctx) { return s->AdjustLoyaltyForUprising(ctx); });
}

UprisingLoyaltyTimer* CreateUprisingLoyaltyTimer() { return NewOrNull<UprisingLoyaltyTimer>(); }

AssaultLoyaltyTimer::AssaultLoyaltyTimer() : TimerEvent() {}

AssaultLoyaltyTimer::AssaultLoyaltyTimer(const AssaultLoyaltyTimer& other) : TimerEvent(other) {}

AssaultLoyaltyTimer::~AssaultLoyaltyTimer() { REB_KEEP_VPTR_STORE(); }

AssaultLoyaltyTimer* AssaultLoyaltyTimer::Clone() { return NewOrNull<AssaultLoyaltyTimer>(*this); }

TrackedStat* AssaultLoyaltyTimer::GetTargetSpec() {
    return timerStatOfOwner<System>(this, ResolveMasterInFamilies_90_98, [](System* s) { return reinterpret_cast<Mission*>(s)->GetStatB(); });
}

void AssaultLoyaltyTimer::Fire() {
    fireOnTimerOwner<System>(this, ResolveMasterInFamilies_90_98, [](System* s, NotifyContext* ctx) { return s->AdjustLoyaltyForAssault(ctx); });
}

AssaultLoyaltyTimer* CreateAssaultLoyaltyTimer() { return NewOrNull<AssaultLoyaltyTimer>(); }

BlockadeLoyaltyTimer::BlockadeLoyaltyTimer() : TimerEvent() {}

BlockadeLoyaltyTimer::BlockadeLoyaltyTimer(const BlockadeLoyaltyTimer& other) : TimerEvent(other) {}

BlockadeLoyaltyTimer::~BlockadeLoyaltyTimer() { REB_KEEP_VPTR_STORE(); }

BlockadeLoyaltyTimer* BlockadeLoyaltyTimer::Clone() { return NewOrNull<BlockadeLoyaltyTimer>(*this); }

TrackedStat* BlockadeLoyaltyTimer::GetTargetSpec() {
    return timerStatOfOwner<System>(this, ResolveMasterInFamilies_90_98, [](System* s) { return s->GetStat3(); });
}

void BlockadeLoyaltyTimer::Fire() {
    fireOnTimerOwner<System>(this, ResolveMasterInFamilies_90_98, [](System* s, NotifyContext* ctx) { return s->AdjustLoyaltyForBlockade(ctx); });
}

BlockadeLoyaltyTimer* CreateBlockadeLoyaltyTimer() { return NewOrNull<BlockadeLoyaltyTimer>(); }

UprisingPulseTimer::UprisingPulseTimer() : TimerEvent() {}

UprisingPulseTimer::UprisingPulseTimer(const UprisingPulseTimer& other) : TimerEvent(other) {}

UprisingPulseTimer::~UprisingPulseTimer() { REB_KEEP_VPTR_STORE(); }

UprisingPulseTimer* UprisingPulseTimer::Clone() { return NewOrNull<UprisingPulseTimer>(*this); }

TrackedStat* UprisingPulseTimer::GetTargetSpec() {
    return timerStatOfOwner<System>(this, ResolveMasterInFamilies_90_98, [](System* s) { return s->GetStat4(); });
}

void UprisingPulseTimer::Fire() {
    fireOnTimerOwner<System>(this, ResolveMasterInFamilies_90_98, [](System* s, NotifyContext* ctx) { return s->PulseUprisingIncident(ctx); });
}

UprisingPulseTimer* CreateUprisingPulseTimer() { return NewOrNull<UprisingPulseTimer>(); }

InformantIncidentTimer::InformantIncidentTimer() : TimerEvent() {}

InformantIncidentTimer::InformantIncidentTimer(const InformantIncidentTimer& other) : TimerEvent(other) {}

InformantIncidentTimer::~InformantIncidentTimer() { REB_KEEP_VPTR_STORE(); }

InformantIncidentTimer* InformantIncidentTimer::Clone() { return NewOrNull<InformantIncidentTimer>(*this); }

TrackedStat* InformantIncidentTimer::GetTargetSpec() {
    return timerStatOfOwner<System>(this, ResolveMasterInFamilies_90_98, [](System* s) { return s->GetStat5(); });
}

void InformantIncidentTimer::Fire() {
    fireOnTimerOwner<System>(this, ResolveMasterInFamilies_90_98, [](System* s, NotifyContext* ctx) { return s->RollInformantIncident(ctx); });
}

InformantIncidentTimer* CreateInformantIncidentTimer() { return NewOrNull<InformantIncidentTimer>(); }

SideStatATimer::SideStatATimer() : TimerEvent() {}

SideStatATimer::SideStatATimer(const SideStatATimer& other) : TimerEvent(other) {}

SideStatATimer::~SideStatATimer() { REB_KEEP_VPTR_STORE(); }

SideStatATimer* SideStatATimer::Clone() { return NewOrNull<SideStatATimer>(*this); }

TrackedStat* SideStatATimer::GetTargetSpec() {
    return timerStatOfOwner<GameObj>(this, ResolveMasterInFamilies_f9_fa, [](GameObj* o) { return statAt(o, 0x58); });
}

void SideStatATimer::Fire() {
    fireOnTimerOwner<GameObj>(this, ResolveMasterInFamilies_f9_fa, [](GameObj* o, NotifyContext* ctx) { return FUN_00556b50(o, ctx); });
}

SideStatATimer* CreateSideStatATimer() { return NewOrNull<SideStatATimer>(); }

SideStatBTimer::SideStatBTimer() : TimerEvent() {}

SideStatBTimer::SideStatBTimer(const SideStatBTimer& other) : TimerEvent(other) {}

SideStatBTimer::~SideStatBTimer() { REB_KEEP_VPTR_STORE(); }

SideStatBTimer* SideStatBTimer::Clone() { return NewOrNull<SideStatBTimer>(*this); }

TrackedStat* SideStatBTimer::GetTargetSpec() {
    return timerStatOfOwner<GameObj>(this, ResolveMasterInFamilies_f9_fa, [](GameObj* o) { return statAt(o, 0x60); });
}

void SideStatBTimer::Fire() {
    fireOnTimerOwner<GameObj>(this, ResolveMasterInFamilies_f9_fa, [](GameObj* o, NotifyContext* ctx) { return FUN_00556be0(o, ctx); });
}

SideStatBTimer* CreateSideStatBTimer() { return NewOrNull<SideStatBTimer>(); }
