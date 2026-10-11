#include "core/containers.h"

#include "core/checked_new.h"

#include <algorithm>
#include <limits>

// ---- AvlNode ---------------------------------------------------------------------------------------------

AvlNode::AvlNode() = default;

AvlNode::AvlNode(const AvlNode&)
{
}

AvlNode::~AvlNode() = default;

// ---- AvlTree ---------------------------------------------------------------------------------------------

AvlTree::AvlTree() : m_nodes(NodeCompare{this})
{
}

AvlTree::~AvlTree()
{
  AvlTree::Clear();
}

bool AvlTree::NodeCompare::operator()(const std::unique_ptr<AvlNode>& a, const std::unique_ptr<AvlNode>& b) const
{
  return tree->IsLess(a.get(), b.get()) != 0;
}

bool AvlTree::NodeCompare::operator()(const std::unique_ptr<AvlNode>& node, const Probe& probe) const
{
  return probe.precedes(node.get());
}

void AvlTree::Clear()
{
  const bool wasClearing = m_clearing;
  m_clearing = true;
  struct RestoreClearingState
  {
    bool& state;
    bool previous;
    ~RestoreClearingState()
    {
      state = previous;
    }
  } restore{m_clearing, wasClearing};
  for (const auto& owned : m_nodes)
    owned->m_owner = nullptr;
  m_index.clear();
  NodeSet detached(NodeCompare{this});
  m_nodes.swap(detached);
}

int AvlTree::Insert(AvlNode* node)
{
  if (!CanInsert(node))
    return 0;
  std::unique_ptr<AvlNode> pending(node);
  NodeSet::iterator position;
  try
  {
    position = m_nodes.insert(std::move(pending));
  }
  catch (...)
  {
    pending.release();
    throw;
  }
  try
  {
    const bool inserted = m_index.emplace(node, position).second;
    if (!inserted)
    {
      auto rejected = m_nodes.extract(position);
      rejected.value().release();
      return 0;
    }
  }
  catch (...)
  {
    auto rejected = m_nodes.extract(position);
    rejected.value().release();
    throw;
  }
  node->m_owner = this;
  return 1;
}

void AvlTree::Remove(AvlNode* node)
{
  if (!node || node->m_owner != this)
    return;
  const auto found = m_index.find(node);
  if (found == m_index.end())
    return;
  node->m_owner = nullptr;
  const auto position = found->second;
  m_index.erase(found);
  auto detached = m_nodes.extract(position);
  detached.value().release();
}

AvlNode* AvlTree::First() const
{
  return m_nodes.empty() ? nullptr : m_nodes.begin()->get();
}

AvlNode* AvlTree::Last() const
{
  return m_nodes.empty() ? nullptr : m_nodes.rbegin()->get();
}

size_t AvlTree::Count() const
{
  return m_nodes.size();
}

AvlNode* AvlTree::LowerBound(const std::function<bool(const AvlNode*)>& candidatePrecedesProbe) const
{
  const auto found = m_nodes.lower_bound(Probe{candidatePrecedesProbe});
  return found == m_nodes.end() ? nullptr : found->get();
}

bool AvlTree::Owns(const AvlNode* node) const
{
  return node && node->m_owner == this;
}

bool AvlTree::CanInsert(const AvlNode* node) const
{
  return !m_clearing && node && !node->m_owner;
}

// ---- IdNode / IdValueNode ----------------------------------------------------------------------------------

IdNode::IdNode() : m_id(0)
{
}

IdNode::IdNode(uint32_t id) : m_id(id)
{
}

IdNode::~IdNode() = default;

void IdNode::SetId(uint32_t id)
{
  m_id = id;
}

void IdNode::ReadId(istream* in)
{
  ReadValue32(in, &m_id);
}

void IdNode::WriteId(ostream* out)
{
  WriteValue32(out, &m_id);
}

IdValueNode::IdValueNode(int value) : IdNode(), m_value(value)
{
}

IdValueNode::~IdValueNode() = default;

void IdValueNode::Read(istream* in)
{
  ReadId(in);
  ReadValue32(in, &m_value);
}

void IdValueNode::Write(ostream* out)
{
  WriteId(out);
  WriteValue32(out, &m_value);
}

// ---- IdAvlTree ---------------------------------------------------------------------------------------------

IdAvlTree::IdAvlTree() : AvlTree(), m_nextId(1)
{
}

IdAvlTree::~IdAvlTree()
{
  IdAvlTree::Clear();
}

void IdAvlTree::Clear()
{
  AvlTree::Clear();
  m_nextId = 1;
}

int IdAvlTree::IsLess(AvlNode* a, AvlNode* b)
{
  if (a && b && static_cast<IdNode*>(a)->m_id < static_cast<IdNode*>(b)->m_id)
    return 1;
  return 0;
}

int IdAvlTree::Insert(AvlNode* node)
{
  if (!CanInsert(node) || static_cast<IdNode*>(node)->m_id == 0)
    return 0;
  AvlNode* equivalent = LowerBound(
    [this, node](const AvlNode* candidate)
    {
      return IsLess(const_cast<AvlNode*>(candidate), node) != 0;
    });
  if (equivalent && !IsLess(node, equivalent))
    return 0;
  return AvlTree::Insert(node);
}

void IdAvlTree::Read(istream* in)
{
  Clear();
  uint32_t count = 0;
  ReadValue32(in, &count);
  if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0)
    return;
  for (uint32_t i = 0; i < count; ++i)
  {
    IdNode* node = NewOrNull<IdNode>();
    if (!node)
    {
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return;
    }
    node->ReadId(in);
    if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0 || !AddWithId(node))
    {
      delete node;
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return;
    }
  }
}

void IdAvlTree::Write(ostream* out)
{
  if (Count() > std::numeric_limits<uint32_t>::max())
  {
    iostream_access::SetState(iostream_access::IosOf(out), iostream_flags::kFail);
    return;
  }
  uint32_t count = static_cast<uint32_t>(Count());
  WriteValue32(out, &count);
  for (const auto& node : Entries())
    static_cast<IdNode*>(node.get())->WriteId(out);
}

uint32_t IdAvlTree::AddWithId(IdNode* node)
{
  if (!CanInsert(node))
    return 0;
  uint32_t id = node->m_id;
  if (id == 0)
  {
    if (m_nextId == 0)
      return 0;
    id = m_nextId;
    node->SetId(id);
    int inserted = 0;
    try
    {
      inserted = Insert(node);
    }
    catch (...)
    {
      node->SetId(0);
      throw;
    }
    if (!inserted)
    {
      node->SetId(0);
      return 0;
    }
    m_nextId = id + 1;
    return id;
  }
  if (!Insert(node))
    return 0;
  if (m_nextId != 0 && m_nextId <= id)
    m_nextId = id + 1;
  return id;
}

IdNode* IdAvlTree::FindById(uint32_t id) const
{
  AvlNode* node = LowerBound(
    [id](const AvlNode* candidate)
    {
      return static_cast<const IdNode*>(candidate)->m_id < id;
    });
  if (node && static_cast<IdNode*>(node)->m_id != id)
    node = nullptr;
  return static_cast<IdNode*>(node);
}

void IdAvlTree::DeleteById(uint32_t id)
{
  IdNode* node = FindById(id);
  if (node)
  {
    Remove(node);
    delete node;
  }
}

IdNode* IdAvlTree::DetachById(uint32_t id)
{
  IdNode* node = FindById(id);
  if (node)
    Remove(node);
  return node;
}

// ---- IdValueTree -------------------------------------------------------------------------------------------

IdValueTree::IdValueTree() : IdAvlTree()
{
}

IdValueTree::~IdValueTree() = default;

void IdValueTree::Read(istream* in)
{
  uint16_t count = 0;
  ReadValue16(in, &count);
  if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0)
    return;
  while (count != 0)
  {
    --count;
    IdValueNode* node = NewOrNull<IdValueNode>(0);
    if (!node)
    {
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return;
    }
    node->Read(in);
    if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0 || !AddWithId(node))
    {
      delete node;
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return;
    }
  }
}

void IdValueTree::Write(ostream* out)
{
  if (Count() > std::numeric_limits<uint16_t>::max())
  {
    iostream_access::SetState(iostream_access::IosOf(out), iostream_flags::kFail);
    return;
  }
  uint16_t count = static_cast<uint16_t>(Count());
  WriteValue16(out, &count);
  for (const auto& node : Entries())
    static_cast<IdValueNode*>(node.get())->Write(out);
}

// ---- CloneableNode / IntKeyNode ----------------------------------------------------------------------------

CloneableNode::CloneableNode() : AvlNode()
{
}

CloneableNode::CloneableNode(const CloneableNode&) : AvlNode()
{
}

CloneableNode::~CloneableNode() = default;

IntKeyNode::IntKeyNode() : CloneableNode(), m_key(0)
{
}

IntKeyNode::IntKeyNode(const IntKeyNode& other) = default;

IntKeyNode::~IntKeyNode() = default;

int IntKeyNode::Read(istream* in)
{
  ReadValue32(in, &m_key);
  return 1;
}

int IntKeyNode::Write(ostream* out)
{
  WriteValue32(out, &m_key);
  return 1;
}

// ---- AvlSet / IntKeyedSet ----------------------------------------------------------------------------------

AvlSet::AvlSet() : AvlTree()
{
}

AvlSet::~AvlSet() = default;

int AvlSet::IsLess(AvlNode* a, AvlNode* b)
{
  return Precedes(a, b);
}

int AvlSet::Contains(AvlNode* probe)
{
  return Find(probe) != nullptr;
}

AvlNode* AvlSet::Find(AvlNode* probe)
{
  AvlNode* node = LowerBound(
    [this, probe](const AvlNode* candidate)
    {
      return Precedes(const_cast<AvlNode*>(candidate), probe);
    });
  if (node && !Matches(probe, node))
    node = nullptr;
  return node;
}

AvlNode* AvlSet::RemoveMatching(AvlNode* probe)
{
  AvlNode* node = Find(probe);
  if (node)
    Remove(node);
  return node;
}

void AvlSet::AddIfAbsent(AvlNode* node)
{
  if (!Contains(node))
    Insert(node);
}

AvlSet& AvlSet::CopyFrom(const AvlSet& other)
{
  if (&other != this)
  {
    AvlTree::Clear();
    for (const auto& node : other.Entries())
    {
      CloneableNode* copy = static_cast<CloneableNode*>(node.get())->Clone();
      if (copy)
        Insert(copy);
    }
  }
  return *this;
}

AvlSet& AvlSet::Intersect(AvlSet& other)
{
  if (&other != this)
  {
    auto position = Entries().begin();
    while (position != Entries().end())
    {
      AvlNode* node = (position++)->get();
      if (!other.Contains(node))
      {
        RemoveMatching(node);
        delete node;
      }
    }
  }
  return *this;
}

IntKeyedSet::IntKeyedSet() : AvlSet()
{
}

IntKeyedSet::~IntKeyedSet() = default;

IntKeyedSet& IntKeyedSet::operator=(const IntKeyedSet& other)
{
  if (this != &other)
    CopyFrom(other);
  return *this;
}

int IntKeyedSet::Precedes(AvlNode* probe, AvlNode* node)
{
  return static_cast<IntKeyNode*>(probe)->m_key < static_cast<IntKeyNode*>(node)->m_key;
}

int IntKeyedSet::Matches(AvlNode* probe, AvlNode* node)
{
  return static_cast<IntKeyNode*>(probe)->m_key == static_cast<IntKeyNode*>(node)->m_key;
}

// ---- LinkNode / IdLinkNode ---------------------------------------------------------------------------------

LinkNode::LinkNode() = default;

LinkNode::LinkNode(const LinkNode&)
{
}

LinkNode::~LinkNode() = default;

void LinkNode::Read(istream*)
{
}

void LinkNode::Write(ostream*)
{
}

IdLinkNode::IdLinkNode() : LinkNode(), m_id(0)
{
}

IdLinkNode::~IdLinkNode() = default;

void IdLinkNode::Read(istream* in)
{
  ReadValue32(in, &m_id);
}

void IdLinkNode::Write(ostream* out)
{
  WriteValue32(out, &m_id);
}

// ---- SortedLinkList / IdSortedLinkList ---------------------------------------------------------------------

SortedLinkList::SortedLinkList() = default;

SortedLinkList::~SortedLinkList()
{
  Clear();
}

int SortedLinkList::Insert(LinkNode* node)
{
  if (!CanInsert(node))
    return 0;

  auto position = m_nodes.begin();
  while (position != m_nodes.end() && !IsLess(node, position->get()))
    ++position;
  return InsertAt(node, position);
}

int SortedLinkList::InsertBefore(LinkNode* node, LinkNode* anchor)
{
  if (!CanInsert(node))
    return 0;
  if (!anchor)
    return InsertAt(node, m_nodes.end());
  if (anchor->m_owner != this)
    return 0;
  const auto found = m_index.find(anchor);
  if (found == m_index.end())
    return 0;
  return InsertAt(node, found->second);
}

int SortedLinkList::InsertAt(LinkNode* node, NodeList::const_iterator position)
{
  std::unique_ptr<LinkNode> pending(node);
  NodeList::iterator inserted;
  try
  {
    inserted = m_nodes.insert(position, std::move(pending));
  }
  catch (...)
  {
    pending.release();
    throw;
  }

  try
  {
    const bool indexed = m_index.emplace(node, inserted).second;
    if (!indexed)
    {
      node = inserted->release();
      m_nodes.erase(inserted);
      return 0;
    }
  }
  catch (...)
  {
    inserted->release();
    m_nodes.erase(inserted);
    throw;
  }

  node->m_owner = this;
  return 1;
}

bool SortedLinkList::CanInsert(const LinkNode* node) const
{
  return !m_clearing && node && !node->m_owner;
}

int SortedLinkList::Remove(LinkNode* node)
{
  if (!node || node->m_owner != this)
    return 0;
  const auto found = m_index.find(node);
  if (found == m_index.end())
    return 0;
  const auto position = found->second;
  node->m_owner = nullptr;
  m_index.erase(found);
  auto detached = std::move(*position);
  m_nodes.erase(position);
  detached.release();
  return 1;
}

void SortedLinkList::Clear()
{
  const bool wasClearing = m_clearing;
  m_clearing = true;
  struct RestoreClearingState
  {
    bool& state;
    bool previous;
    ~RestoreClearingState()
    {
      state = previous;
    }
  } restore{m_clearing, wasClearing};

  for (const auto& owned : m_nodes)
  {
    owned->m_owner = nullptr;
  }
  m_index.clear();
  NodeList detached;
  m_nodes.swap(detached);
}

IdSortedLinkList::IdSortedLinkList() : SortedLinkList(), m_nextId(1)
{
}

IdSortedLinkList::~IdSortedLinkList() = default;

int IdSortedLinkList::IsLess(LinkNode* a, LinkNode* b)
{
  if (a && b && static_cast<IdLinkNode*>(a)->m_id < static_cast<IdLinkNode*>(b)->m_id)
    return 1;
  return 0;
}

void IdSortedLinkList::Reset()
{
  Clear();
  m_nextId = 1;
}

uint32_t IdSortedLinkList::AddWithId(IdLinkNode* node)
{
  if (!CanInsert(node))
    return 0;
  const uint32_t priorId = node->m_id;
  const uint32_t id = priorId == 0 ? m_nextId : priorId;
  if (id == 0 || FindById(id))
    return 0;
  if (priorId == 0)
    node->m_id = id;
  int inserted = 0;
  try
  {
    inserted = Insert(node);
  }
  catch (...)
  {
    if (priorId == 0)
      node->m_id = 0;
    throw;
  }
  if (!inserted)
  {
    if (priorId == 0)
      node->m_id = 0;
    return 0;
  }
  if (m_nextId != 0 && m_nextId <= id)
    m_nextId = id + 1;
  return id;
}

IdLinkNode* IdSortedLinkList::FindById(uint32_t id) const
{
  for (const auto& node : Entries())
    if (static_cast<IdLinkNode*>(node.get())->m_id == id)
      return static_cast<IdLinkNode*>(node.get());
  return nullptr;
}

IdLinkNode* IdSortedLinkList::DetachById(uint32_t id)
{
  IdLinkNode* node = FindById(id);
  if (node)
    Remove(node);
  return node;
}

void IdSortedLinkList::DeleteById(uint32_t id)
{
  IdLinkNode* node = FindById(id);
  if (node)
  {
    Remove(node);
    delete node;
  }
}
