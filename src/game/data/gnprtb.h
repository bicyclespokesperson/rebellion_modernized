#pragma once

// GNPRTB (GData/GNPRTB.DAT, table type 0x3c) is the general parameter table: one record per parameter id with
// eight values, one per difficulty setting. SDPRTB (type 0x3d) has the same layout but two values (one per side)
// per difficulty. At game start the rules code copies the values it needs, for the current difficulty, into the
// globals declared below. Parameter ids are those of the file (see refs/open-rebellion for the documented
// ones); the globals are named after them. Meanings inferred from the callers are noted where known.

extern int g_gnprtbDifficulty; // 0x6b904c, index (0-7) of the difficulty column to read
extern int g_gnprtbDefault;    // 0x6b9050, GNPRTB 1

extern int g_gnprtbA00; // 0x6b9088, Force potential threshold (HasForcePotentialA00)
extern int g_gnprtbA01; // 0x6b9068
extern int g_gnprtbA02; // 0x6b9064
extern int g_gnprtbA03; // 0x6b9084, injury recovery per tick (2563)
extern int g_gnprtbA04; // 0x6b905c, injury recovery per tick (2564)
extern int g_gnprtbA05; // 0x6b9080, injury roll: least chance (2565)
extern int g_gnprtbA06; // 0x6b908c, injury roll: base duration (2566)
extern int g_gnprtbA07; // 0x6b90d8, injury roll: spread (2567)
extern int g_gnprtbA08; // 0x6b90a8
extern int g_gnprtbA09; // 0x6b90a4
extern int g_gnprtbA0A; // 0x6b9098
extern int g_gnprtbA0B; // 0x6b90d0
extern int g_gnprtbA0C; // 0x6b90d4
extern int g_gnprtbA0D; // 0x6b90b8
extern int g_gnprtbA0E; // 0x6b9070
extern int g_gnprtbA0F; // 0x6b9090
extern int g_gnprtbA10; // 0x6b90cc
extern int g_gnprtbA11; // 0x6b90c8
extern int g_gnprtbA12; // 0x6b9060
extern int g_gnprtbA13; // 0x6b90bc, Force potential threshold (HasForcePotentialA13)
extern int g_gnprtbA14; // 0x6b9058, Force potential threshold (HasForcePotentialA14)
extern int g_gnprtbA15; // 0x6b90dc, Force potential threshold (HasForcePotentialA15)
extern int g_gnprtbA16; // 0x6b907c
extern int g_gnprtbA17; // 0x6b90c0
extern int g_gnprtbA18; // 0x6b90ac
extern int g_gnprtbA19; // 0x6b9074
extern int g_gnprtbA20; // 0x6b90c4
extern int g_gnprtbA21; // 0x6b909c

extern int g_sdprtbA00Side1; // 0x6b9094
extern int g_sdprtbA00Side2; // 0x6b9078
extern int g_sdprtbA01Side1; // 0x6b90a0
extern int g_sdprtbA01Side2; // 0x6b90b0
extern int g_sdprtbA02Side1; // 0x6b90b4
extern int g_sdprtbA02Side2; // 0x6b906c

extern int g_gnprtb1C00; // 0x6bb50c
extern int g_gnprtb1C01; // 0x6bb510
extern int g_gnprtb1C02; // 0x6bb508

// Selects the difficulty column (called first by the game-start sequence 0x513f70). Returns 1.
int SetGnprtbDifficulty(int difficultyIndex);

// Reads GNPRTB record `id` for the current difficulty into *value (0 if the record is missing). Returns 1 if the
// record exists and carries that id.
int BindGnprtbParameter(int id, int* value);
// Reads SDPRTB record `id`, side 1 or 2, into *value. Returns 1 on success.
int BindSdprtbParameter(int id, int side, int* value);

int BindGnprtbDefault();
// Binds ids 0xa00-0xa19, 0xa20, 0xa21 and the per-side 0xa00-0xa02 (every binder runs even after a failure).
// Returns 1 if all succeeded.
int BindGeneralParameters();
// Binds ids 0x1c00-0x1c02. Returns 1 if all succeeded.
int BindGnprtb1C();

// RulesScalePercent(a, b) = ScalePercent(a, b), forwarded from the 0x1c rules block.
int RulesScalePercent(int value, int percent);

// Character skill arithmetic used by the Force-potential setters (+0x62/+0x5a skill, +0x8c Force potential,
// +0x94 injury). Both return 1.
int ComputeSkillWithForceAndInjury(int skill, int forcePotential, int injury, int* result);
int ComputeSkillWithForce(int skill, int forcePotential, int* result);

int HasForcePotentialA00(int forcePotential, int* result);
int HasForcePotentialA13(int forcePotential);
int HasForcePotentialA14(int forcePotential);
int HasForcePotentialA15(int forcePotential);

// Duration rolls: with probability max(least chance, 100 - input) ... *result = 0 when the first roll fails,
// else rand(chance) + rand(spread) + base. All return 1.
int RollInjuryDuration(int combatSkill, int* result);
int RollInjuryDurationB(int combatSkill, int* result);
int RollDurationFromDifference(int a, int b, int* result);
int RollDurationUnconditional(int* result);
int RollChanceA11(int* result);

// 0x260-0x263 -> 0-3, anything else 0 (and returns 0).
int MapTypeCodeToIndex(int typeCode, int* index);

int ComputeSideShare(int side, int a, int b);
int RollSideBonus(int side, int enabled);
int RollBonusA20();
