#pragma once
#include <cstdint>

#include "core/layout.h"
#include "economy/procfacil.h"

// ManufacturingFacility (ctor 0x52cbc0, VT 0x660490, secondary ObjectBase VT 0x66048c): abstract base of the facilities that
// manufacture units for a side (families 0x28-0x2a, MANFACSD records), 0x64 bytes, no fields of its own. Vtable: 130 slots;
// 0-128 are ProcFacil's (0 and 60/68/117 are overridden with plain forwards), 129 is new.
//
// The leaf classes below add only their family, the "new empty object" factory and thin forwards of the persistence slots
// to the non-virtual bodies here. Their forwards are identical code, so the linker folded them: the 0x2a class owns the
// addresses (0x52c9a0-0x52c9c0 for slots 5-7, 0x52c350 for slot 9, 0x52c680 for slot 37) that all three vtables use.
//
// The leaves are told apart for the manufacturing manager by GetManufacturingKind (slot 129): 0x2a returns 1 (shared
// "return 1" stub 0x40f340), 0x29 returns 2 (0x43c1f0) and 0x28 returns 0 (shared "return 0" stub 0x6158b0).
class ManufacturingFacility : public ProcFacil {
public:
    ManufacturingFacility();           // 0x52cbc0
    virtual ~ManufacturingFacility();  // 0x52cc00 (scalar deleting destructor 0x52cbe0)

    virtual int OnControlKindChanged(uint32_t oldKind, uint32_t newKind, NotifyContext* ctx) override;  // 60, 0x52cd40
    virtual int OnUsableChanged(int newValue, NotifyContext* ctx) override;                             // 68, 0x52cd60
    virtual int OnSuspendedChanged(int value, NotifyContext* ctx) override;                             // 117, 0x52cd80

    virtual int GetManufacturingKind() = 0;  // 129

    // Slot-5-9 bodies the leaf classes reach through a chain of forwarding wrappers.
    int ReadBody(istream* in);             // 0x52ccd0 (-> ProcFacil::ReadImpl)
    int WriteBody(ostream* out);           // 0x52cce0 (-> ProcFacil::WriteImpl)
    int ChecksumBody(Crc32* crc);          // 0x52cd20 (-> ProcFacil::ChecksumImpl)
    int ApplyAllBody(NotifyContext* ctx);  // 0x52ccf0: a side view runs ProcFacil::ApplyAllImpl; returns IsSideView()
    int ReadForward(istream* in);          // 0x52c9e0
    int WriteForward(ostream* out);        // 0x52c9f0
    int ChecksumForward(Crc32* crc);       // 0x52c6a0
    int ApplyAllForward(NotifyContext* ctx);                                // 0x52ca00
    int CreateViewsForward(int controlKind, NotifyContext* ctx);            // 0x52c370
    // Body of slot 37: the shared character/unit CreateViews (0x55b560), then the processing duration of the data block is
    // set from the record's word +0x5c.
    int CreateViewsImpl(int controlKind, NotifyContext* ctx);               // 0x52cc90
};

// Family 0x2a (VT 0x65fe60, secondary VT 0x65fe58).
class ManufacturingFacility2A : public ManufacturingFacility {
public:
    ManufacturingFacility2A();           // 0x52c200
    virtual ~ManufacturingFacility2A();  // 0x52c240 (scalar deleting destructor 0x52c220)

    virtual uint32_t GetFamily() override { return 0x2a; }                  // 1, 0x52c260
    virtual int Read(istream* in) override;                                 // 5, 0x52c9a0
    virtual int Write(ostream* out) override;                               // 6, 0x52c9b0
    virtual int Checksum(Crc32* crc) override;                              // 7, 0x52c9c0
    virtual int Validate() override { return ProcFacil::ValidateImpl(); }   // 8, 0x52c9d0 (a jump chain to ProcFacil::ValidateImpl)
    virtual int ApplyAll(NotifyContext* ctx) override;                      // 9, 0x52c350
    virtual int CreateViews(int controlKind, NotifyContext* ctx) override;  // 37, 0x52c680
    virtual GameObj* CreateViewObject() override;                           // 57, 0x52c390
    virtual int GetManufacturingKind() override { return 1; }               // 129, 0x40f340
};

// Family 0x29 (VT 0x660070, secondary VT 0x66006c).
class ManufacturingFacility29 : public ManufacturingFacility {
public:
    ManufacturingFacility29();           // 0x52c530
    virtual ~ManufacturingFacility29();  // 0x52c570 (scalar deleting destructor 0x52c550)

    virtual uint32_t GetFamily() override { return 0x29; }                  // 1, 0x52c590
    virtual int Read(istream* in) override;                                 // 5, shared 0x52c9a0
    virtual int Write(ostream* out) override;                               // 6, shared 0x52c9b0
    virtual int Checksum(Crc32* crc) override;                              // 7, shared 0x52c9c0
    virtual int Validate() override { return ProcFacil::ValidateImpl(); }   // 8, shared 0x52c9d0
    virtual int ApplyAll(NotifyContext* ctx) override;                      // 9, shared 0x52c350
    virtual int CreateViews(int controlKind, NotifyContext* ctx) override;  // 37, shared 0x52c680
    virtual GameObj* CreateViewObject() override;                           // 57, 0x52c6b0
    virtual int GetManufacturingKind() override { return 2; }               // 129, 0x43c1f0
};

// Family 0x28 (VT 0x660280, secondary VT 0x66027c).
class ManufacturingFacility28 : public ManufacturingFacility {
public:
    ManufacturingFacility28();           // 0x52c850
    virtual ~ManufacturingFacility28();  // 0x52c890 (scalar deleting destructor 0x52c870)

    virtual uint32_t GetFamily() override { return 0x28; }                  // 1, 0x52c8b0
    virtual int Read(istream* in) override;                                 // 5, shared 0x52c9a0
    virtual int Write(ostream* out) override;                               // 6, shared 0x52c9b0
    virtual int Checksum(Crc32* crc) override;                              // 7, shared 0x52c9c0
    virtual int Validate() override { return ProcFacil::ValidateImpl(); }   // 8, shared 0x52c9d0
    virtual int ApplyAll(NotifyContext* ctx) override;                      // 9, shared 0x52c350
    virtual int CreateViews(int controlKind, NotifyContext* ctx) override;  // 37, shared 0x52c680
    virtual GameObj* CreateViewObject() override;                           // 57, 0x52ca20
    virtual int GetManufacturingKind() override { return 0; }               // 129, shared "return 0" 0x6158b0
};

// Registry factories: a new empty object, or null if allocation fails. 0x52c2e0 and 0x52c390 (the slot-57 method) are
// identical code, as are 0x52c610/0x52c6b0 and 0x52c930/0x52ca20.
GameObjNotifier* CreateManufacturingFacility2A();  // 0x52c2e0
GameObjNotifier* CreateManufacturingFacility29();  // 0x52c610
GameObjNotifier* CreateManufacturingFacility28();  // 0x52c930

// Number of children of `parent` in the manufacturing families (cdecl; the arguments are passed through to
// GameObj::CountChildren).
int CountManufacturingFacilityChildren(GameObj* parent, uint32_t side, uint32_t filter);    // 0x52cc20, families 0x28-0x2b
int CountManufacturingFacility2AChildren(GameObj* parent, uint32_t side, uint32_t filter);  // 0x52c270, family 0x2a
int CountManufacturingFacility29Children(GameObj* parent, uint32_t side, uint32_t filter);  // 0x52c5a0, family 0x29
int CountManufacturingFacility28Children(GameObj* parent, uint32_t side, uint32_t filter);  // 0x52c8c0, family 0x28

inline ProcFacil* AsProcFacil(GameObjNotifier* facility) { return static_cast<ProcFacil*>(facility); }

// Iterator over families [0x28,0x2c) (VT 0x660698, destructor only). The one-argument form uses state filter 1.
class ManufacturingFacilityIterator : public ChildIterator {
public:
    explicit ManufacturingFacilityIterator(GameObjNotifier* parent);               // 0x52cda0
    ManufacturingFacilityIterator(GameObjNotifier* parent, uint32_t stateFilter);  // 0x52ce40
    virtual ~ManufacturingFacilityIterator();                                      // 0x52cec0 (scalar deleting destructor 0x52ce20)
};

// Iterator over family [0x2a,0x2b) (VT 0x660068, destructor only).
class ManufacturingFacility2AIterator : public ChildIterator {
public:
    ManufacturingFacility2AIterator(GameObjNotifier* parent, uint32_t stateFilter);                 // 0x52c420
    ManufacturingFacility2AIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side);  // 0x52c4a0
    virtual ~ManufacturingFacility2AIterator();                                                     // 0x52c520 (scalar deleting destructor 0x52c400)
};

// Iterator over family [0x29,0x2a) (VT 0x660278, destructor only).
class ManufacturingFacility29Iterator : public ChildIterator {
public:
    ManufacturingFacility29Iterator(GameObjNotifier* parent, uint32_t stateFilter);                 // 0x52c740
    ManufacturingFacility29Iterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side);  // 0x52c7c0
    virtual ~ManufacturingFacility29Iterator();                                                     // 0x52c840 (scalar deleting destructor 0x52c720)
};

// Iterator over family [0x28,0x29) (VT 0x660488, destructor only).
class ManufacturingFacility28Iterator : public ChildIterator {
public:
    ManufacturingFacility28Iterator(GameObjNotifier* parent, uint32_t stateFilter);                 // 0x52cab0
    ManufacturingFacility28Iterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side);  // 0x52cb30
    virtual ~ManufacturingFacility28Iterator();                                                     // 0x52cbb0 (scalar deleting destructor 0x52ca90)
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(ManufacturingFacility, 0x64);
REB_ASSERT_SIZE(ManufacturingFacility2A, 0x64);
REB_ASSERT_SIZE(ManufacturingFacility29, 0x64);
REB_ASSERT_SIZE(ManufacturingFacility28, 0x64);
REB_ASSERT_SIZE(ManufacturingFacilityIterator, 0x20);
REB_ASSERT_SIZE(ManufacturingFacility2AIterator, 0x20);
REB_ASSERT_SIZE(ManufacturingFacility29Iterator, 0x20);
REB_ASSERT_SIZE(ManufacturingFacility28Iterator, 0x20);
#pragma clang diagnostic pop
