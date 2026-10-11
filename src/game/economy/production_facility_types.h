#pragma once
#include <cstdint>

#include "economy/production_facility.h"

// The two concrete production facility classes (PROFAC records, families 0x2c and 0x2d). They add no state: each
// supplies its family, thin wrappers over ProductionFacility's bodies, the "new empty object" factory of slot 57 and the
// processing time formula (the record's processing rate through 0x55e2b0). The wrappers of slots 5-9, 37, 60, 68, 133
// and 134 are identical code in both classes, so the linker folded them: the 0x2d class's (0x52d450-0x52d180,
// 0x52d5e0, 0x52d1a0) serve both vtables.

// Bodies of slots 5-7 and 9 that the wrappers above call (thiscall, `this` passed through; identical code for both
// classes, so these serve them both).
int ProductionFacilityReadBody(ProductionFacility* self, istream* in); // 0x52d480 (-> ReadImpl)
int ProductionFacilityWriteBody(ProductionFacility* self, ostream* out); // 0x52d490 (-> WriteImpl)
int ProductionFacilityApplyAllBody(ProductionFacility* self, NotifyContext* ctx); // 0x52d4a0 (-> ApplyAllImpl)
int ProductionFacilityChecksumBody(ProductionFacility* self, Crc32* crc); // 0x52d4b0 (-> ChecksumImpl)

// Family 0x2d (VT 0x6606a0, secondary ObjectBase VT 0x66069c, 0x70 bytes).
class ProductionFacilityType2D : public ProductionFacility
{
public:
  ProductionFacilityType2D(); // 0x52ced0
  ~ProductionFacilityType2D() override; // 0x52cf10 (scalar deleting destructor 0x52cef0)

  uint32_t GetFamily() override
  {
    return 0x2d;
  } // 1, 0x52cf30
  int Read(istream* in) override; // 5, 0x52d450
  int Write(ostream* out) override; // 6, 0x52d460
  int Checksum(Crc32* crc) override; // 7, 0x52d060
  int Validate() override; // 8, 0x52d470 (jump to 0x55adb0)
  int ApplyAll(NotifyContext* ctx) override; // 9, 0x52d040
  int CreateViews(int controlKind, NotifyContext* ctx) override; // 37, 0x52d020
  GameObj* CreateViewObject() override; // 57, 0x52d070
  int OnControlKindChanged(uint32_t oldKind, uint32_t newKind, NotifyContext* ctx) override; // 60, 0x52d0e0
  int OnUsableChanged(int newValue, NotifyContext* ctx) override; // 68, 0x52d180
  int OnCreatedChanged(int newValue, NotifyContext* ctx) override; // 69, 0x52d100
  int CanProcess(const IntPair* work) override; // 133, 0x52d5e0
  int ComputeDuration(const IntPair* work, int modifier, int notStartupCycle, int* duration) override; // 134, 0x52d1a0
};

// Family 0x2c (VT 0x6608c8, secondary ObjectBase VT 0x6608c0, 0x70 bytes).
class ProductionFacilityType2C : public ProductionFacility
{
public:
  ProductionFacilityType2C(); // 0x52d300
  ~ProductionFacilityType2C() override; // 0x52d340 (scalar deleting destructor 0x52d320)

  uint32_t GetFamily() override
  {
    return 0x2c;
  } // 1, 0x52d360
  int Read(istream* in) override; // 5, 0x52d450 (shared)
  int Write(ostream* out) override; // 6, 0x52d460 (shared)
  int Checksum(Crc32* crc) override; // 7, 0x52d060 (shared)
  int Validate() override; // 8, 0x52d470 (shared)
  int ApplyAll(NotifyContext* ctx) override; // 9, 0x52d040 (shared)
  int CreateViews(int controlKind, NotifyContext* ctx) override; // 37, 0x52d020 (shared)
  GameObj* CreateViewObject() override; // 57, 0x52d4d0
  int OnControlKindChanged(uint32_t oldKind, uint32_t newKind, NotifyContext* ctx) override; // 60, 0x52d0e0 (shared)
  int OnUsableChanged(int newValue, NotifyContext* ctx) override; // 68, 0x52d180 (shared)
  int OnCreatedChanged(int newValue, NotifyContext* ctx) override; // 69, 0x52d540
  int OnExistingChanged(int newValue, NotifyContext* ctx) override; // 74, 0x52d5c0
  int Virtual114() override
  {
    return 1;
  } // 114, 0x40f340 (shared "return 1")
  int CanProcess(const IntPair* work) override; // 133, 0x52d5e0 (shared)
  int ComputeDuration(const IntPair* work,
                      int modifier,
                      int notStartupCycle,
                      int* duration) override; // 134, 0x52d1a0 (shared)
};

// Registry factories (families 0x2d and 0x2c): a new empty object, or null if allocation fails.
GameObjNotifier* CreateProductionFacilityType2D(); // 0x52cfb0
GameObjNotifier* CreateProductionFacilityType2C(); // 0x52d3e0

// Number of facilities of the family below `parent` controlled by `side` that pass the state `filter` (cdecl).
int CountProductionFacilityType2D(GameObj* parent, uint32_t side, uint32_t filter); // 0x52cf40
int CountProductionFacilityType2C(GameObj* parent, uint32_t side, uint32_t filter); // 0x52d370

// Iterator over family [0x2d,0x2e) (VT 0x6608bc, destructor only).
class ProductionFacilityType2DIterator : public ChildIterator
{
public:
  ProductionFacilityType2DIterator(GameObjNotifier* parent, uint32_t stateFilter); // 0x52d1f0
  ProductionFacilityType2DIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side); // 0x52d270
  ~ProductionFacilityType2DIterator() override; // 0x52d2f0 (scalar deleting destructor 0x52d1d0)
};

// Iterator over family [0x2c,0x2d) (VT 0x660ae4, destructor only).
class ProductionFacilityType2CIterator : public ChildIterator
{
public:
  ProductionFacilityType2CIterator(GameObjNotifier* parent, uint32_t stateFilter); // 0x52d610
  ProductionFacilityType2CIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side); // 0x52d690
  ~ProductionFacilityType2CIterator() override; // 0x52d710 (scalar deleting destructor 0x52d5f0)
};
