#include "data/random.h"

#include "core/utility_classes.h"

#include "data/game_math.h"

void SetGameRandom(RandomGenerator* generator)
{
  g_gameRandom = generator;
}

int GameRandom(int max)
{
  if (!g_gameRandom)
    return 0;
  if (max >= 0)
    return g_gameRandom->Between(0, max);
  return g_gameRandom->Between(max, 0);
}

int GameRandomRange(int low, int high)
{
  return g_gameRandom ? g_gameRandom->Between(low, high) : low;
}

int GameplayRandomNext(int limit)
{
  return g_gameRandom ? g_gameRandom->Next(limit) : 0;
}

int RollPercent()
{
  return GameRandom(g_percentMax - 1);
}

int PercentChance(int percent)
{
  return RollPercent() < percent;
}
