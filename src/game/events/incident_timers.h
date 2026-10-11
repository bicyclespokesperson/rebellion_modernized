#pragma once
// Recurring timers 0x383-0x390 that drive system incidents (uprisings, informants, loyalty shifts) and the two side stat timers.
#include <cstdint>

#include "events/timer_event.h"

// Slot 10 of the timers: the family range the target lies in (shared by the timers of the same family).
FamilyRange* GetSystemFamilyRange(FamilyRange* out);  // 0x566320: {0x90, 0x98}
FamilyRange* GetFamilyF9Range(FamilyRange* out);      // 0x566670: {0xf9, 0xfa}

// timer 0x383 on a system: ends its uprising (System::EndUprising) when stat 0 is unchanged
class UprisingEndTimer : public TimerEvent {
public:
    UprisingEndTimer();  // 0x5651d0
    UprisingEndTimer(const UprisingEndTimer& other);  // 0x565220
    virtual ~UprisingEndTimer();  // 0x565210 (deleting 0x5651f0)

    virtual UprisingEndTimer* Clone() override;  // 1: 0x565240
    virtual int GetKind() override { return 0x383; }  // 9: 0x5652b0
    virtual FamilyRange* GetTargetRange(FamilyRange* out) override { return GetSystemFamilyRange(out); }  // 10: 0x566320
    virtual TrackedStat* GetTargetSpec() override;  // 11: 0x565330
    virtual void Fire() override;  // 12: 0x565400
};

// timer 0x384 on a system: shifts loyalty for an uprising
class UprisingLoyaltyTimer : public TimerEvent {
public:
    UprisingLoyaltyTimer();  // 0x565500
    UprisingLoyaltyTimer(const UprisingLoyaltyTimer& other);  // 0x565550
    virtual ~UprisingLoyaltyTimer();  // 0x565540 (deleting 0x565520)

    virtual UprisingLoyaltyTimer* Clone() override;  // 1: 0x565570
    virtual int GetKind() override { return 0x384; }  // 9: 0x5655e0
    virtual FamilyRange* GetTargetRange(FamilyRange* out) override { return GetSystemFamilyRange(out); }  // 10: 0x566320
    virtual TrackedStat* GetTargetSpec() override;  // 11: 0x565660
    virtual void Fire() override;  // 12: 0x565730
};

// timer 0x385 on a system: shifts loyalty for an assault
class AssaultLoyaltyTimer : public TimerEvent {
public:
    AssaultLoyaltyTimer();  // 0x565830
    AssaultLoyaltyTimer(const AssaultLoyaltyTimer& other);  // 0x565880
    virtual ~AssaultLoyaltyTimer();  // 0x565870 (deleting 0x565850)

    virtual AssaultLoyaltyTimer* Clone() override;  // 1: 0x5658a0
    virtual int GetKind() override { return 0x385; }  // 9: 0x565910
    virtual FamilyRange* GetTargetRange(FamilyRange* out) override { return GetSystemFamilyRange(out); }  // 10: 0x566320
    virtual TrackedStat* GetTargetSpec() override;  // 11: 0x565990
    virtual void Fire() override;  // 12: 0x565a60
};

// timer 0x386 on a system: shifts loyalty for a blockade
class BlockadeLoyaltyTimer : public TimerEvent {
public:
    BlockadeLoyaltyTimer();  // 0x565b60
    BlockadeLoyaltyTimer(const BlockadeLoyaltyTimer& other);  // 0x565bb0
    virtual ~BlockadeLoyaltyTimer();  // 0x565ba0 (deleting 0x565b80)

    virtual BlockadeLoyaltyTimer* Clone() override;  // 1: 0x565bd0
    virtual int GetKind() override { return 0x386; }  // 9: 0x565c40
    virtual FamilyRange* GetTargetRange(FamilyRange* out) override { return GetSystemFamilyRange(out); }  // 10: 0x566320
    virtual TrackedStat* GetTargetSpec() override;  // 11: 0x565cc0
    virtual void Fire() override;  // 12: 0x565d90
};

// timer 0x38d on a system: uprising incident pulse
class UprisingPulseTimer : public TimerEvent {
public:
    UprisingPulseTimer();  // 0x565e90
    UprisingPulseTimer(const UprisingPulseTimer& other);  // 0x565ee0
    virtual ~UprisingPulseTimer();  // 0x565ed0 (deleting 0x565eb0)

    virtual UprisingPulseTimer* Clone() override;  // 1: 0x565f00
    virtual int GetKind() override { return 0x38d; }  // 9: 0x565f70
    virtual FamilyRange* GetTargetRange(FamilyRange* out) override { return GetSystemFamilyRange(out); }  // 10: 0x566320
    virtual TrackedStat* GetTargetSpec() override;  // 11: 0x565ff0
    virtual void Fire() override;  // 12: 0x5660c0
};

// timer 0x38e on a system: informant incident roll
class InformantIncidentTimer : public TimerEvent {
public:
    InformantIncidentTimer();  // 0x5661c0
    InformantIncidentTimer(const InformantIncidentTimer& other);  // 0x566210
    virtual ~InformantIncidentTimer();  // 0x566200 (deleting 0x5661e0)

    virtual InformantIncidentTimer* Clone() override;  // 1: 0x566230
    virtual int GetKind() override { return 0x38e; }  // 9: 0x5662a0
    virtual FamilyRange* GetTargetRange(FamilyRange* out) override { return GetSystemFamilyRange(out); }  // 10: 0x566320
    virtual TrackedStat* GetTargetSpec() override;  // 11: 0x566340
    virtual void Fire() override;  // 12: 0x566410
};

// timer 0x38f on the family 0xf9 object: stat at +0x58, fires 0x556b50
class SideStatATimer : public TimerEvent {
public:
    SideStatATimer();  // 0x566510
    SideStatATimer(const SideStatATimer& other);  // 0x566560
    virtual ~SideStatATimer();  // 0x566550 (deleting 0x566530)

    virtual SideStatATimer* Clone() override;  // 1: 0x566580
    virtual int GetKind() override { return 0x38f; }  // 9: 0x5665f0
    virtual FamilyRange* GetTargetRange(FamilyRange* out) override { return GetFamilyF9Range(out); }  // 10: 0x566670
    virtual TrackedStat* GetTargetSpec() override;  // 11: 0x566690
    virtual void Fire() override;  // 12: 0x566760
};

// timer 0x390 on the family 0xf9 object: stat at +0x60, fires 0x556be0
class SideStatBTimer : public TimerEvent {
public:
    SideStatBTimer();  // 0x566860
    SideStatBTimer(const SideStatBTimer& other);  // 0x5668b0
    virtual ~SideStatBTimer();  // 0x5668a0 (deleting 0x566880)

    virtual SideStatBTimer* Clone() override;  // 1: 0x5668d0
    virtual int GetKind() override { return 0x390; }  // 9: 0x566940
    virtual FamilyRange* GetTargetRange(FamilyRange* out) override { return GetFamilyF9Range(out); }  // 10: 0x566670
    virtual TrackedStat* GetTargetSpec() override;  // 11: 0x5669c0
    virtual void Fire() override;  // 12: 0x566a90
};

// Factories registered in the event factory table.
UprisingEndTimer* CreateUprisingEndTimer();  // 0x5652c0
UprisingLoyaltyTimer* CreateUprisingLoyaltyTimer();  // 0x5655f0
AssaultLoyaltyTimer* CreateAssaultLoyaltyTimer();  // 0x565920
BlockadeLoyaltyTimer* CreateBlockadeLoyaltyTimer();  // 0x565c50
UprisingPulseTimer* CreateUprisingPulseTimer();  // 0x565f80
InformantIncidentTimer* CreateInformantIncidentTimer();  // 0x5662b0
SideStatATimer* CreateSideStatATimer();  // 0x566600
SideStatBTimer* CreateSideStatBTimer();  // 0x566950
