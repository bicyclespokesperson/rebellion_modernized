#include "data/game_math.h"

#include <cstdlib>
#include <limits>
#include <stdexcept>

const int g_percentMin = 0;
const int g_percentMax = 100;

namespace
{

void Require(bool condition)
{
  if (!condition) {
    std::abort();
  }
}

template <typename Exception, typename Function>
void RequireThrows(Function function)
{
  try {
    function();
  } catch (const Exception&) {
    return;
  } catch (...) {
    std::abort();
  }
  std::abort();
}

} // namespace

int main()
{
  constexpr int kIntMin = std::numeric_limits<int>::min();
  constexpr int kIntMax = std::numeric_limits<int>::max();
  constexpr uint64_t kUint64Max = std::numeric_limits<uint64_t>::max();

  Require(IntegerSqrt(-1) == 0);
  Require(IntegerSqrt(0) == 0);
  Require(IntegerSqrt(1) == 1);
  Require(IntegerSqrt(3) == 1);
  Require(IntegerSqrt(4) == 2);
  Require(IntegerSqrt(8) == 2);
  Require(IntegerSqrt(9) == 3);
  Require(IntegerSqrt(kIntMax) == 46340);
  Require(IntegerSqrt(uint64_t{8589672450}) == 92680);
  Require(IntegerSqrt(kUint64Max) == 4294967295ULL);

  Require(DivideRoundUp(5, 2) == 3);
  Require(DivideRoundUp(-5, 2) == -2);
  Require(DivideRoundUp(5, -2) == -2);
  Require(DivideRoundUp(-5, -2) == 3);
  Require(DivideRoundUp(-6, 3) == -2);
  Require(DivideRoundUp(kIntMin, 1) == kIntMin);

  Require(ScaleRatio(kIntMax, 2, 2) == kIntMax);
  Require(ScaleRatio(kIntMin, 2, 2) == kIntMin);
  Require(ScaleRatio(7, -3, 2) == -10);
  Require(ClampNegativeToZero(-1) == 0);
  Require(ClampNegativeToZero(4) == 4);
  Require(MinInt(4, 4) == 4);
  Require(MaxInt(4, 4) == 4);
  Require(Clamp(12, 0, 10) == 10);
  Require(ClampPercent(-1) == 0);
  Require(ClampPercent(101) == 100);

  RequireThrows<std::domain_error>([] {
    IntDivide(1, 0);
  });
  RequireThrows<std::domain_error>([] {
    DivideRoundUp(1, 0);
  });
  RequireThrows<std::domain_error>([] {
    ScaleRatio(1, 1, 0);
  });
  RequireThrows<std::domain_error>([] {
    RatioToPercent(1, 0);
  });
  RequireThrows<std::overflow_error>([] {
    IntDivide(kIntMin, -1);
  });
  RequireThrows<std::overflow_error>([] {
    DivideRoundUp(kIntMin, -1);
  });
  RequireThrows<std::overflow_error>([] {
    ScaleRatio(kIntMax, kIntMax, 1);
  });
}
