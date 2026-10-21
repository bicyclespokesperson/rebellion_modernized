#include "port/native/native_clock.h"

#include <chrono>
#include <cstdlib>

namespace native_clock
{
namespace
{

constexpr uint32_t kTickBaseMs = 100000;

struct State {
  bool deterministic = false;
  bool noTimers = false;
  uint32_t stepUs = 1000;
  uint32_t counterStepNs = 1000;
  uint64_t counterNs = 0;
  uint64_t clockCalls = 0;
};

unsigned EnvNumber(const char* name, unsigned fallback)
{
  const char* text = getenv(name);
  if (!text || !*text) {
    return fallback;
  }
  return static_cast<unsigned>(strtoul(text, nullptr, 10));
}

State& Get()
{
  static State state = [] {
    State s;
    s.deterministic = EnvNumber("REB_DETERMINISTIC", 0) != 0;
    s.noTimers = EnvNumber("REB_NO_TIMERS", 0) != 0;
    s.stepUs = EnvNumber("REB_CLOCK_STEP_US", 1000);
    s.counterStepNs = EnvNumber("REB_QPC_STEP_NS", 1000);
    return s;
  }();
  return state;
}

} // namespace

bool Deterministic()
{
  return Get().deterministic;
}
bool NoTimers()
{
  return Get().noTimers;
}

uint64_t AdvanceCounterNs()
{
  State& s = Get();
  s.counterNs += s.counterStepNs;
  return s.counterNs;
}

uint32_t VirtualTickMs()
{
  State& s = Get();
  return kTickBaseMs + static_cast<uint32_t>(++s.clockCalls * s.stepUs / 1000);
}

uint32_t RealTickMs()
{
  const auto elapsed = std::chrono::steady_clock::now().time_since_epoch();
  return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
}

uint32_t TickMs()
{
  return Deterministic() ? VirtualTickMs() : RealTickMs();
}

} // namespace native_clock
