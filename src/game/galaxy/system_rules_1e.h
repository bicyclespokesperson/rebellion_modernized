#pragma once

#include "gameobj/child_iterator.h"
#include "gameobj/object_key.h"

// System rules 1e (0x558bb0-0x55a0f0): the GNPRTB/SDPRTB 0x1e00-0x1e57 parameter block and the free formulas over it
// that the System classes (0x50xxxx rules code, the type-0x90 class 0x566xxx, the type-0x92 class 0x56axxx) call:
// initial energy/raw material/populated rolls, side-dependent loyalty, uprising support and informant incidents.
// Parameter globals keep the file ids (g_gnprtb1E<id>, g_sdprtb1E<id>Side<1|2>); the meaning of each is noted on
// the formula that reads it. Function names are provisional where marked "?".

extern int g_gnprtb1E00;  // 0x6bb3dc
extern int g_gnprtb1E01;  // 0x6bb400
extern int g_gnprtb1E02;  // 0x6bb3e4
extern int g_gnprtb1E03;  // 0x6bb474
extern int g_gnprtb1E04;  // 0x6bb458
extern int g_gnprtb1E05;  // 0x6bb3cc
extern int g_gnprtb1E06;  // 0x6bb3f4
extern int g_gnprtb1E07;  // 0x6bb3c0
extern int g_gnprtb1E08;  // 0x6bb3bc
extern int g_gnprtb1E09;  // 0x6bb414
extern int g_gnprtb1E0A;  // 0x6bb490
extern int g_gnprtb1E0B;  // 0x6bb430
extern int g_gnprtb1E0C;  // 0x6bb470
extern int g_gnprtb1E0D;  // 0x6bb48c
extern int g_gnprtb1E0E;  // 0x6bb3d8
extern int g_gnprtb1E0F;  // 0x6bb494
extern int g_gnprtb1E10;  // 0x6bb3fc
extern int g_gnprtb1E11;  // 0x6bb4c8
extern int g_gnprtb1E12;  // 0x6bb3c4
extern int g_gnprtb1E13;  // 0x6bb3a0
extern int g_gnprtb1E14;  // 0x6bb3a4
extern int g_gnprtb1E15;  // 0x6bb420
extern int g_gnprtb1E16;  // 0x6bb49c
extern int g_gnprtb1E17;  // 0x6bb3d0
extern int g_gnprtb1E18;  // 0x6bb484
extern int g_gnprtb1E19;  // 0x6bb47c
extern int g_gnprtb1E1A;  // 0x6bb398
extern int g_gnprtb1E1B;  // 0x6bb39c
extern int g_gnprtb1E1C;  // 0x6bb418
extern int g_gnprtb1E1D;  // 0x6bb3b0
extern int g_gnprtb1E1E;  // 0x6bb3ac
extern int g_gnprtb1E1F;  // 0x6bb4b4
extern int g_gnprtb1E20;  // 0x6bb40c
extern int g_gnprtb1E21;  // 0x6bb3e0
extern int g_gnprtb1E22;  // 0x6bb488
extern int g_gnprtb1E23;  // 0x6bb3ec
extern int g_gnprtb1E24;  // 0x6bb43c
extern int g_gnprtb1E25;  // 0x6bb404
extern int g_gnprtb1E26;  // 0x6bb41c
extern int g_gnprtb1E27;  // 0x6bb4b8
extern int g_gnprtb1E28;  // 0x6bb4a8
extern int g_gnprtb1E29;  // 0x6bb460
extern int g_gnprtb1E2A;  // 0x6bb3b4
extern int g_gnprtb1E2B;  // 0x6bb454
extern int g_gnprtb1E2C;  // 0x6bb448
extern int g_gnprtb1E2D;  // 0x6bb4a0
extern int g_gnprtb1E2E;  // 0x6bb498
extern int g_gnprtb1E2F;  // 0x6bb46c
extern int g_gnprtb1E30;  // 0x6bb450
extern int g_gnprtb1E31;  // 0x6bb410
extern int g_gnprtb1E32;  // 0x6bb428
extern int g_gnprtb1E33;  // 0x6bb3c8
extern int g_gnprtb1E34;  // 0x6bb478
extern int g_gnprtb1E35;  // 0x6bb3f8
extern int g_gnprtb1E36;  // 0x6bb434
extern int g_gnprtb1E37;  // 0x6bb4b0
extern int g_gnprtb1E38;  // 0x6bb438
extern int g_gnprtb1E39;  // 0x6bb424
extern int g_gnprtb1E50;  // 0x6bb4c0
extern int g_gnprtb1E51;  // 0x6bb480
extern int g_gnprtb1E52;  // 0x6bb3a8
extern int g_gnprtb1E53;  // 0x6bb44c
extern int g_gnprtb1E54;  // 0x6bb3b8
extern int g_gnprtb1E55;  // 0x6bb394
extern int g_gnprtb1E56;  // 0x6bb4bc
extern int g_gnprtb1E57;  // 0x6bb45c
extern int g_sdprtb1E00Side1;  // 0x6bb3f0
extern int g_sdprtb1E00Side2;  // 0x6bb440
extern int g_sdprtb1E01Side1;  // 0x6bb408
extern int g_sdprtb1E01Side2;  // 0x6bb4ac
extern int g_sdprtb1E02Side1;  // 0x6bb3d4
extern int g_sdprtb1E02Side2;  // 0x6bb3e8
extern int g_sdprtb1E03Side1;  // 0x6bb464
extern int g_sdprtb1E03Side2;  // 0x6bb42c
extern int g_sdprtb1E04Side1;  // 0x6bb444
extern int g_sdprtb1E04Side2;  // 0x6bb4c4
extern int g_sdprtb1E05Side1;  // 0x6bb4a4
extern int g_sdprtb1E05Side2;  // 0x6bb468
// Binds every 0x1e GNPRTB id and the SDPRTB ids 0x1e00-0x1e05 (both sides) for the current difficulty; every binder
// runs even after a failure. Returns 1 if all succeed.
int BindGnprtb1E();  // 0x558bb0

// Initial System property rolls (called by the type-0x90 class 0x566d40; the "B" variants by the type-0x92 class
// 0x56a550). Energy: Clamp(rand(1E2A) + 1E29, 1E21, 1E22).
int RollSystemEnergy();                // 0x559670
// Clamp(rand(1E2C) + 1E2B, 1E1F, 1E20), then Clamp to [1E1F, cap] (cap = the system's energy). ?
int RollSystemRawMaterial(int cap);    // 0x5596a0
// PercentChance(1E32): is the system populated.
int RollSystemPopulated();             // 0x5596f0
// Initial loyalty for a controller `side` (1 Alliance, 2 Empire, 3 neutral) and `kind` (0 or not), rolled from the
// SDPRTB 0x1e02-0x1e05 pairs (side 3: rand(1E54) + 50 - 1E54/2); 0 for other sides.
int RollInitialLoyalty(int side, int kind);  // 0x559700
// SDPRTB 0x1e00/0x1e01 share (percent) for side 1/2 (by kind 0/other); side 3 with kind 0 gets the remainder of 100,
// with other kinds 0; any other side returns kind.
int SideSharePercent(int side, int kind);    // 0x5597e0
int RollSystemEnergyB();               // 0x559970, rand(1E2E) + rand(1E2F) + 1E2D clamped to [1E21, 1E22]
int RollSystemRawMaterialB(int cap);   // 0x5599c0, as RollSystemRawMaterial with 1E31/1E30
int RollSystemPopulatedB();            // 0x559a10, PercentChance(1E33)
// rand(1E55) + 50 - 1E55/2
int RollCenteredPercent();             // 0x559a20 (the case-3 formula of RollInitialLoyalty uses 1E54)

// Rolls a starting unit for a System: *garrisoned = 0, *outKey = 0; PercentChance((a - b) * 1E56) gives the fixed key
// 0x2c000001 and *garrisoned = 1; else a random record of table 1 (the key stored at +0x24 of the record, if any).
// Returns whether a unit was chosen. The B variant is the same with 1E57 and table 2.
int RollStartingUnitA(int a, int b, int* garrisoned, NullableKey* outKey);  // 0x559850
int RollStartingUnitB(int a, int b, int* garrisoned, NullableKey* outKey);  // 0x559a60

// value * 1E00 if `flag` and side == 2, else value.
int ScaleForEmpire(int value, int side, int flag);  // 0x559b60
// Moves `it` to a uniformly chosen accepted child (Count()-1 rolled, Advance that many times). Returns 1 if the
// iterator has no children or ends on one.
int SelectRandomChild(ChildIterator* it);           // 0x559b80
// value / 1E01 when `flag` and the value favours the side (side 1 > 0, side 2 < 0), else value.
int DampenFavourableShift(int value, int side, int flag);  // 0x559be0
// ScaleRatio(1E53, 100, value == 0 ? 1 : value)
int ScaleByRatio1E53(int value);                    // 0x559c10
// Uprising chance in percent (negated for side 2). ?
int ComputeUprisingPercent(int side, int loyalty, int scaleFlag, int a, int b, int c, int suppressed);  // 0x559c40
// Two-table (0x28/0x29) incident roll; *outA / *outB = table values or 0. ?
void RollUprisingIncident(int side, int shortfallBase, int scaleFlag, int a, int b, int c, int d, int* outA, int* outB);  // 0x559ce0
// Informant incident: table 0x2a at rand(1E1E) + 1E1D. ?
void RollInformantIncident(int* outValue);          // 0x559db0
// *success = PercentChance(1E24). Returns 1.
int RollSupportChance(int* success);                // 0x559df0
// Plays out simultaneous losses between two troop counts; both are reduced, *b ends at most *a... Returns 1. ?
int ResolveMutualLosses(int* a, int* b);            // 0x559e10
// Random shift of the counters via table 0x2b. ?
void ApplyRandomCounterShift(int* a, int* b, int flagA, int flagB);  // 0x559ee0
// *result = !PercentChance(percent). Returns 1.
int RollFailure(int percent, int* result);          // 0x559f90
// valueTotal - ComputeSupportCost(...)
int ComputeRemainingSupport(int side, int x, int flag1, int flag2, int flag3, int total);  // 0x559fb0
// ComputeShortfallUnits(x), divided by 1E00 for side 2 with flag1, multiplied by 1E02 if flag2 && flag3.
int ComputeSupportCost(int side, int x, int flag1, int flag2, int flag3);  // 0x559fe0
// ClampNegativeToZero(100 - a * 1E04 - b * 1E05)
int ComputeResidualPercent(int a, int b);           // 0x55a020
// DivideRoundUp(1E51 - x, -1E52) when x < 1E51, else 0.
int ComputeShortfallUnits(int x);                   // 0x55a050
// 1 if x >= 1E50, else 2 + (100 - x < 1E50) (a classification 1-3).
int ClassifyPercent(int x);                         // 0x55a080
int GetParameter1E03();                             // 0x55a0b0
// 1E09 if x >= 1E0A, else 0.
int ThresholdBonus(int x);                          // 0x55a0c0
int GetParameter1E19();                             // 0x55a0e0
// 1E1A, negated when x == 0.
int SignedParameter1E1A(int x);                     // 0x55a0f0
