#pragma once
#include <cstdint>

#include "core/containers.h"
#include "tactical/tactical_lists.h"
#include "tactical/tactical_math.h"
#include "tactical/tactical_moving_object.h"

// Formations of ship groups (formation module, 0x5e9b60-0x5ee590, part of the simulation: auto-resolve builds and
// updates them). A formation lists the units of a group (each with a FormationSlot) and keeps the group's position,
// heading and the offset every member holds from the leader.

// A unit's place in a formation (VT 0x66d968 / 0x66d948, 0xf4 bytes): the node, then the slot's own movement state. The slot's
// offset from the formation's centre is kept in the movement part's m_position (+0x50).
class FormationSlot : public UnitRefNode, public MovingObject {
public:
    explicit FormationSlot(TacticalShipBase* unit);
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
    FormationBase(int groupHandle, TacticalVector position);  // 0x5ea030
    virtual ~FormationBase();                               // 0x5ea590 (scalar deleting destructor 0x5ec630)
    virtual void Virtual06() = 0;
    virtual int AddMember(int handle);                      // 0x5ea250 slot 7
    virtual int RemoveMember(int handle);                   // 0x5ea630 slot 8
    virtual void Virtual09(int a, int b);                               // 0x5eb040
    virtual void Virtual10(int a, int b);                               // 0x5ebdf0
    virtual void Virtual11(int a, int b);                               // 0x5ea500
    virtual TacticalVector* SlotOffset(TacticalVector* out, TacticalShipBase* unit) = 0;  // slot 12
    virtual float Virtual13();                              // 0x5ea4e0 (the formation's speed)
    virtual void Virtual14(int a);                               // 0x5c7310
    virtual void Virtual15();                               // 0x5c7350
    virtual void Virtual16();                               // 0x5ea740
    virtual void Virtual17(int a);                               // 0x5eb190
    virtual double Turn(float step, float rate) override;   // 0x5eae40
    virtual int Virtual03(int a, int b, int c) override;  // 0x5eb540
    virtual void Read(istream* in) override;                  // 0x5e9e00
    virtual void Write(ostream* out) override;                  // 0x5e9ec0
    virtual void Checksum(Crc32* crc) override;                  // 0x5e9f80

    FormationSlotTree m_slotsByUnit;    // +0xe0
    FormationSlotTree m_slotsByUnit2;   // +0xec
    FormationRankTree m_ranks;          // +0xf8
    int m_leaderHandle;                 // +0x100
    TacticalVector m_extentA;             // +0x104
    TacticalVector m_extentB;             // +0x114
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
    CapitalShipFormation(int groupHandle, TacticalVector position);  // 0x5c72b0
    virtual ~CapitalShipFormation();                               // 0x5ebdd0 (scalar deleting destructor 0x5c7360)
    virtual void Virtual06() override;                             // 0x526f00
    virtual void Virtual09(int a, int b) override;                             // 0x5ebe30
    virtual TacticalVector* SlotOffset(TacticalVector* out, TacticalShipBase* unit) override;  // 0x5ebee0
    virtual float Virtual13() override;                            // 0x5ec0b0
    virtual int Virtual03(int a, int b, int c) override;     // 0x5ec0d0
    virtual void Read(istream* in) override;                         // 0x5ec510
    virtual void Write(ostream* out) override;                         // 0x5ec4e0
    virtual void Checksum(Crc32* crc) override;                         // 0x5ec540

    int m_unk140;  // +0x140
};

class FighterFormation : public FormationBase {  // VT 0x66da08 / 0x66d9e8, 0x140 bytes
public:
    FighterFormation(int groupHandle, TacticalVector position);  // 0x5ec310
    virtual ~FighterFormation();                               // 0x5ec3e0 (scalar deleting destructor 0x5ec670)
    virtual void Virtual06() override;                         // 0x43c1f0
    virtual TacticalVector* SlotOffset(TacticalVector* out, TacticalShipBase* unit) override;  // 0x5ec400
    virtual float Virtual13() override;                        // 0x5ec3c0
    virtual int Virtual03(int a, int b, int c) override;     // 0x5eb1a0
};

extern TacticalVector g_formationOrigin;  // 0x6bc940
