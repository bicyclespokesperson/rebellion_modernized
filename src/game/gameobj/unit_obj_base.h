#pragma once
#include "gameobj/game_obj.h"
#include "gameobj/game_obj_data.h"

// The data block of UnitObjBase objects (VT 0x661ad8, 0x18 bytes, ctor 0x5409d0): GameObjData with its Read/Write/Checksum
// forwarded to the base class. The data blocks of the unit classes derive from it.
class UnitObjData : public GameObjData
{
public:
  UnitObjData(); // 0x5409d0
  ~UnitObjData() override; // 0x5409f0 (deleting), 0x540a10
  int Read(istream* in) override; // 0x540a20
  int Write(ostream* out) override; // 0x540a30
  int Checksum(Crc32* crc) override; // 0x540a40
  // Slot 4 (IsValid) is the shared "return 1" stub.
};

// Abstract base of the unit classes (troop regiments, capital ships, fighter squadrons, facilities, characters, special
// forces and fleets: families 0x08-0x3f). VT 0x65ce68 (112 slots; slot 111 is new), secondary ObjectBase VT 0x65ce60. The
// class adds no fields. Over GameObj it supplies per-side availability from the SD record, the move check, an empty
// event-kind set and the unit data block; slots 1 and 5-9 and 37 and 57 stay pure.
class UnitObjBase : public GameObj
{
public:
  UnitObjBase();
  ~UnitObjBase() override;

  int GetEventKinds(int unused, EventKindSet* out) override; // 15, 0x4f2770
  // Availability for a side (1-3): 0 without a record, 1 unless the object exists, else the record's per-side value.
  int CanBeControlledBy(int side) override; // 20, 0x4f27d0
  int CheckOrder(int a, int orderKind, StatusCode* out) override; // 22, 0x4f2860
  int CheckRequestImpl(const ActivityRequest* request, StatusCode* out) override; // 23, 0x4f28d0
  int InitializeNewObject(intptr_t a) override; // 47, 0x4f29a0
  int InitializeAfterSetup(intptr_t a) override; // 48, 0x4f29a0
  int PrepareForDestroyedRelease(intptr_t a) override; // 55, 0x4f29b0
  GameObjData* CreateData() override; // 58, 0x4f2bf0
  int OnNameChanged(std::string* oldName, std::string* newName, NotifyContext* ctx) override; // 59, 0x4f2a70
  int OnControlKindChanged(uint32_t oldKind, uint32_t newKind, NotifyContext* ctx) override; // 60, 0x4f2a90
  int OnParentChanged(GameObjNotifier* oldParent,
                      GameObjNotifier* newParent,
                      NotifyContext* ctx) override; // 61, 0x4f2ab0
  int OnDestroyedReasonChanged(uint32_t oldValue, uint32_t newValue, NotifyContext* ctx) override; // 64, 0x4f2ad0
  int OnUsableChanged(int newValue, NotifyContext* ctx) override; // 68, 0x4f2af0
  int OnCreatedChanged(int newValue, NotifyContext* ctx) override; // 69, 0x4f2b30
  int OnCompletedChanged(int newValue, NotifyContext* ctx) override; // 70, 0x4f2b10
  int OnDestroyedChanged(int newValue, NotifyContext* ctx) override; // 71, 0x4f2b50
  int OnEnrouteChanged(int newValue, NotifyContext* ctx) override; // 72, 0x4f2b70
  int OnEnrouteActiveChanged(int newValue, NotifyContext* ctx) override; // 73, 0x4f2b90
  int OnExistingChanged(int newValue, NotifyContext* ctx) override; // 74, 0x4f2bb0
  int OnAutoroutingChanged(int newValue, NotifyContext* ctx) override; // 79, 0x4f2bd0
  virtual int GetEncounterCommandKind()
  {
    return 0;
  } // 111, new

  // SD record fields 0x48 / 0x4c (0 without a record).
  uint32_t GetRecordField48(); // 0x4f2980
  uint32_t GetRecordField4c(); // 0x4f2990
  // A side view applies everything GameObj::ApplyAll does; the master does nothing. Returns IsSideView().
  int ApplyAllToSideView(NotifyContext* ctx); // 0x4f29e0
  // The event-kind set initializer the leaf classes call for their static sets: an empty set.
  static int InitEventKinds(int unused, EventKindSet* out); // 0x4f2a10
};
