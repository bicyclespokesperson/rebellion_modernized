#pragma once
#include "gameobj/unit_obj_base.h"

// Detector (0x557e60, VT 0x662558, secondary ObjectBase VT 0x662554, 0x5c bytes): the part of troop regiments, capital
// ships and fighter squadrons that can be fooled by decoys. Its parent class is UnitObjBase (the unit objects of
// families 0x10-0x3f); CombatUnit (ships and squadrons) and TroopReg derive from it. It adds one state flag, IsDecoyed,
// the static event-kind set shared by its subclasses, and slots 112-117; its record is an SdRecordExt2 (fields
// +0x58/+0x5c).
//
// Like GameObj, Detector leaves slots 5-9 pure and supplies non-virtual bodies (Read, Write, Checksum, Validate,
// ApplyAll) that the leaf classes call qualified. It adds no CreateViews body (TroopReg and CombatUnit bring their
// own).
class Detector : public UnitObjBase
{
  public:
  static constexpr uint32_t kIsDecoyedFlag = 0x1;

  Detector();           // 0x557e60
  ~Detector() override; // 0x557ea0 (scalar deleting destructor 0x557e80)

  int GetEventKinds(int unused, EventKindSet* out) override;                      // 15, 0x557ec0
  int CheckOrder(int a, int orderKind, StatusCode* out) override;                 // 22, 0x557ee0
  int CheckRequestImpl(const ActivityRequest* request, StatusCode* out) override; // 23, 0x557fa0
  GameObjData* CreateData() override;                                             // 58, 0x5583a0
  int OnParentChanged(GameObjNotifier* oldParent, GameObjNotifier* newParent,
                      NotifyContext* ctx) override;                  // 61, 0x558270
  int OnUsableChanged(int newValue, NotifyContext* ctx) override;    // 68, 0x558290
  int OnDestroyedChanged(int newValue, NotifyContext* ctx) override; // 71, 0x5582b0
  int OnExistingChanged(int newValue, NotifyContext* ctx) override;  // 74, 0x5582d0
  int OnDamagedChanged(int newValue, NotifyContext* ctx) override;   // 77, 0x5582f0

  // Slots 5-9 stay pure here; the bodies below are what the leaf classes' overrides call first.
  int Read(istream* in) override = 0;            // body 0x5580e0
  int Write(ostream* out) override = 0;          // body 0x558110
  int Checksum(Crc32* crc) override = 0;         // body 0x558180
  int Validate() override = 0;                   // body 0x5581b0
  int ApplyAll(NotifyContext* ctx) override = 0; // body 0x558140

  // Record fields +0x58 / +0x5c (0 without a record).
  virtual uint32_t GetRecordField58(); // 112, 0x558050 (jumps to the shared body 0x520b60)
  virtual uint32_t GetRecordField5c(); // 113, 0x558060 (jumps to 0x520b70)
  virtual int GetCapturedStrengthLoss()
  {
    return 0;
  } // 114, shared "return 0" stub
  virtual int GetDetectionLoss()
  {
    return 0;
  } // 115, shared "return 0" stub
  virtual int OnIsDecoyedChanged(int newValue, NotifyContext* ctx)
  {
    return 1;
  } // 116, shared "return 1" stub
  virtual int OnIsDecoyedNotified(int value, NotifyContext* ctx); // 117, 0x558310

  // Master view only: validated setter. Returns IsMasterView().
  int SetIsDecoyed(int on, NotifyContext* ctx); // 0x558070
  int ApplyIsDecoyed(NotifyContext* ctx);       // 0x5581e0

  // Builds the static kind set on first use (flag at 0x6bb360). Always returns 1.
  static int EnsureEventKinds(); // 0x558350
  // Assigns the Detector kind set to *out (after EnsureEventKinds); the base of every subclass's own set.
  static int InitEventKinds(int unused, EventKindSet* out); // 0x558380

  uint32_t m_detectorFlags; // +0x58, bit 0 IsDecoyed
};
