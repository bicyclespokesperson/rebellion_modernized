#include "core/containers.h"
#include "core/iostream_lib.h"

#include <cstdlib>

namespace
{

void Require(bool condition)
{
  if (!condition)
    std::abort();
}

struct CountedNode final : LinkNode
{
  explicit CountedNode(int& destroyed) : destroyed(destroyed)
  {
  }
  ~CountedNode() override
  {
    Require(m_prev == nullptr && m_next == nullptr);
    ++destroyed;
  }

  int& destroyed;
};

struct QueueClearNode final : LinkNode
{
  QueueClearNode(NodeQueue& queue, int& destroyed, int& rejectedDestroyed)
  : queue(queue),
    destroyed(destroyed),
    rejectedDestroyed(rejectedDestroyed)
  {
  }
  ~QueueClearNode() override
  {
    Require(queue.m_head == nullptr && queue.m_tail == nullptr && queue.m_count == 0);
    Require(m_prev == nullptr && m_next == nullptr);
    queue.Clear();
    auto* rejected = new CountedNode(rejectedDestroyed);
    Require(queue.Enqueue(rejected) == 0);
    delete rejected;
    ++destroyed;
  }

  NodeQueue& queue;
  int& destroyed;
  int& rejectedDestroyed;
};

struct StackClearNode final : LinkNode
{
  StackClearNode(NodeStack& stack, int& destroyed, int& rejectedDestroyed)
  : stack(stack),
    destroyed(destroyed),
    rejectedDestroyed(rejectedDestroyed)
  {
  }
  ~StackClearNode() override
  {
    Require(stack.m_top == nullptr);
    Require(m_prev == nullptr && m_next == nullptr);
    stack.Clear();
    auto* rejected = new CountedNode(rejectedDestroyed);
    Require(stack.Push(rejected) == 0);
    delete rejected;
    ++destroyed;
  }

  NodeStack& stack;
  int& destroyed;
  int& rejectedDestroyed;
};

struct UnorderedList final : SortedLinkList
{
  int IsLess(LinkNode*, LinkNode*) override
  {
    return 0;
  }
};

struct KeyedLinkNode final : IdLinkNode
{
  explicit KeyedLinkNode(int key) : key(key)
  {
    m_id = static_cast<uint32_t>(key);
  }
  int key;
};

struct KeyedList final : SortedLinkList
{
  int IsLess(LinkNode* first, LinkNode* second) override
  {
    return static_cast<KeyedLinkNode*>(first)->key < static_cast<KeyedLinkNode*>(second)->key;
  }
};

struct ListClearNode final : LinkNode
{
  ListClearNode(SortedLinkList& list, int& destroyed, int& rejectedDestroyed)
  : list(list),
    destroyed(destroyed),
    rejectedDestroyed(rejectedDestroyed)
  {
  }
  ~ListClearNode() override
  {
    Require(list.m_head == nullptr && list.m_count == 0);
    Require(m_prev == nullptr && m_next == nullptr);
    list.Clear();
    auto* rejected = new CountedNode(rejectedDestroyed);
    Require(list.Insert(rejected) == 0);
    delete rejected;
    ++destroyed;
  }

  SortedLinkList& list;
  int& destroyed;
  int& rejectedDestroyed;
};

struct NumericNode : AvlNode
{
  explicit NumericNode(int key) : key(key)
  {
  }
  NumericNode(const NumericNode& other) : AvlNode(other), key(other.key)
  {
  }
  int key;
};

struct NumericTree final : AvlTree
{
  int IsLess(AvlNode* first, AvlNode* second) override
  {
    return static_cast<NumericNode*>(first)->key < static_cast<NumericNode*>(second)->key;
  }

  NumericNode* Find(int key) const
  {
    AvlNode* found = LowerBound(
      [key](const AvlNode* candidate)
      {
        return static_cast<const NumericNode*>(candidate)->key < key;
      });
    auto* node = static_cast<NumericNode*>(found);
    return node && node->key == key ? node : nullptr;
  }
};

struct FamilyIdNode final : IdNode
{
  FamilyIdNode(uint32_t family, uint32_t id) : IdNode(id), family(family)
  {
  }
  uint32_t family;
};

struct FamilyIdTree final : IdAvlTree
{
  int IsLess(AvlNode* first, AvlNode* second) override
  {
    const auto* a = static_cast<FamilyIdNode*>(first);
    const auto* b = static_cast<FamilyIdNode*>(second);
    return a->family < b->family || (a->family == b->family && a->m_id < b->m_id);
  }
};

struct ReentrantTreeNode final : NumericNode
{
  ReentrantTreeNode(NumericTree& tree, int& rejectedDestroyed)
  : NumericNode(9),
    tree(tree),
    rejectedDestroyed(rejectedDestroyed)
  {
  }
  ~ReentrantTreeNode() override
  {
    Require(tree.First() == nullptr && tree.Count() == 0);
    tree.Clear();
    auto* rejected = new NumericNode(10);
    Require(tree.Insert(rejected) == 0);
    Require(rejected->m_predecessor == nullptr && rejected->m_successor == nullptr);
    delete rejected;
    ++rejectedDestroyed;
  }

  NumericTree& tree;
  int& rejectedDestroyed;
};

void VerifyQueueOwnershipAndOrder()
{
  int destroyed = 0;
  NodeQueue queue;
  NodeStack stack;
  auto* first = new CountedNode(destroyed);
  auto* second = new CountedNode(destroyed);

  Require(queue.Enqueue(nullptr) == 0);
  Require(queue.Enqueue(first) == 1);
  Require(queue.Enqueue(first) == 0);
  Require(stack.Push(first) == 0);
  Require(queue.Enqueue(second) == 1);
  Require(queue.m_head == first && queue.m_tail == second && queue.m_count == 2);
  Require(first->m_next == second && second->m_prev == first);

  LinkNode* dequeued = queue.Dequeue();
  Require(dequeued == first && dequeued->m_prev == nullptr && dequeued->m_next == nullptr);
  delete dequeued;
  Require(destroyed == 1 && queue.m_head == second && queue.m_tail == second && queue.m_count == 1);
  queue.Clear();
  Require(destroyed == 2 && queue.m_head == nullptr && queue.m_tail == nullptr && queue.m_count == 0);
}

void VerifyStackOwnershipAndOrder()
{
  int destroyed = 0;
  NodeQueue queue;
  NodeStack stack;
  auto* bottom = new CountedNode(destroyed);
  auto* middle = new CountedNode(destroyed);
  auto* top = new CountedNode(destroyed);
  auto* foreign = new CountedNode(destroyed);

  Require(stack.Push(nullptr) == 0);
  Require(queue.Enqueue(foreign) == 1);
  Require(stack.Push(bottom) == 1);
  Require(queue.Enqueue(bottom) == 0);
  Require(stack.Push(middle) == 1);
  Require(stack.Push(top) == 1);
  Require(stack.m_top == top && top->m_next == middle && middle->m_next == bottom);
  Require(stack.RemoveAndDelete(foreign) == 0);
  Require(stack.RemoveAndDelete(middle) == 1);
  Require(destroyed == 1 && top->m_next == bottom && bottom->m_prev == top);

  Require(stack.Pop() == top);
  delete top;
  Require(stack.Pop() == bottom);
  delete bottom;
  Require(destroyed == 3 && stack.m_top == nullptr);
  delete queue.Dequeue();
  Require(destroyed == 4);
}

void VerifyCrossContainerListOwnership()
{
  int destroyed = 0;
  UnorderedList sorted;
  NodeQueue queue;
  auto* node = new CountedNode(destroyed);

  Require(sorted.Insert(node) == 1);
  Require(queue.Enqueue(node) == 0);
  Require(sorted.Remove(node) == 1);
  Require(queue.Enqueue(node) == 1);
  Require(sorted.Insert(node) == 0);
  delete queue.Dequeue();
  Require(destroyed == 1);
}

void VerifyLinkNodeCopiesStartUnlinked()
{
  int destroyed = 0;
  NodeQueue source;
  NodeStack destination;
  auto* owned = new CountedNode(destroyed);
  Require(source.Enqueue(owned) == 1);

  CountedNode copy(*owned);
  Require(copy.m_prev == nullptr && copy.m_next == nullptr);
  Require(destination.Push(&copy) == 1);
  Require(destination.Pop() == &copy);
  delete source.Dequeue();
  Require(destroyed == 1);
}

void VerifySortedListOwnershipAndOrder()
{
  KeyedList list;
  NodeQueue queue;
  auto* high = new KeyedLinkNode(7);
  auto* low = new KeyedLinkNode(1);
  auto* middle = new KeyedLinkNode(4);
  auto* duplicateKey = new KeyedLinkNode(4);
  Require(list.Insert(high) == 1);
  Require(list.Insert(low) == 1);
  Require(list.Insert(middle) == 1);
  Require(list.Insert(duplicateKey) == 1);
  Require(list.m_count == 4 && list.m_head == low);
  Require(low->m_next == middle && middle->m_next == duplicateKey && duplicateKey->m_next == high);
  Require(queue.Enqueue(middle) == 0 && list.Insert(middle) == 0);

  list.m_head = high;
  list.RewindToFirst();
  Require(list.m_head == low);
  Require(list.Remove(middle) == 1);
  Require(middle->m_prev == nullptr && middle->m_next == nullptr);
  Require(queue.Enqueue(middle) == 1);
  Require(list.Insert(middle) == 0);
  Require(queue.Dequeue() == middle);
  Require(list.Insert(middle) == 1);
  Require(list.Remove(high) == 1);
  delete high;
  list.Clear();
  Require(list.m_head == nullptr && list.m_count == 0);
  list.Clear();
}

void VerifySortedListClearRejectsReentrantInsert()
{
  UnorderedList list;
  int destroyed = 0;
  int rejectedDestroyed = 0;
  Require(list.Insert(new ListClearNode(list, destroyed, rejectedDestroyed)) == 1);
  Require(list.Insert(new ListClearNode(list, destroyed, rejectedDestroyed)) == 1);
  list.Clear();
  Require(destroyed == 2 && rejectedDestroyed == 2);
  Require(list.Insert(new CountedNode(rejectedDestroyed)) == 1);
  list.Clear();
}

void VerifySortedListExplicitInsertionOrderAndDetach()
{
  UnorderedList list;
  UnorderedList foreign;
  int destroyed = 0;
  auto* first = new CountedNode(destroyed);
  auto* anchor = new CountedNode(destroyed);
  auto* last = new CountedNode(destroyed);
  auto* inserted = new CountedNode(destroyed);
  auto* foreignAnchor = new CountedNode(destroyed);
  Require(list.Insert(first) == 1);
  Require(list.Insert(anchor) == 1);
  Require(list.Insert(last) == 1);
  Require(foreign.Insert(foreignAnchor) == 1);
  Require(list.InsertBefore(inserted, foreignAnchor) == 0);
  Require(list.InsertBefore(inserted, anchor) == 1);
  Require(list.m_head == first && first->m_next == inserted && inserted->m_next == anchor && anchor->m_next == last);
  Require(list.Remove(anchor) == 1);
  delete anchor;
  Require(destroyed == 1 && inserted->m_next == last);
  list.Clear();
  foreign.Clear();
  Require(destroyed == 5);
}

void VerifySortedListIds()
{
  IdSortedLinkList list;
  auto* first = new IdLinkNode();
  auto* duplicate = new IdLinkNode();
  duplicate->m_id = 1;
  auto* maximum = new IdLinkNode();
  maximum->m_id = UINT32_MAX;
  Require(list.AddWithId(first) == 1);
  Require(list.AddWithId(duplicate) == 0);
  delete duplicate;
  Require(list.AddWithId(maximum) == UINT32_MAX);
  auto* exhausted = new IdLinkNode();
  Require(list.AddWithId(exhausted) == 0 && exhausted->m_id == 0);
  delete exhausted;
  Require(list.DetachById(1) == first);
  delete first;
  list.Clear();
}

void VerifyClearDetachesBeforeDestruction()
{
  int queueDestroyed = 0;
  int queueRejectedDestroyed = 0;
  NodeQueue queue;
  Require(queue.Enqueue(new QueueClearNode(queue, queueDestroyed, queueRejectedDestroyed)) == 1);
  Require(queue.Enqueue(new QueueClearNode(queue, queueDestroyed, queueRejectedDestroyed)) == 1);
  queue.Clear();
  Require(queueDestroyed == 2);
  Require(queueRejectedDestroyed == 2);
  queue.Clear();
  Require(queue.m_head == nullptr && queue.m_tail == nullptr && queue.m_count == 0);
  int afterQueueClearDestroyed = 0;
  auto* afterQueueClear = new CountedNode(afterQueueClearDestroyed);
  Require(queue.Enqueue(afterQueueClear) == 1);
  delete queue.Dequeue();
  Require(afterQueueClearDestroyed == 1);

  int stackDestroyed = 0;
  int stackRejectedDestroyed = 0;
  NodeStack stack;
  Require(stack.Push(new StackClearNode(stack, stackDestroyed, stackRejectedDestroyed)) == 1);
  Require(stack.Push(new StackClearNode(stack, stackDestroyed, stackRejectedDestroyed)) == 1);
  stack.Clear();
  Require(stackDestroyed == 2);
  Require(stackRejectedDestroyed == 2);
  stack.Clear();
  Require(stack.m_top == nullptr);
  int afterStackClearDestroyed = 0;
  auto* afterStackClear = new CountedNode(afterStackClearDestroyed);
  Require(stack.Push(afterStackClear) == 1);
  delete stack.Pop();
  Require(afterStackClearDestroyed == 1);
}

void VerifyAvlOwnershipOrderAndLookup()
{
  NumericTree tree;
  NumericTree foreign;
  auto* first = new NumericNode(4);
  auto* lowerDuplicate = new NumericNode(4);
  auto* middle = new NumericNode(7);
  auto* upperDuplicate = new NumericNode(4);
  Require(tree.Insert(first) == 1);
  Require(tree.Insert(lowerDuplicate) == 1);
  Require(tree.Insert(middle) == 1);
  Require(tree.Insert(upperDuplicate) == 1);
  Require(tree.Count() == 4);
  Require(tree.First() == first && tree.Last() == middle);
  Require(first->m_successor == lowerDuplicate && lowerDuplicate->m_predecessor == first);
  Require(lowerDuplicate->m_successor == upperDuplicate && upperDuplicate->m_predecessor == lowerDuplicate);
  Require(upperDuplicate->m_successor == middle && middle->m_predecessor == upperDuplicate);
  Require(tree.Find(4) == first && tree.Find(7) == middle && tree.Find(8) == nullptr);
  Require(foreign.Insert(first) == 0);
  foreign.Remove(first);
  Require(tree.Count() == 4 && tree.Find(4) == first);

  NumericNode copied(*first);
  Require(copied.m_predecessor == nullptr && copied.m_successor == nullptr);
  Require(foreign.Insert(&copied) == 1);
  foreign.Remove(&copied);

  NumericNode linked(1);
  NumericNode linkedTo(2);
  linked.m_successor = &linkedTo;
  Require(foreign.Insert(&linked) == 0);
  Require(linked.m_successor == &linkedTo);
  linked.ResetLinks();

  tree.Remove(lowerDuplicate);
  Require(lowerDuplicate->m_predecessor == nullptr && lowerDuplicate->m_successor == nullptr);
  Require(tree.Find(4) == first && first->m_successor == upperDuplicate);
  delete lowerDuplicate;
  tree.Remove(first);
  tree.Clear();
  Require(tree.Count() == 0 && tree.First() == nullptr && tree.Last() == nullptr);
  Require(foreign.Insert(first) == 1);
  Require(foreign.Count() == 1);
  foreign.Clear();
}

void VerifyAvlIdTransferAndLargeCount()
{
  IdAvlTree ids;
  IdAvlTree destination;
  auto* automatic = new IdNode();
  Require(ids.AddWithId(automatic) == 1);
  Require(destination.AddWithId(automatic) == 0 && automatic->m_id == 1);
  Require(destination.FindById(1) == nullptr && ids.FindById(1) == automatic);
  Require(ids.DetachById(1) == automatic);
  Require(destination.AddWithId(automatic) == 1);
  Require(destination.FindById(1) == automatic);
  auto* duplicate = new IdNode(1);
  Require(destination.AddWithId(duplicate) == 0 && duplicate->m_id == 1);
  delete duplicate;
  destination.DeleteById(1);
  Require(destination.Count() == 0);

  IdAvlTree exhausted;
  Require(exhausted.AddWithId(new IdNode(0xffffffffu)) == 0xffffffffu);
  auto* afterMaximum = new IdNode();
  Require(exhausted.AddWithId(afterMaximum) == 0 && afterMaximum->m_id == 0);
  delete afterMaximum;

  NumericTree large;
  constexpr int nodeCount = 70000;
  for (int i = nodeCount - 1; i >= 0; --i)
    Require(large.Insert(new NumericNode(i)) == 1);
  Require(large.Count() == static_cast<size_t>(nodeCount));
  Require(large.Find(0)->key == 0 && large.Find(nodeCount - 1)->key == nodeCount - 1);
  large.Clear();
  Require(large.Count() == 0);
}

void VerifyCompositeIdTreeUniquenessUsesComparatorOrder()
{
  FamilyIdTree tree;
  auto* firstFamily = new FamilyIdNode(10, 7);
  auto* secondFamily = new FamilyIdNode(20, 7);
  auto* duplicate = new FamilyIdNode(10, 7);
  Require(tree.AddWithId(firstFamily) == 7);
  Require(tree.AddWithId(secondFamily) == 7);
  Require(tree.AddWithId(duplicate) == 0);
  delete duplicate;
  Require(tree.Count() == 2);
  tree.Clear();
}

void Rewind(ScopedStrstream& stream)
{
  Require(istream_seekg_pos(stream.get()->In(), 0) == stream.get()->In());
}

void VerifyIdTreeCountSerialization()
{
  IdAvlTree source;
  Require(source.AddWithId(new IdNode(7)) == 7);
  Require(source.AddWithId(new IdNode(2)) == 2);
  Require(source.AddWithId(new IdNode(11)) == 11);

  ScopedStrstream stream;
  source.Write(stream.get()->Out());
  Rewind(stream);

  IdAvlTree restored;
  restored.Read(stream.get()->In());
  Require(restored.Count() == 3);
  Require(restored.FindById(2) && restored.FindById(7) && restored.FindById(11));
}

void VerifyIdValueTreeRejectsCountOverflow()
{
  IdValueTree tree;
  constexpr uint32_t nodeCount = 65536;
  for (uint32_t i = 1; i <= nodeCount; ++i)
    Require(tree.AddWithId(new IdValueNode(static_cast<int>(i))) == i);
  Require(tree.Count() == nodeCount);

  ScopedStrstream stream;
  tree.Write(stream.get()->Out());
  Require((stream.get()->Ios()->m_state & iostream_flags::kFail) != 0);
  const auto* buffer = iostream_access::BufferOf(stream.get());
  Require(buffer->m_pptr == buffer->m_pbase);
}

void VerifyAvlClearRejectsReentrantInsert()
{
  NumericTree tree;
  int rejectedDestroyed = 0;
  Require(tree.Insert(new ReentrantTreeNode(tree, rejectedDestroyed)) == 1);
  tree.Clear();
  Require(tree.Count() == 0 && rejectedDestroyed == 1);
  Require(tree.Insert(new NumericNode(11)) == 1);
  tree.Clear();
}

} // namespace

int main()
{
  VerifyQueueOwnershipAndOrder();
  VerifyStackOwnershipAndOrder();
  VerifyCrossContainerListOwnership();
  VerifyLinkNodeCopiesStartUnlinked();
  VerifySortedListOwnershipAndOrder();
  VerifySortedListClearRejectsReentrantInsert();
  VerifySortedListExplicitInsertionOrderAndDetach();
  VerifySortedListIds();
  VerifyClearDetachesBeforeDestruction();
  VerifyAvlOwnershipOrderAndLookup();
  VerifyAvlIdTransferAndLargeCount();
  VerifyCompositeIdTreeUniquenessUsesComparatorOrder();
  VerifyAvlClearRejectsReentrantInsert();
  VerifyIdTreeCountSerialization();
  VerifyIdValueTreeRejectsCountOverflow();
}
