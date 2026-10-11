#pragma once
#include "data/sd_records.h"
#include "economy/facility.h"
#include "gameobj/child_iterator.h"

// Families 0x20-0x21 (secondary ObjectBase VT 0x65f27c): the facility records common to every side.
class AllFacility : public Facility {
public:
    AllFacility();           // 0x5267c0
    virtual ~AllFacility();  // 0x526840 (scalar deleting destructor 0x526820)

    virtual uint32_t GetFamily() override;                                           // 1, 0x526bd0
    virtual int Read(istream* in) override;                                          // 5, 0x51c1d0
    virtual int Write(ostream* out) override;                                        // 6, 0x5277f0
    virtual int Checksum(Crc32* crc) override;                                       // 7, 0x527810
    virtual int Validate() override { return ValidateImpl(); }                       // 8, 0x527820 (a jump to ValidateImpl)
    virtual int ApplyAll(NotifyContext* ctx) override;                               // 9, 0x526bb0
    virtual int GetEventKinds(int unused, EventKindSet* out) override;               // 15, 0x526860
    virtual int Virtual17() override { return 0; }                                   // 17, shared "return 0" stub
    virtual int CheckOrder(int a, int orderKind, StatusCode* out) override;          // 22, 0x526880
    virtual int CheckRequestImpl(const ActivityRequest* request, StatusCode* out) override;  // 23, 0x526940
    virtual int CheckUnknownImpl(const ActivityRequest* request, StatusCode* out) override;  // 27, 0x5269b0
    virtual int CreateViews(int controlKind, NotifyContext* ctx) override;          // 37, 0x55ac90 (shared wrapper)
    virtual GameObj* CreateViewObject() override;                                    // 57, 0x526b40

    // The layers the slot wrappers delegate through (each ends in GameObj's Read/Write/Checksum or Facility::ApplyAllImpl).
    int ReadStep(istream* in);             // 0x526f10
    int WriteStep(ostream* out);           // 0x526be0
    int ChecksumStep(Crc32* crc);          // 0x526f50
    int ApplyAllStep(NotifyContext* ctx);  // 0x526bf0

    // Fills the class's static event kind set once (Facility's set plus 0x201, 0x202 and 0x204); false if it failed.
    static int BuildEventKinds();  // 0x526c00
};

// Abstract base of the defense facility leaves (families 0x22-0x27, record DefenseFacilitySD).
class DefenseFacility : public Facility {
public:
    DefenseFacility();           // 0x526e00
    virtual ~DefenseFacility();  // 0x526e40 (scalar deleting destructor 0x526e20)

    virtual int Read(istream* in) override;                                          // 5, 0x5277b0 (leaves)
    virtual int Write(ostream* out) override;                                        // 6, 0x5277c0 (leaves)
    virtual int Checksum(Crc32* crc) override;                                       // 7, 0x5277d0 (leaves)
    virtual int Validate() override { return ValidateImpl(); }                       // 8, 0x5277e0 (leaves; jumps to 0x527820)
    virtual int ApplyAll(NotifyContext* ctx) override;                               // 9, 0x51c1b0 (leaves)
    virtual int CreateViews(int controlKind, NotifyContext* ctx) override;          // 37, 0x527230 (leaves)
    virtual int Virtual46(NotifyContext* ctx) override;                              // 46, 0x526e60: destroyed reason 4
    virtual int OnControlKindChanged(uint32_t oldKind, uint32_t newKind, NotifyContext* ctx) override;  // 60, 0x526f70
    virtual int OnUsableChanged(int newValue, NotifyContext* ctx) override;          // 68, 0x526f90
    virtual int Virtual111() override;                                               // 111, 0x526f00: 3

    virtual uint32_t GetAttackStrength();   // 115, 0x558060 (shared): record +0x5c, 0 without a record
    virtual uint32_t GetShieldStrength();   // 116, 0x557750 (shared): record +0x60, 0 without a record
    virtual int Virtual117() { return 0; }  // 117, shared "return 0" stub
    virtual int Virtual118() { return 0; }  // 118, shared "return 0" stub

    int ApplyAllLeafStep(NotifyContext* ctx);  // 0x527800
    int ApplyAllBaseStep(NotifyContext* ctx);  // 0x526f20
};

class DefenseFacility22 : public DefenseFacility {
public:
    DefenseFacility22();           // 0x527370
    virtual ~DefenseFacility22();  // 0x5273b0 (scalar deleting destructor 0x527390)
    virtual uint32_t GetFamily() override;         // 1, 0x527440
    virtual GameObj* CreateViewObject() override;  // 57, 0x5274c0
};

class DefenseFacility23 : public DefenseFacility {
public:
    DefenseFacility23();           // 0x5270e0
    virtual ~DefenseFacility23();  // 0x527120 (scalar deleting destructor 0x527100)
    virtual uint32_t GetFamily() override;         // 1, 0x527140
    virtual GameObj* CreateViewObject() override;  // 57, 0x527250
};

class DefenseFacility24 : public DefenseFacility {
public:
    DefenseFacility24();           // 0x527660
    virtual ~DefenseFacility24();  // 0x5276a0 (scalar deleting destructor 0x527680)
    virtual uint32_t GetFamily() override;         // 1, 0x5276c0
    virtual GameObj* CreateViewObject() override;  // 57, 0x527830
};

class DefenseFacility25 : public DefenseFacility {
public:
    DefenseFacility25();           // 0x51c060
    virtual ~DefenseFacility25();  // 0x51c0a0 (scalar deleting destructor 0x51c080)
    virtual uint32_t GetFamily() override;         // 1, 0x51c0c0
    virtual GameObj* CreateViewObject() override;  // 57, 0x51c1e0
};

// Children of `parent` in the facility families that match the side and state filter (cdecl).
int CountAllFacilities(GameObj* parent, uint32_t side, uint32_t filter);        // 0x526a60, families 0x20-0x21
int CountDefenseFacilities(GameObj* parent, uint32_t side, uint32_t filter);    // 0x526e90, families 0x22-0x27
int CountDefenseFacilities22(GameObj* parent, uint32_t side, uint32_t filter);  // 0x5273d0
int CountDefenseFacilities23(GameObj* parent, uint32_t side, uint32_t filter);  // 0x527150
int CountDefenseFacilities24(GameObj* parent, uint32_t side, uint32_t filter);  // 0x5276d0
int CountDefenseFacilities25(GameObj* parent, uint32_t side, uint32_t filter);  // 0x51c0d0

// Registry factories: a new empty object, or null if allocation fails.
GameObjNotifier* CreateAllFacility();        // 0x526ad0
GameObjNotifier* CreateDefenseFacility22();  // 0x527450
GameObjNotifier* CreateDefenseFacility23();  // 0x5271c0
GameObjNotifier* CreateDefenseFacility24();  // 0x527740
GameObjNotifier* CreateDefenseFacility25();  // 0x51c140

// Iterators (destructor-only vtables) over the families named by their class.
class AllFacilityIterator : public ChildIterator {  // [0x20,0x22), VT 0x65f44c
public:
    AllFacilityIterator(GameObjNotifier* parent, uint32_t stateFilter);                 // 0x526cf0
    AllFacilityIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side);  // 0x526d70
    virtual ~AllFacilityIterator();  // 0x526df0 (scalar deleting destructor 0x526cd0)
};

class DefenseFacilityIterator : public ChildIterator {  // [0x22,0x28), VT 0x65f634
public:
    DefenseFacilityIterator(GameObjNotifier* parent, uint32_t stateFilter);                 // 0x526fd0
    DefenseFacilityIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side);  // 0x527050
    virtual ~DefenseFacilityIterator();  // 0x5270d0 (scalar deleting destructor 0x526fb0)
};

class DefenseFacility22Iterator : public ChildIterator {  // [0x22,0x23), VT 0x65fa04
public:
    DefenseFacility22Iterator(GameObjNotifier* parent, uint32_t stateFilter);                 // 0x527550
    DefenseFacility22Iterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side);  // 0x5275d0
    virtual ~DefenseFacility22Iterator();  // 0x527650 (scalar deleting destructor 0x527530)
};

class DefenseFacility23Iterator : public ChildIterator {  // [0x23,0x24), VT 0x65f81c
public:
    DefenseFacility23Iterator(GameObjNotifier* parent, uint32_t stateFilter);  // 0x5272e0
    virtual ~DefenseFacility23Iterator();  // 0x527360 (scalar deleting destructor 0x5272c0)
};

class DefenseFacility24Iterator : public ChildIterator {  // [0x24,0x25), VT 0x65fbec
public:
    DefenseFacility24Iterator(GameObjNotifier* parent, uint32_t stateFilter);                 // 0x5278c0
    DefenseFacility24Iterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side);  // 0x527940
    virtual ~DefenseFacility24Iterator();  // 0x5279c0 (scalar deleting destructor 0x5278a0)
};

class DefenseFacility25Iterator : public ChildIterator {  // [0x25,0x26), VT 0x65ecd4
public:
    DefenseFacility25Iterator(GameObjNotifier* parent, uint32_t stateFilter);  // 0x51c270
    virtual ~DefenseFacility25Iterator();  // 0x51c2f0 (scalar deleting destructor 0x51c250)
};
