#pragma once
#include <cstdint>

#include "ai/ai_common.h"
#include "core/checked_new.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_key.h"
#include "orders/order_base.h"

// Small helpers shared by the AI's postures, planners and agents (src/game/ai): building the orders they hand back and
// walking the key sets.

namespace ai
{

constexpr uint32_t kSystemFamilyFirst = 0x90;
constexpr uint32_t kSystemFamilyLast = 0x98;
constexpr uint32_t kSectorFamilyFirst = 0x80;
constexpr uint32_t kSectorFamilyLast = 0x90;
constexpr uint32_t kFleetFamilyFirst = 8;
constexpr uint32_t kFleetFamilyLast = 0x10;
constexpr uint32_t kShipFamilyFirst = 0x14;
constexpr uint32_t kShipFamilyLast = 0x1c;

constexpr int kMoveOrder = 0x201;

inline bool IsSystemKey(const ObjectKey& key)
{
  return KeyInFamilies(key, kSystemFamilyFirst, kSystemFamilyLast);
}
inline bool IsSectorKey(const ObjectKey& key)
{
  return KeyInFamilies(key, kSectorFamilyFirst, kSectorFamilyLast);
}
inline bool IsFleetKey(const ObjectKey& key)
{
  return KeyInFamilies(key, kFleetFamilyFirst, kFleetFamilyLast);
}
inline bool IsShipKey(const ObjectKey& key)
{
  return KeyInFamilies(key, kShipFamilyFirst, kShipFamilyLast);
}

inline ObjectKey KeyOfNode(ObjectKeyNode* node)
{
  ObjectKey key;
  node->GetKey(&key);
  return key;
}

inline ObjectKey KeyOfObject(GameObjNotifier* object)
{
  ObjectKey key;
  GetObjectKey(object, &key);
  return key;
}

inline void AddToTeam(ObjectKeyValueSet* team, const ObjectKey& key)
{
  auto* node = NewOrNull<ObjectKeyValueNode>(key, 0);
  if (node) {
    team->Add(node);
  }
}

// The order of `kind` for `side` with the given team and target; null if the order cannot be created.
inline OrderBase* NewOrder(int kind, int side, ObjectKeyValueSet* team, const ObjectKey* target)
{
  auto* order = static_cast<OrderBase*>(CreateMessageByKind(kind));
  if (!order) {
    return nullptr;
  }
  order->m_side = side;
  order->SetTeam(team);
  order->SetTarget(target);
  return order;
}

} // namespace ai
