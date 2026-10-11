#pragma once
#include "fleet/combat_unit.h"
#include "gameobj/child_iterator.h"

// CapShip (capshp.cpp, ctor 0x5004e0, VT 0x65dc10, secondary ObjectBase VT 0x65dc08, 0x68 bytes): the strategic capital ship.
// Over CombatUnit it adds the hull damage taken (HullValueDamage) and six "critical hit" allocations (the CH values: how
// many hits each subsystem has taken), and 38 vtable slots (143-180).
//
// Hull damage and the CH values reduce what the SD record (CAPSHPSD.DAT, CapitalShipSD) says the ship is worth: the
// "Damage" slots 129-136 (and 158-165) return the lost part of a record value, e.g. for the primary hyperdrive
// ScaleRatio(value, CH, 1), for the others value * (max - CH) / max * (hull - damage) / hull subtracted from the value.
//
// Like its bases CapShip is abstract: slots 1, 5-9, 37 and 57 stay pure and have non-virtual bodies the two leaf classes
// (CapitalShip 0x14, DeathStar 0x18, in capship_subclasses.h) call: Read -> ReadState, Write -> WriteState,
// Checksum -> ChecksumState, ApplyAll -> ApplyAllState, CreateViews -> CreateViewsState, Validate -> ValidateState.
class CapShip : public CombatUnit
{
public:
  static constexpr int kShieldRechargeShift = 0;
  static constexpr int kWeaponRechargeShift = 4;
  static constexpr int kTractorBeamShift = 8;
  static constexpr int kSpeedShift = 12;
  static constexpr int kPrimaryHyperdriveShift = 16;
  static constexpr int kBackupHyperdriveShift = 20;

  CapShip(); // 0x5004e0
  ~CapShip() override; // 0x500530 (scalar deleting destructor 0x500510)

  int GetTransitSpeed(int arg) override; // 13, 0x500820
  int GetEventKinds(int unused, EventKindSet* out) override; // 15, 0x500880
  int Virtual18() override
  {
    return 1;
  } // 18, shared "return 1" stub
  int CheckOrder(int a, int orderKind, StatusCode* out) override; // 22, 0x5008a0
  int CheckRenameImpl(const ActivityRequest* request, StatusCode* out) override; // 25, 0x500960
  int Virtual29(int a, int b, int c, intptr_t d, StatusCode* out) override; // 29, 0x500ac0
  int Virtual30(const FamilyRange* range, int b, int* o1, int* o2, int* o3, StatusCode* out) override; // 30, 0x500b40
  int CreateViews(int controlKind, NotifyContext* ctx) override = 0; // 37, body 0x500140
  GameObjData* CreateData() override; // 58, 0x502c50
  int OnParentChanged(GameObjNotifier* oldParent,
                      GameObjNotifier* newParent,
                      NotifyContext* ctx) override; // 61, 0x502800
  int OnUsableChanged(int newValue, NotifyContext* ctx) override; // 68, 0x502820
  int OnDestroyedChanged(int newValue, NotifyContext* ctx) override; // 71, 0x502840
  int Virtual111() override
  {
    return 2;
  } // 111, shared "return 2" stub

  int Read(istream* in) override = 0; // body 0x500160
  int Write(ostream* out) override = 0; // body 0x500390
  int Checksum(Crc32* crc) override = 0; // body 0x500170
  int Validate() override = 0; // body 0x500180 (a jump to ValidateState)
  int ApplyAll(NotifyContext* ctx) override = 0; // body 0x5003a0

  int GetRepairLevel() override; // 118, 0x5010a0: GNPRTB 0x804 / 0x805
  int GetDefenseLeft() override; // 119, 0x501470: bombardment modifier minus its lost part
  uint32_t GetDamageField60() override; // 129, 0x501160 (shield recharge CH)
  uint32_t GetDamageField64() override; // 130, 0x5010c0 (speed CH)
  uint32_t GetDamageField6c() override; // 132, 0x5011f0 (primary hyperdrive CH)
  uint32_t GetDamageField70() override; // 133, 0x501230 (backup hyperdrive CH)
  uint32_t GetDamageTotal() override; // 134, 0x5013d0 (weapon recharge CH)
  uint32_t GetDamageColumnTotal(int column) override; // 135, 0x501320 (weapon recharge CH)
  uint32_t GetDamageGridValue(int row, int column) override; // 136, 0x501270 (weapon recharge CH)
  int RepairStep(NotifyContext* ctx) override; // 138, 0x501c50

  virtual int CountFighterSquadrons(); // 143, 0x500fe0
  virtual int CountTroopRegiments(); // 144, 0x501040
  virtual int CheckRequestImpl2(const ActivityRequest* request,
                                StatusCode* out); // 145, 0x500a10 (same as CombatUnit's CheckRequestImpl)
  // Record fields of CapitalShipSD (0 without a record), slots 146-156.
  virtual int GetHull(); // 146, 0x500550 via stub 0x500db0
  virtual int GetTractorBeamPower(); // 147, 0x500570 via 0x500dc0
  virtual int GetTractorBeamRange(); // 148, 0x502f40 via 0x500dd0 (shared)
  virtual int GetGravityWellProjector(); // 149, 0x502f60 via 0x500de0 (shared)
  virtual int GetInterdictionStrength(); // 150, 0x500590 via 0x500df0
  virtual int GetBombardmentModifier(); // 151, 0x5005b0 via 0x500e00
  virtual int GetDamageControl(); // 152, 0x5005d0 via 0x500e10
  virtual int GetWeaponRechargeRate(); // 153, 0x5005f0 via 0x500e20
  virtual int GetShieldRechargeRate(); // 154, 0x500610 via 0x500e30
  virtual int GetFighterCapacity(); // 155, 0x500630 via 0x500e40
  virtual int GetTroopCapacity(); // 156, 0x500650 via 0x500e50
  virtual int GetHullValueDamage()
  {
    return m_hullValueDamage;
  } // 157, 0x503100 (shared with FightSquad)
  virtual int GetTractorBeamPowerLoss(); // 158, 0x500e60
  virtual int GetTractorBeamRangeLoss()
  {
    return 0;
  } // 159, shared "return 0" stub
  virtual int GetGravityWellProjectorLoss()
  {
    return 0;
  } // 160
  virtual int GetInterdictionStrengthLoss()
  {
    return 0;
  } // 161
  virtual int GetBombardmentModifierLoss(); // 162, 0x500f00
  virtual int GetDamageControlLoss()
  {
    return 0;
  } // 163
  virtual int GetWeaponRechargeRateLoss(); // 164, 0x500f60
  virtual int GetShieldRechargeRateLoss(); // 165, 0x500fa0
  // Sets Damaged from the hull damage and the CH values (cleared only if there is neither).
  virtual int UpdateDamaged(NotifyContext* ctx); // 166, 0x501c00
  virtual int OnHullValueDamageChanged(int oldValue, int newValue, NotifyContext* ctx); // 167, 0x502930
  virtual int
  OnShieldRechargeRateCHChanged(int oldValue, int newValue, NotifyContext* ctx); // 168, 0x502980 (shared by 168-173)
  virtual int OnWeaponRechargeRateCHChanged(int oldValue, int newValue, NotifyContext* ctx); // 169
  virtual int OnTractorBeamPowerCHChanged(int oldValue, int newValue, NotifyContext* ctx); // 170
  virtual int OnSpeedCHChanged(int oldValue, int newValue, NotifyContext* ctx); // 171
  virtual int OnPrimaryHyperdriveCHChanged(int oldValue, int newValue, NotifyContext* ctx); // 172
  virtual int OnBackupHyperdriveCHChanged(int oldValue, int newValue, NotifyContext* ctx); // 173
  virtual int OnHullValueDamageNotified(int oldValue, int newValue, NotifyContext* ctx); // 174, 0x5029a0
  virtual int OnShieldRechargeRateCHNotified(int oldValue, int newValue, NotifyContext* ctx); // 175, 0x5029f0
  virtual int OnWeaponRechargeRateCHNotified(int oldValue, int newValue, NotifyContext* ctx); // 176, 0x502a40
  virtual int OnTractorBeamPowerCHNotified(int oldValue, int newValue, NotifyContext* ctx); // 177, 0x502a90
  virtual int OnSpeedCHNotified(int oldValue, int newValue, NotifyContext* ctx); // 178, 0x502ae0
  virtual int OnPrimaryHyperdriveCHNotified(int oldValue, int newValue, NotifyContext* ctx); // 179, 0x502b30
  virtual int OnBackupHyperdriveCHNotified(int oldValue, int newValue, NotifyContext* ctx); // 180, 0x502b80

  // GNPRTB 0x800: the largest CH value a subsystem can take.
  static int GetMaxCriticalHits(); // 0x500670 (takes a dummy this)
  int HasPrimaryHyperdrive(); // 0x500680: record field6c != 0
  int HasBackupHyperdrive(); // 0x500690: record field70 != 0
  uint32_t GetCriticalHit(int shift) const
  {
    return (m_criticalHits >> shift) & 0xf;
  }
  // value - value * (max - CH) / max * (hull - damage) / hull: the part of a record value lost to CH and hull damage.
  int LostShare(int base, int criticalHits);
  // The tail of ValidateState: parent, locked children and the fighter/troop capacity.
  int ValidateParentAndCargo();
  // The sum of the six CH values.
  int GetTotalCriticalHits(); // 0x5006a0
  // CapShipData word at +0x20 (0 without a data block).
  int GetDataWord20(); // 0x5006e0
  void SetDataWord20(int value); // 0x501ba0
  void SetDataWord24(int value); // 0x501bc0

  // The character aboard whose record id (Character +0x96) is `id`, or null; always returns 1.
  int FindCharacterById(int id, GameObjNotifier** out); // 0x5006f0
  // Leftover of a removed character count: sets *out to 0, returns 1.
  int CountCharacters(int* out, int unused1, int unused2); // 0x5007a0

  // Master view only: validated setters. They return the validation result (IsMasterView() and in range).
  int SetHullValueDamage(int value, NotifyContext* ctx); // 0x501490
  int SetShieldRechargeRateCH(uint32_t value, NotifyContext* ctx); // 0x501510
  int SetWeaponRechargeRateCH(uint32_t value, NotifyContext* ctx); // 0x5015a0
  int SetTractorBeamPowerCH(uint32_t value, NotifyContext* ctx); // 0x501640
  int SetSpeedCH(uint32_t value, NotifyContext* ctx); // 0x5016e0
  int SetPrimaryHyperdriveCH(uint32_t value, NotifyContext* ctx); // 0x501780
  int SetBackupHyperdriveCH(uint32_t value, NotifyContext* ctx); // 0x501820
  // Adds `amount` to the hull damage (capped at the hull); at full damage the ship is destroyed with `destroyedReason`.
  int DamageHull(int amount, int destroyedReason, NotifyContext* ctx); // 0x501ae0
  int RepairHullDamage(int amount, NotifyContext* ctx); // 0x501b70

  // Rolls the kind of critical hit that `damage` hull points cause and applies it: type 2-6 raise one CH value (type 6:
  // the primary hyperdrive, else the backup), type 7 destroys the ship with `destroyedReason`.
  int InflictCriticalHit(int damage, int destroyedReason, NotifyContext* ctx); // 0x5018c0
  void SetDataWord28(int value); // 0x501be0

  // Side views: copy from the master and raise the OnXNotified event.
  int ApplyHullValueDamage(NotifyContext* ctx); // 0x5023b0
  int ApplyShieldRechargeRateCH(NotifyContext* ctx); // 0x502430
  int ApplyWeaponRechargeRateCH(NotifyContext* ctx); // 0x5024c0
  int ApplyTractorBeamPowerCH(NotifyContext* ctx); // 0x502560
  int ApplySpeedCH(NotifyContext* ctx); // 0x502600
  int ApplyPrimaryHyperdriveCH(NotifyContext* ctx); // 0x5026a0
  int ApplyBackupHyperdriveCH(NotifyContext* ctx); // 0x502750

  // Bodies of the pure slots (called by the leaf classes' overrides, through the thin slot wrappers above).
  int CreateViewsState(int controlKind, NotifyContext* ctx); // 0x501ee0
  int ReadState(istream* in); // 0x501f00
  int WriteState(ostream* out); // 0x501f40
  int ApplyAllState(NotifyContext* ctx); // 0x501f80
  int ChecksumState(Crc32* crc); // 0x501fe0
  int ValidateState(); // 0x502020

  // Builds the static kind set on first use (flag at 0x6b2b20): CombatUnit's plus 0x203 and 0x270.
  static int EnsureEventKinds(); // 0x502bd0

  int m_hullValueDamage; // +0x60
  uint32_t
    m_criticalHits; // +0x64, six 4-bit CH values (shield, weapon, tractor beam, speed, primary and backup hyperdrive)
};

// Iterators over capital-ship families (own vtable with just the destructor): [0x14,0x1c), the two leaf ranges.
class CapShipIterator : public ChildIterator
{
public:
  CapShipIterator(GameObjNotifier* parent); // 0x502d10 (state filter 1)
  CapShipIterator(GameObjNotifier* parent, uint32_t stateFilter); // 0x502db0
  CapShipIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side); // 0x502e30
  ~CapShipIterator() override; // 0x502eb0 (scalar deleting destructor 0x502d90)
};

// Rolls what a hit of `damage` against a ship with `hullLeft` hull points left does (GNPRTB 0x801-0x803): *hitType is 1
// (nothing), 2-6 (a subsystem; looked up in table 5) or 7 (destroyed). Returns 0 only if the table lookup fails.
int RollCriticalHitType(int hullLeft, int damage, int* hitType); // 0x5585e0

// Number of capital ships (families 0x14-0x1b) below `parent` that pass the side and state filters.
int CountCapitalShipChildren(GameObj* parent, uint32_t side, uint32_t filter); // 0x501e70
