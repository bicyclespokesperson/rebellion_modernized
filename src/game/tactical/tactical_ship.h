#pragma once
#include <cstdint>

#include "core/game_string.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "tactical/tactical_lists.h"
#include "tactical/tactical_math.h"
#include "tactical/tactical_moving_object.h"
#include "tactical/tactical_types.h"
#include "tactical/tactical_units.h"
#include "tactical/tactical_weapon_profile.h"

// The battle units of the tactical engine: capital ships and fighter squadrons share the abstract base class
// TacticalShipBase (0x5b0f70, VT 0x66c560, 0x418 bytes); a capital ship adds the carrier interface TacticalHangar
// (0x5b82a0, 0x30 bytes) behind it. The same objects run in the 3D battle and in the headless simulator; only the
// per-frame update and the presentation hooks are specific to the 3D battle.
//
// Layout: MovingObject (+0x00, frame), TacticalUnit (+0xd4, id node keyed by the unit's handle: the registry
// hands out pointers to this subobject and the unit lists store it), four small interface bases, then the unit's state.

class TacticalShipBase;

// The four small interface bases that follow TacticalUnit in a battle unit. Their destructors (scalar deleting) are
// 0x5a04d0 / 0x5a06a0 / 0x5a07d0 / 0x5a09a0; the third one also has read / write slots (0x5a0750 / 0x5a0770).
class TacticalShipFacetA {
public:
    TacticalShipFacetA() : m_fA_04(0), m_fA_08(0) {}
    virtual ~TacticalShipFacetA() {  }
    int m_fA_04;  // +0x04
    int m_fA_08;  // +0x08
};

class TacticalShipFacetB {
public:
    TacticalShipFacetB() : m_fB_04(0) {}
    virtual ~TacticalShipFacetB() {  }
    int m_fB_04;  // +0x04
};

class TacticalShipFacetC {
public:
    TacticalShipFacetC() : m_fC_04(0) {}
    virtual ~TacticalShipFacetC() {  }
    virtual void Write(ostream* out);  // 0x5a0750
    virtual void Read(istream* in);    // 0x5a0770
    int m_fC_04;                       // +0x04
};

class TacticalShipFacetD {
public:
    TacticalShipFacetD() : m_fD_04(0), m_fD_08(0) {}
    virtual ~TacticalShipFacetD() {  }
    int m_fD_04;  // +0x04
    int m_fD_08;  // +0x08
};

// The ship state the 3D battle synchronises between machines and the group's view of it (0x68 bytes, 0x5b1c00).
struct TacticalShipStatusRecord {
    int m_state;               // +0x00
    int m_id;                  // +0x04
    int m_groupHandle;         // +0x08
    int m_unk178;              // +0x0c
    uint32_t m_speedCH;        // +0x10
    uint32_t m_shieldCH;       // +0x14
    uint32_t m_hyperdriveCH;   // +0x18
    uint32_t m_tractorCH;      // +0x1c
    uint32_t m_weaponCH;       // +0x20
    float m_tractorPower;      // +0x24
    float m_shieldMax;         // +0x28
    float m_shield;            // +0x2c
    float m_hullMax;           // +0x30
    float m_hull;              // +0x34
    float m_weaponRecharge;    // +0x38
    float m_shieldRecharge;    // +0x3c
    float m_hyperdriveCount;   // +0x40
    float m_speed;             // +0x44
    float m_commanderBonus;    // +0x48
    int m_groupField5c;        // +0x4c
    int m_hubValue;            // +0x50
    int m_groupMode;           // +0x54
    int m_groupField68;        // +0x58
    int m_firstMember;         // +0x5c
    int m_secondGroupItem;     // +0x60
    int m_groupField38;        // +0x64
};

// One of the four weapon arcs of a ship (0x74 bytes). The three weapon kinds each have a profile object (damage and
// accuracy factors); `ComputeFirepower` of the owning ship fills m_firepower for a given target kind.
class TacticalWeaponArc {
public:
    TacticalWeaponArc();   // 0x5b0e50
    ~TacticalWeaponArc();  // 0x5b0ee0
    void Read(istream* in);     // 0x5b0c90
    void Write(ostream* out);   // 0x5b0d40
    void Checksum(Crc32* crc);  // 0x5b0dd0

    int m_countA;                   // +0x00 number of weapons of the first kind (grid x2... see the constructor)
    int m_countB;                   // +0x04
    int m_countC;                   // +0x08
    float m_damageA;                // +0x0c
    float m_damageB;                // +0x10
    float m_damageC;                // +0x14
    WeaponProfileA m_profileA;      // +0x18
    WeaponProfileB m_profileB;      // +0x2c
    WeaponProfileC m_profileC;      // +0x40
    float m_unk54;                  // +0x54
    float m_unk58;                  // +0x58
    float m_unk5c;                  // +0x5c
    int m_unk60;                    // +0x60 cleared with the charge
    int m_chargeState;              // +0x64 1 when the arc has finished charging
    float m_charge;                 // +0x68 charge so far
    float m_chargeMax;              // +0x6c charge needed
    float m_firepower;              // +0x70 total of the three damage figures
};

// Carrier interface of a capital ship (VT 0x66c698, 0x30 bytes): the fighters it carries and launches.
class TacticalHangar {
public:
    TacticalHangar();             // 0x5b82a0
    virtual ~TacticalHangar();    // 0x5b83b0 (scalar deleting destructor 0x5b8390)
    virtual void GetLaunchPoint(TacticalVector* out) = 0;  // slot 1: implemented by the capital ship (0x5b0b80)

    int m_ownerHandle;            // +0x04 the handle of the ship that carries this hangar
    uint32_t m_inFlight;          // +0x08 squadrons launched or in the air
    uint32_t m_aboard;            // +0x0c squadrons aboard (hangar slots in use)
    uint32_t m_capacity;          // +0x10
    float m_launchTime;           // +0x14 time of the next launch, 0 when none is scheduled
    float m_launchInterval;       // +0x18
    UnitRefTree* m_docked;   // +0x1c
    UnitRefTree* m_launched; // +0x20
    int m_unk24;                  // +0x24
    UnitRefTree* m_unk28;    // +0x28
    int m_unk2c;                  // +0x2c

    void Write(ostream* out);                   // 0x5b8990
    void Read(istream* in);                     // 0x5b8a10
    void Checksum(Crc32* crc);                  // 0x5b8a90
    int TakeOut(TacticalShipBase* fighter);     // 0x5b8450
    int Dock(TacticalShipBase* fighter);        // 0x5b84a0
    int Launch(TacticalShipBase* fighter);      // 0x5b8570
    double GetLaunchInterval();                 // 0x5b8b00
    void ReleaseSlot(TacticalShipBase* fighter);  // 0x5b8400
    int AllLaunched();                          // 0x5b8480
    int ReserveSlot(TacticalShipBase* fighter); // 0x5b84e0
    void OnFighterLaunched(TacticalShipBase* fighter);    // 0x5b8630
    void UpdateLaunches();                      // 0x5b8800 schedules the next launch from the hangar
    void LaunchNow(TacticalShipBase* fighter);  // 0x5b8c70 puts the squadron into space at the launch point
    void ReleaseDocked();                       // 0x5b8d10 sends every docked squadron into battle
    void Disband();                             // 0x5b8d80 destroys the squadrons the carrier has aboard or in flight
};

class TacticalShipBase : public MovingObject,
                         public TacticalUnit,
                         public TacticalShipFacetA,
                         public TacticalShipFacetB,
                         public TacticalShipFacetC,
                         public TacticalShipFacetD {
public:
    TacticalShipBase();                       // 0x5b0f70
    virtual ~TacticalShipBase();              // 0x5b12d0 (scalar deleting destructor 0x5b12b0)

    // ---- MovingObject slots (1-2) -----------------------------------------------------------------------
    virtual double Turn(float step, float rate) override;   // 0x5b1410
    virtual void Advance(float milliseconds) override;     // 0x5b2f30 (3D battle: the per-frame update)

    // ---- TacticalUnit slot 1 ----------------------------------------------------------------------------------
    virtual int Virtual01() override;                      // 0x5a66f0: 1

    virtual int Destroy();                                 // 7: 0x5b2d20
    virtual int GetUnitClass() = 0;                    // 8: 0 for a capital ship, 1 for a fighter squadron
    virtual uint32_t GetTypeIndex() = 0;                  // 9: 0x5a6760 in both subclasses
    virtual void ReadState(istream* in);                   // 10: 0x5b6eb0
    virtual void ReadExtra(istream* in);                   // 11: 0x5b7350 (carrier lists)
    virtual void WriteState(ostream* out);                 // 12: 0x5b6a00
    virtual void WriteExtra(ostream* out);                 // 13: 0x5b6e90
    virtual void ChecksumState(Crc32* crc);                // 14: 0x5b7370
    virtual void ChecksumExtra(Crc32* crc);                // 15: 0x5b7750
    virtual void StartTractoring(int target);              // 16: 0x5b23e0
    virtual void AddTractorSource(int source);             // 17: 0x5b24d0
    virtual void StopTractoring(int target);               // 18: 0x5b2440
    virtual void RemoveTractorSource(int source);          // 19: 0x5b2480
    virtual double GetValue394();                           // 20: 0x5a66b0
    virtual double GetValue398();                           // 21: 0x5a66c0
    virtual double GetValue3a0();                           // 22: 0x5a66d0
    virtual double GetTractorRange();                       // 23: 0x5a66e0
    virtual double GetWeaponRecharge() = 0;                // 24
    virtual double GetShieldRecharge() = 0;                // 25
    virtual void Virtual26() = 0;                          // 26: capital 0x5b0ad0 / fighter 0x5ba2f0: leaves the carrier
    virtual int GetSoundId() = 0;
    virtual int Virtual28() = 0;
    virtual int Virtual29() = 0;
    virtual int Virtual30() = 0;
    virtual void Virtual31(TacticalVector* out);           // 0x5b4960
    virtual int Virtual32(float a);                        // 32: 0x5b4d40 / 0x5b9a20
    virtual uint32_t SelectArc(int targetClass);           // 33: 0x5b4c60
    virtual void Virtual34();                            // 0x5b3a40
    virtual double GetRadius();                             // 35: 0x5b6900
    virtual double GetSpeed();                              // 36: capital 0x5b17f0, fighter 0x5ba190
    virtual TacticalShipBase* GetNextShip();               // 37: 0x5b6960 the next ship in the registry order
    virtual TacticalShipBase* GetNextShip2();              // 38: 0x5b6960 (the same code as slot 37)
    virtual void Virtual39(float step) = 0;
    virtual void ApplyCriticalHit(int kind);               // 40: 0x5b1970
    virtual void RepairCriticalHit(int kind);              // 41: 0x5b1ab0
    virtual void Virtual42(int a);                         // 0x5b6530
    virtual double Virtual43();                            // 0x5b7770 (the capital ship has its own copy)
    virtual int Virtual44() = 0;
    virtual int Virtual45() = 0;
    virtual int Virtual46() = 0;
    virtual double GetTurnRate();                       // 47: 0x5b0be0 (the fighter overrides it: 0x5ba270)
    virtual void Virtual48(float a);                       // 0x5b5700
    virtual void Virtual49() = 0;
    virtual void Virtual50() = 0;
    virtual void Virtual51() = 0;
    virtual void Virtual52() = 0;
    virtual void Virtual53(float damage, int attacker, int target);                       // 0x5b5750
    virtual void Virtual54(float damage, int attacker, int target);                       // 0x5b5100
    virtual void Virtual55(float damage, int attacker, int target);                       // 0x5b5b60
    virtual void Virtual56(float damage, int attacker, int target);                       // 0x5b5f50

    // ---- not virtual -------------------------------------------------------------------------------------------
    int HasTractoringShips();                  // 0x5b23c0
    void AddTractoringShip(TacticalShipBase* tractor);   // 0x5b2570
    void RemoveTractoringShip(int handle);     // 0x5b25d0
    void BeginTractoring();                    // 0x5b2520
    void EndTractoring();                      // 0x5b2550
    void AddTractorJam(float amount);          // 0x5b2e20
    void AddTractorDelay(float amount);        // 0x5b2e40
    void ConsiderTractoring(int target);       // 0x5b2140
    float DistanceTo(TacticalShipBase* other);
    void RepairOneHit();                       // 0x5b1490
    int HubQuery();                            // 0x5b1480
    String* GetName(String* out);              // 0x5b1610
    void BuildStatusRecord(TacticalShipStatusRecord* out);   // 0x5b1c00
    void GetGroupPosition(TacticalVector* out);              // 0x5b5ad0
    double ScaleByHull(float value);            // 0x5b1770: value * hull / hullMax
    double ScaleByHullFull();                   // 0x5b1640
    double ShieldFraction();                    // 0x5b1650
    double WeaponRechargeFraction();            // 0x5b1680
    double ShieldRechargeFraction();            // 0x5b1710
    double SpeedFraction();                     // 0x5b16b0
    double HyperdriveFraction();                // 0x5b16e0
    double TractorFraction();                   // 0x5b1740
    double TractorPower();                      // 0x5b1790
    double MaxWeaponRange();                   // 0x5b18d0
    int UpdateHyperdriveReady();               // 0x5b1b70
    double WorkingHyperdrives();                // 0x5b1bc0
    double TorpedoStrength();                   // 0x5b23a0
    void TakeDamage(float damage);             // 0x5b54d0
    void Recharge(float seconds);              // 0x5b6320
    void ClearArcCharge(unsigned arc);         // 0x5b6980
    void QueueArcCharge(unsigned arc);         // 0x5b64e0
    void SetState1();                          // 0x5b69d0
    void SetState2();                          // 0x5b69b0
    int IsState1();                            // 0x5b69f0
    double AttackRating(TacticalShipBase* target, int unused);   // 0x5b7780
    double AttackRatingVsClass(int targetClass, int unused);     // 0x5b7a80
    int HasKind1Weapons();                     // 0x5b7ce0
    int HasOnlyKind1Weapons();                 // 0x5b7d10
    int IsUnarmed();                           // 0x5b7d60
    int IsCrippled();                          // 0x5b7d90

    // ---- state ----------------------------------------------------------------------------------------------
    TurnSyncChannelA m_channelA;     // +0x118
    TurnSyncChannelB m_channelB;     // +0x120 (m_data: non-zero for a remote copy)
    uint32_t m_objectFamily;         // +0x128 strategic object family
    uint32_t m_objectId;             // +0x12c strategic object id
    uint32_t m_chargingCount;        // +0x130 arcs waiting in m_chargeQueue
    int m_totalTorpedoes;            // +0x134
    float m_torpedoRange;            // +0x138
    uint32_t m_typeIndex;            // +0x13c
    int m_unitKind;                  // +0x140 0 capital ship, 1 fighter
    String m_name;                   // +0x144
    int m_hasGravityWell;            // +0x150
    int m_unk154;                    // +0x154
    uint32_t m_hitCount;             // +0x158 critical hit slots taken
    int m_unk15c;                    // +0x15c
    float m_unk160;                  // +0x160
    int m_carrierHandle;             // +0x164
    int m_side;                      // +0x168
    int m_hyperdriveReady;           // +0x16c
    int m_state;                     // +0x170
    int m_groupHandle;               // +0x174
    int m_unk178;                    // +0x178
    TacticalWeaponArc m_arcs[4];     // +0x17c
    int m_chargeQueue[4];            // +0x34c
    int m_unk35c;                    // +0x35c
    UnitRefTree m_unk360;       // +0x360
    uint32_t m_speedCH;              // +0x36c critical hits taken per system
    uint32_t m_shieldCH;             // +0x370
    uint32_t m_hyperdriveCH;         // +0x374
    uint32_t m_tractorCH;            // +0x378
    uint32_t m_weaponCH;             // +0x37c
    int m_tractorTarget;             // +0x380
    int m_tractoredBy[4];            // +0x384
    float m_value394;                // +0x394
    float m_value398;                // +0x398
    float m_tractorRange;            // +0x39c
    float m_value3a0;                // +0x3a0
    int m_unk3a4;                    // +0x3a4
    int m_unk3a8;                    // +0x3a8
    int m_unk3ac;                    // +0x3ac
    float m_unk3b0;                  // +0x3b0
    float m_unk3b4;                  // +0x3b4
    float m_tractorPower;            // +0x3b8
    float m_shieldMax;               // +0x3bc
    float m_shield;                  // +0x3c0
    int m_unk3c4;                    // +0x3c4
    float m_hullMax;                 // +0x3c8
    float m_hull;                    // +0x3cc
    int m_unk3d0;                    // +0x3d0
    float m_firepowerTotal;          // +0x3d4
    float m_unk3d8;                  // +0x3d8
    float m_unk3dc;                  // +0x3dc
    float m_unk3e0;                  // +0x3e0
    float m_weaponRecharge;          // +0x3e4
    float m_shieldRecharge;          // +0x3e8
    float m_hyperdriveCount;         // +0x3ec
    float m_unk3f0;                  // +0x3f0
    float m_unk3f4;                  // +0x3f4
    float m_speed;                   // +0x3f8
    float m_commanderBonus;          // +0x3fc
    int m_unk400;                    // +0x400
    int m_unk404;                    // +0x404
    int m_unk408;                    // +0x408
    int m_unk40c;                    // +0x40c
    int m_unk410;                    // +0x410
    int m_unk414;                    // +0x414
};

class TacticalCapitalShip : public TacticalShipBase, public TacticalHangar {
public:
    TacticalCapitalShip(uint32_t family, uint32_t id, int notAlliance, uint32_t typeIndex, String name, float hull,
                        float currentHull, float damageControl, int freeHangar, float tractorPower, float tractorRange,
                        int gravityWell, float interdiction, float field64, float field68, float field60,
                        float hyperdriveCount, float weaponRecharge, float shieldRecharge, int grid02, int grid12,
                        int grid22, int grid32, int grid01, int grid11, int grid21, int grid31, int grid00,
                        int grid10, int grid20, int grid30, int speedCH, int shieldCH, int weaponCH, int hyperdriveCH,
                        int tractorCH, float a42, float a41, float a40, int hundred);  // 0x5b05c0
    virtual ~TacticalCapitalShip();   // 0x5b0520 (scalar deleting destructor 0x5a6770)

    virtual int GetUnitClass() override { return 0; }              // 0x6158b0 (shared stub)
    virtual uint32_t GetTypeIndex() override;          // 0x5a6760 (shared stub; the fighter has an inline copy)
    virtual int Virtual44() override { return 1; }     // 0x40f340 (shared stub)
    virtual int Virtual45() override { return 0; }     // 0x6158b0 (shared stub)
    virtual int Virtual46() override { return 2; }     // 0x43c1f0 (shared stub)
    virtual void Virtual49() override;                 // 0x5b00a0 (sound)
    virtual void Virtual50() override;                 // 0x5b00c0 (sound)
    virtual void Virtual51() override;                 // 0x5b00e0 (sound)
    virtual void Virtual52() override;                 // 0x5b0170 (sound)
    virtual void Virtual26() override;                 // 0x5b0ad0
    virtual int Virtual28() override;                  // 0x5b04c0
    virtual int Virtual29() override;                  // 0x5b04e0
    virtual int Virtual30() override;                  // 0x5b0500
    virtual void Virtual39(float step) override;       // 0x5b0330
    virtual double Virtual43() override;               // 0x5b05b0
    virtual void ReadState(istream* in) override;      // 0x5b0040
    virtual void ReadExtra(istream* in) override;      // 0x5b0050
    virtual void WriteState(ostream* out) override;    // 0x5b0010
    virtual void WriteExtra(ostream* out) override;    // 0x5b0020
    virtual void ChecksumState(Crc32* crc) override;   // 0x5b0070
    virtual void ChecksumExtra(Crc32* crc) override;   // 0x5b0080
    virtual double GetWeaponRecharge() override;        // 0x5b0400
    virtual double GetShieldRecharge() override;        // 0x5b0460
    virtual void GetLaunchPoint(TacticalVector* out) override;  // 0x5b0b80 (see TacticalCapitalShipLaunchPoint)
    virtual int GetSoundId() override;                 // 0x5b0100

    CapitalShipResult* BuildResult(CapitalShipResult* out);   // 0x5b0280
    void ApplyStatus(const void* record);              // 0x5b0230
};

// 0x5b0b80: the capital ship launch point, entered with the hangar subobject as `this` (the original vtable slot).
void TacticalCapitalShipLaunchPoint(TacticalHangar* hangar, TacticalVector* out);

class TacticalFighter : public TacticalShipBase {
public:
    TacticalFighter(uint32_t family, uint32_t id, int notAlliance, uint32_t typeIndex, String name, int carrierHandle,
                    float field64, float field68, float field60, int squadSize, int currentSize, int zero,
                    float hyperdriveCount, int grid02, int grid12, int grid22, int grid32, int grid01, int grid11,
                    int grid21, int grid31, int grid00, int grid10, int grid20, int grid30, float a42, float a41,
                    float a40, int totalTorpedoes, float torpedoRange);  // 0x5b9c60
    virtual ~TacticalFighter();   // 0x5b8ec0 (scalar deleting destructor 0x5a6790)

    virtual double Turn(float step, float rate) override;         // 0x5b97a0 (3D battle)
    virtual void Advance(float milliseconds) override;            // 0x5b8f70 (3D battle: the per-frame update)
    virtual int GetUnitClass() override { return 1; }             // 0x40f340 (shared stub)
    virtual uint32_t GetTypeIndex() override { return m_typeIndex; }  // 0x5a6760 (shared stub)
    virtual int Virtual32(float a) override;           // 0x5b9a20 (3D battle)
    virtual void Virtual39(float step) override {}     // 0x5ae1e0 (shared stub)
    virtual int Virtual44() override { return 3; }     // 0x526f00 (shared stub)
    virtual int Virtual45() override { return 3; }     // 0x526f00 (shared stub)
    virtual int Virtual46() override { return 3; }     // 0x526f00 (shared stub)
    virtual void Virtual49() override;                 // 0x5ba0f0 (sound)
    virtual void Virtual50() override;                 // 0x5b00c0 (sound, the capital ship's body)
    virtual void Virtual51() override;                 // 0x5ba0b0 (sound)
    virtual void Virtual52() override;                 // 0x5ba0d0 (sound)
    virtual void Virtual26() override;                 // 0x5ba2f0
    virtual int Virtual28() override;                  // 0x5b8f00
    virtual int Virtual29() override;                  // 0x5b8f20
    virtual int Virtual30() override;                  // 0x5b8f50
    virtual double GetWeaponRecharge() override;        // 0x5b9c00
    virtual double GetShieldRecharge() override;        // 0x5b9c30
    virtual double GetSpeed() override;                 // 0x5ba190
    virtual double GetTurnRate() override;              // 0x5ba270
    virtual int GetSoundId() override;                  // 0x5b8df0

    FightSquadResult* BuildResult(FightSquadResult* out);     // 0x5b8e00
    void ApplyStatus(const FightSquadResult* record);         // 0x5ba150
    uint32_t SquadsAlive();                                   // 0x5ba330
    uint32_t SquadsMax();                                     // 0x5ba380
};

// Event handlers of the 3D battle (cdecl; registered in the object manager's event table).
void TacticalShipStatusEvent(TacticalEvent* event);           // 0x5b01d0 a ship's synchronised state
void TacticalShipCarrierWaitEvent(TacticalEvent* event);      // 0x5b1e10
void TacticalShipJumpEvent(TacticalEvent* event);             // 0x5b1e40
void TacticalShipImpactEventB(TacticalEvent* event);          // 0x5b1ea0
void TacticalShipImpactEventC(TacticalEvent* event);          // 0x5b1f60
void TacticalShipImpactEventD(TacticalEvent* event);          // 0x5b2080
void TacticalShipTractorStatusEvent(TacticalEvent* event);    // 0x5b2340
void TacticalShipStartTractorEvent(TacticalEvent* event);     // 0x5b2630
void TacticalShipStopTractorEvent(TacticalEvent* event);      // 0x5b26a0
void TacticalShipWithdrawEvent(TacticalEvent* event);         // 0x5b26d0
void TacticalShipJumpAbortEvent(TacticalEvent* event);        // 0x5b2720
void TacticalShipAddDelayEvent(TacticalEvent* event);         // 0x5b27b0
void TacticalShipClearArcEvent(TacticalEvent* event);         // 0x5b27e0
void TacticalShipAddTractorJamEvent(TacticalEvent* event);    // 0x5b2810
void TacticalShipHitSpeedEvent(TacticalEvent* event);         // 0x5b2840
void TacticalShipHitTractorEvent(TacticalEvent* event);       // 0x5b2870
void TacticalShipHitWeaponEvent(TacticalEvent* event);        // 0x5b28a0
void TacticalShipHitShieldEvent(TacticalEvent* event);        // 0x5b28d0
void TacticalShipHitHyperdriveEvent(TacticalEvent* event);    // 0x5b2900
void TacticalShipRepairSpeedEvent(TacticalEvent* event);      // 0x5b2930
void TacticalShipRepairTractorEvent(TacticalEvent* event);    // 0x5b2960
void TacticalShipRepairWeaponEvent(TacticalEvent* event);     // 0x5b2990
void TacticalShipRepairShieldEvent(TacticalEvent* event);     // 0x5b29c0
void TacticalShipRepairHyperdriveEvent(TacticalEvent* event); // 0x5b29f0
void TacticalShipHitEvent54(TacticalEvent* event);            // 0x5b2a20
void TacticalShipHitEvent55(TacticalEvent* event);            // 0x5b2a70
void TacticalShipHitEvent56(TacticalEvent* event);            // 0x5b2ac0
void TacticalShipHitEvent53(TacticalEvent* event);            // 0x5b2b10
void TacticalShipCollisionEvent(TacticalEvent* event);        // 0x5b2b60
void TacticalShipImpactEventE(TacticalEvent* event);          // 0x5b2c70
void TacticalFighterStatusEvent(TacticalEvent* event);        // 0x5ba120
void TacticalCarrierLaunchEvent(TacticalEvent* event);        // 0x5b8b10
void TacticalCarrierLaunchedEvent(TacticalEvent* event);      // 0x5b8bd0
void TacticalDeathStarImpactEvent(TacticalEvent* event);      // 0x5ba520
void TacticalDeathStarStatusEvent(TacticalEvent* event);      // 0x5ba5b0
void TacticalDeathStarFiredEvent(TacticalEvent* event);       // 0x5ba5e0

// 0x5b8ce0: whether the unit with this handle is a ship in state 3.
int TacticalShipIsInState3(int handle);

// The Death Star (0x5ba3a0-0x5ba8b0, 0x7c bytes): a unit that is not a ship: TacticalUnit at +0, its carrier interface at
// +0x1c. Built by the simulator / object manager when the battle has one (HeadlessSimulator::AddDeathStar).
class TacticalDeathStar : public TacticalUnit, public TacticalHangar {
public:
    TacticalDeathStar(uint32_t family, uint32_t id, int notCompleted, int hasFacility, float hull, float hullNow,
                      int hangarFree);       // inlined into 0x5af4e0 / 0x5a9220
    virtual ~TacticalDeathStar();            // 0x5ba3a0 (scalar deleting destructor 0x5add90)
    virtual int Virtual01() override { return 5; }  // 0x404c20
    virtual void GetLaunchPoint(TacticalVector* out) override;  // 0x5ba480 (3D battle; see TacticalDeathStarLaunchPoint)

    int IsOperational();                     // 0x5ba420: has hull left, is not under repair and has not fired
    int IsReady();                           // 0x5ba450: IsOperational and completed
    DeathStarResult* BuildResult(DeathStarResult* out);          // 0x5ba620
    CapitalShipResult* BuildCapitalShipResult(CapitalShipResult* out);  // 0x5ba670
    void ResetCharge(int unused);            // 0x5ba710
    void Charge(float milliseconds);         // 0x5ba7f0
    void FireAt(int target);                 // 0x5ba720: starts the superlaser shot
    void Destroy();                          // 0x5ba890: no hull, no charge, takes the hangar down
    void ResetCharged();                     // 0x5ba8a0

    TurnSyncChannelA m_channelA;  // +0x4c
    TurnSyncChannelB m_channelB;  // +0x54
    uint32_t m_objectFamily;      // +0x5c
    uint32_t m_objectId;          // +0x60
    float m_hullMax;              // +0x64
    float m_hull;                 // +0x68
    float m_charge;               // +0x6c superlaser charge, 100 when ready
    int m_notCompleted;           // +0x70
    int m_hasFacility;            // +0x74
    int m_fired;                  // +0x78
};
