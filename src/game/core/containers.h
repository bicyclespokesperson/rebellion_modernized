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

// Owning container APIs accept borrowed node pointers and take ownership after a successful insertion. Removal
// transfers the node back to the caller; Clear()/destructors delete nodes through their virtual destructors.
//
//   AvlNode / AvlTree        ordered owning tree with borrowed in-order neighbor views
//     IdNode / IdAvlTree     nodes keyed by a 32-bit id the tree hands out; IdValueNode / IdValueTree add a payload
//     CloneableNode / AvlSet set of cloneable nodes (IntKeyNode / IntKeyedSet: keyed by a signed int)
//   OrderedTree              ordered-tree compatibility type
//   LinkNode / SortedLinkList  doubly linked list (IdLinkNode / IdSortedLinkList: ids assigned by the list)
//   NodeQueue, NodeStack     FIFO / LIFO of LinkNodes

class AvlNode {
public:
    AvlNode();
    AvlNode(const AvlNode&);
    AvlNode& operator=(const AvlNode&) = delete;
    AvlNode(AvlNode&&) = delete;
    AvlNode& operator=(AvlNode&&) = delete;
    virtual ~AvlNode();

    // Borrowed in-order predecessor, or null.
    AvlNode* Predecessor();

    void ResetLinks();

    AvlNode* m_predecessor = nullptr;
    AvlNode* m_successor = nullptr;

private:
    friend class AvlTree;
    const void* m_owner = nullptr;
};

class AvlTree {
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
    AvlNode* First() const;  // leftmost
    AvlNode* Last() const;   // rightmost
    size_t Count() const;

protected:
    AvlNode* LowerBound(const std::function<bool(const AvlNode*)>& candidatePrecedesProbe) const;
    bool Owns(const AvlNode* node) const;
    bool CanInsert(const AvlNode* node) const;

private:
    struct Probe {
        std::function<bool(const AvlNode*)> precedes;
    };
    struct NodeCompare {
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
class IdNode : public AvlNode {
public:
    IdNode();
    explicit IdNode(uint32_t id);
    ~IdNode() override;

    void SetId(uint32_t id);
    void ReadId(istream* in);
    void WriteId(ostream* out);

    uint32_t m_id;  // +0x18
};

class IdValueNode : public IdNode {
public:
    explicit IdValueNode(int value);
    ~IdValueNode() override;
    virtual void Read(istream* in);
    virtual void Write(ostream* out);

    int m_value;  // +0x1c
};

// Tree of IdNodes; ids start at 1 and a node added with id 0 is given the next free one.
class IdAvlTree : public AvlTree {
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

    uint32_t m_nextId;  // +0x08
};

// IdAvlTree of IdValueNodes; saved with a 16-bit count and each node's own Read/Write.
class IdValueTree : public IdAvlTree {
public:
    IdValueTree();
    ~IdValueTree() override;

    void Read(istream* in);
    void Write(ostream* out);
};

class CloneableNode : public AvlNode {
public:
    CloneableNode();
    CloneableNode(const CloneableNode& other);
    ~CloneableNode() override;
    virtual CloneableNode* Clone() = 0;
};

// Node keyed by a signed int. Read/Write are pure virtual but have bodies that derived classes call for the key.
class IntKeyNode : public CloneableNode {
public:
    IntKeyNode();
    IntKeyNode(const IntKeyNode& other);
    ~IntKeyNode() override;
    virtual int Read(istream* in) = 0;
    virtual int Write(ostream* out) = 0;

    int m_key;  // +0x18
};

// AVL tree whose ordering and equality are supplied by virtual functions that compare a probe node to a node.
class AvlSet : public AvlTree {
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

class IntKeyedSet : public AvlSet {
public:
    IntKeyedSet();
    ~IntKeyedSet() override;
    int Precedes(AvlNode* probe, AvlNode* node) override;
    int Matches(AvlNode* probe, AvlNode* node) override;
    virtual int Read(istream* in) = 0;
    virtual int Write(ostream* out) = 0;

    IntKeyedSet& operator=(const IntKeyedSet& other);
};

// Compatibility name for ordered event trees; ordering and ownership come from AvlTree.
class OrderedTree : public AvlTree {
public:
    OrderedTree();
    ~OrderedTree() override;
};

// Doubly linked list node. Read/Write are no-ops unless a derived class saves something.
class LinkNode {
public:
    LinkNode();
    LinkNode(const LinkNode&);
    LinkNode& operator=(const LinkNode&) = delete;
    LinkNode(LinkNode&&) = delete;
    LinkNode& operator=(LinkNode&&) = delete;
    virtual ~LinkNode();
    virtual void Read(istream* in);
    virtual void Write(ostream* out);

    LinkNode* m_prev;  // +0x04
    LinkNode* m_next;  // +0x08

private:
    friend class NodeQueue;
    friend class NodeStack;
    friend class SortedLinkList;
    const void* m_owner = nullptr;
};

class IdLinkNode : public LinkNode {
public:
    IdLinkNode();
    ~IdLinkNode() override;
    void Read(istream* in) override;
    void Write(ostream* out) override;

    uint32_t m_id;  // +0x0c
};

// List kept in the order defined by IsLess(newNode, existingNode); the head is the "smallest" node.
class SortedLinkList {
public:
    SortedLinkList();
    virtual ~SortedLinkList();
    SortedLinkList(const SortedLinkList&) = delete;
    SortedLinkList& operator=(const SortedLinkList&) = delete;
    SortedLinkList(SortedLinkList&&) = delete;
    SortedLinkList& operator=(SortedLinkList&&) = delete;
    virtual int IsLess(LinkNode* a, LinkNode* b) = 0;

    int Insert(LinkNode* node);
    int Remove(LinkNode* node);
    void Clear();
    void RewindToFirst();

protected:
    bool CanInsert(const LinkNode* node) const;
    LinkNode* FirstOwned() const;

public:
    int m_count;        // +0x04
    LinkNode* m_head;   // +0x08

private:
    using NodeList = std::list<std::unique_ptr<LinkNode>>;
    NodeList m_nodes;
    std::unordered_map<LinkNode*, NodeList::iterator> m_index;
    bool m_clearing = false;
};

class IdSortedLinkList : public SortedLinkList {
public:
    IdSortedLinkList();
    ~IdSortedLinkList() override;
    int IsLess(LinkNode* a, LinkNode* b) override;

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
    NodeQueue(const NodeQueue&) = delete;
    NodeQueue& operator=(const NodeQueue&) = delete;
    NodeQueue(NodeQueue&&) = delete;
    NodeQueue& operator=(NodeQueue&&) = delete;

    // Takes ownership of an unlinked node; returns 0 if it is null or owned/linked elsewhere.
    int Enqueue(LinkNode* node);
    // Transfers ownership of the head node to the caller.
    LinkNode* Dequeue();
    void Clear();

    // Temporary borrowed legacy views; delete after queue traversal callers migrate.
    LinkNode* m_head;  // +0x04
    LinkNode* m_tail;  // +0x08
    int m_count;       // +0x0c

private:
    std::list<std::unique_ptr<LinkNode>> m_nodes;
    bool m_clearing = false;
};

class NodeStack {
public:
    NodeStack();
    virtual ~NodeStack();
    NodeStack(const NodeStack&) = delete;
    NodeStack& operator=(const NodeStack&) = delete;
    NodeStack(NodeStack&&) = delete;
    NodeStack& operator=(NodeStack&&) = delete;
    virtual void Clear();
    virtual int Virtual02();

    // Takes ownership of an unlinked node; returns 0 if it is null or owned/linked elsewhere.
    int Push(LinkNode* node);
    // Transfers ownership of the top node to the caller.
    LinkNode* Pop();
    // Deletes `node` if it is on the stack, keeping the order of the others. Returns 1 if it was found.
    int RemoveAndDelete(LinkNode* node);

    // Temporary borrowed legacy view; delete after stack traversal callers migrate.
    LinkNode* m_top;  // +0x04

private:
    std::list<std::unique_ptr<LinkNode>> m_nodes;
    bool m_clearing = false;
};
