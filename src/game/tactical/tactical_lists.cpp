#include "tactical/tactical_lists.h"

#include <new>

#include "core/checked_new.h"
#include "tactical/tactical_ship.h"

// ---- UnitRefNode ----------------------------------------------------------------------------------------------------

UnitRefNode::UnitRefNode(TacticalShipBase* unit) {
    m_unit = unit;
    SetId(unit->m_id);
}

UnitRefNode::~UnitRefNode() = default;

int UnitRefNode::Key() { return m_id; }

UnitRefNode* UnitRefNode::Next() { return static_cast<UnitRefNode*>(m_successor); }

UnitRefNode* UnitRefNode::Previous() { return static_cast<UnitRefNode*>(Predecessor()); }

TacticalShipBase* UnitRefNode::Value() { return m_unit; }

// ---- UnitRefTree ----------------------------------------------------------------------------------------------------

UnitRefTree::~UnitRefTree() = default;

uint32_t UnitRefTree::AddUnique(UnitRefNode* node) {
    if (!Find(node->Key())) return AddWithId(node);
    return 0;
}

UnitRefNode* UnitRefTree::FirstNode() { return static_cast<UnitRefNode*>(First()); }

UnitRefNode* UnitRefTree::LastNode() { return static_cast<UnitRefNode*>(Last()); }

uint32_t UnitRefTree::AddUnit(TacticalShipBase* unit) {
    if (unit && !Find(unit->m_id)) {
        void* memory = ::operator new(sizeof(UnitRefNode));
        UnitRefNode* node = memory ? new (memory) UnitRefNode(unit) : nullptr;
        return AddWithId(node);
    }
    return 0;
}

int UnitRefTree::RemoveUnit(TacticalShipBase* unit) {
    if (unit && Find(unit->m_id)) {
        DeleteById(unit->m_id);
        return 1;
    }
    return 0;
}

int UnitRefTree::RemoveById(int handle) {
    if (Find(handle)) {
        DeleteById(handle);
        return 1;
    }
    return 0;
}

TacticalShipBase* UnitRefTree::FirstUnit() {
    if (FirstNode()) return FirstNode()->Value();
    return nullptr;
}

IdNode* UnitRefTree::Find(int handle) { return FindById(handle); }

int UnitRefTree::CountNodes() {
    int count = 0;
    for (UnitRefNode* node = FirstNode(); node; node = node->Next()) ++count;
    return count;
}

// ---- InsertionOrderTree ---------------------------------------------------------------------------------------------

InsertionOrderTree::InsertionOrderTree() = default;

InsertionOrderTree::~InsertionOrderTree() = default;

int InsertionOrderTree::IsLess(AvlNode*, AvlNode*) { return 0; }

IdNode* InsertionOrderTree::FindLinear(uint32_t id) {
    for (AvlNode* node = First(); node; node = node->m_successor) {
        if (static_cast<IdNode*>(node)->m_id == id) return static_cast<IdNode*>(node);
    }
    return nullptr;
}

uint32_t InsertionOrderTree::AddUniqueLinear(IdNode* node) {
    if (!FindLinear(node->m_id)) return AddWithId(node);
    return 0;
}

void InsertionOrderTree::RemoveLinear(uint32_t id) {
    IdNode* node = FindLinear(id);
    if (node) {
        Remove(node);
        delete node;
    }
}

uint32_t InsertionOrderTree::AddUnitLinear(TacticalShipBase* unit) {
    if (FindLinear(unit->m_id)) return 0;
    void* memory = ::operator new(sizeof(UnitRefNode));
    UnitRefNode* node = memory ? new (memory) UnitRefNode(unit) : nullptr;
    return AddUniqueLinear(node);
}

TargetOrderTree::TargetOrderTree() = default;

TargetOrderTree::~TargetOrderTree() = default;

AvlNode* TargetOrderTree::FirstNode() { return First(); }

uint32_t TargetOrderTree::AddUniqueChecked(IdNode* node) {
    if (!FindLinear(node->m_id)) return AddUniqueLinear(node);
    return 0;
}

FormationOrderTree::~FormationOrderTree() = default;

uint32_t FormationOrderTree::AddUniqueLinearSlot(IdNode* node) { return AddUniqueLinear(node); }

AvlNode* FormationOrderTree::FirstNode() { return First(); }

TacticalShipBase* FormationOrderTree::FirstUnit() {
    UnitRefNode* node = static_cast<UnitRefNode*>(FirstNode());
    if (node) return node->Value();
    return nullptr;
}

// ---- ship groups by id ----------------------------------------------------------------------------------------------

GroupRefNode::GroupRefNode(ShipGroup* group) {
    m_group = group;
    SetId(reinterpret_cast<IdNode*>(group)->m_id);
}

GroupRefNode::~GroupRefNode() = default;

AvlNode* GroupRefNode::Next() { return m_successor; }

AvlNode* GroupRefNode::Previous() { return Predecessor(); }

ShipGroup* GroupRefNode::Value() { return m_group; }

GroupRefTree::GroupRefTree(const GroupRefTree& other) { *this = other; }

GroupRefTree::~GroupRefTree() = default;

AvlNode* GroupRefTree::FirstNode() { return First(); }

ShipGroup* GroupRefTree::FirstGroup() {
    AvlNode* first = FirstNode();
    return first ? static_cast<GroupRefNode*>(first)->Value() : nullptr;
}

IdNode* GroupRefTree::Find(int id) { return FindById(id); }

GroupRefTree& GroupRefTree::operator=(const GroupRefTree& other) {
    if (&other != this) {
        Clear();
        for (AvlNode* node = const_cast<GroupRefTree&>(other).FirstNode(); node;) {
            GroupRefNode* groupNode = static_cast<GroupRefNode*>(node);
            AddGroup(groupNode->Value());
            node = groupNode->Next();
        }
    }
    return *this;
}

GroupRefNode* GroupRefTree::AddGroup(ShipGroup* group) {
    void* memory = ::operator new(sizeof(GroupRefNode));
    GroupRefNode* node = memory ? new (memory) GroupRefNode(group) : nullptr;
    AddWithId(node);
    return node;
}

// ---- targets ordered by priority ------------------------------------------------------------------------------------

int UnitRefSlot::SameUnit(UnitRefSlot* other) {
    if (m_unit && other->m_unit) return m_unit->m_id == other->m_unit->m_id;
    return m_unit == other->m_unit;
}

int UnitRefSlot::UnitBefore(UnitRefSlot* other) {
    if (m_unit && other->m_unit) return m_unit->m_id < other->m_unit->m_id;
    return 0;
}

TargetPriorityNode::TargetPriorityNode(float priority, TacticalShipBase* unit) {
    m_priority = priority;
    m_unit.m_unit = unit;
}

TargetPriorityNode::~TargetPriorityNode() = default;

TargetPriorityTree::~TargetPriorityTree() = default;

int TargetPriorityTree::IsLess(AvlNode* a, AvlNode* b) {
    TargetPriorityNode* left = static_cast<TargetPriorityNode*>(a);
    TargetPriorityNode* right = static_cast<TargetPriorityNode*>(b);
    if (left->m_priority == right->m_priority) return left->m_unit.UnitBefore(&right->m_unit);
    return left->m_priority < right->m_priority;
}

UnitRefTree* UnitRefTree::CopyFrom(UnitRefTree* other) {
    if (other != this) {
        for (UnitRefNode* node = other->FirstNode(); node; node = node->Next()) AddUnit(node->Value());
    }
    return this;
}
