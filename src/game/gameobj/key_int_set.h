#pragma once
#include "gameobj/object_key.h"

// 0x53df50, VT 0x661a60: a set of key -> int (IntPairNode) entries.
class ObjectKeyIntSet : public ObjectKeySet
{
public:
  ObjectKeyIntSet(); // 0x53df50
  ObjectKeyIntSet(const ObjectKeyIntSet& other); // 0x53dfa0
  ~ObjectKeyIntSet() override; // 0x53df90, scalar deleting 0x53df70
  ObjectKeyNode* NewNode() override; // 0x53dfc0
  int AddPair(const ObjectKey& key, const int& value); // 0x53e020
};
