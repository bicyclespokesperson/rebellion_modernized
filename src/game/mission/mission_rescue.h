#pragma once
#include "mission/mission_leaf.h"

// MissionRescue (family 0x61, ctor 0x56ab50, VT 0x664600, secondary ObjectBase VT 0x6645fc, 0xa8 bytes, no fields): the
// Rescue mission for a captured character (MissionCharacterTargetBase). Slots 5-9 and 37 are the shared forwarders (slot 8
// is 0x56ad20, which jumps to the shared 0x56cc40); slot 160 is the shared MissionStartIfIdle.
class MissionRescue : public MissionCharacterTargetBase {
public:
    MissionRescue();           // 0x56ab50
    virtual ~MissionRescue();  // 0x56ab70 (deleting), 0x56ab90

    virtual uint32_t GetFamily() override;                                      // 1, 0x56ad30: 0x61
    REB_MISSION_SHARED_PERSISTENCE_DECLS                                        // 5-9, 37
    virtual int GetEventKinds(int unused, EventKindSet* out) override;          // 15, 0x56abb0
    virtual int CheckOrder(int a, int orderKind, StatusCode* out) override;     // 22, 0x56abd0
    virtual int GetCheckFunctions(MissionCheckFunctions* out) override;         // 111, 0x56ac90
    virtual GameObj* CreateViewObject() override;                               // 57, 0x56acb0
    virtual int Virtual157(GameObj* person, int* out) override;                 // 157, 0x56ae30
    virtual int Virtual158(NotifyContext* ctx) override;                        // 158, stub 0x51ebb0
    virtual int Virtual159(GameObj* person, int roll, int* out, NotifyContext* ctx) override;  // 159, 0x56ae50
    virtual int Virtual160(NotifyContext* ctx) override;                        // 160, shared 0x576700

    static int CheckTarget(MissionOrderCheck* check, StatusCode* status, int* out);  // 0x56adb0
    static int BuildEventKinds();      // 0x56af40
    static void InitStatics();         // 0x56af70
    static GameObjNotifier* Create();  // 0x56ad40
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(MissionRescue, 0xa8);
#pragma clang diagnostic pop
