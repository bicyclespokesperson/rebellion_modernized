#include "gameobj/object_queries.h"
#include "events/object_timers.h"
#include "events/object_timers_decl.h"

#include "character/character.h"
#include "economy/procfacil.h"
#include "economy/side.h"
#include "events/event_helpers.h"
#include "gameobj/game_obj.h"

using events::fireOnTimerOwner;
using events::timerStatOfOwner;

FamilyRange* __stdcall GetSideFamilyRange(FamilyRange* out) {
    *out = FamilyRange(0xf3, 0xf4);
    return out;
}

FamilyRange* __stdcall GetProcFacilFamilyRange(FamilyRange* out) {
    *out = FamilyRange(0x28, 0x30);
    return out;
}

FamilyRange* __stdcall GetAnyFamilyRange(FamilyRange* out) {
    *out = FamilyRange(0x1, 0xff);
    return out;
}

FamilyRange* __stdcall GetFamily14Range(FamilyRange* out) {
    *out = FamilyRange(0x14, 0x20);
    return out;
}

FamilyRange* __stdcall GetCharacterFamilyRange(FamilyRange* out) {
    *out = FamilyRange(0x30, 0x3c);
    return out;
}


SideScrapTimer::SideScrapTimer() : TimerEvent() {}

SideScrapTimer::SideScrapTimer(const SideScrapTimer& other) : TimerEvent(other) {}

SideScrapTimer::~SideScrapTimer() { REB_KEEP_VPTR_STORE(); }

SideScrapTimer* SideScrapTimer::Clone() { return NewOrNull<SideScrapTimer>(*this); }

TrackedStat* SideScrapTimer::GetTargetSpec() {
    return timerStatOfOwner<Side>(this, ResolveMasterInFamilies_f3_f4, [](Side* side) { return &side->m_statCc; });
}

void SideScrapTimer::Fire() {
    fireOnTimerOwner<Side>(this, ResolveMasterInFamilies_f3_f4, [](Side* side, NotifyContext* ctx) { return side->ScrapRandomUnit(ctx); });
}

SideScrapTimer* CreateSideScrapTimer() { return NewOrNull<SideScrapTimer>(); }

SideResearchTimer::SideResearchTimer() : TimerEvent() {}

SideResearchTimer::SideResearchTimer(const SideResearchTimer& other) : TimerEvent(other) {}

SideResearchTimer::~SideResearchTimer() { REB_KEEP_VPTR_STORE(); }

SideResearchTimer* SideResearchTimer::Clone() { return NewOrNull<SideResearchTimer>(*this); }

TrackedStat* SideResearchTimer::GetTargetSpec() {
    return timerStatOfOwner<Side>(this, ResolveMasterInFamilies_f3_f4, [](Side* side) { return &side->m_statD4; });
}

void SideResearchTimer::Fire() {
    fireOnTimerOwner<Side>(this, ResolveMasterInFamilies_f3_f4, [](Side* side, NotifyContext* ctx) { return side->UpdateRdProgress(ctx); });
}

SideResearchTimer* CreateSideResearchTimer() { return NewOrNull<SideResearchTimer>(); }

ProcFacilTimer::ProcFacilTimer() : TimerEvent() {}

ProcFacilTimer::ProcFacilTimer(const ProcFacilTimer& other) : TimerEvent(other) {}

ProcFacilTimer::~ProcFacilTimer() { REB_KEEP_VPTR_STORE(); }

ProcFacilTimer* ProcFacilTimer::Clone() { return NewOrNull<ProcFacilTimer>(*this); }

TrackedStat* ProcFacilTimer::GetTargetSpec() {
    return timerStatOfOwner<ProcFacil>(this, ResolveMasterInFamilies_28_30, [](ProcFacil* p) { return FUN_005092b0(p); });
}

void ProcFacilTimer::Fire() {
    fireOnTimerOwner<ProcFacil>(this, ResolveMasterInFamilies_28_30, [](ProcFacil* p, NotifyContext* ctx) { return p->MarkPointProcessedIfProcessing(ctx); });
}

ProcFacilTimer* CreateProcFacilTimer() { return NewOrNull<ProcFacilTimer>(); }

HyperdriveTimer::HyperdriveTimer() : TimerEvent() {}

HyperdriveTimer::HyperdriveTimer(const HyperdriveTimer& other) : TimerEvent(other) {}

HyperdriveTimer::~HyperdriveTimer() { REB_KEEP_VPTR_STORE(); }

HyperdriveTimer* HyperdriveTimer::Clone() { return NewOrNull<HyperdriveTimer>(*this); }

TrackedStat* HyperdriveTimer::GetTargetSpec() {
    return timerStatOfOwner<GameObj>(this, ResolveMasterInFamilies_01_ff, [](GameObj* o) { return static_cast<TrackedStat*>(o->GetDataExtra()); });
}

void HyperdriveTimer::Fire() {
    fireOnTimerOwner<GameObj>(this, ResolveMasterInFamilies_01_ff, [](GameObj* o, NotifyContext* ctx) { return o->UpdateHyperdriveAndEta(ctx); });
}

HyperdriveTimer* CreateHyperdriveTimer() { return NewOrNull<HyperdriveTimer>(); }

FleetSlot138Timer::FleetSlot138Timer() : TimerEvent() {}

FleetSlot138Timer::FleetSlot138Timer(const FleetSlot138Timer& other) : TimerEvent(other) {}

FleetSlot138Timer::~FleetSlot138Timer() { REB_KEEP_VPTR_STORE(); }

FleetSlot138Timer* FleetSlot138Timer::Clone() { return NewOrNull<FleetSlot138Timer>(*this); }

TrackedStat* FleetSlot138Timer::GetTargetSpec() {
    return timerStatOfOwner<GameObj>(this, ResolveMasterInFamilies_14_20, [](GameObj* o) { return FUN_005092b0(o); });
}

void FleetSlot138Timer::Fire() {
    fireOnTimerOwner<GameObj>(this, ResolveMasterInFamilies_14_20, [](GameObj* o, NotifyContext* ctx) { return events::callVirtualSlot<int>(o, 138, ctx); });
}

FleetSlot138Timer* CreateFleetSlot138Timer() { return NewOrNull<FleetSlot138Timer>(); }

CharacterInjuryTimer::CharacterInjuryTimer() : TimerEvent() {}

CharacterInjuryTimer::CharacterInjuryTimer(const CharacterInjuryTimer& other) : TimerEvent(other) {}

CharacterInjuryTimer::~CharacterInjuryTimer() { REB_KEEP_VPTR_STORE(); }

CharacterInjuryTimer* CharacterInjuryTimer::Clone() { return NewOrNull<CharacterInjuryTimer>(*this); }

TrackedStat* CharacterInjuryTimer::GetTargetSpec() {
    return timerStatOfOwner<Character>(this, ResolveMasterInFamilies_30_3c, [](Character* c) { return FUN_005092b0(c); });
}

void CharacterInjuryTimer::Fire() {
    fireOnTimerOwner<Character>(this, ResolveMasterInFamilies_30_3c, [](Character* c, NotifyContext* ctx) { return c->RecoverFromInjury(ctx); });
}

CharacterInjuryTimer* CreateCharacterInjuryTimer() { return NewOrNull<CharacterInjuryTimer>(); }

CharacterTimer389::CharacterTimer389() : TimerEvent() {}

CharacterTimer389::CharacterTimer389(const CharacterTimer389& other) : TimerEvent(other) {}

CharacterTimer389::~CharacterTimer389() { REB_KEEP_VPTR_STORE(); }

CharacterTimer389* CharacterTimer389::Clone() { return NewOrNull<CharacterTimer389>(*this); }

TrackedStat* CharacterTimer389::GetTargetSpec() {
    return timerStatOfOwner<Character>(this, ResolveMasterInFamilies_30_3c, [](Character* c) { return FUN_00520aa0(c); });
}

void CharacterTimer389::Fire() {
    fireOnTimerOwner<Character>(this, ResolveMasterInFamilies_30_3c, [](Character* c, NotifyContext* ctx) {
        // The original calls the character manager (game root +0xc0) with the character as an argument.
        GameObjNotifier* characterManager = GetGameRootMember_c0();
        return characterManager ? FUN_00560a30(characterManager, c, ctx) : 0;
    });
}

CharacterTimer389* CreateCharacterTimer389() { return NewOrNull<CharacterTimer389>(); }

CharacterEscapeTimer::CharacterEscapeTimer() : TimerEvent() {}

CharacterEscapeTimer::CharacterEscapeTimer(const CharacterEscapeTimer& other) : TimerEvent(other) {}

CharacterEscapeTimer::~CharacterEscapeTimer() { REB_KEEP_VPTR_STORE(); }

CharacterEscapeTimer* CharacterEscapeTimer::Clone() { return NewOrNull<CharacterEscapeTimer>(*this); }

TrackedStat* CharacterEscapeTimer::GetTargetSpec() {
    return timerStatOfOwner<Character>(this, ResolveMasterInFamilies_30_3c, [](Character* c) { return FUN_00520ab0(c); });
}

void CharacterEscapeTimer::Fire() {
    fireOnTimerOwner<Character>(this, ResolveMasterInFamilies_30_3c, [](Character* c, NotifyContext* ctx) { return c->PulseEscapeAttempt(ctx); });
}

CharacterEscapeTimer* CreateCharacterEscapeTimer() { return NewOrNull<CharacterEscapeTimer>(); }
