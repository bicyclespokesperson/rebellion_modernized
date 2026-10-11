#include "events/event_queue.h"

#include <limits>

#include "core/iostream_lib.h"
#include "core/utility_classes.h"

namespace
{

} // namespace

// ---- EventQueue ------------------------------------------------------------------------------------------------

EventQueue::EventQueue() : m_events(EventCompare{this}), m_flag0c(1)
{
}

EventQueue::~EventQueue()
{
  Clear();
}

bool EventQueue::EventCompare::operator()(const std::unique_ptr<EventBase>& a,
                                          const std::unique_ptr<EventBase>& b) const
{
  return queue->EventBefore(a.get(), b.get());
}

void EventQueue::Clear()
{
  if (m_clearing)
    return;
  m_clearing = true;
  m_index.clear();
  EventSet detached(EventCompare{this});
  detached.swap(m_events);
  detached.clear();
  m_clearing = false;
}

int EventQueue::Insert(EventBase* event)
{
  if (m_clearing || !event || m_index.contains(event))
    return 0;
  std::unique_ptr<EventBase> pending(event);
  EventSet::iterator position;
  try
  {
    position = m_events.insert(std::move(pending));
  }
  catch (...)
  {
    pending.release();
    throw;
  }
  try
  {
    if (!m_index.emplace(event, position).second)
    {
      auto rejected = m_events.extract(position);
      rejected.value().release();
      return 0;
    }
  }
  catch (...)
  {
    auto rejected = m_events.extract(position);
    rejected.value().release();
    throw;
  }
  return 1;
}

void EventQueue::Remove(EventBase* event)
{
  auto found = m_index.find(event);
  if (found == m_index.end())
    return;
  auto owned = m_events.extract(found->second);
  m_index.erase(found);
  owned.value().release();
}

EventBase* EventQueue::First() const
{
  return m_events.empty() ? nullptr : m_events.begin()->get();
}

int EventQueue::Read(istream* in)
{
  Clear();
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
  for (auto nodePosition = Entries().begin(); nodePosition != Entries().end();)
  {
    EventBase* event = (*(nodePosition++)).get();
    int kind = event->GetKind();
    WriteValue32(out, &kind);
    event->Write(out);
  }
  return 1;
}

int EventQueue::Checksum(Crc32* crc)
{
  crc->AddInt(Count());
  for (auto nodePosition = Entries().begin(); nodePosition != Entries().end();)
  {
    EventBase* event = (*(nodePosition++)).get();
    event->Checksum(crc);
  }
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
  EventBase* first = First();
  if (first)
  {
    Remove(first);
    ran = 1;
    EventBase* event = first;
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

bool TimerQueue::EventBefore(const EventBase* a, const EventBase* b) const
{
  if (a == nullptr || b == nullptr)
    return true;
  return static_cast<const TimedEvent*>(a)->m_dueDay < static_cast<const TimedEvent*>(b)->m_dueDay;
}

EventBase* TimerQueue::GetNext()
{
  EventBase* first = First();
  if (first)
  {
    int due = static_cast<TimedEvent*>(first)->m_dueDay;
    if (CallGameHook2B08() < due)
      first = nullptr;
  }
  return first;
}

// ---- CommandQueue ----------------------------------------------------------------------------------------------

CommandQueue::CommandQueue() : EventQueue()
{
}

CommandQueue::~CommandQueue() = default;

bool CommandQueue::EventBefore(const EventBase*, const EventBase*) const
{
  return false;
}

EventBase* CommandQueue::GetNext()
{
  return First();
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

bool TypeQueue::EventBefore(const EventBase* a, const EventBase* b) const
{
  if (a == nullptr || b == nullptr)
    return true;
  return static_cast<const PlainEvent*>(a)->GetPriority() < static_cast<const PlainEvent*>(b)->GetPriority();
}

EventBase* TypeQueue::GetNext()
{
  return First();
}
