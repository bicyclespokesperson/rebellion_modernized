#pragma once
#include "character/role.h"
#include "gameobj/child_iterator.h"

// SpecialForces (family 0x3c-0x3f, VT 0x65e160, secondary ObjectBase VT 0x65e15c, 0x7c bytes): the covert-ops squads a
// side can field. A Role without any fields of its own; its skills are the base ones and its SD record
// (SpecialForceSD) names the missions it can perform. The vtable has Role's 179 slots; the overrides are below.
class SpecialForces : public Role {
public:
    SpecialForces();           // 0x503ae0
    virtual ~SpecialForces();  // 0x503b00 (deleting), 0x503b20

    virtual uint32_t GetFamily() override;                                                  // 1, 0x503d80: 0x3c
    virtual int Read(istream* in) override;                                                 // 5, 0x503d20
    virtual int Write(ostream* out) override;                                               // 6, 0x503d30
    virtual int Checksum(Crc32* crc) override;                                              // 7, 0x503d60
    virtual int Validate() override;                                                        // 8, 0x503d70 (jump thunk to Role::Validate)
    virtual int ApplyAll(NotifyContext* ctx) override;                                      // 9, 0x503d40
    virtual int GetEventKinds(int unused, EventKindSet* out) override;                      // 15, 0x503b50
    virtual int CheckOrder(int a, int orderKind, StatusCode* out) override;                 // 22, 0x503b70
    virtual int Virtual44(NotifyContext* ctx) override;                                     // 44, 0x503d90
    virtual int Virtual45(int a, NotifyContext* ctx) override;                              // 45, 0x503e20
    virtual int Virtual46(NotifyContext* ctx) override;                                     // 46, 0x503e70
    virtual int CreateViews(int controlKind, NotifyContext* ctx) override;                  // 37, shared stub 0x55ac90
    virtual GameObj* CreateViewObject() override;                                           // 57, 0x503cb0
    virtual int GetDiplomacy() override;                                                    // 119, 0x503c30 (the base values)
    virtual int GetEspionage() override;                                                    // 120, 0x503c40
    virtual int GetShipyardRD() override;                                                   // 121, 0x503c50
    virtual int GetTrainingFacilRD() override;                                              // 122, 0x503c60
    virtual int GetConstructionYardRD() override;                                           // 123, 0x503c70
    virtual int GetCombat() override;                                                       // 124, 0x503c80
    virtual int GetLeadership() override;                                                   // 125, 0x503c90
    virtual int GetLoyalty() override;                                                      // 126, 0x503ca0
    virtual int Virtual127() override { return 1; }                                         // 127
    virtual int Virtual130(NotifyContext* ctx) override;                                    // 130, 0x503ea0
    virtual int Virtual131(const ObjectKey* opponent, NotifyContext* ctx) override;                             // 131, 0x503eb0

    // CreateViews (slot 37) is a shared jump stub at 0x55ac90, outside this class.

    // The record's mission bitmask (0 without a record).
    uint32_t GetRecordMissionBits();  // 0x503b40

    // Bodies of the persistence slots (the slots forward to these, which forward to Role's bodies).
    int ReadImpl(istream* in);              // 0x503f50
    int WriteImpl(ostream* out);            // 0x503f60
    int ApplyAllImpl(NotifyContext* ctx);   // 0x503f70
    int ChecksumImpl(Crc32* crc);           // 0x503f80

    // The event-kind set of SpecialForces (g_specialForcesEventKinds, 0x6b2b58): a copy of Role's, made on first use.
    static int BuildEventKinds();                              // 0x503fa0
    static GameObjNotifier* Create();                          // 0x503ee0, the object factory of family 0x3c
};

// Iterator over the children of a game object in the special-force family (vtable 0x65e42c).
class SpecialForceIterator : public ChildIterator {
public:
    SpecialForceIterator(GameObjNotifier* parent, uint32_t stateFilter);                 // 0x504040
    SpecialForceIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side);  // 0x5040c0
    virtual ~SpecialForceIterator();                                                     // 0x504020 (deleting), 0x504140
};


// Static-initializer pieces of g_specialForcesEventKinds (0x503fe0 constructs it, 0x503ff0 registers its destructor).
