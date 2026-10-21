#include <atomic>
#include <chrono>
#include <cstdlib>
#include <thread>

#include "port/native/thread_sync.h"

namespace
{

void Require(bool condition)
{
  if (!condition) {
    std::abort();
  }
}

using std::chrono::milliseconds;

void WaitReturnsLowestSetEventAndResetsIt()
{
  reb::EventSet events;
  events.At(2)->Set();
  events.At(1)->Set();
  Require(events.Wait() == 1);
  Require(events.Wait() == 2);
  events.At(0)->Set();
  events.Reset();
  events.At(2)->Set();
  Require(events.Wait() == 2);
}

void WaitBlocksUntilAnotherThreadSets()
{
  reb::EventSet events;
  std::thread setter([&events] {
    std::this_thread::sleep_for(milliseconds(50));
    events.At(1)->Set();
  });
  Require(events.Wait() == 1);
  setter.join();
}

void SignalIfLiveIgnoresDestroyedSets()
{
  void* stale = nullptr;
  {
    reb::EventSet events;
    stale = events.At(0);
    reb::EventSet::SignalIfLive(stale);
    Require(events.Wait() == 0);
  }
  reb::EventSet::SignalIfLive(stale); // must not touch the destroyed set
}

void TimedThreadJoinsWithinTimeout()
{
  reb::TimedThread thread;
  std::atomic<bool> ran{false};
  thread.Start([&ran] { ran = true; });
  Require(thread.JoinFor(milliseconds(2000)));
  Require(ran);
  Require(!thread.Joinable());
}

void TimedThreadTimesOutThenDetaches()
{
  reb::TimedThread thread;
  auto release = std::make_shared<std::atomic<bool>>(false);
  thread.Start([release] {
    while (!*release) {
      std::this_thread::sleep_for(milliseconds(5));
    }
  });
  Require(!thread.JoinFor(milliseconds(50)));
  thread.Detach();
  *release = true;
}

} // namespace

int main()
{
  WaitReturnsLowestSetEventAndResetsIt();
  WaitBlocksUntilAnotherThreadSets();
  SignalIfLiveIgnoresDestroyedSets();
  TimedThreadJoinsWithinTimeout();
  TimedThreadTimesOutThenDetaches();
  return 0;
}
