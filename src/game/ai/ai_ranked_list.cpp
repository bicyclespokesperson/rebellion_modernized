#include "ai/ai_ranked_list.h"

#include <algorithm>

#include "core/checked_new.h"
#include "data/random.h"

namespace
{

constexpr int kTieBreakRange = 10;
constexpr int kTieBreakThreshold = 5;

} // namespace

// ---- AiRankedItem -------------------------------------------------------------------------------------------------

AiRankedItem::AiRankedItem()
    : LinkNode()
    , m_key()
    , m_score(0)
    , m_rank(0)
{
}

AiRankedItem::AiRankedItem(const ObjectKey& key, int score)
    : LinkNode()
    , m_key(key)
    , m_score(score)
    , m_rank(0)
{
}

AiRankedItem::AiRankedItem(AiRankedItem* other)
    : LinkNode()
    , m_key()
{
  ObjectKey copy;
  m_key = *other->GetKey(&copy);
  m_score = other->m_score;
  m_rank = other->m_rank;
}

AiRankedItem::~AiRankedItem() = default;

void AiRankedItem::Read(istream* in)
{
  m_key.Read(in);
  ReadValue32(in, &m_score);
  ReadValue32(in, &m_rank);
}

void AiRankedItem::Write(ostream* out)
{
  m_key.Write(out);
  WriteValue32(out, &m_score);
  WriteValue32(out, &m_rank);
}

AiRankedItem* AiRankedItem::Clone()
{
  return NewOrNull<AiRankedItem>(this);
}

ObjectKey* AiRankedItem::GetKey(ObjectKey* out)
{
  out->m_value = m_key.m_value;
  return out;
}

// ---- AiRankedList -------------------------------------------------------------------------------------------------

AiRankedList::AiRankedList()
    : IdLinkNode()
    , m_descending(0)
{
}

AiRankedList::AiRankedList(int descending)
    : IdLinkNode()
    , m_descending(descending)
{
}

AiRankedList::~AiRankedList()
{
  Clear();
}

void AiRankedList::Clear()
{
  decltype(m_items) detached;
  m_items.swap(detached);
}

int AiRankedList::Insert(AiRankedItem* item)
{
  if (!item || PositionOf(item) != m_items.cend()) {
    return 0;
  }
  auto position = m_items.begin();
  for (; position != m_items.end(); ++position) {
    const AiRankedItem* other = (*position).get();
    const bool precedes = item->m_score < other->m_score   ? m_descending == 1
                          : other->m_score < item->m_score ? m_descending != 1
                                                           : GameplayRandomNext(kTieBreakRange) < kTieBreakThreshold;
    if (precedes) {
      break;
    }
  }
  m_items.emplace(position, item);
  return 1;
}

int AiRankedList::Remove(AiRankedItem* item)
{
  const auto position = std::find_if(m_items.begin(), m_items.end(), [item](const auto& owned) {
    return owned.get() == item;
  });
  if (position == m_items.end()) {
    return 0;
  }
  position->release();
  m_items.erase(position);
  return 1;
}

void AiRankedList::Read(istream* in)
{
  IdLinkNode::Read(in);
  ReadValue32(in, &m_descending);
  uint32_t count;
  ReadValue32(in, &count);
  for (uint32_t i = 0; i < count; ++i) {
    auto* item = NewOrNull<AiRankedItem>();
    if (item) {
      item->Read(in);
      AddItem(item);
    }
  }
}

void AiRankedList::Write(ostream* out)
{
  IdLinkNode::Write(out);
  WriteValue32(out, &m_descending);
  int count = static_cast<int>(Count());
  WriteValue32(out, &count);
  for (const auto& entry : Entries()) {
    auto* item = static_cast<AiRankedItem*>(entry.get());
    item->Write(out);
  }
}

void AiRankedListRead(IdLinkNode* base, istream* in)
{
  static_cast<AiRankedList*>(base)->AiRankedList::Read(in);
}

void AiRankedListWrite(IdLinkNode* base, ostream* out)
{
  static_cast<AiRankedList*>(base)->AiRankedList::Write(out);
}

uint32_t AiRankedList::AddItem(AiRankedItem* item)
{
  ObjectKey key;
  if (item) {
    ObjectKey copy;
    key = *item->GetKey(&copy);
    if (key.Id() != 0 && key.Family() != 0) {
      Insert(item);
    }
  }
  return key.Id();
}

AiRankedItem* AiRankedList::RemoveKey(const ObjectKey* key)
{
  AiRankedItem* item = FindKey(key);
  if (item) {
    Remove(item);
  }
  return item;
}

namespace
{

__attribute__((noinline)) ObjectKey ReadItemKey(AiRankedItem* item)
{
  ObjectKey copy;
  return *item->GetKey(&copy);
}

} // namespace

AiRankedItem* AiRankedList::FindKey(const ObjectKey* key)
{
  for (const auto& entry : Entries()) {
    auto* item = static_cast<AiRankedItem*>(entry.get());
    if (key->m_value == ReadItemKey(item).m_value) {
      return item;
    }
  }
  return nullptr;
}

AiRankedItem* AiRankedList::First() const
{
  return Entries().empty() ? nullptr : static_cast<AiRankedItem*>(Entries().front().get());
}

void AiRankedList::Number()
{
  int rank = 1;
  for (const auto& entry : Entries()) {
    auto* item = static_cast<AiRankedItem*>(entry.get());
    item->m_rank = rank++;
  }
}
