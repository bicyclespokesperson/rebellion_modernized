#include "tactical/tactical_lists.h"

#include <cmath>

#include <algorithm>
#include <new>

#include "core/checked_new.h"
#include "tactical/tactical_ship.h"

// ---- UnitRefNode ----------------------------------------------------------------------------------------------------

UnitRefNode::UnitRefNode(TacticalShipBase* unit)
{
  m_unit = unit;
  SetId(unit->m_id);
}

UnitRefNode::~UnitRefNode() = default;

int UnitRefNode::Key()
{
  return m_id;
}

TacticalShipBase* UnitRefNode::Value()
{
  return m_unit;
}

// ---- UnitRefTree ----------------------------------------------------------------------------------------------------

UnitRefTree::~UnitRefTree()
{
  Clear();
}

void UnitRefTree::Clear()
{
  if (m_clearing)
    return;
  m_clearing = true;
  decltype(m_nodes) detached;
  detached.swap(m_nodes);
  m_nextId = 1;
  detached.clear();
  m_clearing = false;
}

int UnitRefTree::Insert(UnitRefNode* node)
{
  if (m_clearing || !node || node->m_id == 0 || FindById(node->m_id))
    return 0;
  std::unique_ptr<UnitRefNode> pending(node);
  try
  {
    m_nodes.insert(std::move(pending));
  }
  catch (...)
  {
    pending.release();
    throw;
  }
  return 1;
}

void UnitRefTree::Remove(UnitRefNode* node)
{
  auto found = std::find_if(m_nodes.begin(), m_nodes.end(),
                            [node](const auto& entry)
                            {
                              return entry.get() == node;
                            });
  if (found == m_nodes.end())
    return;
  auto owned = m_nodes.extract(found);
  owned.value().release();
}

UnitRefNode* UnitRefTree::First() const
{
  return m_nodes.empty() ? nullptr : m_nodes.begin()->get();
}

UnitRefNode* UnitRefTree::Last() const
{
  return m_nodes.empty() ? nullptr : m_nodes.rbegin()->get();
}

UnitRefNode* UnitRefTree::FindById(uint32_t id) const
{
  auto found = std::find_if(m_nodes.begin(), m_nodes.end(),
                            [id](const auto& entry)
                            {
                              return entry->m_id == id;
                            });
  return found == m_nodes.end() ? nullptr : (*found).get();
}

uint32_t UnitRefTree::AddWithId(UnitRefNode* node)
{
  if (!node)
    return 0;
  const bool assignedId = node->m_id == 0;
  uint32_t id = node->m_id;
  if (assignedId)
  {
    if (m_nextId == 0)
      return 0;
    id = m_nextId;
    node->SetId(id);
  }
  if (!Insert(node))
  {
    if (assignedId)
      node->SetId(0);
    return 0;
  }
  if (m_nextId != 0 && m_nextId <= id)
    m_nextId = id + 1;
  return id;
}

void UnitRefTree::DeleteById(uint32_t id)
{
  auto found = std::find_if(m_nodes.begin(), m_nodes.end(),
                            [id](const auto& entry)
                            {
                              return entry->m_id == id;
                            });
  if (found != m_nodes.end())
    m_nodes.erase(found);
}

IdNode* UnitRefTree::DetachById(uint32_t id)
{
  auto found = std::find_if(m_nodes.begin(), m_nodes.end(),
                            [id](const auto& entry)
                            {
                              return entry->m_id == id;
                            });
  if (found == m_nodes.end())
    return nullptr;
  auto owned = m_nodes.extract(found);
  return owned.value().release();
}

uint32_t UnitRefTree::AddUnique(UnitRefNode* node)
{
  if (!Find(node->Key()))
    return AddWithId(node);
  return 0;
}

UnitRefNode* UnitRefTree::FirstNode()
{
  return First();
}

UnitRefNode* UnitRefTree::LastNode()
{
  return Last();
}

uint32_t UnitRefTree::AddUnit(TacticalShipBase* unit)
{
  if (unit && !Find(unit->m_id))
  {
    void* memory = ::operator new(sizeof(UnitRefNode));
    UnitRefNode* node = memory ? new (memory) UnitRefNode(unit) : nullptr;
    return AddWithId(node);
  }
  return 0;
}

int UnitRefTree::RemoveUnit(TacticalShipBase* unit)
{
  if (unit && Find(unit->m_id))
  {
    DeleteById(unit->m_id);
    return 1;
  }
  return 0;
}

int UnitRefTree::RemoveById(int handle)
{
  if (Find(handle))
  {
    DeleteById(handle);
    return 1;
  }
  return 0;
}

TacticalShipBase* UnitRefTree::FirstUnit()
{
  if (FirstNode())
    return FirstNode()->Value();
  return nullptr;
}

IdNode* UnitRefTree::Find(int handle)
{
  return FindById(handle);
}

int UnitRefTree::CountNodes()
{
  return static_cast<int>(Entries().size());
}

// ---- InsertionOrderTree ---------------------------------------------------------------------------------------------

InsertionOrderTree::InsertionOrderTree() = default;

InsertionOrderTree::~InsertionOrderTree()
{
  Clear();
}

void InsertionOrderTree::Clear()
{
  if (m_clearing)
    return;
  m_clearing = true;
  decltype(m_nodes) detached;
  detached.swap(m_nodes);
  m_nextId = 1;
  detached.clear();
  m_clearing = false;
}

UnitRefNode* InsertionOrderTree::First() const
{
  return m_nodes.empty() ? nullptr : m_nodes.front().get();
}

void InsertionOrderTree::Remove(UnitRefNode* node)
{
  auto found = std::find_if(m_nodes.begin(), m_nodes.end(),
                            [node](const auto& entry)
                            {
                              return entry.get() == node;
                            });
  if (found == m_nodes.end())
    return;
  std::unique_ptr<UnitRefNode> owned = std::move(*found);
  m_nodes.erase(found);
  owned.release();
}

UnitRefNode* InsertionOrderTree::FindLinear(uint32_t id)
{
  for (const auto& entry : m_nodes)
  {
    if (entry->m_id == id)
      return entry.get();
  }
  return nullptr;
}

uint32_t InsertionOrderTree::AddUniqueLinear(UnitRefNode* node)
{
  if (!node || m_clearing || FindLinear(node->m_id))
    return 0;
  const bool assignedId = node->m_id == 0;
  uint32_t id = node->m_id;
  if (assignedId)
  {
    if (m_nextId == 0)
      return 0;
    id = m_nextId;
    node->SetId(id);
  }
  std::unique_ptr<UnitRefNode> pending(node);
  try
  {
    m_nodes.push_back(std::move(pending));
  }
  catch (...)
  {
    pending.release();
    if (assignedId)
      node->SetId(0);
    throw;
  }
  if (m_nextId != 0 && m_nextId <= id)
    m_nextId = id + 1;
  return id;
}

void InsertionOrderTree::RemoveLinear(uint32_t id)
{
  UnitRefNode* node = FindLinear(id);
  if (node)
  {
    Remove(node);
    delete node;
  }
}

uint32_t InsertionOrderTree::AddUnitLinear(TacticalShipBase* unit)
{
  if (FindLinear(unit->m_id))
    return 0;
  void* memory = ::operator new(sizeof(UnitRefNode));
  UnitRefNode* node = memory ? new (memory) UnitRefNode(unit) : nullptr;
  return AddUniqueLinear(node);
}

TargetOrderTree::TargetOrderTree() = default;

TargetOrderTree::~TargetOrderTree() = default;

AvlNode* TargetOrderTree::FirstNode()
{
  return First();
}

uint32_t TargetOrderTree::AddUniqueChecked(UnitRefNode* node)
{
  if (!FindLinear(node->m_id))
    return AddUniqueLinear(node);
  return 0;
}

FormationOrderTree::~FormationOrderTree() = default;

uint32_t FormationOrderTree::AddUniqueLinearSlot(UnitRefNode* node)
{
  return AddUniqueLinear(node);
}

AvlNode* FormationOrderTree::FirstNode()
{
  return First();
}

TacticalShipBase* FormationOrderTree::FirstUnit()
{
  auto* node = static_cast<UnitRefNode*>(FirstNode());
  if (node)
    return node->Value();
  return nullptr;
}

// ---- ship groups by id ----------------------------------------------------------------------------------------------

GroupRefNode::GroupRefNode(ShipGroup* group)
{
  m_group = group;
  SetId(reinterpret_cast<IdNode*>(group)->m_id);
}

GroupRefNode::~GroupRefNode() = default;

ShipGroup* GroupRefNode::Value()
{
  return m_group;
}

GroupRefTree::GroupRefTree(const GroupRefTree& other)
{
  *this = other;
}

GroupRefTree::~GroupRefTree()
{
  Clear();
}

void GroupRefTree::Clear()
{
  if (m_clearing)
    return;
  m_clearing = true;
  decltype(m_nodes) detached;
  detached.swap(m_nodes);
  m_nextId = 1;
  detached.clear();
  m_clearing = false;
}

IdNode* GroupRefTree::First() const
{
  return m_nodes.empty() ? nullptr : m_nodes.begin()->get();
}

IdNode* GroupRefTree::FindById(uint32_t id) const
{
  auto found = std::find_if(m_nodes.begin(), m_nodes.end(),
                            [id](const auto& entry)
                            {
                              return entry->m_id == id;
                            });
  return found == m_nodes.end() ? nullptr : (*found).get();
}

uint32_t GroupRefTree::AddWithId(GroupRefNode* node)
{
  if (!node || m_clearing)
    return 0;
  const bool assignedId = node->m_id == 0;
  uint32_t id = node->m_id;
  if (assignedId)
  {
    if (m_nextId == 0)
      return 0;
    id = m_nextId;
    node->SetId(id);
  }
  if (FindById(id))
  {
    if (assignedId)
      node->SetId(0);
    return 0;
  }
  std::unique_ptr<GroupRefNode> pending(node);
  try
  {
    m_nodes.insert(std::move(pending));
  }
  catch (...)
  {
    pending.release();
    if (assignedId)
      node->SetId(0);
    throw;
  }
  if (m_nextId != 0 && m_nextId <= id)
    m_nextId = id + 1;
  return id;
}

IdNode* GroupRefTree::DetachById(uint32_t id)
{
  auto found = std::find_if(m_nodes.begin(), m_nodes.end(),
                            [id](const auto& entry)
                            {
                              return entry->m_id == id;
                            });
  if (found == m_nodes.end())
    return nullptr;
  auto owned = m_nodes.extract(found);
  return owned.value().release();
}

void GroupRefTree::DeleteById(uint32_t id)
{
  delete DetachById(id);
}

AvlNode* GroupRefTree::FirstNode()
{
  return First();
}

ShipGroup* GroupRefTree::FirstGroup()
{
  auto* first = FirstNode();
  return first ? static_cast<GroupRefNode*>(first)->Value() : nullptr;
}

IdNode* GroupRefTree::Find(int id)
{
  return FindById(id);
}

GroupRefTree& GroupRefTree::operator=(const GroupRefTree& other)
{
  if (&other != this)
  {
    Clear();
    for (const auto& entry : other.Entries())
    {
      auto* groupNode = static_cast<GroupRefNode*>(entry.get());
      AddGroup(groupNode->Value());
    }
  }
  return *this;
}

GroupRefNode* GroupRefTree::AddGroup(ShipGroup* group)
{
  void* memory = ::operator new(sizeof(GroupRefNode));
  GroupRefNode* node = memory ? new (memory) GroupRefNode(group) : nullptr;
  AddWithId(node);
  return node;
}

// ---- targets ordered by priority ------------------------------------------------------------------------------------

int UnitRefSlot::SameUnit(UnitRefSlot* other)
{
  if (m_unit && other->m_unit)
    return m_unit->m_id == other->m_unit->m_id;
  return m_unit == other->m_unit;
}

int UnitRefSlot::UnitBefore(UnitRefSlot* other)
{
  if (m_unit && other->m_unit)
    return m_unit->m_id < other->m_unit->m_id;
  return 0;
}

TargetPriorityNode::TargetPriorityNode(float priority, TacticalShipBase* unit)
{
  m_priority = priority;
  m_unit.m_unit = unit;
}

TargetPriorityNode::~TargetPriorityNode() = default;

TargetPriorityTree::~TargetPriorityTree()
{
  Clear();
}

void TargetPriorityTree::Clear()
{
  if (m_clearing)
    return;
  m_clearing = true;
  decltype(m_nodes) detached;
  detached.swap(m_nodes);
  detached.clear();
  m_clearing = false;
}

bool TargetPriorityTree::NodeOrder::operator()(const std::unique_ptr<TargetPriorityNode>& a,
                                               const std::unique_ptr<TargetPriorityNode>& b) const
{
  const bool aNan = std::isnan(a->m_priority);
  const bool bNan = std::isnan(b->m_priority);
  if (aNan != bNan)
    return !aNan;
  if (!aNan && a->m_priority != b->m_priority)
    return a->m_priority < b->m_priority;
  if (a->m_unit.m_unit == b->m_unit.m_unit)
    return false;
  if (!a->m_unit.m_unit || !b->m_unit.m_unit)
    return a->m_unit.m_unit == nullptr;
  return a->m_unit.m_unit->m_id < b->m_unit.m_unit->m_id;
}

void TargetPriorityTree::Insert(TargetPriorityNode* node)
{
  if (!m_clearing && node)
    m_nodes.emplace(node);
}

void TargetPriorityTree::Remove(TargetPriorityNode* node)
{
  auto found = std::find_if(m_nodes.begin(), m_nodes.end(),
                            [node](const auto& entry)
                            {
                              return entry.get() == node;
                            });
  if (found == m_nodes.end())
    return;
  auto owned = m_nodes.extract(found);
  owned.value().release();
}

TargetPriorityNode* TargetPriorityTree::First() const
{
  return m_nodes.empty() ? nullptr : m_nodes.begin()->get();
}

TargetPriorityNode* TargetPriorityTree::Last() const
{
  return m_nodes.empty() ? nullptr : m_nodes.rbegin()->get();
}

UnitRefTree* UnitRefTree::CopyFrom(UnitRefTree* other)
{
  if (other != this)
  {
    for (const auto& entry : other->Entries())
    {
      auto* node = static_cast<UnitRefNode*>(entry.get());
      AddUnit(node->Value());
    }
  }
  return this;
}
