#pragma once
#include <cstdint>

#include "core/layout.h"
#include "fleet/fight_squad.h"
#include "gameobj/child_iterator.h"
#include "gameobj/game_obj.h"
#include "gameobj/side_bits.h"

// System (ctor 0x507130, VT 0x65e640, secondary ObjectBase VT 0x65e63c, 0x8c bytes) is a star system (family 0x90-0x97,
// ids start at 100). It sits directly on GameObj and is abstract: slots 1, 5-9, 37, 57 and 115 stay pure; the leaf
// classes are the type-0x90 class (0x566b90) and the type-0x92 class (0x56a430). Strings: "System*Notif" (OnXNotified),
// "Invalid SystemEnergy value!" etc.
//
// Vtable: 186 slots (0x65e640-0x65e928; the words after it are the SystemIterator and ChildIterator vtables). 0-110 are GameObj's, 111-115 are new non-property virtuals (names provisional), 116-150 are the
// OnXChanged events of the 35 System properties, 151-185 the OnXNotified events (the same order as the fields and the
// Set/Apply functions). The properties:
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
    virtual int Virtual111(const ActivityRequest* request, StatusCode* out);                                                               // 111, 0x507d00
    virtual int Virtual112(const ActivityRequest* request, StatusCode* out);                                                               // 112, 0x507e60
    virtual int Virtual113(const ActivityRequest* request, StatusCode* out);                                                               // 113, 0x507f10
    virtual int Virtual114(const ActivityRequest* request, StatusCode* out);                                                               // 114, 0x507fc0
    virtual int Virtual115(NotifyContext* ctx) = 0;                                                           // 115, pure

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
    int ApplyEnergyAllocated(NotifyContext* ctx) { return reinterpret_cast<FightSquad*>(this)->ApplySquadSizeDamage(ctx); }  // 0x5037f0, folded with an unrelated class's function
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

    // ---- persistence/validation: bodies the leaf classes may call qualified (slots 5-9 stay pure) ----
    virtual int Read(istream* in) override = 0;               // 5, body 0x50e410
    virtual int Write(ostream* out) override = 0;             // 6, body 0x50e4e0
    virtual int Checksum(Crc32* crc) override = 0;            // 7, body 0x50e730
    virtual int Validate() override = 0;                      // 8
    virtual int ApplyAll(NotifyContext* ctx) override = 0;    // 9, body 0x50e5b0

    System* ViewAsSystem(int view) { return static_cast<System*>(ViewAs(view)); }

    SystemData* GetData() { return static_cast<SystemData*>(m_data); }

    // ==== SECTION core (0x507130-0x509cc0): non-virtual helpers; see system_core.cpp ====
    SideBits* GetControlData(SideBits* out);  // 0x50fbc0, a copy of m_controlData

    // ---- core accessors (0x507270, 0x508fb0-0x509c30; system_accessors.cpp) ----
    int GetLoyaltyFor(int side);  // 0x507270, side 1 -> m_loyalty, else 100 - m_loyalty
    int CountFighterSquadrons(uint32_t side, uint32_t stateFilter);  // 0x508fb0
    int CountTroopRegiments(uint32_t side, uint32_t stateFilter);  // 0x509020
    int CountCapitalShips(uint32_t side, uint32_t stateFilter);  // 0x509090, over the fleets there
    int CountDeathStars(uint32_t side, uint32_t stateFilter);  // 0x509140, over the fleets there
    int CountEmpireRegimentsOfKey6();  // 0x5091f0, usable Empire regiments whose record key is 0x10000006
    TrackedStat* GetStat0();  // 0x5092b0, m_data->m_stats[0] or null
    TrackedStat* GetStat3();  // 0x5092c0
    TrackedStat* GetStat4();  // 0x5092d0
    TrackedStat* GetStat5();  // 0x5092e0
    uint32_t GetDataFlagBit0();  // 0x5092f0, m_data->m_flags80 & 1 (0 without data)
    uint32_t GetDataFlagBit3();  // 0x509310, m_data->m_flags80 & 8 (1 without data)
    int FindCharacterByCommandKind(int kind, GameObjNotifier** out);  // 0x509330, always 1
    int FindCharacterUnused(GameObjNotifier** out, int unused1, int unused2);  // 0x5093e0, clears *out and walks the characters for nothing (RET 0xc)
    int FindBestShipOfCommandKind2(uint32_t side, GameObjNotifier** out);  // 0x509460
    int FindBestShipOfCommandKind1(uint32_t side, GameObjNotifier** out);  // 0x509530
    uint32_t GetRecordField44();  // 0x509610
    uint32_t* GetRecordField4c(uint32_t* out);  // 0x509620, returns `out`
    int GetEnergy();  // 0x509650
    int GetRawMaterial();  // 0x509660
    GameObjNotifier* FindManufacturingManagerBySlot123(int value);  // 0x509670
    int HasFleetsOrFighterSquadrons(uint32_t side);  // 0x509710
    int FindFleetWithFlag02(uint32_t side, GameObjNotifier** out);  // 0x5097d0, always 1
    int GetBlockadingSide(uint32_t* out);  // 0x509890, side of the first fleet while blockaded
    int GetMoreLoyalSide();  // 0x509930, 1 Alliance, 2 Empire, 3 equal
    int IsBlockaderMoreLoyal(uint32_t* out);  // 0x509980
    int GetDataWord48(int side);  // 0x5099c0
    int GetDataWord54(int side);  // 0x5099f0
    int SumWords48And54(int side);  // 0x509a20
    int GetDataWord60();  // 0x509ac0
    int GetDataWord64();  // 0x509ad0
    int GetDataWord68(int side);  // 0x509ae0
    int GetDataWord74();  // 0x509b10
    int GetDataWord78();  // 0x509b20
    int GetDataWord7c();  // 0x509b30
    int FindFleetWithFlag04(uint32_t side, GameObjNotifier** out);  // 0x509b40, always 1
    int SetLoyaltyFor(int value, int side, NotifyContext* ctx);  // 0x509c00
    // ---- end of core accessors ----

    // ==== SECTION rules (0x50ac80-0x50e820 and 0x558bb0-0x55a100): see system_rules*.cpp ====
    // Recompute*: derive a property (or the state of the units in the system) from the current situation and run the
    // validated setter / the unit's setter; they return the setters' result (1 = ok). Names are provisional.
    int RecomputeObservation(NotifyContext* ctx);                // 0x50ac80, who observes the system
    int RecomputeRawMaterial(NotifyContext* ctx);                // 0x50ad60, raw material capped to energy
    int RecomputeEnergyAllocated(NotifyContext* ctx);            // 0x50ad80, scraps facilities beyond the energy
    int RecomputeRawMaterialAllocated(NotifyContext* ctx);       // 0x50b0b0
    int RecomputeSmugglingPercent(NotifyContext* ctx);           // 0x50b230
    int RecomputeProductionModifier(NotifyContext* ctx);         // 0x50b2c0
    int RecomputeTroopRegWithdrawPercent(NotifyContext* ctx);    // 0x50b310
    int RecomputeControlKindUprising(NotifyContext* ctx);        // 0x50b4d0
    int RecomputeTroopRegSurplus(NotifyContext* ctx);            // 0x50b500
    int RecomputeTroopRegRequired(NotifyContext* ctx);           // 0x50b5a0
    int RecomputeControlData(NotifyContext* ctx);                // 0x50b610
    int RecomputePopulated(NotifyContext* ctx);                  // 0x50b760
    int RecomputeUprising(NotifyContext* ctx);                   // 0x50b800
    int RecomputeNeverBeenControlled(NotifyContext* ctx);        // 0x50b890
    int RecomputeBlockadeAndBattlePending(NotifyContext* ctx);   // 0x50b8e0
    int RecomputeGarrisoned(NotifyContext* ctx);                 // 0x50ba90
    int RecomputeSuppressing(NotifyContext* ctx);                // 0x50bb00
    int RecomputeCombatUnitFastRepair(NotifyContext* ctx);       // 0x50bb70
    int RecomputeFacilityBattleFlags(NotifyContext* ctx);        // 0x50bbb0
    int RecomputeFleetExploredFlags(NotifyContext* ctx);         // 0x50bc60
    int RecomputeFleetEmptyFlags(NotifyContext* ctx);            // 0x50be00
    int RecomputeFleetBattleFlags(NotifyContext* ctx);           // 0x50bfe0
    int RecomputeFleetBlockadeFlags(NotifyContext* ctx);         // 0x50c0b0
    int RefreshSiteKind0(NotifyContext* ctx);                    // 0x50c180
    int RefreshSiteKind2(NotifyContext* ctx);                    // 0x50c1b0
    int RefreshSiteKind1(NotifyContext* ctx);                    // 0x50c1e0
    int RecomputeTroopWithdrawPercents(NotifyContext* ctx);      // 0x50c210
    int RecomputeProductionModifiers(NotifyContext* ctx);        // 0x50c2b0
    int RecomputeFastRepair(NotifyContext* ctx);                 // 0x50c350
    int PushTroopWithdrawPercent(GameObjNotifier* troops, NotifyContext* ctx);   // 0x50c540
    int PushProductionModifier(GameObjNotifier* facility, NotifyContext* ctx);   // 0x50c560
    int PushFastRepair(GameObjNotifier* unit, NotifyContext* ctx);               // 0x50c580
    int NoteWithdrawingUnit(GameObjNotifier* unit, int mustBeIdle, NotifyContext* ctx);  // 0x50c6c0
    int CheckHyperspaceBlocked(int sideA, int sideB, NotifyContext* ctx);        // 0x50c820
    int EndUprisingIfQuelled(NotifyContext* ctx);                // 0x50c910
    int AdjustLoyalty(int delta, int side, NotifyContext* ctx);  // 0x50c9f0, shifts the loyalty toward `side`
    int EndUprising(NotifyContext* ctx);                         // 0x50ca80
    int AdjustLoyaltyForUprising(NotifyContext* ctx);            // 0x50caa0
    void AdjustLoyaltyForAssault(NotifyContext* ctx);            // 0x50cad0
    void AdjustLoyaltyForBlockade(NotifyContext* ctx);           // 0x50cb20
    int PulseUprisingIncident(NotifyContext* ctx);               // 0x50cb80
    int RollInformantIncident(NotifyContext* ctx);               // 0x50cbe0
    int RollResourceIncident(NotifyContext* ctx);                // 0x50cc70
    int PulseDisasterIncident(NotifyContext* ctx);               // 0x50cdc0
    // Stores into the SystemData block (m_data).
    int SetData48(int value, int side);  // 0x50ce20, m_word48[side - 1] for sides 1-3; returns whether side is 1-3
    int SetData54(int value, int side);  // 0x50ce50
    int SetData60(int side);              // 0x50ce80, sides 1 and 2 only; returns whether it stored
    int SetData64(int side);              // 0x50ceb0, 0-3
    int SetData68(int value, int side);  // 0x50cee0
    int SetData74(int value);             // 0x50cf10
    int SetData78(int value);             // 0x50cf30
    int SetData7c(int value);             // 0x50cf50
    int SetDataFlag1(int on);             // 0x50cf70, bits of the data block's m_flags80
    int SetDataFlag2(int on);             // 0x50cfa0
    int SetDataFlag4(int on);             // 0x50cfd0
    int SetDataFlag8(int on);             // 0x50d000
    int ResolveUprisingTurn(NotifyContext* ctx);                 // 0x50d030, 1 for a side without uprising
    int ApplyUprisingEvent(int event, int side, NotifyContext* ctx);  // 0x50d150
    int RollAndSyncEvent(NotifyContext* ctx);                    // 0x50d510
    int SyncViewForEvent(int mode, int view, NotifyContext* ctx);  // 0x50d5a0
    // The bodies of the persistence and view-creation slots (the leaf classes' slot functions call them qualified).
    virtual int CreateViews(int controlKind, NotifyContext* ctx) override = 0;  // 37, body 0x50e200

    // ==== SECTION keysetters (0x50e820-0x510800): see system_keysetters.cpp ====
    // The four static event-kind sets (by the type bits the callers pass: 4/5, 8/9, 0x10/0x11, 0x40/0x41) are built on
    // first use. True if they are built.
    // Creates the fleet 0x08000004 under this system, flagged as arrived (SetFlag02) or not (SetFlag04); the fleet's result.
    int CreateDefaultFleet(int initArg, int arrived, NotifyContext* ctx);  // 0x512d00
    static int BuildEventKinds();  // 0x50f0b0
    // Points *out at the set for `type` (its bit 0 is ignored); 0 (and *out null) for an unknown type.
    static int SelectEventKinds(uint32_t type, EventKindSet** out);  // 0x50f2e0
    static void InitStatics4();   // 0x512db0, constructs g_systemEventKinds4 and registers its destructor
    static void InitStatics8();   // 0x512e00
    static void InitStatics10();  // 0x512e50
    static void InitStatics40();  // 0x512ea0

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

// Re-sends the commanding characters of the system's master (and the stale ones of the side's view) to the view (a
// plain stdcall function in the original).
int __stdcall ReapplySystemCharacterViews(GameObj* object, NotifyContext* ctx);  // 0x50eea0

// Iterator over the systems (families 0x90-0x97) under a parent, vtable 0x65e928 (only the destructor is virtual).
class SystemIterator : public ChildIterator {
public:
    SystemIterator(GameObjNotifier* parent);                         // 0x512ef0, state filter 1
    SystemIterator(GameObjNotifier* parent, uint32_t stateFilter);   // 0x512f90
    virtual ~SystemIterator();                                       // 0x512f70 (deleting), 0x513010
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(System, 0x8c);
REB_ASSERT_SIZE(SystemIterator, 0x20);
REB_ASSERT_OFFSET(System, m_loyalty, 0x58);
REB_ASSERT_OFFSET(System, m_troopRegWithdrawPercent, 0x74);
REB_ASSERT_OFFSET(System, m_controlBits, 0x78);
REB_ASSERT_OFFSET(System, m_controlData, 0x84);
REB_ASSERT_OFFSET(System, m_flags, 0x88);
#pragma clang diagnostic pop
