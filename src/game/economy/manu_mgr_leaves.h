#pragma once
#include "core/reb_abi.h"
#include <cstdint>

#include "core/layout.h"
#include "economy/manu_mgr.h"

// The three concrete manufacturing managers (0x8c bytes each, no fields of their own), one per kind of product. A
// manager serves the manufacturing facilities whose GetManufacturingKind equals its GetFacilityKind (slot 123), and
// Side::GetXxx accessors index them the same way (see rd_kind in side.h):
//
//   kind 0  ShipyardManuMgr          family 0xa2  VT 0x662e60  secondary 0x662e5c  (facility family 0x28)
//   kind 1  ConstructionYardManuMgr  family 0xa0  VT 0x662c00  secondary 0x662bfc  (facility family 0x2a)
//   kind 2  TrainingFacilManuMgr     family 0xa4  VT 0x6629a0  secondary 0x662998  (facility family 0x29)
//
// The classes differ in family, the order kinds their event set accepts (slot 15/22, a per-class static set), the
// "new empty object" factory (slot 57) and slots 122/123. The wrappers of slots 5-9 and 37 are identical code, so the
// linker folded them: the 0xa4 class owns the addresses (0x55bc80-0x55bcd0, 0x55bca0) that all three vtables use. Slot 37
// is the shared stub 0x55b560. The shipyard class additionally overrides slots 129 and 132 with plain forwards to
// ManuMgr's. Slot 122 (an order kind, 0x210-0x212) lives in the order-kind classes' address range and is declared inline.
//
// Which in-game facility each kind is follows the Side's kind order (Shipyard, ConstructionYard, TrainingFacil) and
// the lookup of a system's manager by slot 123; the facility family column is from ManufacturingFacility's slot 129.

// Bodies of slots 5-7 and 9 (thiscall, `this` passed through; the leaves' wrappers forward to them).
int REB_THISCALL ManuMgrReadBody(ManuMgr* self, istream* in);          // 0x55bce0 (-> ManuMgr::Read)
int REB_THISCALL ManuMgrWriteBody(ManuMgr* self, ostream* out);        // 0x55bcf0 (-> ManuMgr::Write)
int REB_THISCALL ManuMgrApplyAllBody(ManuMgr* self, NotifyContext* ctx);  // 0x55bd00 (-> ManuMgr::ApplyAll)
int REB_THISCALL ManuMgrChecksumBody(ManuMgr* self, Crc32* crc);       // 0x55bd10 (-> ManuMgr::Checksum)

// The shared CreateViews stubs (thiscall): both end in GameObj's CreateViews through 0x5774f0. 0x55b560 serves the
// manufacturing facilities and these managers, 0x55ac90 the defense facilities.
int REB_THISCALL CreateViewsStubShared(GameObj* self, int controlKind, NotifyContext* ctx);   // 0x55b560
int REB_THISCALL CreateViewsStubDefense(GameObj* self, int controlKind, NotifyContext* ctx);  // 0x55ac90

// Family 0xa4.
class TrainingFacilManuMgr : public ManuMgr {
public:
    TrainingFacilManuMgr();           // 0x55b2f0
    virtual ~TrainingFacilManuMgr();  // 0x55b370 (scalar deleting destructor 0x55b350)

    virtual uint32_t GetFamily() override { return 0xa4; }                           // 1, 0x55b390
    virtual int Read(istream* in) override;                                          // 5, 0x55bc80
    virtual int Write(ostream* out) override;                                        // 6, 0x55bc90
    virtual int Checksum(Crc32* crc) override;                                       // 7, 0x55bcc0
    virtual int Validate() override { return ManuMgr::Validate(); }                  // 8, 0x55bcd0 (a jump chain to ManuMgr::Validate)
    virtual int ApplyAll(NotifyContext* ctx) override;                               // 9, 0x55bca0
    virtual int GetEventKinds(int unused, EventKindSet* out) override;               // 15, 0x55b3a0
    virtual int CheckOrder(int a, int orderKind, StatusCode* out) override;          // 22, 0x55b3c0
    virtual int CreateViews(int controlKind, NotifyContext* ctx) override;           // 37, 0x55b560
    virtual GameObj* CreateViewObject() override;                                    // 57, 0x55b5d0
    virtual int Virtual122() override { return 0x212; }                              // 122, 0x538ed0
    virtual int GetFacilityKind() override { return 2; }                             // 123, 0x43c1f0

    // Fills the class's static event kind set once (ManuMgr's set plus 0x212); false if it failed.
    static int BuildEventKinds();            // 0x55b580
    static void InitStatics();               // 0x55b640 (from the CRT initialisers)
    static void ConstructEventKinds();       // 0x55b650
    static void RegisterEventKindsAtExit();  // 0x55b660
};

// Family 0xa0.
class ConstructionYardManuMgr : public ManuMgr {
public:
    ConstructionYardManuMgr();           // 0x55b690
    virtual ~ConstructionYardManuMgr();  // 0x55b710 (scalar deleting destructor 0x55b6f0)

    virtual uint32_t GetFamily() override { return 0xa0; }                           // 1, 0x55b730
    virtual int Read(istream* in) override;                                          // 5, shared 0x55bc80
    virtual int Write(ostream* out) override;                                        // 6, shared 0x55bc90
    virtual int Checksum(Crc32* crc) override;                                       // 7, shared 0x55bcc0
    virtual int Validate() override { return ManuMgr::Validate(); }                  // 8, shared 0x55bcd0
    virtual int ApplyAll(NotifyContext* ctx) override;                               // 9, shared 0x55bca0
    virtual int GetEventKinds(int unused, EventKindSet* out) override;               // 15, 0x55b740
    virtual int CheckOrder(int a, int orderKind, StatusCode* out) override;          // 22, 0x55b760
    virtual int CreateViews(int controlKind, NotifyContext* ctx) override;           // 37, shared 0x55b560
    virtual GameObj* CreateViewObject() override;                                    // 57, 0x55b950
    virtual int Virtual122() override { return 0x210; }                              // 122, 0x538d30
    virtual int GetFacilityKind() override { return 1; }                             // 123, 0x40f340

    // Fills the class's static event kind set once (ManuMgr's set plus 0x210); false if it failed.
    static int BuildEventKinds();            // 0x55b900
    static void InitStatics();               // 0x55b9c0
    static void ConstructEventKinds();       // 0x55b9d0
    static void RegisterEventKindsAtExit();  // 0x55b9e0
};

// Family 0xa2.
class ShipyardManuMgr : public ManuMgr {
public:
    ShipyardManuMgr();           // 0x55ba10
    virtual ~ShipyardManuMgr();  // 0x55ba90 (scalar deleting destructor 0x55ba70)

    virtual uint32_t GetFamily() override { return 0xa2; }                           // 1, 0x55bab0
    virtual int Read(istream* in) override;                                          // 5, shared 0x55bc80
    virtual int Write(ostream* out) override;                                        // 6, shared 0x55bc90
    virtual int Checksum(Crc32* crc) override;                                       // 7, shared 0x55bcc0
    virtual int Validate() override { return ManuMgr::Validate(); }                  // 8, shared 0x55bcd0
    virtual int ApplyAll(NotifyContext* ctx) override;                               // 9, shared 0x55bca0
    virtual int GetEventKinds(int unused, EventKindSet* out) override;               // 15, 0x55bac0
    virtual int CheckOrder(int a, int orderKind, StatusCode* out) override;          // 22, 0x55bae0
    virtual int CreateViews(int controlKind, NotifyContext* ctx) override;           // 37, shared 0x55b560
    virtual GameObj* CreateViewObject() override;                                    // 57, 0x55bda0
    virtual int Virtual122() override { return 0x211; }                              // 122, 0x538e00
    virtual int GetFacilityKind() override { return 0; }                             // 123, shared "return 0" 0x6158b0
    virtual int OnRemainingGameObjCountChanged(int oldValue, int newValue, NotifyContext* ctx) override;  // 129, 0x55be10
    virtual int OnSeedKeyChanged(NullableKey* oldKey, NullableKey* newKey, NotifyContext* ctx) override;  // 132, 0x55be30

    // Fills the class's static event kind set once (ManuMgr's set plus 0x215 and 0x211); false if it failed.
    static int BuildEventKinds();            // 0x55bd30
    static void InitStatics();               // 0x55be50
    static void ConstructEventKinds();       // 0x55be60
    static void RegisterEventKindsAtExit();  // 0x55be70
};

// Registry factories: a new empty object, or null if allocation fails. Identical code to the slot-57 methods.
GameObjNotifier* CreateTrainingFacilManuMgr();      // 0x55b4f0
GameObjNotifier* CreateConstructionYardManuMgr();   // 0x55b890
GameObjNotifier* CreateShipyardManuMgr();           // 0x55bc10

// Number of children of `parent` in each manager family (cdecl; the arguments are passed through to
// GameObj::CountChildren).
int CountTrainingFacilManuMgrChildren(GameObj* parent, uint32_t side, uint32_t filter);     // 0x55b480, family 0xa4
int CountConstructionYardManuMgrChildren(GameObj* parent, uint32_t side, uint32_t filter);  // 0x55b820, family 0xa0
int CountShipyardManuMgrChildren(GameObj* parent, uint32_t side, uint32_t filter);          // 0x55bba0, family 0xa2

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(TrainingFacilManuMgr, 0x8c);
REB_ASSERT_SIZE(ConstructionYardManuMgr, 0x8c);
REB_ASSERT_SIZE(ShipyardManuMgr, 0x8c);
#pragma clang diagnostic pop
