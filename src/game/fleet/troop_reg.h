#pragma once
#include "data/sd_records.h"
#include "fleet/detector.h"
#include "gameobj/child_iterator.h"
#include "gameobj/object_family.h"

// TroopReg (0x504150, VT 0x65e438, secondary ObjectBase VT 0x65e430, 0x64 bytes): a troop regiment (families 0x10-0x13,
// record TroopSD). Over Detector it adds the DestroyedRunningBlockade flag (+0x5c bit 0) and WithdrawPercent (+0x60,
// 0-100, 100 when the regiment becomes usable). When a regiment starts to move and fails its WithdrawPercent roll it is
// marked DestroyedRunningBlockade, which destroys it (reason 0xd).
class TroopReg : public Detector
{
public:
  static constexpr uint32_t kDestroyedRunningBlockadeFlag = 0x1;

  TroopReg(); // 0x504150
  ~TroopReg() override; // 0x5041a0 (scalar deleting destructor 0x504180)

  uint32_t GetFamily() override
  {
    return family::kTroopRegiment;
  } // 1, shared "return 0x10" stub 0x48af30
  int Read(istream* in) override; // 5, 0x504640
  int Write(ostream* out) override; // 6, 0x504650
  int Checksum(Crc32* crc) override; // 7, 0x504670
  int Validate() override; // 8, 0x504790 (0x504680 jumps here)
  int ApplyAll(NotifyContext* ctx) override; // 9, 0x504660
  int GetEventKinds(int unused, EventKindSet* out) override; // 15, 0x5041c0
  int CheckOrder(int a, int orderKind, StatusCode* out) override; // 22, 0x5041e0
  int CheckRequestImpl(const ActivityRequest* request, StatusCode* out) override; // 23, 0x5042a0
  int CheckUnknownImpl(const ActivityRequest* request, StatusCode* out) override; // 27, 0x504350
  int CreateViews(int controlKind, NotifyContext* ctx) override; // 37, 0x55ac90 (wrapper shared with other classes)
  GameObj* CreateViewObject() override; // 57, 0x5045d0
  int OnUsableChanged(int newValue, NotifyContext* ctx) override; // 68, 0x504960
  int OnEnrouteActiveChanged(int newValue, NotifyContext* ctx) override; // 73, 0x504990
  int GetEncounterCommandKind() override
  {
    return 3;
  } // 111, shared "return 3" stub

  // Record fields +0x60..+0x68 (0 without a record).
  virtual uint32_t GetBombardmentDefense()
  {
    return m_record ? static_cast<TroopSD*>(m_record)->m_bombardmentDefense : 0;
  } // 118, body folded into 0x520b80
  virtual uint32_t GetAttackStrength()
  {
    return m_record ? static_cast<TroopSD*>(m_record)->m_attackStrength : 0;
  } // 119, 0x520b90
  virtual uint32_t GetDefenseStrength()
  {
    return m_record ? static_cast<TroopSD*>(m_record)->m_defenseStrength : 0;
  } // 120, 0x520ba0
  virtual int GetBombardmentDefenseLoss()
  {
    return 0;
  } // 121, shared "return 0" stub
  virtual int GetAttackStrengthLoss()
  {
    return 0;
  } // 122
  virtual int GetDefenseStrengthLoss()
  {
    return 0;
  } // 123
  virtual int
  OnDestroyedRunningBlockadeChanged(int newValue,
                                    NotifyContext* ctx); // 124, 0x5049e0: destroys the regiment (reason 0xd)
  virtual int OnWithdrawPercentChanged(int oldValue, int newValue, NotifyContext* ctx)
  {
    return 1;
  } // 125, shared "return 1" stub
  virtual int OnDestroyedRunningBlockadeNotified(int value, NotifyContext* ctx); // 126, 0x504a00
  virtual int OnWithdrawPercentNotified(int oldValue, int newValue, NotifyContext* ctx); // 127, 0x504af0

  // Master view only. Return IsMasterView() / the range check's result.
  int SetDestroyedRunningBlockade(int on, NotifyContext* ctx); // 0x504400
  int SetWithdrawPercent(int value, NotifyContext* ctx); // 0x504470, "TroopReg.cpp: Invalid WithdrawPercent value!"
  int ApplyDestroyedRunningBlockade(NotifyContext* ctx); // 0x504850
  int ApplyWithdrawPercent(NotifyContext* ctx); // 0x5048e0

  // Bodies of the slot 5-9 wrappers.
  int ReadState(istream* in); // 0x504690
  int WriteState(ostream* out); // 0x5046d0
  int ApplyAllState(NotifyContext* ctx); // 0x504710
  int ChecksumState(Crc32* crc); // 0x504750

  static int EnsureEventKinds(); // 0x504b30

  uint32_t m_runningBlockadeFlags; // +0x5c, bit 0 DestroyedRunningBlockade
  int m_withdrawPercent; // +0x60
};

// Troop regiments directly under `parent` (families 0x10-0x13) matching the side and state filter (0x5044f0).
int CountTroopRegs(GameObj* parent, uint32_t side, uint32_t filter);

// The factory the object registry uses (0x504560): a new regiment or null.
GameObj* CreateTroopReg();

// Iterator over the troop regiment families (VT 0x65e638).
class TroopRegIterator : public ChildIterator
{
public:
  TroopRegIterator(GameObjNotifier* parent, uint32_t stateFilter); // 0x504c40
  TroopRegIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side); // 0x504cc0
  ~TroopRegIterator() override; // 0x504d40 (scalar deleting destructor 0x504c20)
};
