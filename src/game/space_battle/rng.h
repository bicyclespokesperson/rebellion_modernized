#pragma once
#include <cstdint>

// The simulator's own generator (PCG32, XSH-RR) with explicit integer-to-range mappings, so a battle replays
// identically whatever the standard library (tactical_combat.md §11.3).
namespace reb::battle
{

class Pcg32
{
  public:
  Pcg32() = default;
  Pcg32(uint64_t seed, uint64_t stream)
  {
    m_increment = (stream << 1U) | 1U;
    Next();
    m_state += seed;
    Next();
  }

  uint32_t Next()
  {
    const uint64_t old = m_state;
    m_state = old * 6364136223846793005ULL + m_increment;
    const auto xorshifted = static_cast<uint32_t>(((old >> 18U) ^ old) >> 27U);
    const auto rotation = static_cast<uint32_t>(old >> 59U);
    return (xorshifted >> rotation) | (xorshifted << ((32U - rotation) & 31U));
  }

  // [0, 1) with 24 bits of precision.
  float Unit()
  {
    return static_cast<float>(Next() >> 8U) * (1.0f / 16777216.0f);
  }

  // [lo, hi], inclusive.
  int32_t Range(int32_t lo, int32_t hi)
  {
    const auto span = static_cast<uint64_t>(static_cast<int64_t>(hi) - lo + 1);
    return lo + static_cast<int32_t>((static_cast<uint64_t>(Next()) * span) >> 32U);
  }

  float Between(float lo, float hi)
  {
    return lo + (hi - lo) * Unit();
  }

  bool Chance(float probability)
  {
    return Unit() < probability;
  }

  uint64_t& StateWord()
  {
    return m_state;
  }
  uint64_t& IncrementWord()
  {
    return m_increment;
  }

  private:
  uint64_t m_state = 0x853c49e6748fea9bULL;
  uint64_t m_increment = 0xda3e39cb94b95bdbULL;
};

} // namespace reb::battle
