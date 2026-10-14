#pragma once
#include "data/sd_records.h"
#include "fleet/combat_unit.h"
#include "gameobj/child_iterator.h"

// FightSquad (0x502ec0, VT 0x65def0, secondary ObjectBase VT 0x65dee8, 0x64 bytes): a fighter squadron (family 0x1c-0x1f,
// record FighterSD). Over CombatUnit it adds one saved number, SquadSizeDamage (+0x60: fighters lost, 0..squadron size),
// and 11 slots (143-153). The squadron is destroyed when every fighter is lost. The "current" strengths of CombatUnit's
// slots 120-128 are the record values times the squadron size; 129-136 are the same times the fighters lost.
class FightSquad : public CombatUnit
{
public:
  FightSquad(); // 0x502ec0
  ~FightSquad() override; // 0x502f00 (scalar deleting destructor 0x502ee0)

  uint32_t GetFamily() override; // 1, 0x503500
  int Read(istream* in) override; // 5, 0x5034a0
  int Write(ostream* out) override; // 6, 0x5034b0
  int Checksum(Crc32* crc) override; // 7, 0x5034e0
  int Validate() override; // 8, 0x5036f0 (0x5034f0 jumps here)
  int ApplyAll(NotifyContext* ctx) override; // 9, 0x5034c0
  int GetTransitSpeed(int arg) override; // 13, 0x502f80
  int GetEventKinds(int unused, EventKindSet* out) override; // 15, 0x502fe0
  int CheckOrder(int a, int orderKind, StatusCode* out) override; // 22, 0x503000
  int CreateViews(int controlKind, NotifyContext* ctx) override; // 37, 0x500140 (wrapper shared with the capital ships)
  int HandleControlMismatch(NotifyContext* ctx) override; // 44, 0x503510: autoroute unless destroyed
  GameObj* CreateViewObject() override; // 57, 0x503430
  int OnParentChanged(GameObjNotifier* oldParent,
                      GameObjNotifier* newParent,
                      NotifyContext* ctx) override; // 61, 0x503870
  int OnUsableChanged(int newValue, NotifyContext* ctx) override; // 68, 0x503890
  int GetEncounterCommandKind() override
  {
    return 1;
  } // 111, shared "return 1" stub
  int GetRepairLevel() override; // 118, 0x503140
  int GetDefenseLeft() override; // 119, 0x5032a0: bombardment defense minus the share lost
  uint32_t GetField60() override; // 120, 0x503160
  uint32_t GetGridColumnTotal(int column) override; // 126, 0x5031b0
  uint32_t GetGridValue(int row, int column) override; // 127, 0x503180
  uint32_t GetGridTotal() override; // 125, 0x5031e0
  uint32_t GetDamageField60() override; // 129, 0x503200
  uint32_t GetDamageTotal() override; // 134, 0x503280
  uint32_t GetDamageColumnTotal(int column) override; // 135, 0x503250
  uint32_t GetDamageGridValue(int row, int column) override; // 136, 0x503220
  int RepairStep(NotifyContext* ctx) override; // 138, 0x503530: repairs one fighter

  virtual int GetTotalTorpedoes(); // 143, 0x5030c0: torpedoes per fighter times the squadron size
  virtual uint32_t GetTorpedoRange()
  {
    return m_record ? static_cast<FighterSD*>(m_record)->m_torpedoRange : 0;
  } // 144 (record +0xc4; body folded into 0x500570)
  virtual uint32_t GetSquadSize(); // 145, 0x502f40 (0x500dd0 jumps here)
  virtual uint32_t GetBombardmentDefense(); // 146, 0x502f60 (0x500de0 jumps here)
  virtual int GetTorpedoesLost(); // 147, 0x5030e0: torpedoes per fighter times the fighters lost
  // The original meaning of slot 148 is unknown; no callers were found.
  virtual int UnreconstructedSquadronLoss()
  {
    return 0;
  } // 148, shared "return 0" stub
  virtual int GetSquadSizeDamage(); // 149, 0x503100
  virtual int GetBombardmentDefenseLost(); // 150, 0x503110
  virtual int UpdateDamaged(NotifyContext* ctx); // 151, 0x503340: Damaged = any fighter lost
  virtual int OnSquadSizeDamageChanged(int oldValue, int newValue, NotifyContext* ctx); // 152, 0x5038b0
  virtual int OnSquadSizeDamageNotified(int oldValue, int newValue, NotifyContext* ctx); // 153, 0x5038e0

  uint32_t GetRecordTorpedoes(); // 0x502f20 (0 without a record)

  // Master view only; the value must be 0..GetSquadSize() ("Invalid SquadSizeDamage value"). Returns the check's result.
  int SetSquadSizeDamage(int value, NotifyContext* ctx); // 0x5032c0
  int ApplySquadSizeDamage(NotifyContext* ctx); // 0x5037f0
  // Adds `amount` (>= 0) fighters lost, clamped to the squadron size; when none is left the squadron is destroyed with
  // `destroyedReason`.
  int InflictSquadSizeDamage(int amount, int destroyedReason, NotifyContext* ctx); // 0x503370
  // Takes `amount` (>= 0) off the fighters lost, down to 0.
  int RepairSquadSizeDamage(int amount, NotifyContext* ctx); // 0x503400

  // Bodies of the slot 5-9 wrappers.
  int ReadState(istream* in); // 0x503630
  int WriteState(ostream* out); // 0x503660
  int ApplyAllState(NotifyContext* ctx); // 0x503690
  int ChecksumState(Crc32* crc); // 0x5036c0

  static int EnsureEventKinds(); // 0x503930

  int m_squadSizeDamage; // +0x60
};

// Fighter squadrons directly under `parent` (families 0x1c-0x1f) matching the side and state filter (0x503550).
int CountFightSquads(GameObj* parent, uint32_t side, uint32_t filter);

// The factory the object registry uses (0x5035c0): a new squadron or null.
GameObj* CreateFightSquad();

// Iterator over the fighter squadron families (VT 0x65e158).
class FightSquadIterator : public ChildIterator
{
public:
  FightSquadIterator(GameObjNotifier* parent, uint32_t stateFilter); // 0x5039d0
  FightSquadIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side); // 0x503a50
  ~FightSquadIterator() override; // 0x503ad0 (scalar deleting destructor 0x5039b0)
};
