#include "orders/order_list.h"

#include "core/iostream_lib.h"
#include "core/stream_io.h"

MessageTree::MessageTree() : IdAvlTree()
{
  m_unk0c = 1;
}

MessageTree::~MessageTree() = default;

void MessageTree::Read(istream* in)
{
  uint32_t count = 0;
  ReadValue32(in, &count);
  if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0)
    return;
  for (uint32_t index = 0; index < count; ++index)
  {
    int kind = 0;
    ReadValue32(in, &kind);
    if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0)
      return;
    MessageBase* message = static_cast<MessageBase*>(CreateMessageByKind(kind));
    if (!message)
    {
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return;
    }
    message->Read(in);
    if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0 || !AddWithId(static_cast<IdNode*>(message)))
    {
      delete message;
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return;
    }
  }
}

void MessageTree::Write(ostream* out)
{
  if (!WriteCount32(out, Count()))
    return;
  for (AvlNode* node = First(); node; node = node->m_successor)
  {
    MessageBase* message = static_cast<MessageBase*>(node);
    int kind = message->GetKind();
    WriteValue32(out, &kind);
    message->Write(out);
  }
}

void MessageTree::Adopt(IdNode* message)
{
  if (!message)
    return;
  if (message->m_id == 0 || FindById(message->m_id))
  {
    delete message;
    return;
  }
  AddWithId(message);
}

IdNode* MessageTree::Detach(uint32_t id)
{
  return DetachById(id);
}
