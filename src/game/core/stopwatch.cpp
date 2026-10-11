#include "core/stopwatch.h"

void Stopwatch::Reset()
{
  m_start = std::chrono::steady_clock::now();
}

double Stopwatch::ElapsedSeconds() const
{
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - m_start).count();
}
