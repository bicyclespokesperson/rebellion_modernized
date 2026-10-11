#pragma once
#include <cstdint>

#include "ai/ai_ranked_list.h"
#include "core/containers.h"
#include "gameobj/object_key.h"

// Picks one candidate out of several ranked lists (0x4355d0-0x4359f0, VT 0x658d90, 0x10 bytes). Every candidate query of
// the AI (AiCharacterRecordSet::RankByValue and the pool queries of AiSideController) returns an AiRankedList; a selector
// collects the lists of one decision (Add) and PickBest() takes the candidate with the best summed rank in all of them.
// An IdSortedLinkList that hands out ids to the added lists; the lists are owned by it (Reset() deletes them).
class AiRankedSelector : public IdSortedLinkList
{
public:
  AiRankedSelector(); // 0x4355d0
  ~AiRankedSelector() override; // 0: 0x4355f0 (deleting), 0x435610
  virtual void Read(istream* in); // 2: 0x435660
  virtual void Write(ostream* out); // 3: 0x435720

  // Takes ownership of the list (a null list is added as id 0, i.e. ignored).
  void Add(AiRankedList* list); // 0x435790
  // The key that is best in the first list and present in every other list: for each entry of the first list its ranks in
  // the other lists are added (an entry missing from one of them is dropped from the first list); the highest total
  // wins, the earliest on a tie. The winner is removed from the first list. Returns the unset key if there is none.
  ObjectKey* PickBest(ObjectKey* out); // 0x4357b0

  // 1 if the key is in the list; adds the entry's rank to *total.
  static int AddRankOf(const ObjectKey* key, int* total, AiRankedList* list); // 0x435980
};
