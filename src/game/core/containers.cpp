#include "core/containers.h"

#include "core/checked_new.h"

#include <algorithm>

namespace {

void DetachLinkNode(LinkNode* node) {
    node->m_prev = nullptr;
    node->m_next = nullptr;
}

}  // namespace

// ---- AvlNode ---------------------------------------------------------------------------------------------

AvlNode::AvlNode() : m_left(nullptr), m_right(nullptr), m_parent(nullptr), m_successor(nullptr), m_height(0) {}

AvlNode::~AvlNode() = default;

AvlNode* AvlNode::Predecessor() {
    AvlNode* node = m_left;
    if (node) {
        for (AvlNode* right = node->m_right; right; right = node->m_right) node = right;
        return node;
    }
    AvlNode* child = this;
    for (AvlNode* ancestor = m_parent; ancestor; ancestor = ancestor->m_parent) {
        if (ancestor->m_right == child) return ancestor;
        child = ancestor;
    }
    return nullptr;
}

// The three helpers below are linker-folded copies shared with other tree users (0x5ee3f0, 0x5ee410, 0x5ee440).
void AvlNode::ResetLinks() {
    m_left = nullptr;
    m_right = nullptr;
    m_parent = nullptr;
    m_successor = nullptr;
    m_height = 0;
}

void AvlNode::ReplaceChild(AvlNode* oldChild, AvlNode* newChild) {
    if (oldChild == m_right) {
        m_right = newChild;
        return;
    }
    if (oldChild == m_left) m_left = newChild;
}

int AvlNode::UpdateHeight() {
    uint16_t rightHeight = 0;
    uint16_t leftHeight = 0;
    if (m_right) rightHeight = static_cast<uint16_t>(m_right->m_height + 1);
    if (m_left) leftHeight = static_cast<uint16_t>(m_left->m_height + 1);
    m_height = rightHeight > leftHeight ? rightHeight : leftHeight;
    return static_cast<int>(rightHeight) - static_cast<int>(leftHeight);
}

// ---- AvlTree ---------------------------------------------------------------------------------------------

AvlTree::AvlTree() : m_root(nullptr) {}

AvlTree::~AvlTree() { Clear();  }

void AvlTree::Clear() {
    while (AvlNode* node = First()) {
        Remove(node);
        delete node;
    }
}

void AvlTree::Insert(AvlNode* node) {
    node->ResetLinks();
    AvlNode* predecessor = nullptr;
    AvlNode* current = m_root;
    if (!current) {
        m_root = node;
        Rebalance(node);
        return;
    }
    do {
        AvlNode* child;
        if (IsLess(node, current)) {
            child = current->m_left;
            if (!child) {
                current->m_left = node;
                node->m_parent = current;
                if (predecessor) predecessor->m_successor = node;
            }
            node->m_successor = current;
        } else {
            if (node == current) return;
            child = current->m_right;
            predecessor = current;
            if (!child) {
                current->m_right = node;
                node->m_parent = current;
                current->m_successor = node;
            }
        }
        current = child;
    } while (current);
    Rebalance(node);
}

namespace {

// Splices `node` out of the tree below `root` (children and parent links only) and returns the node from which
// the tree has to be rebalanced.
AvlNode* UnlinkNode(AvlNode*& root, AvlNode* node) {
    AvlNode* left = node->m_left;
    AvlNode* right = node->m_right;
    AvlNode* parent = node->m_parent;
    AvlNode* replacement;
    AvlNode* rebalanceFrom = parent;
    if (!left) {
        if (right) right->m_parent = parent;
        if (parent) parent->ReplaceChild(node, right);
        replacement = right;
    } else if (!right) {
        left->m_parent = parent;
        if (parent) parent->ReplaceChild(node, left);
        replacement = left;
    } else {
        right->m_parent = parent;
        if (parent) parent->ReplaceChild(node, right);
        AvlNode* leftmost = right;
        for (AvlNode* next = right->m_left; next; next = leftmost->m_left) leftmost = next;
        left->m_parent = leftmost;
        leftmost->m_left = left;
        rebalanceFrom = leftmost;
        replacement = right;
    }
    if (root == node) root = replacement;
    return rebalanceFrom;
}

void SetPredecessorSuccessor(AvlNode* node, AvlNode*& fallbackSlot) {
    AvlNode* predecessor = node->Predecessor();
    if (predecessor)
        predecessor->m_successor = node->m_successor;
    else
        fallbackSlot = node->m_successor;
}

}  // namespace

void AvlTree::Remove(AvlNode* node) {
    AvlNode* predecessor = node->Predecessor();
    if (predecessor) predecessor->m_successor = node->m_successor;
    AvlNode* rebalanceFrom = UnlinkNode(m_root, node);
    node->ResetLinks();
    Rebalance(rebalanceFrom);
}

AvlNode* AvlTree::First() const {
    AvlNode* node = m_root;
    AvlNode* first = nullptr;
    if (node) {
        first = node;
        for (node = node->m_left; node; node = node->m_left) first = node;
    }
    return first;
}

AvlNode* AvlTree::Last() const {
    AvlNode* node = m_root;
    AvlNode* last = nullptr;
    if (node) {
        last = node;
        for (node = node->m_right; node; node = node->m_right) last = node;
    }
    return last;
}

short AvlTree::Count() const {
    short count = 0;
    for (AvlNode* node = First(); node; node = node->m_successor) ++count;
    return count;
}

void AvlTree::Rebalance(AvlNode* node) {
    for (; node; node = node->m_parent) {
        short balance = static_cast<short>(node->UpdateHeight());
        while (balance < -1) {
            AvlNode* leftChild = node->m_left;
            if (static_cast<short>(leftChild->UpdateHeight()) > 0) RotateLeft(leftChild);
            node = RotateRight(node);
            balance = static_cast<short>(node->UpdateHeight());
        }
        while (balance > 1) {
            AvlNode* rightChild = node->m_right;
            if (static_cast<short>(rightChild->UpdateHeight()) < 0) RotateRight(rightChild);
            node = RotateLeft(node);
            balance = static_cast<short>(node->UpdateHeight());
        }
    }
}

AvlNode* AvlTree::RotateLeft(AvlNode* node) {
    AvlNode* pivot = node->m_right;
    AvlNode* parent = node->m_parent;
    if (!pivot) return node;
    AvlNode* inner = pivot->m_left;
    if (inner) inner->m_parent = node;
    node->m_right = inner;
    pivot->m_left = node;
    node->m_parent = pivot;
    node->UpdateHeight();
    if (parent) {
        parent->ReplaceChild(node, pivot);
        pivot->m_parent = parent;
        pivot->UpdateHeight();
        return pivot;
    }
    m_root = pivot;
    pivot->m_parent = nullptr;
    pivot->UpdateHeight();
    return pivot;
}

AvlNode* AvlTree::RotateRight(AvlNode* node) {
    AvlNode* pivot = node->m_left;
    AvlNode* parent = node->m_parent;
    if (!pivot) return node;
    AvlNode* inner = pivot->m_right;
    if (inner) inner->m_parent = node;
    node->m_left = inner;
    pivot->m_right = node;
    node->m_parent = pivot;
    node->UpdateHeight();
    if (parent) {
        parent->ReplaceChild(node, pivot);
        pivot->m_parent = parent;
        pivot->UpdateHeight();
        return pivot;
    }
    m_root = pivot;
    pivot->m_parent = nullptr;
    pivot->UpdateHeight();
    return pivot;
}

// ---- IdNode / IdValueNode ----------------------------------------------------------------------------------

IdNode::IdNode() : m_id(0) {}

IdNode::IdNode(uint32_t id) : m_id(id) {}

IdNode::~IdNode() = default;

void IdNode::SetId(uint32_t id) { m_id = id; }

void IdNode::ReadId(istream* in) { ReadValue32(in, &m_id); }

void IdNode::WriteId(ostream* out) { WriteValue32(out, &m_id); }

IdValueNode::IdValueNode(int value) : IdNode(), m_value(value) {}

IdValueNode::~IdValueNode() = default;

void IdValueNode::Read(istream* in) {
    ReadId(in);
    ReadValue32(in, &m_value);
}

void IdValueNode::Write(ostream* out) {
    WriteId(out);
    WriteValue32(out, &m_value);
}

// ---- IdAvlTree ---------------------------------------------------------------------------------------------

IdAvlTree::IdAvlTree() : AvlTree(), m_nextId(1) {}

IdAvlTree::~IdAvlTree() { IdAvlTree::Clear();  }

void IdAvlTree::Clear() {
    AvlTree::Clear();
    m_nextId = 1;
}

int IdAvlTree::IsLess(AvlNode* a, AvlNode* b) {
    if (a && b && static_cast<IdNode*>(a)->m_id < static_cast<IdNode*>(b)->m_id) return 1;
    return 0;
}

void IdAvlTree::Read(istream* in) {
    Clear();
    uint32_t count;
    ReadValue32(in, &count);
    for (uint32_t i = 0; i < count; ++i) {
        IdNode* node = NewOrNull<IdNode>();
        // BUG(original): a failed allocation is not checked before the node is used.
        node->ReadId(in);
        AddWithId(node);
    }
}

void IdAvlTree::Write(ostream* out) {
    uint32_t count = static_cast<uint16_t>(Count());
    WriteValue32(out, &count);
    for (AvlNode* node = First(); node; node = node->m_successor) static_cast<IdNode*>(node)->WriteId(out);
}

uint32_t IdAvlTree::AddWithId(IdNode* node) {
    if (!node) return 0;
    uint32_t id = node->m_id;
    if (id == 0) {
        node->SetId(m_nextId);
        id = m_nextId++;
        Insert(node);
        return id;
    }
    if (m_nextId <= id) m_nextId = id + 1;
    Insert(node);
    return id;
}

IdNode* IdAvlTree::FindById(uint32_t id) const {
    AvlNode* node = m_root;
    while (node && id != static_cast<IdNode*>(node)->m_id) {
        if (static_cast<IdNode*>(node)->m_id < id)
            node = node->m_right;
        else
            node = node->m_left;
    }
    return static_cast<IdNode*>(node);
}

void IdAvlTree::DeleteById(uint32_t id) {
    IdNode* node = FindById(id);
    if (node) {
        Remove(node);
        delete node;
    }
}

IdNode* IdAvlTree::DetachById(uint32_t id) {
    IdNode* node = FindById(id);
    if (node) Remove(node);
    return node;
}

// ---- IdValueTree -------------------------------------------------------------------------------------------

IdValueTree::IdValueTree() : IdAvlTree() {}

IdValueTree::~IdValueTree() = default;

void IdValueTree::Read(istream* in) {
    uint16_t count;
    ReadValue16(in, &count);
    while (count != 0) {
        --count;
        IdValueNode* node = NewOrNull<IdValueNode>(0);
        if (node) {
            node->Read(in);
            AddWithId(node);
        }
    }
}

void IdValueTree::Write(ostream* out) {
    uint16_t count = static_cast<uint16_t>(Count());
    WriteValue16(out, &count);
    for (AvlNode* node = First(); node; node = node->m_successor) static_cast<IdValueNode*>(node)->Write(out);
}

// ---- CloneableNode / IntKeyNode ----------------------------------------------------------------------------

CloneableNode::CloneableNode() : AvlNode() {}

CloneableNode::CloneableNode(const CloneableNode&) : AvlNode() {}

CloneableNode::~CloneableNode() = default;

IntKeyNode::IntKeyNode() : CloneableNode(), m_key(0) {}

IntKeyNode::IntKeyNode(const IntKeyNode& other)  = default;

IntKeyNode::~IntKeyNode() = default;

int IntKeyNode::Read(istream* in) {
    ReadValue32(in, &m_key);
    return 1;
}

int IntKeyNode::Write(ostream* out) {
    WriteValue32(out, &m_key);
    return 1;
}

// ---- AvlSet / IntKeyedSet ----------------------------------------------------------------------------------

AvlSet::AvlSet() : AvlTree() {}

AvlSet::~AvlSet() = default;

int AvlSet::IsLess(AvlNode* a, AvlNode* b) { return Precedes(a, b); }

int AvlSet::Contains(AvlNode* probe) { return Find(probe) != nullptr; }

AvlNode* AvlSet::Find(AvlNode* probe) {
    AvlNode* node = m_root;
    while (node && !Matches(probe, node)) {
        if (!Precedes(probe, node) && !Matches(probe, node))
            node = node->m_right;
        else
            node = node->m_left;
    }
    return node;
}

AvlNode* AvlSet::RemoveMatching(AvlNode* probe) {
    AvlNode* node = Find(probe);
    if (node) Remove(node);
    return node;
}

void AvlSet::AddIfAbsent(AvlNode* node) {
    if (!Contains(node)) Insert(node);
}

AvlSet& AvlSet::CopyFrom(const AvlSet& other) {
    if (&other != this) {
        AvlTree::Clear();
        for (AvlNode* node = other.First(); node; node = node->m_successor) {
            CloneableNode* copy = static_cast<CloneableNode*>(node)->Clone();
            if (copy) Insert(copy);
        }
    }
    return *this;
}

AvlSet& AvlSet::Intersect(AvlSet& other) {
    if (&other != this) {
        AvlNode* node = First();
        while (node) {
            AvlNode* next = node->m_successor;
            if (!other.Contains(node)) {
                RemoveMatching(node);
                delete node;
            }
            node = next;
        }
    }
    return *this;
}

IntKeyedSet::IntKeyedSet() : AvlSet() {}

IntKeyedSet::~IntKeyedSet() = default;

IntKeyedSet& IntKeyedSet::operator=(const IntKeyedSet& other) {
    if (this != &other) CopyFrom(other);
    return *this;
}

int IntKeyedSet::Precedes(AvlNode* probe, AvlNode* node) {
    return static_cast<IntKeyNode*>(probe)->m_key < static_cast<IntKeyNode*>(node)->m_key;
}

int IntKeyedSet::Matches(AvlNode* probe, AvlNode* node) {
    return static_cast<IntKeyNode*>(probe)->m_key == static_cast<IntKeyNode*>(node)->m_key;
}

// ---- OrderedTree -------------------------------------------------------------------------------------------

OrderedTree::OrderedTree() : m_root(nullptr), m_first(nullptr) {}

OrderedTree::~OrderedTree() { OrderedTree::Clear();  }

void OrderedTree::Clear() {
    while (AvlNode* node = m_first) {
        Remove(node);
        delete node;
    }
}

void OrderedTree::Insert(AvlNode* node) {
    AvlNode* predecessor = nullptr;
    AvlNode* current = m_root;
    if (!current) {
        m_root = node;
        m_first = node;
        node->m_left = nullptr;
        node->m_right = nullptr;
        node->m_parent = nullptr;
        node->m_successor = nullptr;
        return;
    }
    do {
        AvlNode* child;
        if (IsLess(node, current)) {
            child = current->m_left;
            if (!child) {
                current->m_left = node;
                node->m_parent = current;
                if (predecessor)
                    predecessor->m_successor = node;
                else
                    m_first = node;
            }
            node->m_successor = current;
        } else {
            child = current->m_right;
            predecessor = current;
            if (!child) {
                current->m_right = node;
                node->m_parent = current;
                current->m_successor = node;
            }
        }
        current = child;
    } while (current);
}

void OrderedTree::Remove(AvlNode* node) {
    SetPredecessorSuccessor(node, m_first);
    UnlinkNode(m_root, node);
    node->ResetLinks();
}

short OrderedTree::Count() const {
    short count = 0;
    for (AvlNode* node = m_first; node; node = node->m_successor) ++count;
    return count;
}

// ---- LinkNode / IdLinkNode ---------------------------------------------------------------------------------

LinkNode::LinkNode() : m_prev(nullptr), m_next(nullptr) {}

LinkNode::LinkNode(const LinkNode&) : m_prev(nullptr), m_next(nullptr) {}

LinkNode::~LinkNode() = default;

void LinkNode::Read(istream*) {}

void LinkNode::Write(ostream*) {}

IdLinkNode::IdLinkNode() : LinkNode(), m_id(0) {}

IdLinkNode::~IdLinkNode() = default;

void IdLinkNode::Read(istream* in) { ReadValue32(in, &m_id); }

void IdLinkNode::Write(ostream* out) { WriteValue32(out, &m_id); }

// ---- SortedLinkList / IdSortedLinkList ---------------------------------------------------------------------

SortedLinkList::SortedLinkList() : m_count(0), m_head(nullptr) {}

SortedLinkList::~SortedLinkList() { Clear();  }

int SortedLinkList::Insert(LinkNode* node) {
    if (!node || node->m_owner || node->m_prev || node->m_next) return 0;
    if (!m_head) {
        m_head = node;
        node->m_prev = nullptr;
        m_head->m_next = nullptr;
    } else {
        if (IsLess(node, m_head)) {
            m_head->m_prev = node;
            node->m_next = m_head;
            ++m_count;
            m_head = node;
            node->m_owner = this;
            return 1;
        }
        LinkNode* previous = m_head;
        if (previous == node) return 0;
        LinkNode* next = previous->m_next;
        while (next && !IsLess(node, next)) {
            if (next == node) return 0;
            previous = next;
            next = next->m_next;
        }
        previous->m_next = node;
        node->m_prev = previous;
        node->m_next = next;
        if (next) {
            next->m_prev = node;
            ++m_count;
            node->m_owner = this;
            return 1;
        }
    }
    node->m_owner = this;
    ++m_count;
    return 1;
}

int SortedLinkList::Remove(LinkNode* node) {
    if (!node || node->m_owner != this) return 0;
    if (node == m_head) {
        LinkNode* next = m_head->m_next;
        m_head = next;
        if (next) next->m_prev = nullptr;
    } else {
        LinkNode* previous = node->m_prev;
        LinkNode* next = node->m_next;
        if (previous) previous->m_next = next;
        if (next) next->m_prev = previous;
    }
    node->m_prev = nullptr;
    node->m_next = nullptr;
    node->m_owner = nullptr;
    --m_count;
    return 1;
}

void SortedLinkList::Clear() {
    LinkNode* next = m_head;
    while (LinkNode* node = next) {
        next = node->m_next;
        Remove(node);
        delete node;
    }
    m_count = 0;
}

void SortedLinkList::RewindToFirst() {
    if (m_head) {
        for (LinkNode* previous = m_head->m_prev; previous; previous = previous->m_prev) m_head = previous;
    }
}

IdSortedLinkList::IdSortedLinkList() : SortedLinkList(), m_nextId(1) {}

IdSortedLinkList::~IdSortedLinkList() = default;

int IdSortedLinkList::IsLess(LinkNode* a, LinkNode* b) {
    if (a && b && static_cast<IdLinkNode*>(a)->m_id < static_cast<IdLinkNode*>(b)->m_id) return 1;
    return 0;
}

void IdSortedLinkList::Reset() {
    Clear();
    m_nextId = 1;
}

uint32_t IdSortedLinkList::AddWithId(IdLinkNode* node) {
    uint32_t id = 0;
    if (node) {
        id = node->m_id;
        if (id == 0) node->m_id = m_nextId;
        if (Insert(node)) {
            if (id == 0) {
                id = m_nextId;
                m_nextId = id + 1;
                return id;
            }
            if (m_nextId <= id) m_nextId = id + 1;
        }
    }
    return id;
}

IdLinkNode* IdSortedLinkList::FindById(uint32_t id) const {
    LinkNode* node = m_head;
    while (node && static_cast<IdLinkNode*>(node)->m_id != id) node = node->m_next;
    return static_cast<IdLinkNode*>(node);
}

IdLinkNode* IdSortedLinkList::DetachById(uint32_t id) {
    IdLinkNode* node = FindById(id);
    if (node) Remove(node);
    return node;
}

void IdSortedLinkList::DeleteById(uint32_t id) {
    IdLinkNode* node = FindById(id);
    if (node) {
        Remove(node);
        delete node;
    }
}

// ---- NodeQueue / NodeStack ---------------------------------------------------------------------------------

NodeQueue::NodeQueue() : m_head(nullptr), m_tail(nullptr), m_count(0) {}

NodeQueue::~NodeQueue() { Clear(); }

int NodeQueue::Enqueue(LinkNode* node) {
    if (m_clearing || !node || node->m_owner || node->m_prev || node->m_next) return 0;
    m_nodes.emplace_back(node);
    node->m_owner = this;
    if (m_tail) {
        m_tail->m_next = node;
        node->m_prev = m_tail;
        m_tail = node;
        ++m_count;
        return 1;
    }
    m_head = node;
    m_tail = node;
    ++m_count;
    return 1;
}

LinkNode* NodeQueue::Dequeue() {
    LinkNode* head = m_head;
    if (!head) return nullptr;
    LinkNode* next = head->m_next;
    m_head = next;
    if (next) {
        next->m_prev = nullptr;
    } else {
        m_tail = nullptr;
    }
    DetachLinkNode(head);
    head->m_owner = nullptr;
    m_nodes.front().release();
    m_nodes.pop_front();
    --m_count;
    return head;
}

void NodeQueue::Clear() {
    const bool wasClearing = m_clearing;
    m_clearing = true;
    struct RestoreClearingState {
        bool& state;
        bool previous;
        ~RestoreClearingState() { state = previous; }
    } restore{m_clearing, wasClearing};

    m_head = nullptr;
    m_tail = nullptr;
    m_count = 0;
    std::list<std::unique_ptr<LinkNode>> detached;
    detached.splice(detached.end(), m_nodes);
    for (const auto& node : detached) {
        node->m_owner = nullptr;
        DetachLinkNode(node.get());
    }
}

NodeStack::NodeStack() : m_top(nullptr) {}

NodeStack::~NodeStack() { NodeStack::Clear(); }

void NodeStack::Clear() {
    const bool wasClearing = m_clearing;
    m_clearing = true;
    struct RestoreClearingState {
        bool& state;
        bool previous;
        ~RestoreClearingState() { state = previous; }
    } restore{m_clearing, wasClearing};

    m_top = nullptr;
    std::list<std::unique_ptr<LinkNode>> detached;
    detached.splice(detached.end(), m_nodes);
    for (const auto& node : detached) {
        node->m_owner = nullptr;
        DetachLinkNode(node.get());
    }
}

int NodeStack::Virtual02() { return 0; }

int NodeStack::Push(LinkNode* node) {
    if (m_clearing || !node || node->m_owner || node->m_prev || node->m_next) return 0;
    m_nodes.emplace_front(node);
    node->m_owner = this;
    if (m_top) {
        m_top->m_prev = node;
        node->m_next = m_top;
    }
    node->m_prev = nullptr;
    m_top = node;
    return 1;
}

LinkNode* NodeStack::Pop() {
    LinkNode* top = m_top;
    if (!top) return nullptr;
    LinkNode* next = top->m_next;
    m_top = next;
    if (next) next->m_prev = nullptr;
    DetachLinkNode(top);
    top->m_owner = nullptr;
    m_nodes.front().release();
    m_nodes.pop_front();
    return top;
}

int NodeStack::RemoveAndDelete(LinkNode* target) {
    const auto found = std::find_if(m_nodes.begin(), m_nodes.end(), [target](const auto& owned) {
        return owned.get() == target;
    });
    if (found == m_nodes.end()) return 0;
    LinkNode* node = found->get();
    if (node == m_top) {
        m_top = node->m_next;
        if (m_top) m_top->m_prev = nullptr;
    } else {
        if (node->m_prev) node->m_prev->m_next = node->m_next;
        if (node->m_next) node->m_next->m_prev = node->m_prev;
    }
    node->m_owner = nullptr;
    DetachLinkNode(node);
    m_nodes.erase(found);
    return 1;
}
