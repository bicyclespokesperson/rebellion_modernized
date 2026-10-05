#pragma once
// Recurring timers on sides (0x381, 0x382), process facilities (0x394), objects with hyperdrive (0x387), the 0x14-0x1f families (0x380) and characters (0x388-0x38a).
#include <cstdint>

#include "events/timer_event.h"

// Slot 10 of the timers: the family range of the objects they act on.
FamilyRange* __stdcall GetSideFamilyRange(FamilyRange* out);  // 0x5782c0: {0xf3, 0xf4}
FamilyRange* __stdcall GetProcFacilFamilyRange(FamilyRange* out);  // 0x578610: {0x28, 0x30}
FamilyRange* __stdcall GetAnyFamilyRange(FamilyRange* out);  // 0x57be90: {0x1, 0xff}
FamilyRange* __stdcall GetFamily14Range(FamilyRange* out);  // 0x57c1d0: {0x14, 0x20}
FamilyRange* __stdcall GetCharacterFamilyRange(FamilyRange* out);  // 0x57cf00: {0x30, 0x3c}

// timer 0x382 on a side: stat at +0xcc; fires Side::ScrapRandomUnit
class SideScrapTimer : public TimerEvent {
public:
    SideScrapTimer();  // 0x577e30
    SideScrapTimer(const SideScrapTimer& other);  // 0x577e80
    virtual ~SideScrapTimer();  // 0x577e70 (deleting 0x577e50)

    virtual SideScrapTimer* Clone() override;  // 1: 0x577ea0
    virtual int GetKind() override { return 0x382; }  // 9: 0x577f10
    virtual FamilyRange* GetTargetRange(FamilyRange* out) override { return GetSideFamilyRange(out); }  // 10: 0x5782c0
    virtual TrackedStat* GetTargetSpec() override;  // 11: 0x577f90
    virtual void Fire() override;  // 12: 0x578060
};

// timer 0x381 on a side: stat at +0xd4; fires Side::UpdateRdProgress
class SideResearchTimer : public TimerEvent {
public:
    SideResearchTimer();  // 0x578160
    SideResearchTimer(const SideResearchTimer& other);  // 0x5781b0
    virtual ~SideResearchTimer();  // 0x5781a0 (deleting 0x578180)

    virtual SideResearchTimer* Clone() override;  // 1: 0x5781d0
    virtual int GetKind() override { return 0x381; }  // 9: 0x578240
    virtual FamilyRange* GetTargetRange(FamilyRange* out) override { return GetSideFamilyRange(out); }  // 10: 0x5782c0
    virtual TrackedStat* GetTargetSpec() override;  // 11: 0x5782e0
    virtual void Fire() override;  // 12: 0x5783b0
};

// timer 0x394 on a process facility (families 0x28-0x2f): fires ProcFacil::MarkPointProcessedIfProcessing
class ProcFacilTimer : public TimerEvent {
public:
    ProcFacilTimer();  // 0x5784b0
    ProcFacilTimer(const ProcFacilTimer& other);  // 0x578500
    virtual ~ProcFacilTimer();  // 0x5784f0 (deleting 0x5784d0)

    virtual ProcFacilTimer* Clone() override;  // 1: 0x578520
    virtual int GetKind() override { return 0x394; }  // 9: 0x578590
    virtual FamilyRange* GetTargetRange(FamilyRange* out) override { return GetProcFacilFamilyRange(out); }  // 10: 0x578610
    virtual TrackedStat* GetTargetSpec() override;  // 11: 0x578630
    virtual void Fire() override;  // 12: 0x578700
};

// timer 0x387 on any object: stat is the data block extra; fires GameObj::UpdateHyperdriveAndEta
class HyperdriveTimer : public TimerEvent {
public:
    HyperdriveTimer();  // 0x57bd30
    HyperdriveTimer(const HyperdriveTimer& other);  // 0x57bd80
    virtual ~HyperdriveTimer();  // 0x57bd70 (deleting 0x57bd50)

    virtual HyperdriveTimer* Clone() override;  // 1: 0x57bda0
    virtual int GetKind() override { return 0x387; }  // 9: 0x57be10
    virtual FamilyRange* GetTargetRange(FamilyRange* out) override { return GetAnyFamilyRange(out); }  // 10: 0x57be90
    virtual TrackedStat* GetTargetSpec() override;  // 11: 0x57beb0
    virtual void Fire() override;  // 12: 0x57bf80
};

// timer 0x380 on the objects of families 0x14-0x1f: fires their virtual in slot 138
class FleetSlot138Timer : public TimerEvent {
public:
    FleetSlot138Timer();  // 0x57c080
    FleetSlot138Timer(const FleetSlot138Timer& other);  // 0x57c0d0
    virtual ~FleetSlot138Timer();  // 0x57c0c0 (deleting 0x57c0a0)

    virtual FleetSlot138Timer* Clone() override;  // 1: 0x57c0f0
    virtual int GetKind() override { return 0x380; }  // 9: 0x43c970
    virtual FamilyRange* GetTargetRange(FamilyRange* out) override { return GetFamily14Range(out); }  // 10: 0x57c1d0
    virtual TrackedStat* GetTargetSpec() override;  // 11: 0x57c1f0
    virtual void Fire() override;  // 12: 0x57c2c0
};

// timer 0x388 on a character: fires Character::RecoverFromInjury
class CharacterInjuryTimer : public TimerEvent {
public:
    CharacterInjuryTimer();  // 0x57ca70
    CharacterInjuryTimer(const CharacterInjuryTimer& other);  // 0x57cac0
    virtual ~CharacterInjuryTimer();  // 0x57cab0 (deleting 0x57ca90)

    virtual CharacterInjuryTimer* Clone() override;  // 1: 0x57cae0
    virtual int GetKind() override { return 0x388; }  // 9: 0x57cb50
    virtual FamilyRange* GetTargetRange(FamilyRange* out) override { return GetCharacterFamilyRange(out); }  // 10: 0x57cf00
    virtual TrackedStat* GetTargetSpec() override;  // 11: 0x57cbd0
    virtual void Fire() override;  // 12: 0x57cca0
};

// timer 0x389 on a character: fires 0x560a30
class CharacterTimer389 : public TimerEvent {
public:
    CharacterTimer389();  // 0x57cda0
    CharacterTimer389(const CharacterTimer389& other);  // 0x57cdf0
    virtual ~CharacterTimer389();  // 0x57cde0 (deleting 0x57cdc0)

    virtual CharacterTimer389* Clone() override;  // 1: 0x57ce10
    virtual int GetKind() override { return 0x389; }  // 9: 0x57ce80
    virtual FamilyRange* GetTargetRange(FamilyRange* out) override { return GetCharacterFamilyRange(out); }  // 10: 0x57cf00
    virtual TrackedStat* GetTargetSpec() override;  // 11: 0x57cf20
    virtual void Fire() override;  // 12: 0x57cff0
};

// timer 0x38a on a character: fires Character::PulseEscapeAttempt
class CharacterEscapeTimer : public TimerEvent {
public:
    CharacterEscapeTimer();  // 0x57d100
    CharacterEscapeTimer(const CharacterEscapeTimer& other);  // 0x57d150
    virtual ~CharacterEscapeTimer();  // 0x57d140 (deleting 0x57d120)

    virtual CharacterEscapeTimer* Clone() override;  // 1: 0x57d170
    virtual int GetKind() override { return 0x38a; }  // 9: 0x57d1e0
    virtual FamilyRange* GetTargetRange(FamilyRange* out) override { return GetCharacterFamilyRange(out); }  // 10: 0x57cf00
    virtual TrackedStat* GetTargetSpec() override;  // 11: 0x57d260
    virtual void Fire() override;  // 12: 0x57d330
};

// Factories registered in the event factory table.
SideScrapTimer* CreateSideScrapTimer();  // 0x577f20
SideResearchTimer* CreateSideResearchTimer();  // 0x578250
ProcFacilTimer* CreateProcFacilTimer();  // 0x5785a0
HyperdriveTimer* CreateHyperdriveTimer();  // 0x57be20
FleetSlot138Timer* CreateFleetSlot138Timer();  // 0x57c160
CharacterInjuryTimer* CreateCharacterInjuryTimer();  // 0x57cb60
CharacterTimer389* CreateCharacterTimer389();  // 0x57ce90
CharacterEscapeTimer* CreateCharacterEscapeTimer();  // 0x57d1f0

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(SideScrapTimer, 0x4c);
REB_ASSERT_SIZE(SideResearchTimer, 0x4c);
REB_ASSERT_SIZE(ProcFacilTimer, 0x4c);
REB_ASSERT_SIZE(HyperdriveTimer, 0x4c);
REB_ASSERT_SIZE(FleetSlot138Timer, 0x4c);
REB_ASSERT_SIZE(CharacterInjuryTimer, 0x4c);
REB_ASSERT_SIZE(CharacterTimer389, 0x4c);
REB_ASSERT_SIZE(CharacterEscapeTimer, 0x4c);
#pragma clang diagnostic pop
