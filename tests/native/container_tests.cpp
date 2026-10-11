#include "core/containers.h"
#include "core/iostream_lib.h"

#include <cstdlib>
#include <iterator>

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
    ++destroyed;
  }

  int& destroyed;
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
    Require(list.Empty());
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
    Require(tree.PositionOf(rejected) == tree.Entries().end());
    delete rejected;
    ++rejectedDestroyed;
  }

  NumericTree& tree;
  int& rejectedDestroyed;
};

void VerifyCrossContainerListOwnership()
{
  int destroyed = 0;
  UnorderedList source;
  UnorderedList destination;
  auto* node = new CountedNode(destroyed);
  Require(source.Insert(node) == 1);
  Require(destination.Insert(node) == 0);
  Require(source.Remove(node) == 1);
  Require(destination.Insert(node) == 1);
  Require(source.Insert(node) == 0);
  destination.Clear();
  Require(destroyed == 1);
}

void VerifyLinkNodeCopiesStartUnlinked()
{
  int destroyed = 0;
  UnorderedList source;
  UnorderedList destination;
  auto* owned = new CountedNode(destroyed);
  Require(source.Insert(owned) == 1);
  CountedNode copy(*owned);
  Require(destination.Insert(&copy) == 1);
  Require(destination.Remove(&copy) == 1);
  source.Clear();
  Require(destroyed == 1);
}

void VerifySortedListOwnershipAndOrder()
{
  KeyedList list;
  UnorderedList foreign;
  auto* high = new KeyedLinkNode(7);
  auto* low = new KeyedLinkNode(1);
  auto* middle = new KeyedLinkNode(4);
  auto* duplicateKey = new KeyedLinkNode(4);
  Require(list.Insert(high) == 1);
  Require(list.Insert(low) == 1);
  Require(list.Insert(middle) == 1);
  Require(list.Insert(duplicateKey) == 1);
  Require(list.Count() == 4 && list.First() == low);
  auto position = list.Entries().begin();
  Require((position++)->get() == low);
  Require((position++)->get() == middle);
  Require((position++)->get() == duplicateKey);
  Require((position++)->get() == high && position == list.Entries().end());
  Require(foreign.Insert(middle) == 0 && list.Insert(middle) == 0);
  Require(list.Remove(middle) == 1);
  Require(foreign.Insert(middle) == 1 && list.Insert(middle) == 0);
  Require(foreign.Remove(middle) == 1);
  Require(list.Insert(middle) == 1);
  Require(list.Remove(high) == 1);
  delete high;
  list.Clear();
  Require(list.Empty());
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
  auto position = list.Entries().begin();
  Require((position++)->get() == first);
  Require((position++)->get() == inserted);
  Require((position++)->get() == anchor);
  Require((position++)->get() == last && position == list.Entries().end());
  Require(list.Remove(anchor) == 1);
  delete anchor;
  Require(destroyed == 1 && std::next(list.PositionOf(inserted))->get() == last);
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
  auto position = tree.Entries().begin();
  Require((position++)->get() == first);
  Require((position++)->get() == lowerDuplicate);
  Require((position++)->get() == upperDuplicate);
  Require((position++)->get() == middle && position == tree.Entries().end());
  Require(tree.Find(4) == first && tree.Find(7) == middle && tree.Find(8) == nullptr);
  Require(foreign.Insert(first) == 0);
  foreign.Remove(first);
  Require(tree.Count() == 4 && tree.Find(4) == first);

  NumericNode copied(*first);
  Require(foreign.Insert(&copied) == 1);
  foreign.Remove(&copied);

  tree.Remove(lowerDuplicate);
  Require(tree.Find(4) == first && std::next(tree.PositionOf(first))->get() == upperDuplicate);
  delete lowerDuplicate;
  tree.Remove(first);
  tree.Clear();
  Require(tree.Count() == 0 && tree.First() == nullptr && tree.Last() == nullptr);
  Require(foreign.Insert(first) == 1);
  Require(foreign.Count() == 1);
  foreign.Clear();
}

void VerifyIterationDuringErase()
{
  NumericTree tree;
  for (int key : {1, 2, 3, 4, 5})
    Require(tree.Insert(new NumericNode(key)) == 1);
  auto position = tree.Entries().begin();
  while (position != tree.Entries().end())
  {
    auto* node = static_cast<NumericNode*>((position++)->get());
    if (node->key % 2 == 0)
    {
      tree.Remove(node);
      delete node;
    }
  }
  Require(tree.Count() == 3 && tree.Find(1) && tree.Find(3) && tree.Find(5));
  Require(tree.PositionOf(nullptr) == tree.Entries().end());
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
  VerifyCrossContainerListOwnership();
  VerifyLinkNodeCopiesStartUnlinked();
  VerifySortedListOwnershipAndOrder();
  VerifySortedListClearRejectsReentrantInsert();
  VerifySortedListExplicitInsertionOrderAndDetach();
  VerifySortedListIds();
  VerifyAvlOwnershipOrderAndLookup();
  VerifyIterationDuringErase();
  VerifyAvlIdTransferAndLargeCount();
  VerifyCompositeIdTreeUniquenessUsesComparatorOrder();
  VerifyAvlClearRejectsReentrantInsert();
  VerifyIdTreeCountSerialization();
  VerifyIdValueTreeRejectsCountOverflow();
}
