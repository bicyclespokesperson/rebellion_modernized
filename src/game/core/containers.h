#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <set>
#include <unordered_map>

#include "core/game_string.h"
#include "core/stream_io.h"

// Transitional ordered registries use standard owning storage. Entries() exposes ordinary const iterators;
// PositionOf() returns end() for a foreign or detached node. Removing a node invalidates only its iterator.
// Successful insertion takes ownership; Remove transfers ownership back to the caller. The remaining virtual
// comparison and ID/persistence interfaces will move to typed domain registries.

class AvlNode
{
public:
  AvlNode();
  AvlNode(const AvlNode&);
  AvlNode& operator=(const AvlNode&) = delete;
  AvlNode(AvlNode&&) = delete;
  AvlNode& operator=(AvlNode&&) = delete;
  virtual ~AvlNode();

private:
  friend class AvlTree;
  const void* m_owner = nullptr;
};

class AvlTree
{
public:
  AvlTree();
  virtual ~AvlTree();
  AvlTree(const AvlTree&) = delete;
  AvlTree& operator=(const AvlTree&) = delete;
  AvlTree(AvlTree&&) = delete;
  AvlTree& operator=(AvlTree&&) = delete;
  virtual void Clear();
  virtual int IsLess(AvlNode* a, AvlNode* b) = 0;

  // Equivalent keys retain insertion order. Inserts reject nodes already owned by a tree.
  virtual int Insert(AvlNode* node);
  void Remove(AvlNode* node);
  AvlNode* First() const; // leftmost
  AvlNode* Last() const; // rightmost
  size_t Count() const;
  bool Empty() const
  {
    return m_nodes.empty();
  }
  const auto& Entries() const
  {
    return m_nodes;
  }
  auto PositionOf(const AvlNode* node) const
  {
    const auto found = m_index.find(const_cast<AvlNode*>(node));
    return found == m_index.end() ? m_nodes.cend() : NodeSet::const_iterator(found->second);
  }

protected:
  AvlNode* LowerBound(const std::function<bool(const AvlNode*)>& candidatePrecedesProbe) const;
  bool Owns(const AvlNode* node) const;
  bool CanInsert(const AvlNode* node) const;

private:
  struct Probe
  {
    std::function<bool(const AvlNode*)> precedes;
  };
  struct NodeCompare
  {
    using is_transparent = void;
    AvlTree* tree;
    bool operator()(const std::unique_ptr<AvlNode>& a, const std::unique_ptr<AvlNode>& b) const;
    bool operator()(const std::unique_ptr<AvlNode>& node, const Probe& probe) const;
  };
  using NodeSet = std::multiset<std::unique_ptr<AvlNode>, NodeCompare>;
  NodeSet m_nodes;
  std::unordered_map<AvlNode*, NodeSet::iterator> m_index;
  bool m_clearing = false;
};

// Node with a 32-bit id.
class IdNode : public AvlNode
{
public:
  IdNode();
  explicit IdNode(uint32_t id);
  ~IdNode() override;

  void SetId(uint32_t id);
  void ReadId(istream* in);
  void WriteId(ostream* out);

  uint32_t m_id; // +0x18
};

class IdValueNode : public IdNode
{
public:
  explicit IdValueNode(int value);
  ~IdValueNode() override;
  virtual void Read(istream* in);
  virtual void Write(ostream* out);

  int m_value; // +0x1c
};

// Tree of IdNodes; ids start at 1 and a node added with id 0 is given the next free one.
class IdAvlTree : public AvlTree
{
public:
  IdAvlTree();
  ~IdAvlTree() override;
  void Clear() override;
  int Insert(AvlNode* node) override;
  int IsLess(AvlNode* a, AvlNode* b) override;

  // Saves/loads the ids only (32-bit count); Read creates plain IdNodes.
  void Read(istream* in);
  void Write(ostream* out);
  uint32_t AddWithId(IdNode* node);
  void DeleteById(uint32_t id);
  IdNode* DetachById(uint32_t id);
  IdNode* FindById(uint32_t id) const;

  uint32_t m_nextId; // +0x08
};

// IdAvlTree of IdValueNodes; saved with a 16-bit count and each node's own Read/Write.
class IdValueTree : public IdAvlTree
{
public:
  IdValueTree();
  ~IdValueTree() override;

  void Read(istream* in);
  void Write(ostream* out);
};

class CloneableNode : public AvlNode
{
public:
  CloneableNode();
  CloneableNode(const CloneableNode& other);
  ~CloneableNode() override;
  virtual CloneableNode* Clone() = 0;
};

// Node keyed by a signed int. Read/Write are pure virtual but have bodies that derived classes call for the key.
class IntKeyNode : public CloneableNode
{
public:
  IntKeyNode();
  IntKeyNode(const IntKeyNode& other);
  ~IntKeyNode() override;
  virtual int Read(istream* in) = 0;
  virtual int Write(ostream* out) = 0;

  int m_key; // +0x18
};

// AVL tree whose ordering and equality are supplied by virtual functions that compare a probe node to a node.
class AvlSet : public AvlTree
{
public:
  AvlSet();
  ~AvlSet() override;
  int IsLess(AvlNode* a, AvlNode* b) override;
  virtual int Precedes(AvlNode* probe, AvlNode* node) = 0;
  virtual int Matches(AvlNode* probe, AvlNode* node) = 0;

  int Contains(AvlNode* probe);
  AvlNode* Find(AvlNode* probe);
  AvlNode* RemoveMatching(AvlNode* probe);
  void AddIfAbsent(AvlNode* node);
  // Replaces the contents with clones of other's nodes.
  AvlSet& CopyFrom(const AvlSet& other);
  // Deletes the nodes that have no match in other.
  AvlSet& Intersect(AvlSet& other);
};

class IntKeyedSet : public AvlSet
{
public:
  IntKeyedSet();
  ~IntKeyedSet() override;
  int Precedes(AvlNode* probe, AvlNode* node) override;
  int Matches(AvlNode* probe, AvlNode* node) override;
  virtual int Read(istream* in) = 0;
  virtual int Write(ostream* out) = 0;

  IntKeyedSet& operator=(const IntKeyedSet& other);
};

// Polymorphic entry retained by the transitional sorted registry.
class LinkNode
{
public:
  LinkNode();
  LinkNode(const LinkNode&);
  LinkNode& operator=(const LinkNode&) = delete;
  LinkNode(LinkNode&&) = delete;
  LinkNode& operator=(LinkNode&&) = delete;
  virtual ~LinkNode();
  virtual void Read(istream* in);
  virtual void Write(ostream* out);

private:
  friend class SortedLinkList;
  const void* m_owner = nullptr;
};

class IdLinkNode : public LinkNode
{
public:
  IdLinkNode();
  ~IdLinkNode() override;
  void Read(istream* in) override;
  void Write(ostream* out) override;

  uint32_t m_id; // +0x0c
};

// List kept in the order defined by IsLess(newNode, existingNode); the head is the "smallest" node.
class SortedLinkList
{
public:
  SortedLinkList();
  virtual ~SortedLinkList();
  SortedLinkList(const SortedLinkList&) = delete;
  SortedLinkList& operator=(const SortedLinkList&) = delete;
  SortedLinkList(SortedLinkList&&) = delete;
  SortedLinkList& operator=(SortedLinkList&&) = delete;
  virtual int IsLess(LinkNode* a, LinkNode* b) = 0;

  int Insert(LinkNode* node);
  // Inserts before an owned anchor, or appends when anchor is null.
  int InsertBefore(LinkNode* node, LinkNode* anchor);
  int Remove(LinkNode* node);
  void Clear();
  LinkNode* First() const
  {
    return m_nodes.empty() ? nullptr : m_nodes.front().get();
  }
  LinkNode* Last() const
  {
    return m_nodes.empty() ? nullptr : m_nodes.back().get();
  }
  size_t Count() const
  {
    return m_nodes.size();
  }
  bool Empty() const
  {
    return m_nodes.empty();
  }
  const auto& Entries() const
  {
    return m_nodes;
  }
  auto PositionOf(const LinkNode* node) const
  {
    const auto found = m_index.find(const_cast<LinkNode*>(node));
    return found == m_index.end() ? m_nodes.cend() : NodeList::const_iterator(found->second);
  }

protected:
  bool CanInsert(const LinkNode* node) const;

private:
  using NodeList = std::list<std::unique_ptr<LinkNode>>;
  NodeList m_nodes;
  std::unordered_map<LinkNode*, NodeList::iterator> m_index;
  bool m_clearing = false;

  int InsertAt(LinkNode* node, NodeList::const_iterator position);
};

class IdSortedLinkList : public SortedLinkList
{
public:
  IdSortedLinkList();
  ~IdSortedLinkList() override;
  int IsLess(LinkNode* a, LinkNode* b) override;

  void Reset();
  uint32_t AddWithId(IdLinkNode* node);
  IdLinkNode* FindById(uint32_t id) const;
  IdLinkNode* DetachById(uint32_t id);
  void DeleteById(uint32_t id);

  uint32_t m_nextId; // +0x0c
};
