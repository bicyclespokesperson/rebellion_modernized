#pragma once
#include "core/id_registry.h"
// The tree of pending messages (0x536eb0-0x537030): an ID registry of MessageBase objects keyed by message id. The game
// session keeps one for the messages waiting to be delivered (0x435b90 +0x24) and the network layer one per player
// (0x48a660 +0x4). Saved as a count followed by {kind, message body} pairs; loading creates each message through the
// order factory table (CreateMessageByKind).
#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <ranges>

#include "core/entry_identity.h"
#include "orders/order_base.h"

class MessageTree
{
  public:
  void Clear()
  {
    m_registry.Clear();
  }
  size_t Count() const
  {
    return m_registry.Count();
  }
  bool Empty() const
  {
    return m_registry.Empty();
  }
  auto Entries() const
  {
    return m_registry.Entries();
  }
  auto PositionOf(const MessageBase* node) const
  {
    return m_registry.PositionOf(node);
  }
  MessageBase* First() const
  {
    return m_registry.First();
  }
  MessageBase* Last() const
  {
    return m_registry.Last();
  }
  MessageBase* FindById(uint32_t id) const
  {
    return m_registry.FindById(id);
  }
  MessageBase* DetachById(uint32_t id)
  {
    return m_registry.DetachById(id);
  }
  void DeleteById(uint32_t id)
  {
    m_registry.DeleteById(id);
  }
  void Remove(MessageBase* node)
  {
    m_registry.Remove(node);
  }
  uint32_t AddWithId(MessageBase* node)
  {
    return m_registry.AddWithId(node);
  }
  uint32_t NextId() const
  {
    return m_registry.NextId();
  }
  MessageTree(); // 0x536eb0
  ~MessageTree();

  // Appends messages from the stream; an unknown kind or malformed body sets fail state.
  void Read(istream* in);   // 0x536f00
  void Write(ostream* out); // 0x536f70
  // Takes ownership of `message` (any message, only its id is used): queued under its id, or deleted when it has no id
  // or the id is already queued.
  void Adopt(MessageBase* message); // 0x536fe0
  MessageBase* Detach(uint32_t id); // 0x537030

  uint32_t m_unk0c; // +0x0c, 1
  private:
  IdRegistry<MessageBase> m_registry;
};
