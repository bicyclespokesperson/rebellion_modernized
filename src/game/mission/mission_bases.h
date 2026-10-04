#pragma once
#include "mission/mission.h"

// The mission base classes between Mission and the concrete mission types. None of them has fields of its own (all are
// 0xa8 bytes); they differ in the event kinds they report (slot 15), the order check (22), the "check functions" pair of
// slot 111 and a few more overrides. Each class's constructor stores its own vtable (primary at +0x00, secondary
// ObjectBase vtable at +0x30).
//
//   Mission (0x5208b0)                                                        mission.h
//     MissionTypeBase (0x593730, VT 0x66b0e0)       overrides 0, 15, 22, 120   galaxy/mission_decl.h
//     MissionRuleBase (0x5923a0, VT 0x66abc0)       overrides 0, 15, 22, 111, 114, 120, 125
//       MissionSystemTargetBase (0x5866c0, VT 0x66a2c8)     0, 15, 22, 111
//       MissionCharacterTargetBase (0x586c20, VT 0x66a558)  0, 15, 22, 111
//       MissionObjectTargetBase (0x5933a0, VT 0x66ae50)     0, 15, 22, 111     (Sabotage, DS Sabotage)
//       MissionStoryBase (0x5941c0, VT 0x66b378)            0, 15, 22, 111, 125 (Pickup, Vacation, Dagobah)
//
// The classes' ObjectBase subobject sits at +0x30 like everywhere in the mission family.

// The check callbacks of a base's GetCheckFunctions pair: (member check, target check). Their signatures are known only
// by shape: __cdecl(int a, ...); see mission_rules.cpp.
class MissionRuleBase : public Mission {
public:
    MissionRuleBase();            // 0x5923a0
    virtual ~MissionRuleBase();   // 0x5923e0 (scalar deleting destructor 0x5923c0)

    virtual int GetEventKinds(int unused, EventKindSet* out) override;                        // 15, 0x592400
    virtual int CheckOrder(int a, int orderKind, StatusCode* out) override;                   // 22, 0x592420
    virtual int GetCheckFunctions(MissionCheckFunctions* out) override;                       // 111, 0x5924e0
    virtual int IsFinalPhase() override { return m_phase == 9; }                                         // 114, 0x592500
    virtual int Virtual120() override;                                                        // 120, 0x592510
    virtual int OnPhaseChanged(int oldValue, int newValue, NotifyContext* ctx) override;      // 125, 0x592f50

    // ---- slots 157-161: per-type callbacks (the rule-base branch has 162 slots; MissionTypeBase's branch differs) ----
    // Signatures provisional, taken from MissionSubdue (157 0x569b90, 158 stub 0x51ebb0, 159 0x569c20, 160 0x576700).
    virtual int Virtual157(GameObj* person, int a) = 0;                                       // 157
    virtual int Virtual158(int a) = 0;                                                        // 158
    virtual int Virtual159(GameObj* person, int a, int* out, NotifyContext* ctx) = 0;         // 159
    virtual int Virtual160(int a) = 0;                                                        // 160
    virtual int Virtual161(NotifyContext* ctx);                                               // 161, 0x592c80

    static int BuildEventKinds();  // 0x5932d0
};

class MissionSystemTargetBase : public MissionRuleBase {
public:
    MissionSystemTargetBase();           // 0x5866c0
    virtual ~MissionSystemTargetBase();  // 0x586700 (scalar deleting destructor 0x5866e0)

    virtual int GetEventKinds(int unused, EventKindSet* out) override;                        // 15, 0x5867c0
    virtual int CheckOrder(int a, int orderKind, StatusCode* out) override;                   // 22, 0x5867e0
    virtual int GetCheckFunctions(MissionCheckFunctions* out) override;                       // 111, 0x5868a0

    static int BuildEventKinds();  // 0x586ad0
};

class MissionCharacterTargetBase : public MissionRuleBase {
public:
    MissionCharacterTargetBase();           // 0x586c20
    virtual ~MissionCharacterTargetBase();  // 0x586c60 (scalar deleting destructor 0x586c40)

    virtual int GetEventKinds(int unused, EventKindSet* out) override;                        // 15, 0x586d20
    virtual int CheckOrder(int a, int orderKind, StatusCode* out) override;                   // 22, 0x586d40
    virtual int GetCheckFunctions(MissionCheckFunctions* out) override;                       // 111, 0x586e00

    static int BuildEventKinds();  // 0x587060
};

class MissionObjectTargetBase : public MissionRuleBase {
public:
    MissionObjectTargetBase();           // 0x5933a0
    virtual ~MissionObjectTargetBase();  // 0x5933e0 (scalar deleting destructor 0x5933c0)

    virtual int GetEventKinds(int unused, EventKindSet* out) override;                        // 15, 0x593400
    virtual int CheckOrder(int a, int orderKind, StatusCode* out) override;                   // 22, 0x593420
    virtual int GetCheckFunctions(MissionCheckFunctions* out) override;                       // 111, 0x5934e0

    static int BuildEventKinds();  // 0x593690
};

class MissionStoryBase : public MissionRuleBase {
public:
    MissionStoryBase();           // 0x5941c0
    virtual ~MissionStoryBase();  // 0x594200 (scalar deleting destructor 0x5941e0)

    virtual int GetEventKinds(int unused, EventKindSet* out) override;                        // 15, 0x594220
    virtual int CheckOrder(int a, int orderKind, StatusCode* out) override;                   // 22, 0x594240
    virtual int GetCheckFunctions(MissionCheckFunctions* out) override;                       // 111, 0x594300
    virtual int OnPhaseChanged(int oldValue, int newValue, NotifyContext* ctx) override;      // 125, 0x594460

    static int BuildEventKinds();  // 0x594480
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(MissionRuleBase, 0xa8);
REB_ASSERT_SIZE(MissionSystemTargetBase, 0xa8);
REB_ASSERT_SIZE(MissionCharacterTargetBase, 0xa8);
REB_ASSERT_SIZE(MissionObjectTargetBase, 0xa8);
REB_ASSERT_SIZE(MissionStoryBase, 0xa8);
#pragma clang diagnostic pop
