#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "tactical/tactical_simulator.h"

// The id-keyed trees and nodes the tactical engine keeps its unit and group collections in (ship-group module,
// 0x59cd60-0x5a7500 and 0x5c5b30-0x5d2360). All are IdAvlTree or AvlTree subclasses that add a few virtual functions;
// the nodes only point at the unit or group they stand for.

class ShipGroup;

// The capital-ship / fighter class is SimUnit (tactical_simulator.h). Its id (the registry handle) is at +0xec.

// A node of a UnitRefTree; the class lives in the formation module (constructor 0x5e9cc0, VT 0x66d820, 0x20 bytes).
class UnitRefNode : public IdNode {
public:
    virtual int Key();             // slot 1
    virtual AvlNode* Next();       // slot 2
    virtual AvlNode* Previous();   // slot 3
    virtual SimUnit* Value();      // slot 4

    SimUnit* m_unit;  // +0x1c
};
UnitRefNode* __thiscall FUN_005e9cc0(void* memory, SimUnit* unit);

// Tree of UnitRefNodes keyed by the unit's handle (VT 0x66bdd0): a battle side's fleet, hangars and group members.
class UnitRefTree : public IdAvlTree {
public:
    UnitRefTree() {}
    virtual ~UnitRefTree();                         // 0x5c5ff0 (scalar deleting destructor 0x5a0bd0)
    virtual uint32_t AddUnique(UnitRefNode* node);  // 0x5c6000 slot 3
    virtual AvlNode* FirstNode();                   // slot 4 (0x5f6590 is AvlTree::First)
    virtual AvlNode* LastNode();                    // slot 5 (0x5c6030)

    uint32_t AddUnit(SimUnit* unit);      // 0x5c5f20 adds a node for the unit unless its handle is already there
    int RemoveUnit(SimUnit* unit);        // 0x5c5fb0
    int RemoveById(int handle);           // 0x5c60d0
    SimUnit* FirstUnit();                 // 0x5c6070
    IdNode* Find(int handle);             // 0x5ca4b0
};

// Tree whose nodes are never "less" than each other, so insertion order is kept (ids are not unique per se).
class InsertionOrderTree : public IdAvlTree {
public:
    InsertionOrderTree();                 // 0x5c6150
    virtual ~InsertionOrderTree();        // 0x5c6190 (scalar deleting destructor 0x5c6170)
    virtual int IsLess(AvlNode* a, AvlNode* b) override;  // 0x5c61f0 always 0

    uint32_t AddUniqueLinear(IdNode* node);  // 0x5c61a0 adds unless a node with that id is there
    IdNode* FindLinear(uint32_t id);         // 0x5c61d0 (walks the successor links)
    void RemoveLinear(uint32_t id);          // 0x5c6200 removes and deletes the node
};

// The two lists of targets / formation members a group's order keeps (VT 0x66bdb8 and 0x66cda0).
class TargetOrderTree : public InsertionOrderTree {
public:
    TargetOrderTree();                    // 0x5a1750
    virtual ~TargetOrderTree();           // 0x5c5b60 (scalar deleting destructor 0x5a17c0)
    virtual AvlNode* FirstNode();         // slot 3
    virtual uint32_t AddUniqueChecked(IdNode* node);  // 0x5c5d80 slot 4
};

class FormationOrderTree : public InsertionOrderTree {
public:
    FormationOrderTree() {}
    virtual ~FormationOrderTree();        // 0x5d2180 (scalar deleting destructor 0x5cbcb0)
    virtual uint32_t AddUniqueLinearSlot(IdNode* node);  // 0x5cbc90 slot 3
    virtual AvlNode* FirstNode();         // 0x5cbca0 slot 4
};

// ---- ship groups by id ----------------------------------------------------------------------------------------------

// A node standing for a ship group, keyed by the group's id (VT 0x66cdb8, 0x20 bytes).
class GroupRefNode : public IdNode {
public:
    explicit GroupRefNode(ShipGroup* group);  // 0x5ca350
    virtual ~GroupRefNode();                  // 0x5ca3d0 (scalar deleting destructor 0x5ca3b0)
    virtual AvlNode* Next();                  // slot 1 (0x5c7530)
    virtual AvlNode* Previous();              // slot 2 (0x5e9d30)
    virtual ShipGroup* Value();               // slot 3 (0x52bed0)

    ShipGroup* m_group;  // +0x1c
};

// Tree of GroupRefNodes (VT 0x66c390): the groups a battle side owns.
class GroupRefTree : public IdAvlTree {
public:
    GroupRefTree() {}
    GroupRefTree(const GroupRefTree& other);  // 0x5ca440
    virtual ~GroupRefTree();                  // 0x5ca3e0 (scalar deleting destructor 0x5ac6a0)
    virtual AvlNode* FirstNode();             // slot 3
    virtual ShipGroup* FirstGroup();          // 0x5ca490 slot 4
    virtual IdNode* Find(int id);             // 0x5ca4b0 slot 5

    GroupRefTree& operator=(const GroupRefTree& other);  // 0x5ca3f0
    GroupRefNode* AddGroup(ShipGroup* group);            // 0x5ca4c0
};

// ---- targets ordered by priority ------------------------------------------------------------------------------------

// Compares two unit pointers by handle; a null pointer never orders and equals only another null.
struct UnitRefSlot {
    SimUnit* m_unit;
    int SameUnit(UnitRefSlot* other);    // 0x5ced00
    int UnitBefore(UnitRefSlot* other);  // 0x5ced40
};

// A unit with a priority (VT 0x66cc6c, 0x20 bytes). Lower priority sorts first.
class TargetPriorityNode : public AvlNode {
public:
    TargetPriorityNode(float priority, SimUnit* unit);
    virtual ~TargetPriorityNode();  // 0x5c96f0 (scalar deleting destructor 0x5c96d0)

    float m_priority;  // +0x18
    UnitRefSlot m_unit;  // +0x1c
};

// AvlTree of TargetPriorityNodes (VT 0x66cc38): by priority, then by unit handle.
class TargetPriorityTree : public AvlTree {
public:
    TargetPriorityTree() {}
    virtual ~TargetPriorityTree();                         // 0x5c9600 (scalar deleting destructor 0x5c9670)
    virtual int IsLess(AvlNode* a, AvlNode* b) override;   // 0x5c9610
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(UnitRefTree, 0x0c);
REB_ASSERT_SIZE(InsertionOrderTree, 0x0c);
REB_ASSERT_SIZE(GroupRefNode, 0x20);
REB_ASSERT_SIZE(GroupRefTree, 0x0c);
REB_ASSERT_SIZE(TargetPriorityNode, 0x20);
REB_ASSERT_OFFSET(TargetPriorityNode, m_unit, 0x1c);
REB_ASSERT_SIZE(TargetPriorityTree, 0x08);
#pragma clang diagnostic pop
