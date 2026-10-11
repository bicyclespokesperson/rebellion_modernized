#include "events/event_queue.h"

#include <limits>

#include "core/iostream_lib.h"
#include "core/utility_classes.h"

namespace
{

EventBase* asEvent(AvlNode* node)
{
  return static_cast<EventBase*>(node);
}

} // namespace

// ---- EventQueue ------------------------------------------------------------------------------------------------

EventQueue::EventQueue() : OrderedTree(), m_flag0c(1)
{
}

EventQueue::~EventQueue()
{
  OrderedTree::Clear();
}

int EventQueue::Read(istream* in)
{
  OrderedTree::Clear();
  uint32_t count = 0;
  ReadValue32(in, &count);
  if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0)
    return 0;
  for (uint32_t index = 0; index < count; ++index)
  {
    int kind = 0;
    ReadValue32(in, &kind);
    if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0)
      return 0;
    EventBase* event = static_cast<EventBase*>(CreateEventById(kind));
    if (!event)
    {
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return 0;
    }
    event->Read(in);
    if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0 || !Insert(event))
    {
      delete event;
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return 0;
    }
  }
  return 1;
}

int EventQueue::Write(ostream* out)
{
  if (Count() > std::numeric_limits<uint32_t>::max())
  {
    iostream_access::SetState(iostream_access::IosOf(out), iostream_flags::kFail);
    return 0;
  }
  uint32_t count = static_cast<uint32_t>(Count());
  WriteValue32(out, &count);
  for (AvlNode* node = First(); node; node = node->m_successor)
  {
    EventBase* event = asEvent(node);
    int kind = event->GetKind();
    WriteValue32(out, &kind);
    event->Write(out);
  }
  return 1;
}

int EventQueue::Checksum(Crc32* crc)
{
  crc->AddInt(Count());
  for (AvlNode* node = First(); node; node = node->m_successor)
    asEvent(node)->Checksum(crc);
  return 1;
}

int EventQueue::Validate()
{
  return 1;
}

int EventQueue::RunNext()
{
  if (GetNext() == nullptr)
    return 0;
  return RunFirst();
}

int EventQueue::RunFirst()
{
  int ran = 0;
  AvlNode* first = First();
  if (first)
  {
    Remove(first);
    ran = 1;
    EventBase* event = asEvent(first);
    if (event->Execute() != 0)
    {
      Add(event);
      return 1;
    }
    delete event;
  }
  return ran;
}

void EventQueue::Add(EventBase* event)
{
  if (event)
    Insert(event);
}

// ---- TimerQueue ------------------------------------------------------------------------------------------------

TimerQueue::TimerQueue() : EventQueue()
{
}

TimerQueue::~TimerQueue() = default;

int TimerQueue::IsLess(AvlNode* a, AvlNode* b)
{
  if (a == nullptr || b == nullptr)
    return 1;
  return static_cast<TimedEvent*>(static_cast<EventBase*>(a))->m_dueDay <
         static_cast<TimedEvent*>(static_cast<EventBase*>(b))->m_dueDay;
}

EventBase* TimerQueue::GetNext()
{
  AvlNode* first = First();
  if (first)
  {
    int due = static_cast<TimedEvent*>(static_cast<EventBase*>(first))->m_dueDay;
    if (CallGameHook2B08() < due)
      first = nullptr;
  }
  return static_cast<EventBase*>(first);
}

// ---- CommandQueue ----------------------------------------------------------------------------------------------

CommandQueue::CommandQueue() : EventQueue()
{
}

CommandQueue::~CommandQueue() = default;

int CommandQueue::IsLess(AvlNode*, AvlNode*)
{
  return 0;
}

EventBase* CommandQueue::GetNext()
{
  return static_cast<EventBase*>(First());
}

void CommandQueue::AddCommand(EventBase* event)
{
  Add(event);
}

// ---- TypeQueue -------------------------------------------------------------------------------------------------

TypeQueue::TypeQueue() : EventQueue()
{
}

TypeQueue::~TypeQueue() = default;

int TypeQueue::IsLess(AvlNode* a, AvlNode* b)
{
  if (a == nullptr || b == nullptr)
    return 1;
  return static_cast<PlainEvent*>(a)->GetPriority() < static_cast<PlainEvent*>(b)->GetPriority();
}

EventBase* TypeQueue::GetNext()
{
  return static_cast<EventBase*>(First());
}
