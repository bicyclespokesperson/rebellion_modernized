#pragma once
#include "mission/mission_leaf.h"

// MissionDSSabotage (family 0x6a, ctor 0x574250, VT 0x667ca8, secondary ObjectBase VT 0x667ca0, 0xa8 bytes, no fields): the
// Death Star sabotage mission (MissionObjectTargetBase). Slots 5-9 and 37 are the shared forwarders (37 is 0x574420,
// shared with MissionSabotage); slot 160 is MissionDestroyOriginOnSuccess (0x5746e0).
class MissionDSSabotage : public MissionObjectTargetBase {
public:
    MissionDSSabotage();           // 0x574250
    virtual ~MissionDSSabotage();  // 0x574270 (deleting), 0x574290

    virtual uint32_t GetFamily() override;                                      // 1, 0x574440: 0x6a
    REB_MISSION_SHARED_PERSISTENCE_DECLS                                        // 5-9, 37
    virtual int GetEventKinds(int unused, EventKindSet* out) override;          // 15, 0x5742b0
    virtual int CheckOrder(int a, int orderKind, StatusCode* out) override;     // 22, 0x5742d0
    virtual int GetCheckFunctions(MissionCheckFunctions* out) override;         // 111, 0x574390
    virtual GameObj* CreateViewObject() override;                               // 57, 0x5743b0
    virtual int Virtual157(GameObj* person, int* out) override;                 // 157, 0x574600
    virtual int Virtual158(NotifyContext* ctx) override;                        // 158, stub 0x51ebb0
    virtual int Virtual159(GameObj* person, int roll, int* out, NotifyContext* ctx) override;  // 159, 0x574630
    virtual int Virtual160(NotifyContext* ctx) override;                        // 160, shared 0x5746e0

    static int CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out);  // 0x5744c0
    static int BuildEventKinds();      // 0x574770
    static GameObjNotifier* Create();  // 0x574450
};
