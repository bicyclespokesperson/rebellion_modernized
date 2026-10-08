#pragma once
#include <cstdint>

#include "core/game_string.h"
#include "core/layout.h"
#include "tactical/tactical_ship_decl.h"
#include "tactical/tactical_types.h"
#include "tactical/tactical_units.h"

// The battle units of the tactical engine: capital ships and fighter squadrons share the abstract base class
// TacticalShipBase (0x5b0f70, VT 0x66c560, 0x418 bytes); a capital ship adds the carrier interface TacticalHangar
// (0x5b82a0, 0x30 bytes) behind it. The same objects run in the 3D battle and in the headless simulator; only the
// per-frame update and the presentation hooks are specific to the 3D battle.
//
// Layout: TacticalShipObject (+0x00, frame), TacticalUnit (+0xd4, id node keyed by the unit's handle: the registry
// hands out pointers to this subobject and the unit lists store it), four small interface bases, then the unit's state.

class TacticalShipBase;

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
// The 0x14-byte object describing how well one weapon kind hurts a target class (classes of the visual-resources
// module, 0x5d2360); only the factors the damage computation reads are named.
struct TacticalWeaponProfile {
    void* m_vptr;
    float m_f04;
    float m_f08;
    float m_f0c;
    float m_f10;
};

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
    TacticalWeaponProfile m_profileA;  // +0x18 object built by 0x5d2530
    TacticalWeaponProfile m_profileB;  // +0x2c object built by 0x5d24e0
    TacticalWeaponProfile m_profileC;  // +0x40 object built by 0x5d2490
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
    TacticalUnitTree* m_docked;   // +0x1c
    TacticalUnitTree* m_launched; // +0x20
    int m_unk24;                  // +0x24
    TacticalUnitTree* m_unk28;    // +0x28
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
    void ReleaseDocked();                       // 0x5b8d10 sends every docked squadron into battle
    void Disband();                             // 0x5b8d80 destroys the squadrons the carrier has aboard or in flight
};

class TacticalShipBase : public TacticalShipObject,
                         public TacticalUnit,
                         public TacticalShipFacetA,
                         public TacticalShipFacetB,
                         public TacticalShipFacetC,
                         public TacticalShipFacetD {
public:
    TacticalShipBase();                       // 0x5b0f70
    virtual ~TacticalShipBase();              // 0x5b12d0 (scalar deleting destructor 0x5b12b0)

    // ---- TacticalShipObject slots (1-2) -----------------------------------------------------------------------
    virtual double Turn(float step, float rate) override;   // 0x5b1410
    virtual void Advance(float milliseconds) override;     // 0x5b2f30 (3D battle: the per-frame update)

    // ---- TacticalUnit slot 1 ----------------------------------------------------------------------------------
    virtual void Virtual01() override;                     // 0x5a66f0 (shared stub)

    virtual int Destroy();                                 // 7: 0x5b2d20
    virtual int GetUnitClass();                        // 8: 0 for a capital ship, 1 for a fighter squadron
    virtual uint32_t GetTypeIndex();                       // 9: 0x5a6760
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
    virtual double GetWeaponRecharge();                 // 24
    virtual double GetShieldRecharge();                 // 25
    virtual void Virtual26();                              // 26: capital 0x5b0ad0 / fighter 0x5ba2f0: leaves the carrier
    virtual int GetSoundId();
    virtual int Virtual28();
    virtual int Virtual29();
    virtual int Virtual30();
    virtual void Virtual31(TacticalVector* out);           // 0x5b4960
    virtual int Virtual32(float a);                        // 32: 0x5b4d40 / 0x5b9a20
    virtual uint32_t SelectArc(int targetClass);           // 33: 0x5b4c60
    virtual void Virtual34(TacticalVector* out);           // 0x5b3a40
    virtual double GetRadius();                             // 35: 0x5b6900
    virtual double GetSpeed();                              // 36: capital 0x5b17f0, fighter 0x5ba190
    virtual TacticalShipBase* GetNextShip();               // 37: 0x5b6960 the next ship in the registry order
    virtual TacticalShipBase* GetNextShip2();              // 38: 0x5b6960
    virtual void Virtual39(float step);
    virtual void ApplyCriticalHit(int kind);               // 40: 0x5b1970
    virtual void RepairCriticalHit(int kind);              // 41: 0x5b1ab0
    virtual void Virtual42(int a);                         // 0x5b6530
    virtual double Virtual43();
    virtual int Virtual44();
    virtual int Virtual45();
    virtual int Virtual46();
    virtual double GetTurnRate();                       // 47: 0x5b0be0 / 0x5ba270
    virtual void Virtual48(float a);                       // 0x5b5700
    virtual void Virtual49();
    virtual void Virtual50();
    virtual void Virtual51();
    virtual void Virtual52();
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
    TacticalUnitTree m_unk360;       // +0x360
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

    virtual int GetUnitClass() override;               // 0x6158b0 (shared stub)
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
    virtual double GetTurnRate() override;              // 0x5b0be0
    virtual void GetLaunchPoint(TacticalVector* out) override;  // 0x5b0b80 (see TacticalCapitalShipLaunchPoint)
    virtual int GetSoundId() override;                 // 0x5b0100

    CapitalShipResult* BuildResult(CapitalShipResult* out);   // 0x5b0280
    void ApplyStatus(const void* record);              // 0x5b0230
};

// 0x5b0b80: the capital ship launch point, entered with the hangar subobject as `this` (the original vtable slot).
void __thiscall TacticalCapitalShipLaunchPoint(TacticalHangar* hangar, TacticalVector* out);

class TacticalFighter : public TacticalShipBase {
public:
    TacticalFighter(uint32_t family, uint32_t id, int notAlliance, uint32_t typeIndex, String name, int carrierHandle,
                    float field64, float field68, float field60, int squadSize, int currentSize, int zero,
                    float hyperdriveCount, int grid02, int grid12, int grid22, int grid32, int grid01, int grid11,
                    int grid21, int grid31, int grid00, int grid10, int grid20, int grid30, float a42, float a41,
                    float a40, int totalTorpedoes, float torpedoRange);  // 0x5b9c60
    virtual ~TacticalFighter();   // 0x5b8ec0 (scalar deleting destructor 0x5a6790)

    virtual int GetUnitClass() override;               // 0x40f340 (shared stub)
    virtual void Virtual26() override;                 // 0x5ba2f0
    virtual int Virtual28() override;                  // 0x5b8f00
    virtual int Virtual29() override;                  // 0x5b8f20
    virtual int Virtual30() override;                  // 0x5b8f50
    virtual double Virtual43() override;               // 0x5b7770
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

// What the object manager hands an event handler: the payload is an array of 32-bit words.
struct TacticalEvent {
    void* m_unk00;
    int* m_args;  // +0x04
};

// Event handlers of the 3D battle (cdecl; registered in the object manager's event table).
void __cdecl TacticalShipStatusEvent(TacticalEvent* event);           // 0x5b01d0 a ship's synchronised state
void __cdecl TacticalShipCarrierWaitEvent(TacticalEvent* event);      // 0x5b1e10
void __cdecl TacticalShipJumpEvent(TacticalEvent* event);             // 0x5b1e40
void __cdecl TacticalShipImpactEventB(TacticalEvent* event);          // 0x5b1ea0
void __cdecl TacticalShipImpactEventC(TacticalEvent* event);          // 0x5b1f60
void __cdecl TacticalShipImpactEventD(TacticalEvent* event);          // 0x5b2080
void __cdecl TacticalShipTractorStatusEvent(TacticalEvent* event);    // 0x5b2340
void __cdecl TacticalShipStartTractorEvent(TacticalEvent* event);     // 0x5b2630
void __cdecl TacticalShipStopTractorEvent(TacticalEvent* event);      // 0x5b26a0
void __cdecl TacticalShipWithdrawEvent(TacticalEvent* event);         // 0x5b26d0
void __cdecl TacticalShipJumpAbortEvent(TacticalEvent* event);        // 0x5b2720
void __cdecl TacticalShipAddDelayEvent(TacticalEvent* event);         // 0x5b27b0
void __cdecl TacticalShipClearArcEvent(TacticalEvent* event);         // 0x5b27e0
void __cdecl TacticalShipAddTractorJamEvent(TacticalEvent* event);    // 0x5b2810
void __cdecl TacticalShipHitSpeedEvent(TacticalEvent* event);         // 0x5b2840
void __cdecl TacticalShipHitTractorEvent(TacticalEvent* event);       // 0x5b2870
void __cdecl TacticalShipHitWeaponEvent(TacticalEvent* event);        // 0x5b28a0
void __cdecl TacticalShipHitShieldEvent(TacticalEvent* event);        // 0x5b28d0
void __cdecl TacticalShipHitHyperdriveEvent(TacticalEvent* event);    // 0x5b2900
void __cdecl TacticalShipRepairSpeedEvent(TacticalEvent* event);      // 0x5b2930
void __cdecl TacticalShipRepairTractorEvent(TacticalEvent* event);    // 0x5b2960
void __cdecl TacticalShipRepairWeaponEvent(TacticalEvent* event);     // 0x5b2990
void __cdecl TacticalShipRepairShieldEvent(TacticalEvent* event);     // 0x5b29c0
void __cdecl TacticalShipRepairHyperdriveEvent(TacticalEvent* event); // 0x5b29f0
void __cdecl TacticalShipHitEvent54(TacticalEvent* event);            // 0x5b2a20
void __cdecl TacticalShipHitEvent55(TacticalEvent* event);            // 0x5b2a70
void __cdecl TacticalShipHitEvent56(TacticalEvent* event);            // 0x5b2ac0
void __cdecl TacticalShipHitEvent53(TacticalEvent* event);            // 0x5b2b10
void __cdecl TacticalShipCollisionEvent(TacticalEvent* event);        // 0x5b2b60
void __cdecl TacticalShipImpactEventE(TacticalEvent* event);          // 0x5b2c70
void __cdecl TacticalFighterStatusEvent(TacticalEvent* event);        // 0x5ba120

// 0x5b8ce0: whether the unit with this handle is a ship in state 3.
int __stdcall TacticalShipIsInState3(int handle);

// The Death Star (0x5ba3a0-0x5ba8b0, 0x7c bytes): a unit that is not a ship: TacticalUnit at +0, its carrier interface at
// +0x1c. Built by the simulator / object manager when the battle has one (HeadlessSimulator::AddDeathStar).
class TacticalDeathStar : public TacticalUnit, public TacticalHangar {
public:
    TacticalDeathStar(uint32_t family, uint32_t id, int notCompleted, int hasFacility, float hull, float hullNow,
                      int hangarFree);       // inlined into 0x5af4e0 / 0x5a9220
    virtual ~TacticalDeathStar();            // 0x5ba3a0 (scalar deleting destructor 0x5add90)
    virtual void Virtual01() override;       // 0x404c20 (shared stub: returns 1)
    virtual void GetLaunchPoint(TacticalVector* out) override;  // 0x5ba480 (3D battle; see TacticalDeathStarLaunchPoint)

    int IsOperational();                     // 0x5ba420: has hull left, is not under repair and has not fired
    int IsReady();                           // 0x5ba450: IsOperational and completed
    DeathStarResult* BuildResult(DeathStarResult* out);          // 0x5ba620
    CapitalShipResult* BuildCapitalShipResult(CapitalShipResult* out);  // 0x5ba670
    void ResetCharge(int unused);            // 0x5ba710
    void Charge(float milliseconds);         // 0x5ba7f0
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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(TacticalDeathStar, 0x7c);
REB_ASSERT_OFFSET(TacticalDeathStar, m_channelA, 0x4c);
REB_ASSERT_OFFSET(TacticalDeathStar, m_hull, 0x68);
REB_ASSERT_OFFSET(TacticalDeathStar, m_fired, 0x78);
REB_ASSERT_SIZE(TacticalWeaponArc, 0x74);
REB_ASSERT_OFFSET(TacticalWeaponArc, m_firepower, 0x70);
REB_ASSERT_SIZE(TacticalHangar, 0x30);
REB_ASSERT_SIZE(TacticalShipBase, 0x418);
REB_ASSERT_SIZE(TacticalCapitalShip, 0x448);
REB_ASSERT_SIZE(TacticalFighter, 0x418);
REB_ASSERT_OFFSET(TacticalShipBase, m_channelA, 0x118);
REB_ASSERT_OFFSET(TacticalShipBase, m_objectFamily, 0x128);
REB_ASSERT_OFFSET(TacticalShipBase, m_name, 0x144);
REB_ASSERT_OFFSET(TacticalShipBase, m_state, 0x170);
REB_ASSERT_OFFSET(TacticalShipBase, m_arcs, 0x17c);
REB_ASSERT_OFFSET(TacticalShipBase, m_chargeQueue, 0x34c);
REB_ASSERT_OFFSET(TacticalShipBase, m_unk360, 0x360);
REB_ASSERT_OFFSET(TacticalShipBase, m_speedCH, 0x36c);
REB_ASSERT_OFFSET(TacticalShipBase, m_tractoredBy, 0x384);
REB_ASSERT_OFFSET(TacticalShipBase, m_hullMax, 0x3c8);
REB_ASSERT_OFFSET(TacticalShipBase, m_speed, 0x3f8);
REB_ASSERT_OFFSET(TacticalShipBase, m_unk414, 0x414);
#pragma clang diagnostic pop
