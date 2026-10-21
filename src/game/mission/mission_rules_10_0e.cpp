#include "data/game_math.h"
#include "data/probability_table.h"
#include "data/random.h"
#include "mission/mission_rules.h"

// Rules over GNPRTB 0x10xx and 0x0exx (0x55e290-0x55e5b0): production time, decoy / foil / detection chances.

int ComputeProcessingDuration(int rate, const IntPair* work, int modifier, int notStartupCycle, int* duration)
{
  *duration = 0;
  int perCycle = ScalePercent(static_cast<int>(work->m_first), g_gnprtb1000);
  if (perCycle == 0) {
    perCycle = 1;
  }
  const int cycles = DivideRoundUp(static_cast<int>(work->m_second), perCycle);
  *duration = cycles + rate;
  *duration = ScalePercent(cycles + rate, modifier);
  if (notStartupCycle == 0) {
    *duration = GameRandom(*duration) + *duration / 2;
  }
  return 1;
}

void RollDecoyTable(int unused, int a, int b, int c, int foil, int* success, int* chance)
{
  *success = 1;
  *chance = 0;
  const int bonus = ScalePercent(b, g_gnprtb0E04);
  RollTableChanceWithValue((foil != 0) + 10, (a - c) - bonus, success, chance);
}

void RollFoilTable(int a, int b, int unused, int c, int d, int unused2, int* success, int* chance)
{
  *success = 1;
  *chance = 0;
  const int bonus = ScalePercent(b, g_gnprtb0E05);
  RollTableChanceWithValue(0xc, (((a - c) - d) - bonus) - g_gnprtb0E00, success, chance);
}

int ComputeDetectionChance(int unused, int a, int b, int* roll, int* chance)
{
  *roll = 0;
  *chance = 0;
  if (a != 0 && b != 0) {
    *chance = b + a + g_gnprtb0E03;
  }
  if (*chance > 0) {
    *roll = PercentChance(*chance);
  }
  return 1;
}

int RollComplementChance(int unused, int value, int* success, int* chance)
{
  *chance = 0;
  *chance = 100 - value;
  *success = PercentChance(100 - value);
  return 1;
}

int RollDirectChance(int unused, int chance, int* success, int* chanceOut)
{
  *chanceOut = chance;
  *success = PercentChance(chance);
  return 1;
}

int IsAtLeastGnprtb0E01(int value)
{
  return g_gnprtb0E01 <= value;
}

int IsAtLeastGnprtb0E02(int value)
{
  return g_gnprtb0E02 <= value;
}
