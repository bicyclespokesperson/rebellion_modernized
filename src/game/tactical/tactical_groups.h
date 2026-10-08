#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "tactical/tactical_formation.h"
#include "tactical/tactical_lists.h"
#include "tactical/tactical_units.h"

// Ship groups of the tactical engine (ship-group module 0x59cd60-0x5a7500, tactical AI and formations 0x5c5b30-0x5d2360):
// the battle sides organise their capital ships (kind 4) and fighter squadrons (kind 3) into groups, give each group an
// order (a mode of the group's state machine) and a formation. Groups are registered in the TacticalRegistry.

class TacticalSideBase;

// The formation object of a group (tactical_formation.h).

// The embedded link a behavior object keeps (VT 0x66c678, 0x18 bytes: an AvlNode with three virtual functions).
class BehaviorLink : public AvlNode {
public:
    BehaviorLink() {}
    virtual ~BehaviorLink();  // 0x5b8240 (scalar deleting destructor 0x5b8220)
    virtual void Virtual01();  // 0x5b7dc0
    virtual void Virtual02();  // 0x5b7e30
    virtual void Virtual03();  // 0x5b7e40
};

// What a group is currently doing about its target (0x3c bytes). The base class is abstract (VT 0x66ccf0); the one a
// ship group creates is ShipGroupBehavior (VT 0x66ccd0).
class ShipGroup;
class GroupBehavior {
public:
    explicit GroupBehavior(ShipGroup* group);  // 0x5cbd20
    virtual ~GroupBehavior();                  // 0x5cbd90 (scalar deleting destructor 0x5cbd70)
    virtual void Virtual01() = 0;
    virtual void Virtual02() = 0;
    virtual void Virtual03() = 0;
    virtual void Virtual04() = 0;
    virtual void Virtual05();  // 0x5edd50
    virtual void Virtual06();  // 0x5eddf0
    virtual void Virtual07();  // 0x5ede90

    int m_unk04;            // +0x04
    ShipGroup* m_group;     // +0x08
    float m_range;          // +0x0c 5.0 by default
    int m_unk10;            // +0x10
    int m_unk14;            // +0x14
    BehaviorLink m_link;    // +0x18
    int m_unk30;            // +0x30
    float m_rangeNow;       // +0x34 starts as m_range
    int m_unk38;            // +0x38
};

class ShipGroupBehavior : public GroupBehavior {
public:
    explicit ShipGroupBehavior(ShipGroup* group);
    virtual ~ShipGroupBehavior();
    virtual void Virtual01() override;  // 0x5edf80
    virtual void Virtual02() override;  // 0x5ee250
    virtual void Virtual03() override;  // 0x5ee280
    virtual void Virtual04() override;  // 0x5ee290
};

// ---- orders -------------------------------------------------------------------------------------------------------

// What a group is ordered to do (0x24 bytes). A group's mode (ShipGroup::SetMode) selects the class.
class GroupOrder {
public:
    explicit GroupOrder(ShipGroup* group);
    virtual ~GroupOrder();      // 0x5cbd10 (scalar deleting destructor 0x5cbcf0)
    // The slots below are implemented in the formation module (not reconstructed); the stubs only complete the vtable
    // that the original one replaces. 0x5cf910 / 0x5cf920 / 0x403e90 / 0x5c7530 / 0x5ceee0 / 0x5ceda0 / 0x5cee20 /
    // 0x5cee90 / 0x619730 are slots 3-11, 0x5ae1e0 or the mode's own function slots 1 and 2.
    virtual void Virtual01() = 0;
    virtual void Virtual02() = 0;
    virtual int Virtual03() { return 0; }
    virtual int Virtual04() { return 0; }
    virtual int Virtual05() { return 0; }
    virtual int Virtual06() { return 0; }
    virtual int Virtual07() { return 0; }
    virtual int Virtual08() { return 0; }
    virtual int Virtual09() { return 0; }
    virtual int Virtual10() { return 0; }
    virtual int Virtual11() { return 0; }

    int m_unk04;                  // +0x04
    ShipGroup* m_group;           // +0x08
    int m_unk0c;                  // +0x0c
    int m_unk10;                  // +0x10
    int m_unk14;                  // +0x14
    TargetOrderTree m_targets;    // +0x18
};

#define REB_DECLARE_GROUP_ORDER(Name)                                  \
    class Name : public GroupOrder {                                   \
    public:                                                            \
        explicit Name(ShipGroup* group);                               \
        virtual void Virtual01() override {}                           \
        virtual void Virtual02() override {}                           \
    }

REB_DECLARE_GROUP_ORDER(GroupOrderMode1);
REB_DECLARE_GROUP_ORDER(GroupOrderMode2);
REB_DECLARE_GROUP_ORDER(GroupOrderMode3);
REB_DECLARE_GROUP_ORDER(GroupOrderMode4);
REB_DECLARE_GROUP_ORDER(GroupOrderMode5);
REB_DECLARE_GROUP_ORDER(GroupOrderMode7);
REB_DECLARE_GROUP_ORDER(GroupOrderMode8);
REB_DECLARE_GROUP_ORDER(GroupOrderMode9);
REB_DECLARE_GROUP_ORDER(GroupOrderMode10);
REB_DECLARE_GROUP_ORDER(GroupOrderMode11);
#undef REB_DECLARE_GROUP_ORDER

// The order of mode 6 (0x84 bytes, constructor 0x5d0820) is not reconstructed.
GroupOrder* __thiscall FUN_005d0820(void* memory, ShipGroup* group);

// ---- groups -------------------------------------------------------------------------------------------------------

// A group of ships of one side. It is a TacticalUnit (id = the group's handle) and, as a second base at +0x1c, the tree
// of the units that belong to it.
class ShipGroup : public TacticalUnit, public UnitRefTree {
public:
    explicit ShipGroup(int seedHandle);  // 0x5c9db0
    virtual ~ShipGroup();                // 0x5ca1e0 (scalar deleting destructor 0x5c9f90)
    virtual void SetSeedHandle(int handle);  // 0x5ca1a0 slot 2
    virtual void Virtual03();                // 0x5c9bd0
    virtual void Virtual04();                // 0x5c65e0
    virtual void Virtual05() = 0;            // capital ships 0x5c6610, fighters 0x5cc300
    virtual void Virtual06();                // 0x5cb050
    virtual void Virtual07();                // 0x5cb600
    virtual void Virtual08();                // 0x5cb430
    virtual void Virtual09();                // 0x5cb490
    virtual void AddMember(int handle);      // 0x5cb3e0 slot 10
    virtual void Virtual11();                // 0x5cb460
    virtual void RemoveMember(int handle);   // 0x5cb510 slot 12
    virtual void Virtual13();                // 0x5cb4d0
    virtual void Virtual14();                // 0x5cb5b0
    virtual void Virtual15();                // 0x5cb8e0
    virtual TacticalVector* GetPosition(TacticalVector* out);  // 0x5cafa0 slot 16
    virtual void Virtual17();                // 0x5cb990
    virtual void SetMode(int mode);          // 0x5ca6d0 slot 18
    virtual void Virtual19();                // 0x5cb770
    virtual void Virtual20();                // 0x5cb7b0
    virtual void Virtual21(int handle);      // 0x5cb870
    virtual void Virtual22();                // 0x5cb840
    virtual void Virtual23();                // 0x5cb7e0
    virtual void Virtual24();                // 0x5cb810
    virtual void Virtual25();                // 0x5ca530
    virtual void Virtual26();                // 0x5c9940
    virtual void Virtual27();                // 0x5c9780
    virtual void Virtual28();                // 0x5c9ae0

    float MemberStrengthAgainst(TacticalShipBase* target);  // 0x5cb650

    ShipGroup(int seedHandle, int id);  // 0x5c9fb0 the variant that takes the group's id

    TurnSyncChannelA m_channelA;      // +0x28
    TurnSyncChannelB m_channelB;      // +0x30
    int m_unk38;                      // +0x38
    int m_unk3c;                      // +0x3c
    GroupBehavior* m_behavior;        // +0x40
    UnitRefTree m_founders;           // +0x44 the unit the group was created for
    int m_side;                       // +0x50 the side of the seed unit
    FormationBase* m_formation;       // +0x54
    int m_unk58;                      // +0x58
    int m_unk5c;                      // +0x5c
    GroupOrder* m_order;              // +0x60
    int m_mode;                       // +0x64 see SetMode
    int m_unk68;                      // +0x68
    int m_firstMember;                // +0x6c handle of the first member
    int m_seedHandle;                 // +0x70
    TargetOrderTree m_targetsA;       // +0x74
    TargetOrderTree m_targetsB;       // +0x80
    FormationOrderTree m_formationA;  // +0x8c
    FormationOrderTree m_formationB;  // +0x98
    FormationOrderTree m_formationC;  // +0xa4
    int m_unkb0;                      // +0xb0
    int m_unkb4;                      // +0xb4
    int m_unkb8;                      // +0xb8
};

class CapitalShipGroup : public ShipGroup {
public:
    explicit CapitalShipGroup(int seedHandle);  // 0x5c62b0
    virtual ~CapitalShipGroup();                // scalar deleting destructor 0x5a6710
    virtual void Virtual01() override;          // 0x5a6700 returns 4
    virtual void Virtual05() override;          // 0x5c6610
    virtual void Virtual06() override;          // 0x5c7060
    virtual void SetMode(int mode) override;    // 0x5c7130
    virtual void Virtual26() override;          // 0x5c6270
    virtual void Virtual27() override;          // 0x5c6230

    void SetRole(int role);  // 0x5c7140

    int m_role;   // +0xbc 4 until the simulator names one
    int m_unkc0;  // +0xc0
};

class FighterGroup : public ShipGroup {
public:
    explicit FighterGroup(int seedHandle);  // 0x5cbf50
    virtual ~FighterGroup();                // scalar deleting destructor 0x5a6740
    virtual void Virtual01() override;      // 0x5a6730 returns 3
    virtual void Virtual05() override;      // 0x5cc300
};

// The group an id-tree node of the registry stands for (a group starts with its TacticalUnit, which is the node).
inline ShipGroup* ShipGroupOf(IdNode* node) { return static_cast<ShipGroup*>(static_cast<TacticalUnit*>(node)); }

// Removes a unit from its group (a null group is ignored); 0x5caed0.
void __stdcall RemoveMemberFromGroup(ShipGroup* group, int handle);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(BehaviorLink, 0x18);
REB_ASSERT_SIZE(GroupBehavior, 0x3c);
REB_ASSERT_OFFSET(GroupBehavior, m_link, 0x18);
REB_ASSERT_SIZE(GroupOrder, 0x24);
REB_ASSERT_OFFSET(GroupOrder, m_targets, 0x18);
REB_ASSERT_OFFSET(ShipGroup, m_channelA, 0x28);
REB_ASSERT_OFFSET(ShipGroup, m_founders, 0x44);
REB_ASSERT_OFFSET(ShipGroup, m_side, 0x50);
REB_ASSERT_OFFSET(ShipGroup, m_order, 0x60);
REB_ASSERT_OFFSET(ShipGroup, m_targetsA, 0x74);
REB_ASSERT_OFFSET(ShipGroup, m_formationA, 0x8c);
REB_ASSERT_OFFSET(ShipGroup, m_unkb4, 0xb4);
REB_ASSERT_SIZE(ShipGroup, 0xbc);
REB_ASSERT_SIZE(CapitalShipGroup, 0xc4);
REB_ASSERT_SIZE(FighterGroup, 0xbc);
#pragma clang diagnostic pop
