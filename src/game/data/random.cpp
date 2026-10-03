#include "data/random.h"

#include "data/game_math.h"

// RandomGenerator::Next(limit): value in [0, limit), 0 unless limit > 1.
int __thiscall FUN_005f5700(RandomGenerator* generator, int limit);

void SetGameRandom(RandomGenerator* generator) { g_gameRandom = generator; }

int GameRandom(int max) {
    if (!g_gameRandom) return 0;
    if (max >= 0) return FUN_005f5700(g_gameRandom, max + 1);
    return -FUN_005f5700(g_gameRandom, 1 - max);
}

int GameRandomRange(int low, int high) { return GameRandom(high - low) + low; }

int RollPercent() { return GameRandom(g_percentMax - 1); }

int PercentChance(int percent) { return RollPercent() < percent; }
