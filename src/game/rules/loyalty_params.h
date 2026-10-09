#pragma once
#include <cstdint>

#include "data/table_entry.h"
#include "galaxy/system_neighbours.h"
#include "gameobj/object_key.h"
#include "gameobj/side_bits.h"
#include "gameobj/status_code.h"
#include "rules/loyalty_rule.h"

// The "rules 14" block (0x55cb60-0x55e290): GNPRTB/SDPRTB 0x1400-0x1445, the parameters of the loyalty rules and of the
// new-game seeding that sits on top of them. At game start BindGnprtb14 copies the values for the current difficulty
// into the globals below. The functions fill a LoyaltyRule from them, or hand the seeding stages their table rows
// and budgets. Parameter meanings are inferred from the callers where noted.

// ---- parameters ----
extern int g_gnprtb1400;  // 0x006bb6e8
extern int g_gnprtb1401;  // 0x006bb630
extern int g_gnprtb1402;  // 0x006bb5fc
extern int g_gnprtb1403;  // 0x006bb694
extern int g_gnprtb1404;  // 0x006bb628
extern int g_gnprtb1405;  // 0x006bb63c
extern int g_gnprtb1406;  // 0x006bb638
extern int g_gnprtb1407;  // 0x006bb5d0
extern int g_gnprtb1408;  // 0x006bb6b8
extern int g_gnprtb1409;  // 0x006bb648
extern int g_gnprtb140A;  // 0x006bb6dc
extern int g_gnprtb140B;  // 0x006bb6a0
extern int g_gnprtb140C;  // 0x006bb6b4
extern int g_gnprtb140D;  // 0x006bb618
extern int g_gnprtb140E;  // 0x006bb6ac
extern int g_gnprtb140F;  // 0x006bb644
extern int g_gnprtb1410;  // 0x006bb6b0
extern int g_gnprtb1411;  // 0x006bb5cc
extern int g_gnprtb1412;  // 0x006bb5ec
extern int g_gnprtb1413;  // 0x006bb640
extern int g_gnprtb1414;  // 0x006bb674
extern int g_gnprtb1415;  // 0x006bb6cc
extern int g_gnprtb1416;  // 0x006bb6a4
extern int g_gnprtb1417;  // 0x006bb658
extern int g_gnprtb1420;  // 0x006bb5f0
extern int g_sdprtb1400Side1;  // 0x006bb5f8
extern int g_sdprtb1400Side2;  // 0x006bb6bc
extern int g_sdprtb1401Side1;  // 0x006bb5e8
extern int g_sdprtb1401Side2;  // 0x006bb670
extern int g_sdprtb1402Side1;  // 0x006bb6d8
extern int g_sdprtb1402Side2;  // 0x006bb6d4
extern int g_sdprtb1408Side1;  // 0x006bb614
extern int g_sdprtb1408Side2;  // 0x006bb5f4
extern int g_sdprtb1409Side1;  // 0x006bb6d0
extern int g_sdprtb1409Side2;  // 0x006bb6c4
extern int g_sdprtb140ASide1;  // 0x006bb66c
extern int g_sdprtb140ASide2;  // 0x006bb6a8
extern int g_sdprtb140BSide1;  // 0x006bb6c0
extern int g_sdprtb140BSide2;  // 0x006bb624
extern int g_sdprtb1410Side1;  // 0x006bb5e0
extern int g_sdprtb1410Side2;  // 0x006bb604
extern int g_sdprtb1411Side1;  // 0x006bb6ec
extern int g_sdprtb1411Side2;  // 0x006bb690
extern int g_sdprtb1412Side1;  // 0x006bb6fc
extern int g_sdprtb1412Side2;  // 0x006bb62c
extern int g_sdprtb1413Side1;  // 0x006bb6f0
extern int g_sdprtb1413Side2;  // 0x006bb6e0
extern int g_sdprtb1414Side1;  // 0x006bb6c8
extern int g_sdprtb1414Side2;  // 0x006bb634
extern int g_sdprtb1415Side1;  // 0x006bb678
extern int g_sdprtb1415Side2;  // 0x006bb5dc
extern int g_sdprtb1420Side1;  // 0x006bb660
extern int g_sdprtb1420Side2;  // 0x006bb67c
extern int g_sdprtb1421Side1;  // 0x006bb6e4
extern int g_sdprtb1421Side2;  // 0x006bb608
extern int g_sdprtb1422Side1;  // 0x006bb668
extern int g_sdprtb1422Side2;  // 0x006bb60c
extern int g_sdprtb1423Side1;  // 0x006bb684
extern int g_sdprtb1423Side2;  // 0x006bb5d4
extern int g_sdprtb1430Side1;  // 0x006bb664
extern int g_sdprtb1430Side2;  // 0x006bb600
extern int g_sdprtb1431Side1;  // 0x006bb6f4
extern int g_sdprtb1431Side2;  // 0x006bb610
extern int g_sdprtb1432Side1;  // 0x006bb69c
extern int g_sdprtb1432Side2;  // 0x006bb654
extern int g_sdprtb1440Side1;  // 0x006bb698
extern int g_sdprtb1440Side2;  // 0x006bb6f8
extern int g_sdprtb1441Side1;  // 0x006bb620
extern int g_sdprtb1441Side2;  // 0x006bb680
extern int g_sdprtb1442Side1;  // 0x006bb5d8
extern int g_sdprtb1442Side2;  // 0x006bb68c
extern int g_sdprtb1443Side1;  // 0x006bb64c
extern int g_sdprtb1443Side2;  // 0x006bb61c
extern int g_sdprtb1444Side1;  // 0x006bb5e4
extern int g_sdprtb1444Side2;  // 0x006bb65c
extern int g_sdprtb1445Side1;  // 0x006bb688
extern int g_sdprtb1445Side2;  // 0x006bb650

// Binds all of the above for the current difficulty (ids 0x1400-0x1417 and 0x1420 of GNPRTB, the per-side ids of
// SDPRTB); every binder runs even after a failure and the results are dropped.
void BindGnprtb14();  // 0x55cb60

// ---- new-game seeding parameters (GameRoot setup stages 0x10-0x12) ----

// *atYavin = PercentChance(GNPRTB 0x1409): whether the starting fleet is placed at Yavin. Returns 1.
int RollStartingFleetAtYavin(int* atYavin);  // 0x55d500
// Each stores the table type of the initial-force rows for one place and the first and last row. Return 1.
int GetYavinForceRows(int* tableType, int* first, int* last);         // 0x55d520, type 0x50
int GetKey154ForceRows(int* tableType, int* first, int* last);        // 0x55d550, 0x51
int GetCoruscantForceRows(int* tableType, int* first, int* last);     // 0x55d580, 0x52
int GetStartingFleetRows(int* tableType, int* first, int* last);      // 0x55d5b0, 0x53
int GetCoruscantFleetRows(int* tableType, int* first, int* last);     // 0x55d5e0, 0x54
int GetCoreSystemRows(int* tableType, int* first, int* last);         // 0x55d610, 0x55
int GetCoruscantSystemRows(int* tableType, int* first, int* last);    // 0x55d640, 0x56
// *out = ScalePercent(value, SDPRTB 0x1430 + galaxy size - 1, side 1 / side 2). Returns whether the galaxy size
// (1-3) is valid; *out is 0 otherwise.
int ScaleAllianceReserveBudget(int value, int galaxySize, int* out);  // 0x55d670
int ScaleEmpireReserveBudget(int value, int galaxySize, int* out);    // 0x55d6d0
// Picks a random row (percent roll 1-100) of reserve table 3 / 4: *count = its first value, *list = its unit list.
// Returns the list's ok flag, 0 if there is no such row.
int PickAllianceReserveUnits(int* count, ObjectRefList* list);  // 0x55d730
int PickEmpireReserveUnits(int* count, ObjectRefList* list);    // 0x55d780

// ---- distances and travel time ----

// *count = the starting number of minor characters of `side` (1 Alliance, 2 Empire) for a galaxy of size 1-3
// (SDPRTB 0x1400 + size - 1); 0 for anything else. Returns 1.
int GetStartingCharacterCount(int side, int galaxySize, int* count);  // 0x55d7d0
// Straight-line distance between two map positions.
int MapDistance(const MapPosition* from, const MapPosition* to);  // 0x55d860
// Days to travel between two points at the default speed (GNPRTB 1) / at `speedPercent`; at least 1 unless the
// points coincide.
int TravelDays(const MapPosition* from, const MapPosition* to);                    // 0x55d8a0
int TravelDaysAtSpeed(const MapPosition* from, const MapPosition* to, int speed);  // 0x55d8c0

// ---- loyalty rules ----
// Each starts by resetting *rule. Those that can refuse return 0 (and leave the rule reset).

// Loyalty fallout of a system being captured by `side` (1 or 2): `inner` picks the core-system family range
// (0x90-0x92) over the outer one (0x92-0x94). Two variants (callers: the planet assault resolver).
int BuildCaptureRule(int side, int inner, LoyaltyRule* rule);     // 0x55d900
int BuildCaptureRuleAlt(int side, int inner, LoyaltyRule* rule);  // 0x55da90
// Loyalty pull of a strength imbalance between the two sides' forces. Returns whether `side` was 1 or 2.
int BuildStrengthImbalanceRule(int totalSide1, int totalSide2, int side, int valueSide1, int valueSide2,
                               LoyaltyRule* rule);  // 0x55dbf0
// Rules for a system whose side bits change from *from to *to. Return 1.
int BuildControlChangeRule(SideBits* from, SideBits* to, LoyaltyRule* rule);        // 0x55de50
int BuildControlChangeRuleSimple(SideBits* from, SideBits* to, LoyaltyRule* rule);  // 0x55e010
int BuildEnrouteRule(LoyaltyRule* rule);    // 0x55e150
int BuildDepartureRule(LoyaltyRule* rule);  // 0x55e1f0
