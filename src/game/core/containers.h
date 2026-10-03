#pragma once
#include <cstddef>
#include <cstdint>

#include "core/layout.h"
#include "core/stream_io.h"

// The game's own intrusive containers (no STL). Nodes carry their links and are owned by the container that
// holds them: Clear()/destructors delete every node through its virtual destructor.
//
//   AvlNode / AvlTree        height-balanced binary tree with a threaded in-order successor pointer
//     IdNode / IdAvlTree     nodes keyed by a 32-bit id the tree hands out; IdValueNode / IdValueTree add a payload
//     CloneableNode / AvlSet set of cloneable nodes (IntKeyNode / IntKeyedSet: keyed by a signed int)
//   OrderedTree              unbalanced threaded tree that caches its first node
//   LinkNode / SortedLinkList  doubly linked list (IdLinkNode / IdSortedLinkList: ids assigned by the list)
//   NodeQueue, NodeStack     FIFO / LIFO of LinkNodes

class String;

class AvlNode {
public:
    AvlNode();
    virtual ~AvlNode();

    // In-order predecessor (the node whose m_successor is this one), or null.
    AvlNode* Predecessor();

    void ResetLinks();
    void ReplaceChild(AvlNode* oldChild, AvlNode* newChild);
    // Recomputes m_height from the children and returns height(right) - height(left) (modulo 2^16).
    int UpdateHeight();

    AvlNode* m_left;       // +0x04
    AvlNode* m_right;      // +0x08
    AvlNode* m_parent;     // +0x0c
    AvlNode* m_successor;  // +0x10
    uint16_t m_height;     // +0x14
    uint16_t m_pad16;      // +0x16
};

class AvlTree {
public:
    AvlTree();
    virtual ~AvlTree();
    virtual void Clear();
    virtual int IsLess(AvlNode* a, AvlNode* b) = 0;

    // Duplicates (nodes for which neither IsLess(a,b) nor IsLess(b,a)) go to the right; re-inserting a node that
    // is already in the tree is detected only when the search reaches it.
    void Insert(AvlNode* node);
    void Remove(AvlNode* node);
    AvlNode* First() const;  // leftmost
    AvlNode* Last() const;   // rightmost
    short Count() const;
    void Rebalance(AvlNode* node);
    AvlNode* RotateLeft(AvlNode* node);
    AvlNode* RotateRight(AvlNode* node);

    AvlNode* m_root;  // +0x04
};

// Node with a 32-bit id.
class IdNode : public AvlNode {
public:
    IdNode();
    explicit IdNode(uint32_t id);
    virtual ~IdNode();

    void SetId(uint32_t id);
    void ReadId(istream* in);
    void WriteId(ostream* out);

    uint32_t m_id;  // +0x18
};

class IdValueNode : public IdNode {
public:
    explicit IdValueNode(int value);
    virtual ~IdValueNode();
    virtual void Read(istream* in);
    virtual void Write(ostream* out);

    int m_value;  // +0x1c
};

// Tree of IdNodes; ids start at 1 and a node added with id 0 is given the next free one.
class IdAvlTree : public AvlTree {
public:
    IdAvlTree();
    virtual ~IdAvlTree();
    virtual void Clear() override;
    virtual int IsLess(AvlNode* a, AvlNode* b) override;

    // Saves/loads the ids only (32-bit count); Read creates plain IdNodes.
    void Read(istream* in);
    void Write(ostream* out);
    uint32_t AddWithId(IdNode* node);
    void DeleteById(uint32_t id);
    IdNode* DetachById(uint32_t id);
    IdNode* FindById(uint32_t id) const;

    uint32_t m_nextId;  // +0x08
};

// IdAvlTree of IdValueNodes; saved with a 16-bit count and each node's own Read/Write.
class IdValueTree : public IdAvlTree {
public:
    IdValueTree();
    virtual ~IdValueTree();

    void Read(istream* in);
    void Write(ostream* out);
};

class CloneableNode : public AvlNode {
public:
    CloneableNode();
    CloneableNode(const CloneableNode& other);
    virtual ~CloneableNode();
    virtual CloneableNode* Clone() = 0;
};

// Node keyed by a signed int. Read/Write are pure virtual but have bodies that derived classes call for the key.
class IntKeyNode : public CloneableNode {
public:
    IntKeyNode();
    IntKeyNode(const IntKeyNode& other);
    virtual ~IntKeyNode();
    virtual int Read(istream* in) = 0;
    virtual int Write(ostream* out) = 0;

    int m_key;  // +0x18
};

// AVL tree whose ordering and equality are supplied by virtual functions that compare a probe node to a node.
class AvlSet : public AvlTree {
public:
    AvlSet();
    virtual ~AvlSet();
    virtual int IsLess(AvlNode* a, AvlNode* b) override;
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

class IntKeyedSet : public AvlSet {
public:
    IntKeyedSet();
    virtual ~IntKeyedSet();
    virtual int Precedes(AvlNode* probe, AvlNode* node) override;
    virtual int Matches(AvlNode* probe, AvlNode* node) override;
    virtual int Read(istream* in) = 0;
    virtual int Write(ostream* out) = 0;

    IntKeyedSet& operator=(const IntKeyedSet& other);
};

// Like AvlTree but never rebalances, and keeps its first (lowest) node in m_first.
class OrderedTree {
public:
    OrderedTree();
    virtual ~OrderedTree();
    virtual void Clear();
    virtual int IsLess(AvlNode* a, AvlNode* b) = 0;
    virtual void Virtual03() = 0;

    void Insert(AvlNode* node);
    void Remove(AvlNode* node);
    short Count() const;

    AvlNode* m_root;   // +0x04
    AvlNode* m_first;  // +0x08
};

// Doubly linked list node. Read/Write are no-ops unless a derived class saves something.
class LinkNode {
public:
    LinkNode();
    virtual ~LinkNode();
    virtual void Read(istream* in);
    virtual void Write(ostream* out);

    LinkNode* m_prev;  // +0x04
    LinkNode* m_next;  // +0x08
};

class IdLinkNode : public LinkNode {
public:
    IdLinkNode();
    virtual ~IdLinkNode();
    virtual void Read(istream* in) override;
    virtual void Write(ostream* out) override;

    uint32_t m_id;  // +0x0c
};

// List kept in the order defined by IsLess(newNode, existingNode); the head is the "smallest" node.
class SortedLinkList {
public:
    SortedLinkList();
    virtual ~SortedLinkList();
    virtual int IsLess(LinkNode* a, LinkNode* b) = 0;

    int Insert(LinkNode* node);
    int Remove(LinkNode* node);
    void Clear();
    void RewindToFirst();

    int m_count;        // +0x04
    LinkNode* m_head;   // +0x08
};

class IdSortedLinkList : public SortedLinkList {
public:
    IdSortedLinkList();
    virtual ~IdSortedLinkList();
    virtual int IsLess(LinkNode* a, LinkNode* b) override;

    void Reset();
    uint32_t AddWithId(IdLinkNode* node);
    IdLinkNode* FindById(uint32_t id) const;
    IdLinkNode* DetachById(uint32_t id);
    void DeleteById(uint32_t id);

    uint32_t m_nextId;  // +0x0c
};

class NodeQueue {
public:
    NodeQueue();
    virtual ~NodeQueue();

    int Enqueue(LinkNode* node);
    LinkNode* Dequeue();
    void Clear();

    LinkNode* m_head;  // +0x04
    LinkNode* m_tail;  // +0x08
    int m_count;       // +0x0c
};

class NodeStack {
public:
    NodeStack();
    virtual ~NodeStack();
    virtual void Clear();
    virtual int Virtual02();

    int Push(LinkNode* node);
    LinkNode* Pop();
    // Deletes `node` if it is on the stack, keeping the order of the others. Returns 1 if it was found.
    int RemoveAndDelete(LinkNode* node);

    LinkNode* m_top;  // +0x04
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(AvlNode, 0x18);
REB_ASSERT_OFFSET(AvlNode, m_left, 0x04);
REB_ASSERT_OFFSET(AvlNode, m_height, 0x14);
REB_ASSERT_SIZE(AvlTree, 0x08);
REB_ASSERT_SIZE(IdNode, 0x1c);
REB_ASSERT_OFFSET(IdNode, m_id, 0x18);
REB_ASSERT_SIZE(IdValueNode, 0x20);
REB_ASSERT_SIZE(IdAvlTree, 0x0c);
REB_ASSERT_OFFSET(IdAvlTree, m_nextId, 0x08);
REB_ASSERT_SIZE(IdValueTree, 0x0c);
REB_ASSERT_SIZE(CloneableNode, 0x18);
REB_ASSERT_SIZE(IntKeyNode, 0x1c);
REB_ASSERT_OFFSET(IntKeyNode, m_key, 0x18);
REB_ASSERT_SIZE(AvlSet, 0x08);
REB_ASSERT_SIZE(IntKeyedSet, 0x08);
REB_ASSERT_SIZE(OrderedTree, 0x0c);
REB_ASSERT_SIZE(LinkNode, 0x0c);
REB_ASSERT_SIZE(IdLinkNode, 0x10);
REB_ASSERT_OFFSET(IdLinkNode, m_id, 0x0c);
REB_ASSERT_SIZE(SortedLinkList, 0x0c);
REB_ASSERT_OFFSET(SortedLinkList, m_head, 0x08);
REB_ASSERT_SIZE(IdSortedLinkList, 0x10);
REB_ASSERT_OFFSET(IdSortedLinkList, m_nextId, 0x0c);
REB_ASSERT_SIZE(NodeQueue, 0x10);
REB_ASSERT_SIZE(NodeStack, 0x08);
#pragma clang diagnostic pop
