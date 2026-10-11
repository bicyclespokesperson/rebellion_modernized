#pragma once

// The game's gameplay random number source: a RandomGenerator object (class owned by the util module,
// 0x5f5650-0x5f5810) created by the strategy game (0x519740) and registered here. All gameplay randomness
// goes through these wrappers, so draw order is part of the game's behaviour. The CRT rand() is only used to
// pick a seed when none is given.
class RandomGenerator;

extern RandomGenerator* g_gameRandom;  // 0x6b9054, null outside a running game

void SetGameRandom(RandomGenerator* generator);
// Uniform value in [0, max] for max >= 0, in [max, 0] for max < 0; 0 without a generator.
int GameRandom(int max);
// Inclusive range; reversed bounds are normalized.
int GameRandomRange(int low, int high);
// Uniform value in [0, limit), or 0 without a gameplay generator.
int GameplayRandomNext(int limit);
// GameRandom(99)
int RollPercent();
// 1 if RollPercent() < percent.
int PercentChance(int percent);
