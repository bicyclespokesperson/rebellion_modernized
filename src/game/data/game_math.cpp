#include "data/game_math.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace
{

int checkedInt(int64_t value)
{
  if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max()) {
    throw std::overflow_error("integer math result is outside int range");
  }
  return static_cast<int>(value);
}

int64_t divide(int64_t numerator, int64_t denominator)
{
  if (denominator == 0) {
    throw std::domain_error("division by zero");
  }
  return numerator / denominator;
}

} // namespace

int ClampPercent(int value)
{
  return Clamp(value, g_percentMin, g_percentMax);
}

int Clamp(int value, int low, int high)
{
  return MinInt(high, MaxInt(low, value));
}

int ClampNegativeToZero(int value)
{
  return std::max(value, 0);
}

int MaxInt(int a, int b)
{
  return std::max(a, b);
}

int MinInt(int a, int b)
{
  return std::min(a, b);
}

int IntDivide(int numerator, int denominator)
{
  if (denominator == 0) {
    throw std::domain_error("division by zero");
  }
  if (numerator == std::numeric_limits<int>::min() && denominator == -1) {
    throw std::overflow_error("integer division result is outside int range");
  }
  return numerator / denominator;
}

int DivideRoundUp(int numerator, int denominator)
{
  const int64_t quotient = divide(numerator, denominator);
  const int64_t remainder = static_cast<int64_t>(numerator) % denominator;
  return checkedInt(quotient + ((remainder != 0 && ((numerator < 0) == (denominator < 0))) ? 1 : 0));
}

int ScaleRatio(int value, int numerator, int denominator)
{
  return checkedInt(divide(static_cast<int64_t>(value) * numerator, denominator));
}

int ScalePercent(int value, int percent)
{
  return ScaleRatio(value, percent, g_percentMax);
}

int RatioToPercent(int part, int whole)
{
  return ScaleRatio(g_percentMax, part, whole);
}

int IntegerSqrt(int value)
{
  return value < 0 ? 0 : static_cast<int>(IntegerSqrt(static_cast<uint64_t>(value)));
}

uint64_t IntegerSqrt(uint64_t value)
{
  uint64_t low = 0;
  uint64_t high = (uint64_t{1} << 32);
  while (low + 1 < high) {
    const uint64_t middle = low + (high - low) / 2;
    if (middle <= value / middle) {
      low = middle;
    } else {
      high = middle;
    }
  }
  return low;
}
