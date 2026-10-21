// Standard-library replacements for the Win32 event and thread-wait calls the audio and video threads used
// (CreateEvent / SetEvent / WaitForMultipleObjects / WaitForSingleObject with a timeout).
#pragma once
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <utility>

namespace reb
{

// Up to three auto-reset events that one thread waits on together. Wait() blocks until any event is set, resets the
// lowest-numbered set event and returns its index, as WaitForMultipleObjects(FALSE) does.
class EventSet
{
  public:
  static constexpr int kMaxEvents = 3;

  class Event
  {
    public:
    void Set()
    {
      m_owner->Signal(m_index);
    }

    private:
    friend class EventSet;
    EventSet* m_owner = nullptr;
    int m_index = 0;
  };

  EventSet()
  {
    std::lock_guard<std::mutex> lock(LiveMutex());
    for (int i = 0; i < kMaxEvents; ++i) {
      m_events[i].m_owner = this;
      m_events[i].m_index = i;
      Live().insert(&m_events[i]);
    }
  }
  ~EventSet()
  {
    std::lock_guard<std::mutex> lock(LiveMutex());
    for (Event& event : m_events) {
      Live().erase(&event);
    }
  }
  EventSet(const EventSet&) = delete;
  EventSet& operator=(const EventSet&) = delete;

  Event* At(int index)
  {
    return &m_events[index];
  }

  // Clears every event, so a thread started later does not see signals left over from an earlier run.
  void Reset()
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pending = 0;
  }

  int Wait()
  {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_changed.wait(lock, [this] { return m_pending != 0; });
    int index = 0;
    while ((m_pending & (1u << index)) == 0) {
      ++index;
    }
    m_pending &= ~(1u << index);
    return index;
  }

  // Sets the event behind the opaque pointer a DirectSound position notification carries. The mixer signals after it has
  // released its own lock, so the event may belong to a buffer that has just been destroyed: unknown pointers are ignored.
  static void SignalIfLive(void* event)
  {
    std::lock_guard<std::mutex> lock(LiveMutex());
    if (Live().count(static_cast<Event*>(event)) != 0) {
      static_cast<Event*>(event)->Set();
    }
  }

  private:
  void Signal(int index)
  {
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_pending |= 1u << index;
    }
    m_changed.notify_one();
  }

  static std::mutex& LiveMutex()
  {
    static std::mutex mutex;
    return mutex;
  }
  static std::unordered_set<Event*>& Live()
  {
    static std::unordered_set<Event*> live;
    return live;
  }

  std::mutex m_mutex;
  std::condition_variable m_changed;
  unsigned m_pending = 0;
  Event m_events[kMaxEvents];
};

// A std::thread whose exit can be waited for with a timeout, because std::thread::join cannot time out.
class TimedThread
{
  public:
  TimedThread() = default;
  ~TimedThread()
  {
    if (m_thread.joinable()) {
      m_thread.detach();
    }
  }
  TimedThread(const TimedThread&) = delete;
  TimedThread& operator=(const TimedThread&) = delete;

  template <typename Function>
  void Start(Function function)
  {
    m_state = std::make_shared<State>();
    m_thread = std::thread([state = m_state, function = std::move(function)]() mutable {
      function();
      {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->finished = true;
      }
      state->changed.notify_all();
    });
  }

  bool Joinable() const
  {
    return m_thread.joinable();
  }

  // True when the thread has finished (and has been joined); false on timeout, leaving it running.
  bool JoinFor(std::chrono::milliseconds timeout)
  {
    if (!m_thread.joinable()) {
      return true;
    }
    {
      std::unique_lock<std::mutex> lock(m_state->mutex);
      if (!m_state->changed.wait_for(lock, timeout, [this] { return m_state->finished; })) {
        return false;
      }
    }
    m_thread.join();
    return true;
  }

  // Gives up on a thread that did not finish. It keeps running unobserved, as an abandoned Win32 thread handle would.
  void Detach()
  {
    if (m_thread.joinable()) {
      m_thread.detach();
    }
  }

  private:
  struct State {
    std::mutex mutex;
    std::condition_variable changed;
    bool finished = false;
  };
  std::shared_ptr<State> m_state;
  std::thread m_thread;
};

} // namespace reb
