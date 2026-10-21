#pragma once
#include <cstdint>

#include "ai/ai_common.h"
#include "core/checked_new.h"
#include "gameobj/child_iterator.h"
#include "gameobj/object_key.h"
#include "orders/order_base.h"

// Helpers the AI's task types share (src/game/ai/ai_task_*.cpp).

namespace aitask
{

inline bool IsSectorKey(const ObjectKey& key)
{
  return ai::KeyInFamilies(key, 0x80, 0x90) != 0;
}
inline bool IsSystemKey(const ObjectKey& key)
{
  return ai::KeyInFamilies(key, 0x90, 0x98) != 0;
}

inline ObjectKey KeyOfValue(uint32_t value)
{
  ObjectKey key;
  key.m_value = value;
  return key;
}

inline ObjectKey KeyOfNode(ObjectKeyNode* node)
{
  ObjectKey key;
  node->GetKey(&key);
  return key;
}

// Adds the object the iterator is at to the team; the node that was made (null if there was no object or no memory).
inline ObjectKeyValueNode* AddCurrentToTeam(ObjectKeyValueSet* team, ChildIterator* it)
{
  if (!it->Current()) {
    return nullptr;
  }
  ObjectKey key;
  GetObjectKey(it->Current(), &key);
  auto* node = NewOrNull<ObjectKeyValueNode>(key, 0);
  if (node) {
    team->Add(node);
  }
  return node;
}

// Adds the first object of the iterator's family to the team; the node that was made.
template <class Iterator>
ObjectKeyValueNode* AddFirstNode(ObjectKeyValueSet* team, GameObjNotifier* parent)
{
  Iterator it(parent, 1);
  it.Begin();
  return AddCurrentToTeam(team, &it);
}

// Adds the first object of the iterator's family to the team; true if a node was made.
template <class Iterator>
bool AddFirstOfKind(ObjectKeyValueSet* team, GameObjNotifier* parent)
{
  Iterator it(parent, 1);
  it.Begin();
  return AddCurrentToTeam(team, &it) != nullptr;
}

// Adds every object of the iterator's family; the node of the last one (`previous` if there is none).
template <class Iterator>
ObjectKeyValueNode* AddAllOfKind(ObjectKeyValueSet* team, GameObjNotifier* parent, ObjectKeyValueNode* previous)
{
  Iterator it(parent, 1);
  for (it.Begin(); it.Current(); it.Advance()) {
    previous = AddCurrentToTeam(team, &it);
  }
  return previous;
}

// A new order of the kind for the side with the team; null if the factory has none.
inline OrderBase* NewTeamOrder(int kind, int side, ObjectKeyValueSet* team)
{
  auto* order = static_cast<OrderBase*>(CreateMessageByKind(kind));
  if (order) {
    order->m_side = side;
    order->SetTeam(team);
  }
  return order;
}

} // namespace aitask
