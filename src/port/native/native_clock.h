// The native build's clocks. With REB_DETERMINISTIC=1 they are virtual: every GetTickCount/timeGetTime call advances the
// millisecond clock by REB_CLOCK_STEP_US (default 1000) from a base of 100000 ms, every QueryPerformanceCounter call
// advances the counter by REB_QPC_STEP_NS (default 1000) nanoseconds (frequency 1 MHz), and time() is 2000-01-01
// 12:00:00 UTC. Without it they read the host.
#pragma once
#include <stdint.h>

namespace native_clock
{

bool Deterministic();
uint64_t AdvanceCounterNs();
uint32_t VirtualTickMs();
uint32_t RealTickMs();
uint32_t TickMs(); // the game's GetTickCount: virtual in deterministic mode, otherwise the host clock
bool NoTimers();

} // namespace native_clock
