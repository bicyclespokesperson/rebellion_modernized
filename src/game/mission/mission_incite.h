#pragma once
#include "mission/mission_leaf.h"

// MissionIncite (family 0x56, ctor 0x571700, VT 0x666a70, secondary ObjectBase VT 0x666a6c, 0xa8 bytes, no fields): the
// Incite Uprising mission against a system (MissionSystemTargetBase). Slots 5-9 and 37 are the shared forwarders, 160 is
// the shared MissionStartIfIdle.
class MissionIncite : public MissionSystemTargetBase {
public:
    MissionIncite();           // 0x571700
    virtual ~MissionIncite();  // 0x571720 (deleting), 0x571740

    virtual uint32_t GetFamily() override;                                      // 1, 0x5718d0: 0x56
    REB_MISSION_SHARED_PERSISTENCE_DECLS                                        // 5-9, 37
    virtual int GetEventKinds(int unused, EventKindSet* out) override;          // 15, 0x571760
    virtual int CheckOrder(int a, int orderKind, StatusCode* out) override;     // 22, 0x571780
    virtual int GetCheckFunctions(MissionCheckFunctions* out) override;         // 111, 0x571840
    virtual GameObj* CreateViewObject() override;                               // 57, 0x571860
    virtual int Virtual157(GameObj* person, int* out) override;                 // 157, 0x5719d0
    virtual int Virtual158(NotifyContext* ctx) override;                        // 158, stub 0x51ebb0
    virtual int Virtual159(GameObj* person, int roll, int* out, NotifyContext* ctx) override;  // 159, 0x571a60
    virtual int Virtual160(NotifyContext* ctx) override;                        // 160, shared 0x576700

    static int CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out);  // 0x571950
    static int BuildEventKinds();      // 0x571ba0
    static GameObjNotifier* Create();  // 0x5718e0
};
