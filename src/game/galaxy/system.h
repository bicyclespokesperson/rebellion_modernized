#pragma once
#include <cstdint>

#include "core/layout.h"
#include "gameobj/child_iterator.h"
#include "gameobj/game_obj.h"
#include "gameobj/side_bits.h"

// System (ctor 0x507130, VT 0x65e640, secondary ObjectBase VT 0x65e63c, 0x8c bytes) is a star system (family 0x90-0x97,
// ids start at 100). It sits directly on GameObj and is abstract: slots 1, 5-9, 37, 57 and 115 stay pure; the leaf
// classes are the type-0x90 class (0x566b90) and the type-0x92 class (0x56a430). Strings: "System*Notif" (OnXNotified),
// "Invalid SystemEnergy value!" etc.
//
// Vtable: 188 slots. 0-110 are GameObj's, 111-115 are new non-property virtuals (names provisional), 116-150 are the
// OnXChanged events of the 35 System properties, 151-185 the OnXNotified events (the same order as the fields and the
// Set/Apply functions), 186-187 are two more new virtuals. The properties:
//   Loyalty +0x58 (0..100 when flag Populated is set, constructed as 50), Energy +0x5c, EnergyAllocated +0x60,
//   RawMaterial +0x64, RawMaterialAllocated +0x68, SmugglingPercent +0x6c (-100..100), ProductionModifier +0x70,
//   TroopRegWithdrawPercent +0x74
//   4 two-bit fields of m_controlBits +0x78 (bits 0-1, 2-3, 4-5, 6-7): ControlKindBattleWon, ControlKindBattleWithdrew,
//   ControlKindUprising, ControlKindBattleReady
//   TroopRegSurplus +0x7c, TroopRegRequired +0x80, ControlData +0x84 (a SideBits: controller / loyalty / garrison)
//   20 flags of m_flags +0x88: Populated 0x1, Explored 0x2, Uprising 0x4, NeverBeenControlled 0x8, Battle 0x10,
//   Blockade 0x20, Bombard 0x40, Assault 0x80, Garrisoned 0x100, Suppressing 0x200, CombatUnitFastRepair 0x400,
//   DeathStarNearby 0x800, BattlePending 0x1000, LoyaltyCausedCurrentControlKind 0x2000,
//   BattlePendingCausedCurrentBlockade 0x4000, UprisingIncident 0x10000, InformantIncident 0x20000,
//   DisasterIncident 0x40000, ResourceIncident 0x80000, BlockadeAndBattlePendingManagementRequired 0x100000
// The Set/Apply/OnChanged/OnNotified pattern is the one of GameObj (see game_obj.h). ApplyEnergyAllocated (0x5037f0) is
// folded (ICF) with an identical function of another class; it is declared here but not defined.

class Crc32;
class EventKindSet;
class System;

// 0x5037f0: ApplyEnergyAllocated, identical to a function of the fleet code and folded with it (not reconstructed here).
int __thiscall FUN_005037f0(System* system, NotifyContext* ctx);

namespace system_flag {
constexpr uint32_t kPopulated = 0x1;
constexpr uint32_t kExplored = 0x2;
constexpr uint32_t kUprising = 0x4;
constexpr uint32_t kNeverBeenControlled = 0x8;
constexpr uint32_t kBattle = 0x10;
constexpr uint32_t kBlockade = 0x20;
constexpr uint32_t kBombard = 0x40;
constexpr uint32_t kAssault = 0x80;
constexpr uint32_t kGarrisoned = 0x100;
constexpr uint32_t kSuppressing = 0x200;
constexpr uint32_t kCombatUnitFastRepair = 0x400;
constexpr uint32_t kDeathStarNearby = 0x800;
constexpr uint32_t kBattlePending = 0x1000;
constexpr uint32_t kLoyaltyCausedCurrentControlKind = 0x2000;
constexpr uint32_t kBattlePendingCausedCurrentBlockade = 0x4000;
constexpr uint32_t kUprisingIncident = 0x10000;
constexpr uint32_t kInformantIncident = 0x20000;
constexpr uint32_t kDisasterIncident = 0x40000;
constexpr uint32_t kResourceIncident = 0x80000;
constexpr uint32_t kBlockadeAndBattlePendingManagementRequired = 0x100000;
}  // namespace system_flag

// One of the four 2-bit fields of System::m_controlBits (shift 0, 2, 4 or 6).
inline uint32_t ControlBitsField(uint32_t bits, int shift) { return (bits >> shift) & 3; }
inline uint32_t WithControlBitsField(uint32_t bits, int shift, uint32_t value) {
    return (bits & ~(3u << shift)) | ((value & 3) << shift);
}

class System : public GameObj {
public:
    System();                    // 0x507130
    virtual ~System();           // 0x5071f0 (scalar deleting), 0x507210

    // ---- overrides of GameObj slots ----
    virtual int GetOwnerKey(ObjectKey* out) override;                                       // 3, 0x508070
    // Slot 10 is 0x50e1c0, shared (ICF) with Character::SetRecord: not declared here so that it is defined only once.
    virtual int CollectDependencies(void* context, int value, ObjectKeyValueSet* out) override;  // 11, 0x512700
    virtual int GetEventKinds(int unused, EventKindSet* out) override;                      // 15, 0x507290
    virtual int Virtual16() override { return 1; }                                          // 16
    virtual int CheckSameControl(GameObj* other, StatusCode* out) override;                 // 21, 0x5072d0
    virtual int CheckOrder(int a, int b, StatusCode* out) override;                         // 22, 0x507c30
    virtual int Virtual29(int a, int b, int c, int d, StatusCode* out) override;            // 29, 0x5073d0
    virtual int Virtual30(const FamilyRange* range, int b, int* o1, int* o2, int* o3, StatusCode* out) override;  // 30, 0x507750
    virtual int Virtual32(const FamilyRange* range, int b, int* o1, int* o2, int* o3, StatusCode* out) override;  // 32, 0x507ab0
    virtual int ChangeDestroyedReason(int reason, NotifyContext* ctx) override;             // 43, 0x5080d0
    virtual int Virtual47(int a) override;                                                  // 47, 0x5081f0
    virtual int Virtual48(int a) override;                                                  // 48, 0x508250
    virtual int Virtual49(int a, int b, int c) override;                                    // 49, 0x508440
    virtual int Virtual50(int a, int b, int c) override;                                    // 50, 0x508660
    virtual int Virtual52(int a, int b, int c) override;                                    // 52, 0x508c60
    virtual int Virtual53(int a, int b, int c) override;                                    // 53, 0x508da0
    virtual int ApplyAllToViewOf(int view, int unused, NotifyContext* ctx) override;        // 54, 0x508f90
    virtual GameObjData* CreateData() override;                                             // 58, 0x512c90
    virtual int OnNameChanged(String* oldName, String* newName, NotifyContext* ctx) override;  // 59, 0x510800
    virtual int OnControlKindChanged(uint32_t oldKind, uint32_t newKind, NotifyContext* ctx) override;  // 60, 0x510820
    virtual int OnDestroyedChanged(int newValue, NotifyContext* ctx) override;              // 71, 0x510950
    virtual int OnExistingChanged(int newValue, NotifyContext* ctx) override;               // 74, 0x5109a0

    // ---- new non-property virtuals (slots 111-115; names provisional) ----
    virtual int Virtual111();                                                               // 111, 0x507d00
    virtual int Virtual112();                                                               // 112, 0x507e60
    virtual int Virtual113();                                                               // 113, 0x507f10
    virtual int Virtual114();                                                               // 114, 0x507fc0
    virtual int Virtual115() = 0;                                                           // 115, pure

    // ---- OnXChanged events on the master (slots 116-150; the empty ones are the shared return-1 stubs) ----
    virtual int OnLoyaltyChanged(int oldValue, int newValue, NotifyContext* ctx);  // 116, 0x5109f0
    virtual int OnEnergyChanged(int oldValue, int newValue, NotifyContext* ctx);  // 117, 0x510ae0
    virtual int OnEnergyAllocatedChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }  // 118, 0x524b10
    virtual int OnRawMaterialChanged(int oldValue, int newValue, NotifyContext* ctx);  // 119, 0x510b20
    virtual int OnRawMaterialAllocatedChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }  // 120, 0x524b10
    virtual int OnSmugglingPercentChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }  // 121, 0x524b10
    virtual int OnProductionModifierChanged(int oldValue, int newValue, NotifyContext* ctx);  // 122, 0x510b60
    virtual int OnTroopRegWithdrawPercentChanged(int oldValue, int newValue, NotifyContext* ctx);  // 123, 0x510b80
    virtual int OnControlKindBattleWonChanged(int oldValue, int newValue, NotifyContext* ctx);  // 124, 0x510bb0
    virtual int OnControlKindBattleWithdrewChanged(int oldValue, int newValue, NotifyContext* ctx);  // 125, 0x510c50
    virtual int OnControlKindUprisingChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }  // 126, 0x524b10
    virtual int OnControlKindBattleReadyChanged(int oldValue, int newValue, NotifyContext* ctx) { return 1; }  // 127, 0x524b10
    virtual int OnTroopRegSurplusChanged(int oldValue, int newValue, NotifyContext* ctx);  // 128, 0x510cf0
    virtual int OnTroopRegRequiredChanged(int oldValue, int newValue, NotifyContext* ctx);  // 129, 0x510d40
    virtual int OnControlDataChanged(const SideBits* oldValue, const SideBits* newValue, NotifyContext* ctx);  // 130, 0x510d70
    virtual int OnPopulatedChanged(int newValue, NotifyContext* ctx);  // 131, 0x510e30
    virtual int OnExploredChanged(int newValue, NotifyContext* ctx) { return 1; }  // 132, 0x524fb0
    virtual int OnUprisingChanged(int newValue, NotifyContext* ctx);  // 133, 0x510f20
    virtual int OnNeverBeenControlledChanged(int newValue, NotifyContext* ctx) { return 1; }  // 134, 0x524fb0
    virtual int OnBattleChanged(int newValue, NotifyContext* ctx);  // 135, 0x511080
    virtual int OnBlockadeChanged(int newValue, NotifyContext* ctx);  // 136, 0x511300
    virtual int OnBombardChanged(int newValue, NotifyContext* ctx);  // 137, 0x5114f0
    virtual int OnAssaultChanged(int newValue, NotifyContext* ctx);  // 138, 0x5115e0
    virtual int OnGarrisonedChanged(int newValue, NotifyContext* ctx);  // 139, 0x511610
    virtual int OnSuppressingChanged(int newValue, NotifyContext* ctx);  // 140, 0x5116d0
    virtual int OnCombatUnitFastRepairChanged(int newValue, NotifyContext* ctx);  // 141, 0x511710
    virtual int OnDeathStarNearbyChanged(int newValue, NotifyContext* ctx);  // 142, 0x511740
    virtual int OnBattlePendingChanged(int newValue, NotifyContext* ctx);  // 143, 0x511780
    virtual int OnLoyaltyCausedCurrentControlKindChanged(int newValue, NotifyContext* ctx) { return 1; }  // 144, 0x524fb0
    virtual int OnBattlePendingCausedCurrentBlockadeChanged(int newValue, NotifyContext* ctx) { return 1; }  // 145, 0x524fb0
    virtual int OnUprisingIncidentChanged(int newValue, NotifyContext* ctx);  // 146, 0x511840
    virtual int OnInformantIncidentChanged(int newValue, NotifyContext* ctx);  // 147, 0x511860
    virtual int OnDisasterIncidentChanged(int newValue, NotifyContext* ctx);  // 148, 0x511930
    virtual int OnResourceIncidentChanged(int newValue, NotifyContext* ctx) { return 1; }  // 149, 0x524fb0
    virtual int OnBlockadeAndBattlePendingManagementRequiredChanged(int newValue, NotifyContext* ctx);  // 150, 0x511b30

    // ---- OnXNotified events on a side view (slots 151-185, the "System*Notif" strings) ----
    virtual int OnLoyaltyNotified(int oldValue, int newValue, NotifyContext* ctx);  // 151, 0x511b90
    virtual int OnEnergyNotified(int oldValue, int newValue, NotifyContext* ctx);  // 152, 0x511c90
    virtual int OnEnergyAllocatedNotified(int oldValue, int newValue, NotifyContext* ctx);  // 153, 0x511ce0
    virtual int OnRawMaterialNotified(int oldValue, int newValue, NotifyContext* ctx);  // 154, 0x511d30
    virtual int OnRawMaterialAllocatedNotified(int oldValue, int newValue, NotifyContext* ctx);  // 155, 0x511d80
    virtual int OnSmugglingPercentNotified(int oldValue, int newValue, NotifyContext* ctx);  // 156, 0x511e20
    virtual int OnProductionModifierNotified(int oldValue, int newValue, NotifyContext* ctx);  // 157, 0x511dd0
    virtual int OnTroopRegWithdrawPercentNotified(int oldValue, int newValue, NotifyContext* ctx);  // 158, 0x511e70
    virtual int OnControlKindBattleWonNotified(int oldValue, int newValue, NotifyContext* ctx);  // 159, 0x511ec0
    virtual int OnControlKindBattleWithdrewNotified(int oldValue, int newValue, NotifyContext* ctx);  // 160, 0x511f00
    virtual int OnControlKindUprisingNotified(int oldValue, int newValue, NotifyContext* ctx);  // 161, 0x511f40
    virtual int OnControlKindBattleReadyNotified(int oldValue, int newValue, NotifyContext* ctx);  // 162, 0x511f90
    virtual int OnTroopRegSurplusNotified(int oldValue, int newValue, NotifyContext* ctx);  // 163, 0x511fd0
    virtual int OnTroopRegRequiredNotified(int oldValue, int newValue, NotifyContext* ctx);  // 164, 0x512020
    virtual int OnControlDataNotified(const SideBits* oldValue, const SideBits* newValue, NotifyContext* ctx);  // 165, 0x512070
    virtual int OnPopulatedNotified(int value, NotifyContext* ctx);  // 166, 0x512140
    virtual int OnExploredNotified(int value, NotifyContext* ctx);  // 167, 0x512190
    virtual int OnUprisingNotified(int value, NotifyContext* ctx);  // 168, 0x5121e0
    virtual int OnNeverBeenControlledNotified(int value, NotifyContext* ctx);  // 169, 0x512230
    virtual int OnBattleNotified(int value, NotifyContext* ctx);  // 170, 0x512280
    virtual int OnBlockadeNotified(int value, NotifyContext* ctx);  // 171, 0x5122d0
    virtual int OnBombardNotified(int value, NotifyContext* ctx);  // 172, 0x512320
    virtual int OnAssaultNotified(int value, NotifyContext* ctx);  // 173, 0x512370
    virtual int OnGarrisonedNotified(int value, NotifyContext* ctx);  // 174, 0x5123c0
    virtual int OnSuppressingNotified(int value, NotifyContext* ctx);  // 175, 0x512400
    virtual int OnCombatUnitFastRepairNotified(int value, NotifyContext* ctx);  // 176, 0x512440
    virtual int OnDeathStarNearbyNotified(int value, NotifyContext* ctx);  // 177, 0x512480
    virtual int OnBattlePendingNotified(int value, NotifyContext* ctx);  // 178, 0x5124c0
    virtual int OnLoyaltyCausedCurrentControlKindNotified(int value, NotifyContext* ctx);  // 179, 0x512500
    virtual int OnBattlePendingCausedCurrentBlockadeNotified(int value, NotifyContext* ctx);  // 180, 0x512540
    virtual int OnUprisingIncidentNotified(int value, NotifyContext* ctx);  // 181, 0x512580
    virtual int OnInformantIncidentNotified(int value, NotifyContext* ctx);  // 182, 0x5125d0
    virtual int OnDisasterIncidentNotified(int value, NotifyContext* ctx);  // 183, 0x512620
    virtual int OnResourceIncidentNotified(int value, NotifyContext* ctx);  // 184, 0x512670
    virtual int OnBlockadeAndBattlePendingManagementRequiredNotified(int value, NotifyContext* ctx);  // 185, 0x5126c0

    // ---- validated setters (master view only) and Apply functions (side views) ----
    int SetLoyalty(int value, NotifyContext* ctx);  // 0x509c30
    int SetEnergy(int value, NotifyContext* ctx);  // 0x509cc0
    int SetEnergyAllocated(int value, NotifyContext* ctx);  // 0x509d40
    int SetRawMaterial(int value, NotifyContext* ctx);  // 0x509dc0
    int SetRawMaterialAllocated(int value, NotifyContext* ctx);  // 0x509e40
    int SetSmugglingPercent(int value, NotifyContext* ctx);  // 0x509ec0
    int SetProductionModifier(int value, NotifyContext* ctx);  // 0x509f40
    int SetTroopRegWithdrawPercent(int value, NotifyContext* ctx);  // 0x509fc0
    int SetControlKindBattleWon(int value, NotifyContext* ctx);  // 0x50a040
    int SetControlKindBattleWithdrew(int value, NotifyContext* ctx);  // 0x50a0b0
    int SetControlKindUprising(int value, NotifyContext* ctx);  // 0x50a130
    int SetControlKindBattleReady(int value, NotifyContext* ctx);  // 0x50a1b0
    int SetTroopRegSurplus(int value, NotifyContext* ctx);  // 0x50a230
    int SetTroopRegRequired(int value, NotifyContext* ctx);  // 0x50a290
    int SetControlData(const SideBits* value, NotifyContext* ctx);  // 0x50a300
    int SetPopulated(int on, NotifyContext* ctx);  // 0x50a3c0
    int SetExplored(int on, NotifyContext* ctx);  // 0x50a430
    int SetUprising(int on, NotifyContext* ctx);  // 0x50a4a0
    int SetNeverBeenControlled(int on, NotifyContext* ctx);  // 0x50a510
    int SetBattle(int on, NotifyContext* ctx);  // 0x50a580
    int SetBlockade(int on, NotifyContext* ctx);  // 0x50a5f0
    int SetBombard(int on, NotifyContext* ctx);  // 0x50a660
    int SetAssault(int on, NotifyContext* ctx);  // 0x50a6d0
    int SetGarrisoned(int on, NotifyContext* ctx);  // 0x50a740
    int SetSuppressing(int on, NotifyContext* ctx);  // 0x50a7b0
    int SetCombatUnitFastRepair(int on, NotifyContext* ctx);  // 0x50a820
    int SetDeathStarNearby(int on, NotifyContext* ctx);  // 0x50a890
    int SetBattlePending(int on, NotifyContext* ctx);  // 0x50a900
    int SetLoyaltyCausedCurrentControlKind(int on, NotifyContext* ctx);  // 0x50a970
    int SetBattlePendingCausedCurrentBlockade(int on, NotifyContext* ctx);  // 0x50a9e0
    int SetUprisingIncident(int on, NotifyContext* ctx);  // 0x50aa50
    int SetInformantIncident(int on, NotifyContext* ctx);  // 0x50aac0
    int SetDisasterIncident(int on, NotifyContext* ctx);  // 0x50ab30
    int SetResourceIncident(int on, NotifyContext* ctx);  // 0x50aba0
    int SetBlockadeAndBattlePendingManagementRequired(int on, NotifyContext* ctx);  // 0x50ac10
    int ApplyLoyalty(NotifyContext* ctx);  // 0x50f390
    int ApplyEnergy(NotifyContext* ctx);  // 0x50f410
    int ApplyEnergyAllocated(NotifyContext* ctx) { return FUN_005037f0(this, ctx); }  // 0x5037f0, folded with an unrelated class's function
    int ApplyRawMaterial(NotifyContext* ctx);  // 0x50f490
    int ApplyRawMaterialAllocated(NotifyContext* ctx);  // 0x50f510
    int ApplySmugglingPercent(NotifyContext* ctx);  // 0x50f590
    int ApplyProductionModifier(NotifyContext* ctx);  // 0x50f690
    int ApplyTroopRegWithdrawPercent(NotifyContext* ctx);  // 0x50f610
    int ApplyControlKindBattleWon(NotifyContext* ctx);  // 0x50f710
    int ApplyControlKindBattleWithdrew(NotifyContext* ctx);  // 0x50f7a0
    int ApplyControlKindUprising(NotifyContext* ctx);  // 0x50f840
    int ApplyControlKindBattleReady(NotifyContext* ctx);  // 0x50f8e0
    int ApplyTroopRegSurplus(NotifyContext* ctx);  // 0x50f980
    int ApplyTroopRegRequired(NotifyContext* ctx);  // 0x50fa00
    int ApplyControlData(NotifyContext* ctx);  // 0x50fa90
    int ApplyPopulated(NotifyContext* ctx);  // 0x50fbf0
    int ApplyExplored(NotifyContext* ctx);  // 0x50fc80
    int ApplyUprising(NotifyContext* ctx);  // 0x50fd10
    int ApplyNeverBeenControlled(NotifyContext* ctx);  // 0x50fda0
    int ApplyBattle(NotifyContext* ctx);  // 0x50fe30
    int ApplyBlockade(NotifyContext* ctx);  // 0x50fec0
    int ApplyBombard(NotifyContext* ctx);  // 0x50ff50
    int ApplyAssault(NotifyContext* ctx);  // 0x50ffe0
    int ApplyGarrisoned(NotifyContext* ctx);  // 0x510080
    int ApplySuppressing(NotifyContext* ctx);  // 0x510120
    int ApplyCombatUnitFastRepair(NotifyContext* ctx);  // 0x5101c0
    int ApplyDeathStarNearby(NotifyContext* ctx);  // 0x510260
    int ApplyBattlePending(NotifyContext* ctx);  // 0x510300
    int ApplyLoyaltyCausedCurrentControlKind(NotifyContext* ctx);  // 0x5103a0
    int ApplyBattlePendingCausedCurrentBlockade(NotifyContext* ctx);  // 0x510440
    int ApplyUprisingIncident(NotifyContext* ctx);  // 0x5104e0
    int ApplyInformantIncident(NotifyContext* ctx);  // 0x510580
    int ApplyDisasterIncident(NotifyContext* ctx);  // 0x510620
    int ApplyResourceIncident(NotifyContext* ctx);  // 0x5106c0
    int ApplyBlockadeAndBattlePendingManagementRequired(NotifyContext* ctx);  // 0x510760

    // ---- two more new virtuals (slots 186-187; names provisional) ----
    virtual int Virtual186();                                                               // 186, 0x512f70
    virtual int Virtual187();                                                               // 187, 0x513020

    // ---- persistence/validation: bodies the leaf classes may call qualified (slots 5-9 stay pure) ----
    virtual int Read(istream* in) override = 0;               // 5
    virtual int Write(ostream* out) override = 0;             // 6
    virtual int Checksum(Crc32* crc) override = 0;            // 7
    virtual int Validate() override = 0;                      // 8
    virtual int ApplyAll(NotifyContext* ctx) override = 0;    // 9

    System* ViewAsSystem(int view) { return static_cast<System*>(ViewAs(view)); }

    SystemData* GetData() { return static_cast<SystemData*>(m_data); }

    // ==== SECTION core (0x507130-0x509cc0): non-virtual helpers; see system_core.cpp ====
    SideBits* GetControlData(SideBits* out);  // 0x50fbc0, a copy of m_controlData

    // ---- core accessors (0x507270, 0x508fb0-0x509c30; system_accessors.cpp) ----
    // ---- end of core accessors ----

    // ==== SECTION rules (0x50ac80-0x50e820 and 0x558bb0-0x55a100): see system_rules*.cpp ====

    // ==== SECTION keysetters (0x50e820-0x510800): see system_keysetters.cpp ====

    // ---- fields ----
    int m_loyalty;                  // +0x58, constructed as 50
    int m_energy;                   // +0x5c
    int m_energyAllocated;          // +0x60
    int m_rawMaterial;              // +0x64
    int m_rawMaterialAllocated;     // +0x68
    int m_smugglingPercent;         // +0x6c
    int m_productionModifier;       // +0x70
    int m_troopRegWithdrawPercent;  // +0x74
    uint32_t m_controlBits;         // +0x78, four 2-bit fields (see above)
    int m_troopRegSurplus;          // +0x7c
    int m_troopRegRequired;         // +0x80
    SideBits m_controlData;         // +0x84
    uint32_t m_flags;               // +0x88, the 20 flags of system_flag
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(System, 0x8c);
REB_ASSERT_OFFSET(System, m_loyalty, 0x58);
REB_ASSERT_OFFSET(System, m_troopRegWithdrawPercent, 0x74);
REB_ASSERT_OFFSET(System, m_controlBits, 0x78);
REB_ASSERT_OFFSET(System, m_controlData, 0x84);
REB_ASSERT_OFFSET(System, m_flags, 0x88);
#pragma clang diagnostic pop
