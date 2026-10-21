#pragma once
// The event queues (0x54ea70-0x54eed0): ordered trees of EventBase nodes. The game manager keeps three of them
// (+0xa8 a TimerQueue, +0xac a CommandQueue-like queue, +0xb0 a TypeQueue) and serves them in order; orders keep a
// CommandQueue of the commands that carry them out.
#include <cstdint>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include "core/entry_identity.h"
#include "core/stream_io.h"
#include "events/event_base.h"

class Crc32;

// Abstract base (VT 0x661fb8). Its typed ordering rule decides where Add puts an event; GetNext returns the event that
// is next to run.
class EventQueue
{
  public:
  EventQueue();          // 0x54ea70
  virtual ~EventQueue(); // 0x54eab0

  virtual int Read(istream* in); // clears, then reads count + (kind, event) pairs
  virtual int Write(ostream* out);
  virtual int Checksum(Crc32* crc);
  virtual int Validate();
  virtual EventBase* GetNext() = 0;

  void Clear();
  int Insert(EventBase* event);
  void Remove(EventBase* event);
  EventBase* First() const;
  size_t Count() const
  {
    return m_events.size();
  }
  const auto& Entries() const
  {
    return m_events;
  }

  // Pops GetNext() if there is one and runs it.
  int RunNext(); // 0x54ec70
  // Pops the first event and runs it; if its Execute returns non-zero it goes back in the queue, otherwise it is
  // deleted. Returns 1 if there was an event.
  int RunFirst();             // 0x54ec90
  void Add(EventBase* event); // 0x54ece0

  int m_flag0c; // +0x0c, 1

  protected:
  virtual bool EventBefore(const EventBase* a, const EventBase* b) const = 0;

  private:
  struct EventCompare {
    const EventQueue* queue;
    bool operator()(const std::unique_ptr<EventBase>& a, const std::unique_ptr<EventBase>& b) const;
  };
  using EventSet = std::multiset<std::unique_ptr<EventBase>, EventCompare>;
  EventSet m_events;
  std::unordered_map<EventBase*, EventSet::iterator> m_index;
  bool m_clearing = false;
};

// Ordered by due day (TimedEvent +0x40); GetNext returns the first event only if it is due today (0x54edc0).
class TimerQueue : public EventQueue
{
  public:
  TimerQueue();           // 0x54ed40
  ~TimerQueue() override; // 0x54ed80
  bool EventBefore(const EventBase* a, const EventBase* b) const override;
  EventBase* GetNext() override; // 0x54edc0
};

// An order's commands: first in, first out (IsLess is always false); GetNext is the first one.
class CommandQueue : public EventQueue
{
  public:
  CommandQueue();           // 0x54ede0
  ~CommandQueue() override; // 0x54ee20
  bool EventBefore(const EventBase* a, const EventBase* b) const override;
  EventBase* GetNext() override; // 0x54eed0

  // Forwards to Add (0x54ee30).
  void AddCommand(EventBase* event);

  Command* FirstCommand() const
  {
    return static_cast<Command*>(First());
  }
};

// Ordered by the events' priority (slot 10, 0x10 for the events found so far); GetNext is the first one.
class TypeQueue : public EventQueue
{
  public:
  TypeQueue();           // 0x54ee40
  ~TypeQueue() override; // 0x54ee80
  bool EventBefore(const EventBase* a, const EventBase* b) const override;
  EventBase* GetNext() override; // 0x54eed0
};
