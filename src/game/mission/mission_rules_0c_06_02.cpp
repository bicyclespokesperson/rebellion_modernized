#include "data/probability_table.h"
#include "data/random.h"
#include "mission/mission_rules.h"

// Rules over GNPRTB 0x0cxx (escape), 0x06xx and 0x02xx (identify / betrayal) (0x55e5b0-0x55ec90).

int RollEscapeTable(int unused, int a, int b, int c, int d, int* success)
{
  *success = 0;
  return RollTableChance(mission_table::kEscape, ((b + a) - c) - d, success);
}

int ComputeEscapeBase(int* out)
{
  *out = GameRandom(g_gnprtb0C08) + g_gnprtb0C07;
  return 1;
}

int ComputeScaledByGnprtb0600(int a, int b, int* out)
{
  *out = (b / g_gnprtb0600 + 1) * a;
  return 1;
}

int ComputeScaledByGnprtb0601(int a, int b, int* out)
{
  *out = (b / g_gnprtb0601 + 1) * a;
  return 1;
}

int RollBelowRange0602(int value, unsigned* out)
{
  const int threshold = GameRandomRange(g_gnprtb0602, g_gnprtb0603);
  *out = value < threshold;
  return 1;
}

void SplitDifference(int a, int b, int* positive, int* negative)
{
  if (b < a) {
    *positive = a - b;
    *negative = 0;
    return;
  }
  *negative = b - a;
  *positive = 0;
}

int RollChance0606(int* out)
{
  *out = PercentChance(g_gnprtb0606);
  return 1;
}

int RollChanceFromGnprtb0200(int value, int* out)
{
  *out = 0;
  *out = PercentChance(value / g_gnprtb0200);
  return 1;
}

int ComputeCompareFlags0200(int a, int b, int c, int d, unsigned* low, unsigned* high)
{
  *low = 0;
  *high = 0;
  b = b / g_gnprtb0201;
  d = d / g_gnprtb0201;
  const int score = GameRandom(g_gnprtb0202) + ((b + a) - (d + c));
  *low = score <= g_gnprtb0203;
  *high = g_gnprtb0204 <= score;
  return 1;
}

int CountSuccesses0205(int attempts, int* count)
{
  *count = 0;
  for (; attempts > 0; --attempts) {
    *count += PercentChance(g_gnprtb0205) != 0;
  }
  return 1;
}

int RollIdentifyOutcome(int a, int b, int c, int d, int* r1, int* r2, int* r3, int* r4)
{
  *r1 = 0;
  *r2 = 0;
  *r3 = 0;
  a = a - b;
  *r4 = 0;
  const int span = (d != 0) + a + (c != 0);
  if (span != 0) {
    const int roll = GameRandom(span - 1);
    if (roll < a) {
      *r1 = 1;
      *r2 = roll;
      return 1;
    }
    if (roll == a) {
      if (c != 0) {
        *r3 = 1;
        return 1;
      }
      if (d != 0) {
        *r4 = 1;
        return 1;
      }
    } else if (d != 0) {
      *r4 = 1;
    }
  }
  return 1;
}
