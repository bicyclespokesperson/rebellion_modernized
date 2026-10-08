#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "tactical/tactical_lists.h"
#include "tactical/tactical_math.h"

// Formations of ship groups (formation module, 0x5e9b60-0x5ee590, part of the simulation: auto-resolve builds and
// updates them). A formation lists the units of a group (each with a FormationSlot) and keeps the group's position,
// heading and the offset every member holds from the leader.

// The movement state shared by everything that flies (VT 0x66c648, 0xd4 bytes; the slots are in the object module).
class MovingObject {
public:
    MovingObject();            // 0x5ebc90
    virtual ~MovingObject();   // 0x5cd2c0 (scalar deleting destructor 0x5b2420)
    virtual void MoveVirtual01();  // 0x5cd460
    virtual void MoveVirtual02();  // 0x5cd640
    virtual void MoveVirtual03();  // 0x5cd6b0
    virtual void MoveVirtual04();  // 0x5cd000
    virtual void MoveVirtual05();  // 0x5cd0f0
    virtual void MoveVirtual06();  // 0x5cd1e0

    float m_scale;             // +0x04 1.0
    int m_unk08;               // +0x08
    int m_unk0c;               // +0x0c
    TacticalVec4 m_heading;    // +0x10 (0, 0, 1)
    TacticalVec4 m_v20;        // +0x20
    TacticalVec4 m_position;   // +0x30
    TacticalVec4 m_v40;        // +0x40
    int m_unk50;               // +0x50
    int m_unk54;               // +0x54
    int m_unk58;               // +0x58
    TacticalVec4 m_v5c;        // +0x5c
    TacticalVec4 m_v6c;        // +0x6c
    TacticalVec4 m_v7c;        // +0x7c
    TacticalVec4 m_v8c;        // +0x8c
    TacticalVec4 m_v9c;        // +0x9c
    TacticalVec4 m_vac;        // +0xac
    TacticalVec4 m_vbc;        // +0xbc
    int m_unkcc;               // +0xcc
    int m_unkd0;               // +0xd0
};

// A unit's place in a formation (VT 0x66d968 / 0x66d948, 0xf4 bytes): the node, then the slot's own movement state. The slot's
// offset from the formation's centre is kept in the movement part's m_position (+0x50).
class FormationSlot : public UnitRefNode, public MovingObject {
public:
    explicit FormationSlot(SimUnit* unit);
    virtual ~FormationSlot();  // 0x5f2a90 (scalar deleting destructor 0x5ec5f0)
};

// Lists of slots keyed by the unit's handle; the formation keeps two of them (VT 0x66d938).
class FormationSlotTree : public InsertionOrderTree {
public:
    FormationSlotTree() {}
    virtual ~FormationSlotTree();  // 0x5f2af0 (scalar deleting destructor 0x5ec610)
};

// Slots sorted by (float, int) (VT 0x66d928).
class FormationRankTree : public AvlTree {
public:
    FormationRankTree() {}
    virtual ~FormationRankTree();                         // 0x5ec5e0 (scalar deleting destructor 0x5ec5c0)
    virtual int IsLess(AvlNode* a, AvlNode* b) override;  // 0x5ec570
};

// The formation of a group (0x140 bytes; the class of a group's formation is the base of CapitalShipFormation and
// FighterFormation). It is the group's unit tree (UnitRefTree at +0) and a MovingObject at +0xc.
class FormationBase : public UnitRefTree, public MovingObject {
public:
    FormationBase(int groupHandle, TacticalVec4 position);  // 0x5ea030
    virtual ~FormationBase();                               // 0x5ea590 (scalar deleting destructor 0x5ec630)
    virtual void Virtual06() = 0;
    virtual int AddMember(int handle);                      // 0x5ea250 slot 7
    virtual int RemoveMember(int handle);                   // 0x5ea630 slot 8
    virtual void Virtual09();                               // 0x5eb040
    virtual void Virtual10();                               // 0x5ebdf0
    virtual void Virtual11();                               // 0x5ea500
    virtual TacticalVec4* SlotOffset(TacticalVec4* out, SimUnit* unit) = 0;  // slot 12
    virtual float Virtual13();                              // 0x5ea4e0 (the formation's speed)
    virtual void Virtual14();                               // 0x5c7310
    virtual void Virtual15();                               // 0x5c7350
    virtual void Virtual16();                               // 0x5ea740
    virtual void Virtual17();                               // 0x5eb190
    virtual void MoveVirtual01() override;                  // 0x5eae40
    virtual void MoveVirtual03() override;                  // 0x5eb540
    virtual void MoveVirtual04() override;                  // 0x5e9e00
    virtual void MoveVirtual05() override;                  // 0x5e9ec0
    virtual void MoveVirtual06() override;                  // 0x5e9f80

    FormationSlotTree m_slotsByUnit;    // +0xe0
    FormationSlotTree m_slotsByUnit2;   // +0xec
    FormationRankTree m_ranks;          // +0xf8
    int m_leaderHandle;                 // +0x100
    TacticalVec4 m_extentA;             // +0x104
    TacticalVec4 m_extentB;             // +0x114
    int m_unk124;                       // +0x124
    float m_unk128;                     // +0x128 4.0
    unsigned m_count;                   // +0x12c units in the formation
    unsigned m_maxCount;                // +0x130 the most units it has held
    int m_unk134;                       // +0x134
    float m_angle;                      // +0x138 +-pi / 2 by the side
    float m_radius;                     // +0x13c the biggest unit radius (times 1.2), at least 12
};

class CapitalShipFormation : public FormationBase {  // VT 0x66cb50 / 0x66cb30, 0x144 bytes
public:
    CapitalShipFormation(int groupHandle, TacticalVec4 position);  // 0x5c72b0
    virtual ~CapitalShipFormation();                               // 0x5ebdd0 (scalar deleting destructor 0x5c7360)
    virtual void Virtual06() override;                             // 0x526f00
    virtual void Virtual09() override;                             // 0x5ebe30
    virtual TacticalVec4* SlotOffset(TacticalVec4* out, SimUnit* unit) override;  // 0x5ebee0
    virtual float Virtual13() override;                            // 0x5ec0b0
    virtual void MoveVirtual03() override;                         // 0x5ec0d0
    virtual void MoveVirtual04() override;                         // 0x5ec510
    virtual void MoveVirtual05() override;                         // 0x5ec4e0
    virtual void MoveVirtual06() override;                         // 0x5ec540

    int m_unk140;  // +0x140
};

class FighterFormation : public FormationBase {  // VT 0x66da08 / 0x66d9e8, 0x140 bytes
public:
    FighterFormation(int groupHandle, TacticalVec4 position);  // 0x5ec310
    virtual ~FighterFormation();                               // 0x5ec3e0 (scalar deleting destructor 0x5ec670)
    virtual void Virtual06() override;                         // 0x43c1f0
    virtual TacticalVec4* SlotOffset(TacticalVec4* out, SimUnit* unit) override;  // 0x5ec400
    virtual float Virtual13() override;                        // 0x5ec3c0
    virtual void MoveVirtual03() override;                     // 0x5eb1a0
};

extern TacticalVec4 g_formationOrigin;  // 0x6bc940

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(MovingObject, 0xd4);
REB_ASSERT_SIZE(FormationSlot, 0xf4);
REB_ASSERT_OFFSET(MovingObject, m_position, 0x30);
REB_ASSERT_OFFSET(MovingObject, m_vbc, 0xbc);
REB_ASSERT_OFFSET(FormationBase, m_slotsByUnit, 0xe0);
REB_ASSERT_OFFSET(FormationBase, m_ranks, 0xf8);
REB_ASSERT_OFFSET(FormationBase, m_leaderHandle, 0x100);
REB_ASSERT_OFFSET(FormationBase, m_count, 0x12c);
REB_ASSERT_OFFSET(FormationBase, m_radius, 0x13c);
REB_ASSERT_SIZE(FormationBase, 0x140);
REB_ASSERT_SIZE(CapitalShipFormation, 0x144);
REB_ASSERT_SIZE(FighterFormation, 0x140);
#pragma clang diagnostic pop
