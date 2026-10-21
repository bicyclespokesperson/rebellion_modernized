#include "ai/ai_selector.h"
#include "core/id_registry.h"
#include "core/iostream_lib.h"
#include "gameobj/object_key.h"
#include "gameobj/status_code.h"

#include <cstdlib>
#include <limits>

int64_t GameSeedSeconds()
{
  return 1;
}

int GameplayRandomNext(int)
{
  std::abort();
}

namespace
{
void Require(bool condition)
{
  if (!condition) {
    std::abort();
  }
}

void Rewind(ScopedStrstream& stream)
{
  Require(istream_seekg_pos(stream.get()->In(), 0) == stream.get()->In());
}

struct RegistryEntry {
  uint32_t m_id = 0;
  IdRegistry<RegistryEntry>* owner = nullptr;
  int* destroyed = nullptr;
  bool tryReinsert = false;
  ~RegistryEntry()
  {
    if (destroyed) {
      ++*destroyed;
    }
    if (owner) {
      Require(!owner->Remove(this));
      if (tryReinsert) {
        auto replacement = std::make_unique<RegistryEntry>();
        Require(owner->AddWithId(replacement.get()) == 0);
      }
    }
  }
};

void VerifyRegistryLifetimeAndIdentity()
{
  IdRegistry<RegistryEntry> first;
  IdRegistry<RegistryEntry> second;
  int destroyed = 0;
  auto entry = std::make_unique<RegistryEntry>();
  entry->owner = &first;
  entry->destroyed = &destroyed;
  entry->tryReinsert = true;
  Require(first.AddWithId(entry.get()) == 1);
  Require(!second.Remove(entry.get()));
  Require(first.FindById(1) == entry.get());
  Require(first.PositionOf(entry.get()) == first.Entries().begin());
  Require(second.PositionOf(entry.get()) == second.Entries().end());
  Require(first.PositionOf(nullptr) == first.Entries().end());
  entry.release();
  first.Clear();
  Require(first.Empty() && destroyed == 1 && first.NextId() == 1);

  auto explicitId = std::make_unique<RegistryEntry>();
  explicitId->m_id = 7;
  Require(first.AddWithId(explicitId.get()) == 7);
  Require(!first.AddWithId(explicitId.get()));
  Require(first.NextId() == 8);
  explicitId.release();
  std::unique_ptr<RegistryEntry> transferred(first.DetachById(7));
  Require(first.Empty() && transferred);
  Require(second.AddWithId(transferred.get()) == 7);
  transferred.release();
  Require(second.NextId() == 8);
}

void VerifyObjectKeyOrderAndCopies()
{
  ObjectKeyValueSet keys;
  const ObjectKey a = ObjectKey::Make(0x90, 7);
  const ObjectKey b = ObjectKey::Make(0x91, 7);
  Require(keys.AddKeyValue(&b, 3));
  Require(keys.AddKeyValue(&a, 2));
  Require(keys.AddKeyValue(&a, 4));
  Require(keys.Count() == 3);
  auto position = keys.Entries().begin();
  Require(static_cast<ObjectKeyValueNode*>(position->get())->m_value == 2);
  ++position;
  Require(static_cast<ObjectKeyValueNode*>(position->get())->m_value == 4);
  ++position;
  Require((*position)->m_key.m_value == b.m_value);

  ObjectKeyValueSet copy(keys);
  Require(copy == keys);
  Require(copy.First() != keys.First());
  std::unique_ptr<ObjectKeyNode> detached(keys.Detach(&a));
  Require(detached && keys.Count() == 2 && copy.Count() == 3);
  Require(keys.Add(detached.get()) == a.Id());
  detached.release();

  ScopedStrstream stream;
  copy.Write(stream.get()->Out());
  Rewind(stream);
  ObjectKeyValueSet restored;
  restored.Read(stream.get()->In());
  Require(iostream_access::State(stream.get()->In()) == 0);
  Require(restored == copy);
}

void VerifyReferenceIdsAndTransfer()
{
  ObjectRefList source;
  auto entry = std::make_unique<ObjectRefNode>();
  entry->m_id = 17;
  entry->m_key = ObjectKey::Make(0x90, 3);
  Require(source.Add(entry.get()));
  entry.release();
  auto duplicate = std::make_unique<ObjectRefNode>();
  duplicate->m_id = 17;
  Require(!source.Add(duplicate.get()));
  auto appended = std::make_unique<ObjectRefNode>();
  Require(source.Add(appended.get()));
  Require(appended->m_id == 18);
  appended.release();

  ObjectRefList destination;
  std::unique_ptr<ObjectRefNode> transferred(source.DetachById(17));
  Require(transferred && !source.FindById(17));
  Require(destination.Add(transferred.get()));
  transferred.release();
  auto exhausted = std::make_unique<ObjectRefNode>();
  exhausted->m_id = std::numeric_limits<uint32_t>::max();
  Require(destination.Add(exhausted.get()));
  exhausted.release();
  auto unassigned = std::make_unique<ObjectRefNode>();
  Require(!destination.Add(unassigned.get()));
  Require(unassigned->m_id == 0);
  destination.Clear();
  Require(destination.Add(unassigned.get()));
  Require(unassigned->m_id == 1);
  unassigned.release();
}

void VerifyReferenceCountAndCodec()
{
  ObjectRefList references;
  constexpr uint32_t count = 65536;
  for (uint32_t i = 0; i < count; ++i) {
    Require(references.AddKey(ObjectKey::Make(0x90, i + 1)));
  }
  Require(references.Size() == count);
  ScopedStrstream stream;
  Require(references.Write(stream.get()->Out()));
  Rewind(stream);
  ObjectRefList restored;
  Require(restored.Read(stream.get()->In()));
  Require(restored.Count() == count);
  Require(restored.Last()->m_key.m_value == references.Last()->m_key.m_value);

  ScopedStrstream malformed;
  int ok = 1;
  uint32_t truncatedCount = 3;
  WriteValue32(malformed.get()->Out(), &ok);
  WriteValue32(malformed.get()->Out(), &truncatedCount);
  Rewind(malformed);
  Require(!restored.Read(malformed.get()->In()));
  Require(restored.Empty());
  Require(iostream_access::State(malformed.get()->In()) != 0);
}

void VerifyEventKindValuesAndCodec()
{
  EventKindSet kinds;
  Require(kinds.Add(0x200, 5));
  Require(kinds.Add(-7, 2));
  Require(!kinds.Add(0x200, 9));
  EventKindSet copy;
  copy = kinds;
  Require(copy.Find(0x200)->m_value == 5);
  copy.Find(0x200)->m_value = 6;
  Require(kinds.Find(0x200)->m_value == 5);
  EventKindSet allowed;
  allowed.Add(0x200, 1);
  copy.Intersect(allowed);
  Require(copy.Count() == 1 && !copy.Contains(-7));

  ScopedStrstream stream;
  Require(kinds.Write(stream.get()->Out()));
  Rewind(stream);
  EventKindSet restored;
  Require(restored.Read(stream.get()->In()));
  Require(restored.Count() == 2);
  Require(restored.Find(-7)->m_value == 2 && restored.Find(0x200)->m_value == 5);

  ScopedStrstream malformed;
  uint32_t count = 1;
  WriteValue32(malformed.get()->Out(), &count);
  Rewind(malformed);
  Require(!restored.Read(malformed.get()->In()));
  Require(restored.Count() == 0);
}

class CountedRankedItem : public AiRankedItem
{
  public:
  CountedRankedItem(const ObjectKey& key, int score, int* destroyed)
      : AiRankedItem(key, score)
      , m_destroyed(destroyed)
  {
  }

  ~CountedRankedItem() override
  {
    ++*m_destroyed;
  }

  private:
  int* m_destroyed;
};

void VerifyRankedListRemovalOwnership()
{
  const ObjectKey removedKey = ObjectKey::Make(0x90, 1);
  int removedDestroyed = 0;
  AiRankedList list(1);
  auto* removed = new CountedRankedItem(removedKey, 10, &removedDestroyed);
  Require(list.AddItem(removed) == removedKey.Id());
  Require(!list.Insert(removed));
  Require(removedDestroyed == 0 && list.Count() == 1);
  Require(list.RemoveKey(&removedKey) == removed);
  Require(list.Empty() && removedDestroyed == 0);
  delete removed;
  Require(removedDestroyed == 1);

  const ObjectKey sharedKey = ObjectKey::Make(0x91, 10);
  const ObjectKey missingKey = ObjectKey::Make(0x91, 11);
  int firstListDestroyed = 0;
  int secondListDestroyed = 0;
  auto* first = new AiRankedList(1);
  Require(first->AddItem(new CountedRankedItem(sharedKey, 10, &firstListDestroyed)) == sharedKey.Id());
  Require(first->AddItem(new CountedRankedItem(missingKey, 5, &firstListDestroyed)) == missingKey.Id());
  first->Number();
  auto* second = new AiRankedList(1);
  Require(second->AddItem(new CountedRankedItem(sharedKey, 10, &secondListDestroyed)) == sharedKey.Id());
  second->Number();
  AiRankedSelector selector;
  selector.Add(first);
  selector.Add(second);

  ObjectKey best;
  Require(selector.PickBest(&best) == &best && best.m_value == sharedKey.m_value);
  Require(firstListDestroyed == 2 && secondListDestroyed == 0);
  Require(selector.PickBest(&best) == &best && best.IsSet() == 0);
  Require(firstListDestroyed == 2 && secondListDestroyed == 0);
  selector.Reset();
  Require(firstListDestroyed == 2 && secondListDestroyed == 1);
}
} // namespace

int main()
{
  VerifyRegistryLifetimeAndIdentity();
  VerifyObjectKeyOrderAndCopies();
  VerifyReferenceIdsAndTransfer();
  VerifyReferenceCountAndCodec();
  VerifyEventKindValuesAndCodec();
  VerifyRankedListRemovalOwnership();
}
