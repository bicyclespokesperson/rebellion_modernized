#pragma once
#include "mission/mission_leaf.h"

// MissionRecon (family 0x54, ctor 0x56bbd0, VT 0x664b28, secondary ObjectBase VT 0x664b20, 0xa8 bytes, no fields): the
// Recon mission on a system (MissionSystemTargetBase). Slots 5-9 and 37 are the shared forwarders.
class MissionRecon : public MissionSystemTargetBase {
public:
    MissionRecon();           // 0x56bbd0
    virtual ~MissionRecon();  // 0x56bbf0 (deleting), 0x56bc10

    virtual uint32_t GetFamily() override;                                      // 1, 0x56bda0: 0x54
    REB_MISSION_SHARED_PERSISTENCE_DECLS                                        // 5-9, 37
    virtual int GetEventKinds(int unused, EventKindSet* out) override;          // 15, 0x56bc30
    virtual int CheckOrder(int a, int orderKind, StatusCode* out) override;     // 22, 0x56bc50
    virtual int GetCheckFunctions(MissionCheckFunctions* out) override;         // 111, 0x56bd10
    virtual GameObj* CreateViewObject() override;                               // 57, 0x56bd30
    virtual int Virtual157(GameObj* person, int* out) override;                 // 157, 0x56bea0
    virtual int Virtual158(NotifyContext* ctx) override;                        // 158, stub 0x51ebb0
    virtual int Virtual159(GameObj* person, int roll, int* out, NotifyContext* ctx) override;  // 159, 0x56bec0
    virtual int Virtual160(NotifyContext* ctx) override;                        // 160, 0x56bee0

    static int CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out);  // 0x56be20
    static int BuildEventKinds();      // 0x56bff0
    static GameObjNotifier* Create();  // 0x56bdb0
};
