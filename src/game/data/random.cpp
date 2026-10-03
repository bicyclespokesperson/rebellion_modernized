#include "data/random.h"

#include "core/utility_classes.h"

#include "data/game_math.h"


void SetGameRandom(RandomGenerator* generator) { g_gameRandom = generator; }

int GameRandom(int max) {
    if (!g_gameRandom) return 0;
    if (max >= 0) return g_gameRandom->Next(max + 1);
    return -g_gameRandom->Next(1 - max);
}

int GameRandomRange(int low, int high) { return GameRandom(high - low) + low; }

int RollPercent() { return GameRandom(g_percentMax - 1); }

int PercentChance(int percent) { return RollPercent() < percent; }
