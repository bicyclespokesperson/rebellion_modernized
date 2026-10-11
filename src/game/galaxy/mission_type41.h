#pragma once
#include "galaxy/mission_decl.h"

// Mission41 (family 0x41, VT 0x665578, secondary ObjectBase VT 0x665570, 0xa8 bytes): the sibling of Mission42 at
// 0x56ea00 (the code map's "obj_type41"). It adds no fields over its base and shares the persistence slots 5-9 and 37
// with Mission42 (the shared forwarders 0x56da40, 0x56ebb0, 0x56da70, 0x56cc40, 0x576980, 0x5761b0), so only the slots
// below have bodies of their own; the overrides of Read/Write/Checksum/Validate/ApplyAll/CreateViews repeat the shared
// work and are not hooked.
class Mission41 : public MissionTypeBase {
public:
    Mission41();           // 0x56ea00
    virtual ~Mission41();  // 0x56ea20 (deleting), 0x56ea40

    virtual uint32_t GetFamily() override;                                      // 1, 0x56ebc0: 0x41
    virtual int Read(istream* in) override;                                     // 5, shared 0x56da40
    virtual int Write(ostream* out) override;                                   // 6, shared 0x56ebb0
    virtual int Checksum(Crc32* crc) override;                                  // 7, shared 0x56da70
    virtual int Validate() override;                                            // 8, shared 0x56cc40
    virtual int ApplyAll(NotifyContext* ctx) override;                          // 9, shared 0x576980
    virtual int GetEventKinds(int unused, EventKindSet* out) override;          // 15, 0x56ea60
    virtual int CheckOrder(int a, int orderKind, StatusCode* out) override;     // 22, 0x56ea80
    virtual int CreateViews(int controlKind, NotifyContext* ctx) override;      // 37, shared 0x5761b0
    virtual GameObj* CreateViewObject() override;                               // 57, 0x56eb40

    static int BuildEventKinds();      // 0x56ec60, g_mission41EventKinds at 0x6bb9b8
    static GameObjNotifier* Create();  // 0x56ebd0, the object factory of family 0x41
};
