#pragma once
#include <cstdint>
#include <list>
#include <memory>

#include "ai/ai_ranked_list.h"
#include "gameobj/object_key.h"

// Picks one candidate out of several ranked lists (0x4355d0-0x4359f0, VT 0x658d90, 0x10 bytes). Every candidate query
// of the AI (AiCharacterRecordSet::RankByValue and the pool queries of AiSideController) returns an AiRankedList; a
// selector collects the lists of one decision (Add) and PickBest() takes the candidate with the best summed rank in all
// of them. The selector owns added lists in id order and hands them ids (Reset() deletes them).
class AiRankedSelector
{
  public:
  AiRankedSelector();               // 0x4355d0
  ~AiRankedSelector();              // 0: 0x4355f0 (deleting), 0x435610
  virtual void Read(istream* in);   // 2: 0x435660
  virtual void Write(ostream* out); // 3: 0x435720

  // Takes ownership of the list (a null list is ignored).
  void Add(AiRankedList* list); // 0x435790
  void Reset();
  size_t Count() const
  {
    return m_lists.size();
  }
  bool Empty() const
  {
    return m_lists.empty();
  }
  const auto& Entries() const
  {
    return m_lists;
  }
  auto PositionOf(const AiRankedList* list) const
  {
    for (auto position = m_lists.cbegin(); position != m_lists.cend(); ++position) {
      if ((*position).get() == list) {
        return position;
      }
    }
    return m_lists.cend();
  }
  // The key that is best in the first list and present in every other list: for each entry of the first list its ranks
  // in the other lists are added (an entry missing from one of them is dropped from the first list); the highest total
  // wins, the earliest on a tie. The winner is removed from the first list. Returns the unset key if there is none.
  ObjectKey* PickBest(ObjectKey* out); // 0x4357b0

  // 1 if the key is in the list; adds the entry's rank to *total.
  static int AddRankOf(const ObjectKey* key, int* total, AiRankedList* list); // 0x435980

  uint32_t m_nextId = 1;

  private:
  using List = std::list<std::unique_ptr<AiRankedList>>;
  List m_lists;
};
