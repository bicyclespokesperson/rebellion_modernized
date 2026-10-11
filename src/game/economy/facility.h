#pragma once
#include <cstdint>

#include "gameobj/child_iterator.h"
#include "gameobj/unit_obj_base.h"

// Facility (ctor 0x539980, VT 0x6613a8, secondary ObjectBase VT 0x6613a0): abstract base of the facility families 0x20-0x2f
// (ALLFAC/DEFFAC/MANFAC/PROFAC records), 0x58 bytes, no fields of its own. Vtable: 115 slots, 0-111 are UnitObjBase's,
// 112-114 are new (112 reads the SD record's word +0x58, the shared body Detector uses too).
//
// Hierarchy:
//   Facility
//     AllFacility (0x5267c0, VT 0x65f280, families 0x20-0x21)         economy/defense_facility.h
//     DefenseFacility (0x526e00, VT 0x65f458) -> types 0x22-0x25       economy/defense_facility.h
//     ProcFacil (0x53a670, VT 0x6615b0)                                economy/procfacil.h
//       ManufacturingFacility (0x52cbc0) -> types 0x28-0x2a
//       ProductionBase (0x55a600) -> production facilities 0x2c-0x2d
// The data block of Facility objects (VT 0x66a150, 0x18 bytes, ctor 0x584100): a UnitObjData without fields.
class FacilityData : public UnitObjData
{
public:
  FacilityData(); // 0x584100
  ~FacilityData() override; // 0x584120 (deleting), 0x584140
};

class Facility : public UnitObjBase
{
public:
  Facility(); // 0x539980
  ~Facility() override; // 0x5399c0 (scalar deleting destructor 0x5399a0)

  int GetEventKinds(int unused, EventKindSet* out) override; // 15, 0x5399e0
  int CheckOrder(int a, int orderKind, StatusCode* out) override; // 22, 0x539a00
  GameObjData* CreateData() override; // 58, 0x539c90
  int OnControlKindChanged(uint32_t oldKind, uint32_t newKind, NotifyContext* ctx) override; // 60, 0x539c70
  int OnUsableChanged(int newValue, NotifyContext* ctx) override; // 68, 0x558290 (shared jump)
  int OnExistingChanged(int newValue, NotifyContext* ctx) override; // 74, 0x5582d0 (shared jump)

  virtual uint32_t
  GetRecordField58(); // 112, 0x558050 (shared with Detector; a jump to 0x520b60): record +0x58, 0 without a record
  virtual int Virtual113()
  {
    return 0;
  } // 113, shared "return 0" stub
  virtual int Virtual114()
  {
    return 0;
  } // 114, shared "return 0" stub

  // Record word +0x5c (the production facilities' processing rate), 0 without a record. Not virtual here: 0x520b70.
  uint32_t GetRecordField5c();

  // Body of the leaf classes' ApplyAll (slot 9): a side view applies everything; returns IsSideView().
  int ApplyAllImpl(NotifyContext* ctx); // 0x539b30
  // Body of the leaf classes' Validate (slot 8): GameObj's checks, the facility has no children, and its parent is not
  // destroyed.
  int ValidateImpl(); // 0x539b60

  // Fills the class's static event kind set once (kind 0x200); false if it could not be built.
  static int BuildEventKinds(); // 0x539c20
  // (0x539d10) and the atexit registration of its destructor (0x539d20).
};

// Runs UpdateChildren over the facility families 0x20-0x30 of `object` (0x539ac0, cdecl).
int UpdateFacilityChildren(GameObj* object, NotifyContext* ctx);

// Iterator over families [0x20,0x30) (VT 0x661574, destructor only).
class FacilityIterator : public ChildIterator
{
public:
  FacilityIterator(GameObjNotifier* parent, uint32_t stateFilter); // 0x539d70
  FacilityIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side); // 0x539df0
  ~FacilityIterator() override; // 0x539e70 (scalar deleting destructor 0x539d50)
};
