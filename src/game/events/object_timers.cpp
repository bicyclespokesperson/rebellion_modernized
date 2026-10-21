#include "events/object_timers.h"
#include "character/character_mgr.h"
#include "events/object_timers_decl.h"
#include "gameobj/object_queries.h"

#include "character/character.h"
#include "character/character_data.h"
#include "economy/procfacil.h"
#include "economy/side.h"
#include "events/event_helpers.h"
#include "fleet/combat_unit.h"
#include "fleet/unit_data_decl.h"
#include "gameobj/game_obj.h"

using events::fireOnTimerOwner;
using events::timerStatOfOwner;

FamilyRange* GetSideFamilyRange(FamilyRange* out)
{
  *out = FamilyRange(0xf3, 0xf4);
  return out;
}

FamilyRange* GetProcFacilFamilyRange(FamilyRange* out)
{
  *out = FamilyRange(0x28, 0x30);
  return out;
}

FamilyRange* GetAnyFamilyRange(FamilyRange* out)
{
  *out = FamilyRange(0x1, 0xff);
  return out;
}

FamilyRange* GetFamily14Range(FamilyRange* out)
{
  *out = FamilyRange(0x14, 0x20);
  return out;
}

FamilyRange* GetCharacterFamilyRange(FamilyRange* out)
{
  *out = FamilyRange(0x30, 0x3c);
  return out;
}

SideScrapTimer::SideScrapTimer()
    : TimerEvent()
{
}

SideScrapTimer::SideScrapTimer(const SideScrapTimer& other) = default;

SideScrapTimer::~SideScrapTimer() = default;

SideScrapTimer* SideScrapTimer::Clone()
{
  return NewOrNull<SideScrapTimer>(*this);
}

TrackedStat* SideScrapTimer::GetTargetSpec()
{
  return timerStatOfOwner<Side>(this, ResolveMasterInFamilies_f3_f4, [](Side* side) {
    return &side->m_statCc;
  });
}

void SideScrapTimer::Fire()
{
  fireOnTimerOwner<Side>(this, ResolveMasterInFamilies_f3_f4, [](Side* side, NotifyContext* ctx) {
    return side->ScrapRandomUnit(ctx);
  });
}

SideScrapTimer* CreateSideScrapTimer()
{
  return NewOrNull<SideScrapTimer>();
}

SideResearchTimer::SideResearchTimer()
    : TimerEvent()
{
}

SideResearchTimer::SideResearchTimer(const SideResearchTimer& other) = default;

SideResearchTimer::~SideResearchTimer() = default;

SideResearchTimer* SideResearchTimer::Clone()
{
  return NewOrNull<SideResearchTimer>(*this);
}

TrackedStat* SideResearchTimer::GetTargetSpec()
{
  return timerStatOfOwner<Side>(this, ResolveMasterInFamilies_f3_f4, [](Side* side) {
    return &side->m_statD4;
  });
}

void SideResearchTimer::Fire()
{
  fireOnTimerOwner<Side>(this, ResolveMasterInFamilies_f3_f4, [](Side* side, NotifyContext* ctx) {
    return side->UpdateRdProgress(ctx);
  });
}

SideResearchTimer* CreateSideResearchTimer()
{
  return NewOrNull<SideResearchTimer>();
}

ProcFacilTimer::ProcFacilTimer()
    : TimerEvent()
{
}

ProcFacilTimer::ProcFacilTimer(const ProcFacilTimer& other) = default;

ProcFacilTimer::~ProcFacilTimer() = default;

ProcFacilTimer* ProcFacilTimer::Clone()
{
  return NewOrNull<ProcFacilTimer>(*this);
}

TrackedStat* ProcFacilTimer::GetTargetSpec()
{
  return timerStatOfOwner<ProcFacil>(this, ResolveMasterInFamilies_28_30, [](ProcFacil* p) {
    return p->m_data ? &static_cast<ProcFacilData*>(p->m_data)->m_processingStat : nullptr;
  });
}

void ProcFacilTimer::Fire()
{
  fireOnTimerOwner<ProcFacil>(this, ResolveMasterInFamilies_28_30, [](ProcFacil* p, NotifyContext* ctx) {
    return p->MarkPointProcessedIfProcessing(ctx);
  });
}

ProcFacilTimer* CreateProcFacilTimer()
{
  return NewOrNull<ProcFacilTimer>();
}

HyperdriveTimer::HyperdriveTimer()
    : TimerEvent()
{
}

HyperdriveTimer::HyperdriveTimer(const HyperdriveTimer& other) = default;

HyperdriveTimer::~HyperdriveTimer() = default;

HyperdriveTimer* HyperdriveTimer::Clone()
{
  return NewOrNull<HyperdriveTimer>(*this);
}

TrackedStat* HyperdriveTimer::GetTargetSpec()
{
  return timerStatOfOwner<GameObj>(this, ResolveMasterInFamilies_01_ff, [](GameObj* o) {
    return static_cast<TrackedStat*>(o->GetDataExtra());
  });
}

void HyperdriveTimer::Fire()
{
  fireOnTimerOwner<GameObj>(this, ResolveMasterInFamilies_01_ff, [](GameObj* o, NotifyContext* ctx) {
    return o->UpdateHyperdriveAndEta(ctx);
  });
}

HyperdriveTimer* CreateHyperdriveTimer()
{
  return NewOrNull<HyperdriveTimer>();
}

FleetSlot138Timer::FleetSlot138Timer()
    : TimerEvent()
{
}

FleetSlot138Timer::FleetSlot138Timer(const FleetSlot138Timer& other) = default;

FleetSlot138Timer::~FleetSlot138Timer() = default;

FleetSlot138Timer* FleetSlot138Timer::Clone()
{
  return NewOrNull<FleetSlot138Timer>(*this);
}

TrackedStat* FleetSlot138Timer::GetTargetSpec()
{
  return timerStatOfOwner<GameObj>(this, ResolveMasterInFamilies_14_20, [](GameObj* o) {
    return o->m_data ? &static_cast<CombatUnitData*>(o->m_data)->m_repairStat : nullptr;
  });
}

void FleetSlot138Timer::Fire()
{
  fireOnTimerOwner<GameObj>(this, ResolveMasterInFamilies_14_20, [](GameObj* o, NotifyContext* ctx) {
    return static_cast<CombatUnit*>(o)->RepairStep(ctx);
  });
}

FleetSlot138Timer* CreateFleetSlot138Timer()
{
  return NewOrNull<FleetSlot138Timer>();
}

CharacterInjuryTimer::CharacterInjuryTimer()
    : TimerEvent()
{
}

CharacterInjuryTimer::CharacterInjuryTimer(const CharacterInjuryTimer& other) = default;

CharacterInjuryTimer::~CharacterInjuryTimer() = default;

CharacterInjuryTimer* CharacterInjuryTimer::Clone()
{
  return NewOrNull<CharacterInjuryTimer>(*this);
}

TrackedStat* CharacterInjuryTimer::GetTargetSpec()
{
  return timerStatOfOwner<Character>(this, ResolveMasterInFamilies_30_3c, [](Character* c) {
    return c->m_data ? &static_cast<CharacterData*>(c->m_data)->m_healingStat : nullptr;
  });
}

void CharacterInjuryTimer::Fire()
{
  fireOnTimerOwner<Character>(this, ResolveMasterInFamilies_30_3c, [](Character* c, NotifyContext* ctx) {
    return c->RecoverFromInjury(ctx);
  });
}

CharacterInjuryTimer* CreateCharacterInjuryTimer()
{
  return NewOrNull<CharacterInjuryTimer>();
}

CharacterTimer389::CharacterTimer389()
    : TimerEvent()
{
}

CharacterTimer389::CharacterTimer389(const CharacterTimer389& other) = default;

CharacterTimer389::~CharacterTimer389() = default;

CharacterTimer389* CharacterTimer389::Clone()
{
  return NewOrNull<CharacterTimer389>(*this);
}

TrackedStat* CharacterTimer389::GetTargetSpec()
{
  return timerStatOfOwner<Character>(this, ResolveMasterInFamilies_30_3c, [](Character* c) {
    return c->m_data ? &static_cast<CharacterData*>(c->m_data)->m_forceDiscoveryStat : nullptr;
  });
}

void CharacterTimer389::Fire()
{
  fireOnTimerOwner<Character>(this, ResolveMasterInFamilies_30_3c, [](Character* c, NotifyContext* ctx) {
    // The original calls the character manager (game root +0xc0) with the character as an argument.
    CharacterMgr* characterManager = GetCharacterManager();
    return characterManager ? characterManager->DiscoverForceUser(c, ctx) : 0;
  });
}

CharacterTimer389* CreateCharacterTimer389()
{
  return NewOrNull<CharacterTimer389>();
}

CharacterEscapeTimer::CharacterEscapeTimer()
    : TimerEvent()
{
}

CharacterEscapeTimer::CharacterEscapeTimer(const CharacterEscapeTimer& other) = default;

CharacterEscapeTimer::~CharacterEscapeTimer() = default;

CharacterEscapeTimer* CharacterEscapeTimer::Clone()
{
  return NewOrNull<CharacterEscapeTimer>(*this);
}

TrackedStat* CharacterEscapeTimer::GetTargetSpec()
{
  return timerStatOfOwner<Character>(this, ResolveMasterInFamilies_30_3c, [](Character* c) {
    return c->m_data ? &static_cast<CharacterData*>(c->m_data)->m_escapeStat : nullptr;
  });
}

void CharacterEscapeTimer::Fire()
{
  fireOnTimerOwner<Character>(this, ResolveMasterInFamilies_30_3c, [](Character* c, NotifyContext* ctx) {
    return c->PulseEscapeAttempt(ctx);
  });
}

CharacterEscapeTimer* CreateCharacterEscapeTimer()
{
  return NewOrNull<CharacterEscapeTimer>();
}
