#pragma once

#include <chrono>

class Stopwatch
{
  public:
  Stopwatch() = default;

  // Restarts the clock.
  void Reset();
  // Seconds since construction or the last Reset().
  double ElapsedSeconds() const;

  private:
  std::chrono::steady_clock::time_point m_start = std::chrono::steady_clock::now();
};
