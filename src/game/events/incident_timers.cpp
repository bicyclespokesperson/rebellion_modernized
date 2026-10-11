#include "events/incident_timers.h"

#include "events/event_helpers.h"
#include "events/incident_timers_decl.h"
#include "galaxy/system.h"
#include "mission/mission.h"
#include "rules/rules_mgr.h"

using events::findTimerOwner;
using events::fireOnTimerOwner;
using events::timerStatOfOwner;

namespace
{

} // namespace

FamilyRange* GetSystemFamilyRange(FamilyRange* out)
{
  *out = FamilyRange(0x90, 0x98);
  return out;
}

FamilyRange* GetFamilyF9Range(FamilyRange* out)
{
  *out = FamilyRange(0xf9, 0xfa);
  return out;
}

UprisingEndTimer::UprisingEndTimer() : TimerEvent()
{
}

UprisingEndTimer::UprisingEndTimer(const UprisingEndTimer& other) = default;

UprisingEndTimer::~UprisingEndTimer() = default;

UprisingEndTimer* UprisingEndTimer::Clone()
{
  return NewOrNull<UprisingEndTimer>(*this);
}

TrackedStat* UprisingEndTimer::GetTargetSpec()
{
  return timerStatOfOwner<System>(this, ResolveMasterInFamilies_90_98,
                                  [](System* s)
                                  {
                                    return s->GetStat0();
                                  });
}

void UprisingEndTimer::Fire()
{
  fireOnTimerOwner<System>(this, ResolveMasterInFamilies_90_98,
                           [](System* s, NotifyContext* ctx)
                           {
                             return s->EndUprising(ctx);
                           });
}

UprisingEndTimer* CreateUprisingEndTimer()
{
  return NewOrNull<UprisingEndTimer>();
}

UprisingLoyaltyTimer::UprisingLoyaltyTimer() : TimerEvent()
{
}

UprisingLoyaltyTimer::UprisingLoyaltyTimer(const UprisingLoyaltyTimer& other) = default;

UprisingLoyaltyTimer::~UprisingLoyaltyTimer() = default;

UprisingLoyaltyTimer* UprisingLoyaltyTimer::Clone()
{
  return NewOrNull<UprisingLoyaltyTimer>(*this);
}

TrackedStat* UprisingLoyaltyTimer::GetTargetSpec()
{
  return timerStatOfOwner<System>(this, ResolveMasterInFamilies_90_98,
                                  [](System* s)
                                  {
                                    return reinterpret_cast<Mission*>(s)->Mission::GetStatA();
                                  });
}

void UprisingLoyaltyTimer::Fire()
{
  fireOnTimerOwner<System>(this, ResolveMasterInFamilies_90_98,
                           [](System* s, NotifyContext* ctx)
                           {
                             return s->AdjustLoyaltyForUprising(ctx);
                           });
}

UprisingLoyaltyTimer* CreateUprisingLoyaltyTimer()
{
  return NewOrNull<UprisingLoyaltyTimer>();
}

AssaultLoyaltyTimer::AssaultLoyaltyTimer() : TimerEvent()
{
}

AssaultLoyaltyTimer::AssaultLoyaltyTimer(const AssaultLoyaltyTimer& other) = default;

AssaultLoyaltyTimer::~AssaultLoyaltyTimer() = default;

AssaultLoyaltyTimer* AssaultLoyaltyTimer::Clone()
{
  return NewOrNull<AssaultLoyaltyTimer>(*this);
}

TrackedStat* AssaultLoyaltyTimer::GetTargetSpec()
{
  return timerStatOfOwner<System>(this, ResolveMasterInFamilies_90_98,
                                  [](System* s)
                                  {
                                    return reinterpret_cast<Mission*>(s)->Mission::GetStatB();
                                  });
}

void AssaultLoyaltyTimer::Fire()
{
  fireOnTimerOwner<System>(this, ResolveMasterInFamilies_90_98,
                           [](System* s, NotifyContext* ctx)
                           {
                             return s->AdjustLoyaltyForAssault(ctx);
                           });
}

AssaultLoyaltyTimer* CreateAssaultLoyaltyTimer()
{
  return NewOrNull<AssaultLoyaltyTimer>();
}

BlockadeLoyaltyTimer::BlockadeLoyaltyTimer() : TimerEvent()
{
}

BlockadeLoyaltyTimer::BlockadeLoyaltyTimer(const BlockadeLoyaltyTimer& other) = default;

BlockadeLoyaltyTimer::~BlockadeLoyaltyTimer() = default;

BlockadeLoyaltyTimer* BlockadeLoyaltyTimer::Clone()
{
  return NewOrNull<BlockadeLoyaltyTimer>(*this);
}

TrackedStat* BlockadeLoyaltyTimer::GetTargetSpec()
{
  return timerStatOfOwner<System>(this, ResolveMasterInFamilies_90_98,
                                  [](System* s)
                                  {
                                    return s->GetStat3();
                                  });
}

void BlockadeLoyaltyTimer::Fire()
{
  fireOnTimerOwner<System>(this, ResolveMasterInFamilies_90_98,
                           [](System* s, NotifyContext* ctx)
                           {
                             return s->AdjustLoyaltyForBlockade(ctx);
                           });
}

BlockadeLoyaltyTimer* CreateBlockadeLoyaltyTimer()
{
  return NewOrNull<BlockadeLoyaltyTimer>();
}

UprisingPulseTimer::UprisingPulseTimer() : TimerEvent()
{
}

UprisingPulseTimer::UprisingPulseTimer(const UprisingPulseTimer& other) = default;

UprisingPulseTimer::~UprisingPulseTimer() = default;

UprisingPulseTimer* UprisingPulseTimer::Clone()
{
  return NewOrNull<UprisingPulseTimer>(*this);
}

TrackedStat* UprisingPulseTimer::GetTargetSpec()
{
  return timerStatOfOwner<System>(this, ResolveMasterInFamilies_90_98,
                                  [](System* s)
                                  {
                                    return s->GetStat4();
                                  });
}

void UprisingPulseTimer::Fire()
{
  fireOnTimerOwner<System>(this, ResolveMasterInFamilies_90_98,
                           [](System* s, NotifyContext* ctx)
                           {
                             return s->PulseUprisingIncident(ctx);
                           });
}

UprisingPulseTimer* CreateUprisingPulseTimer()
{
  return NewOrNull<UprisingPulseTimer>();
}

InformantIncidentTimer::InformantIncidentTimer() : TimerEvent()
{
}

InformantIncidentTimer::InformantIncidentTimer(const InformantIncidentTimer& other) = default;

InformantIncidentTimer::~InformantIncidentTimer() = default;

InformantIncidentTimer* InformantIncidentTimer::Clone()
{
  return NewOrNull<InformantIncidentTimer>(*this);
}

TrackedStat* InformantIncidentTimer::GetTargetSpec()
{
  return timerStatOfOwner<System>(this, ResolveMasterInFamilies_90_98,
                                  [](System* s)
                                  {
                                    return s->GetStat5();
                                  });
}

void InformantIncidentTimer::Fire()
{
  fireOnTimerOwner<System>(this, ResolveMasterInFamilies_90_98,
                           [](System* s, NotifyContext* ctx)
                           {
                             return s->RollInformantIncident(ctx);
                           });
}

InformantIncidentTimer* CreateInformantIncidentTimer()
{
  return NewOrNull<InformantIncidentTimer>();
}

SideStatATimer::SideStatATimer() : TimerEvent()
{
}

SideStatATimer::SideStatATimer(const SideStatATimer& other) = default;

SideStatATimer::~SideStatATimer() = default;

SideStatATimer* SideStatATimer::Clone()
{
  return NewOrNull<SideStatATimer>(*this);
}

TrackedStat* SideStatATimer::GetTargetSpec()
{
  return timerStatOfOwner<GameObj>(this, ResolveMasterInFamilies_f9_fa,
                                   [](GameObj* o)
                                   {
                                     return &static_cast<RulesMgr*>(o)->m_timerA;
                                   });
}

void SideStatATimer::Fire()
{
  fireOnTimerOwner<GameObj>(this, ResolveMasterInFamilies_f9_fa,
                            [](GameObj* o, NotifyContext* ctx)
                            {
                              return static_cast<RulesMgr*>(o)->RulesMgr::FireDisasterTimer(ctx);
                            });
}

SideStatATimer* CreateSideStatATimer()
{
  return NewOrNull<SideStatATimer>();
}

SideStatBTimer::SideStatBTimer() : TimerEvent()
{
}

SideStatBTimer::SideStatBTimer(const SideStatBTimer& other) = default;

SideStatBTimer::~SideStatBTimer() = default;

SideStatBTimer* SideStatBTimer::Clone()
{
  return NewOrNull<SideStatBTimer>(*this);
}

TrackedStat* SideStatBTimer::GetTargetSpec()
{
  return timerStatOfOwner<GameObj>(this, ResolveMasterInFamilies_f9_fa,
                                   [](GameObj* o)
                                   {
                                     return &static_cast<RulesMgr*>(o)->m_timerB;
                                   });
}

void SideStatBTimer::Fire()
{
  fireOnTimerOwner<GameObj>(this, ResolveMasterInFamilies_f9_fa,
                            [](GameObj* o, NotifyContext* ctx)
                            {
                              return static_cast<RulesMgr*>(o)->RulesMgr::FireResourceTimer(ctx);
                            });
}

SideStatBTimer* CreateSideStatBTimer()
{
  return NewOrNull<SideStatBTimer>();
}
