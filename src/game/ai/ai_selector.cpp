#include "ai/ai_selector.h"

#include "core/checked_new.h"

AiRankedSelector::AiRankedSelector() = default;

AiRankedSelector::~AiRankedSelector()
{
  Reset();
}

void AiRankedSelector::Read(istream* in)
{
  uint32_t nextId;
  ReadValue32(in, &nextId);
  m_nextId = nextId;
  uint32_t count;
  ReadValue32(in, &count);
  for (uint32_t i = 0; i < count; ++i)
  {
    auto* list = NewOrNull<AiRankedList>();
    if (list)
    {
      static_cast<IdLinkNode*>(list)->Read(in);
      Add(list);
    }
  }
}

void AiRankedSelector::Write(ostream* out)
{
  uint32_t nextId = m_nextId;
  WriteValue32(out, &nextId);
  int count = static_cast<int>(Count());
  WriteValue32(out, &count);
  for (const auto& entry : Entries())
  {
    auto* list = static_cast<AiRankedList*>(entry.get());
    static_cast<IdLinkNode*>(list)->Write(out);
  }
}

void AiRankedSelector::Add(AiRankedList* list)
{
  if (!list)
    return;
  const uint32_t oldId = list->m_id;
  const uint32_t id = oldId == 0 ? m_nextId : oldId;
  if (id == 0)
    return;
  for (const auto& entry : m_lists)
    if (entry->m_id == id)
      return;
  if (oldId == 0)
    list->m_id = id;
  auto position = m_lists.begin();
  for (; position != m_lists.end() && (*position)->m_id < id; ++position)
  {
  }
  m_lists.emplace(position, list);
  if (m_nextId != 0 && m_nextId <= id)
    m_nextId = id + 1;
}

void AiRankedSelector::Reset()
{
  decltype(m_lists) detached;
  m_lists.swap(detached);
  m_nextId = 1;
}

int AiRankedSelector::AddRankOf(const ObjectKey* key, int* total, AiRankedList* list)
{
  int found = 0;
  auto position = list->Entries().begin();
  while (position != list->Entries().end() && !found)
  {
    auto* item = static_cast<AiRankedItem*>((*position).get());
    ObjectKey itemKey;
    if (item->GetKey(&itemKey)->m_value == key->m_value)
    {
      found = 1;
      *total += item->m_rank;
    }
    else
    {
      ++position;
    }
  }
  return found;
}

ObjectKey* AiRankedSelector::PickBest(ObjectKey* out)
{
  ObjectKey best;
  ObjectKey current;
  int bestScore = 0;
  int score = 0;
  AiRankedList* first = Entries().empty() ? nullptr : static_cast<AiRankedList*>(Entries().front().get());
  if (first)
  {
    auto position = first->Entries().begin();
    while (position != first->Entries().end())
    {
      auto* item = static_cast<AiRankedItem*>((*position).get());
      int inAll = 1;
      ObjectKey itemKey;
      current = *item->GetKey(&itemKey);
      score = item->m_rank;
      auto listPosition = PositionOf(first);
      ++listPosition;
      for (; listPosition != Entries().end(); ++listPosition)
      {
        auto* other = static_cast<AiRankedList*>((*listPosition).get());
        if (!inAll)
          break;
        inAll = AddRankOf(&current, &score, other);
      }
      if (!inAll)
      {
        ++position;
        AiRankedItem* removed = first->RemoveKey(&current);
        delete removed;
      }
      else
      {
        ++position;
        if (bestScore < score)
        {
          bestScore = score;
          best = current;
        }
      }
    }
  }
  if (best.Family() != 0 && best.Family() <= 0xfe)
  {
    AiRankedItem* removed = first->RemoveKey(&best);
    delete removed;
  }
  *out = best;
  return out;
}
