#pragma once
#include "galaxy/mission_decl.h"
#include "mission/mission_leaf.h"

// MissionAdrift (family 0x44, ctor 0x5767b0, VT 0x668d58, secondary ObjectBase VT 0x668d50, 0xa8 bytes): a plain
// mission type on MissionTypeBase without fields, built like Mission42 (galaxy/mission_type42.h). Slots 5-9 are the
// forwarders shared with Mission42; CreateViews (slot 37, 0x576960 -> 0x576a20) also seeds the phase levels.
class MissionAdrift : public MissionTypeBase {
public:
    MissionAdrift();           // 0x5767b0
    virtual ~MissionAdrift();  // 0x5767d0 (deleting), 0x5767f0

    virtual uint32_t GetFamily() override;                                      // 1, 0x5769a0: 0x44
    virtual int Read(istream* in) override;                                     // 5, shared 0x56da40
    virtual int Write(ostream* out) override;                                   // 6, shared 0x56ebb0
    virtual int Checksum(Crc32* crc) override;                                  // 7, shared 0x56da70
    virtual int Validate() override;                                            // 8, shared 0x56cc40
    virtual int ApplyAll(NotifyContext* ctx) override;                          // 9, shared 0x576980
    virtual int GetEventKinds(int unused, EventKindSet* out) override;          // 15, 0x576810
    virtual int CheckOrder(int a, int orderKind, StatusCode* out) override;     // 22, 0x576830
    virtual int CreateViews(int controlKind, NotifyContext* ctx) override;      // 37, own 0x576960
    virtual GameObj* CreateViewObject() override;                               // 57, 0x5768f0

    static int BuildEventKinds();      // 0x576ac0
    static void InitStatics();         // 0x576af0
    int CreateViewsImpl(int controlKind, NotifyContext* ctx);  // 0x576a20

    static GameObjNotifier* Create();  // 0x5769b0
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(MissionAdrift, 0xa8);
#pragma clang diagnostic pop
