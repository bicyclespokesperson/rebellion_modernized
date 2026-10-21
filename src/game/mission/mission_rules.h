#pragma once

#include "gameobj/status_code.h"

// The mission rules: GNPRTB records 0x18xx (mission formulas), 0x10xx, 0x0exx (decoys/foils/detection), 0x0cxx
// (escape), 0x06xx and 0x02xx (identification) copied for the current difficulty into globals (the Bind functions run
// from the game-start sequence 0x513f70; every binder runs even after a failure, true if all succeeded), and the
// formulas the mission classes call. The *MSTB tables (Diplomacy .. Subdue) are looked up by the Lookup...MissionTable
// functions; *out is 0 if the record is missing.

// BindGnprtb18 (0x55bfd0)
extern int g_gnprtb1800; // 0x6bb594
extern int g_gnprtb1801; // 0x6bb52c
extern int g_gnprtb1802; // 0x6bb560
extern int g_gnprtb1803; // 0x6bb5bc
extern int g_gnprtb1804; // 0x6bb550
extern int g_gnprtb1805; // 0x6bb548
extern int g_gnprtb1806; // 0x6bb5b8
extern int g_gnprtb1807; // 0x6bb564
extern int g_gnprtb1808; // 0x6bb544
extern int g_gnprtb1809; // 0x6bb590
extern int g_gnprtb180A; // 0x6bb524
extern int g_gnprtb180B; // 0x6bb5b4
extern int g_gnprtb180C; // 0x6bb588
extern int g_gnprtb180D; // 0x6bb534
extern int g_gnprtb180E; // 0x6bb568
extern int g_gnprtb180F; // 0x6bb5b0
extern int g_gnprtb1810; // 0x6bb5a0
extern int g_gnprtb1811; // 0x6bb538
extern int g_gnprtb1812; // 0x6bb55c
extern int g_gnprtb1813; // 0x6bb574
extern int g_gnprtb1814; // 0x6bb584
extern int g_gnprtb1815; // 0x6bb58c
extern int g_gnprtb1816; // 0x6bb518
extern int g_gnprtb1817; // 0x6bb5c0
extern int g_gnprtb1818; // 0x6bb57c
extern int g_gnprtb1819; // 0x6bb570
extern int g_gnprtb181A; // 0x6bb5a8
extern int g_gnprtb181B; // 0x6bb5c4
extern int g_gnprtb181C; // 0x6bb578
extern int g_gnprtb181D; // 0x6bb528
extern int g_gnprtb181E; // 0x6bb59c
extern int g_gnprtb181F; // 0x6bb514
extern int g_gnprtb1820; // 0x6bb554
extern int g_gnprtb1821; // 0x6bb530
extern int g_gnprtb1822; // 0x6bb520
extern int g_gnprtb1823; // 0x6bb558
extern int g_gnprtb1824; // 0x6bb5a4
extern int g_gnprtb1825; // 0x6bb54c
extern int g_gnprtb1827; // 0x6bb56c
extern int g_gnprtb1828; // 0x6bb5ac
extern int g_gnprtb1829; // 0x6bb51c
extern int g_gnprtb182A; // 0x6bb598
extern int g_gnprtb182B; // 0x6bb5c8
extern int g_gnprtb182C; // 0x6bb540
extern int g_gnprtb182D; // 0x6bb53c
extern int g_gnprtb182E; // 0x6bb580

// BindGnprtb10 (0x55e290)
extern int g_gnprtb1000; // 0x6bb700

// BindGnprtb0E (0x55e340)
extern int g_gnprtb0E00; // 0x6bb714
extern int g_gnprtb0E01; // 0x6bb708
extern int g_gnprtb0E02; // 0x6bb704
extern int g_gnprtb0E03; // 0x6bb718
extern int g_gnprtb0E04; // 0x6bb710
extern int g_gnprtb0E05; // 0x6bb70c

// BindGnprtb0C (0x55e5b0)
extern int g_gnprtb0C00; // 0x6bb72c
extern int g_gnprtb0C01; // 0x6bb720
extern int g_gnprtb0C02; // 0x6bb728
extern int g_gnprtb0C03; // 0x6bb754
extern int g_gnprtb0C04; // 0x6bb730
extern int g_gnprtb0C05; // 0x6bb74c
extern int g_gnprtb0C06; // 0x6bb740
extern int g_gnprtb0C07; // 0x6bb734
extern int g_gnprtb0C08; // 0x6bb750
extern int g_gnprtb0C09; // 0x6bb758
extern int g_gnprtb0C0A; // 0x6bb744
extern int g_gnprtb0C0B; // 0x6bb748
extern int g_gnprtb0C0C; // 0x6bb73c
extern int g_gnprtb0C0D; // 0x6bb71c
extern int g_gnprtb0C0E; // 0x6bb724
extern int g_gnprtb0C0F; // 0x6bb738

// BindGnprtb06 (0x55e840)
extern int g_gnprtb0600; // 0x6bb768
extern int g_gnprtb0601; // 0x6bb774
extern int g_gnprtb0602; // 0x6bb75c
extern int g_gnprtb0603; // 0x6bb764
extern int g_gnprtb0604; // 0x6bb760
extern int g_gnprtb0605; // 0x6bb770
extern int g_gnprtb0606; // 0x6bb76c

// BindGnprtb02 (0x55ea00)
extern int g_gnprtb0200; // 0x6bb788
extern int g_gnprtb0201; // 0x6bb780
extern int g_gnprtb0202; // 0x6bb78c
extern int g_gnprtb0203; // 0x6bb778
extern int g_gnprtb0204; // 0x6bb784
extern int g_gnprtb0205; // 0x6bb77c

int BindGnprtb18();
int BindGnprtb10();
int BindGnprtb0E();
int BindGnprtb0C();
int BindGnprtb06();
int BindGnprtb02();

namespace mission_table
{
constexpr int kDiplomacy = 0x14;
constexpr int kRescue = 0x15;
constexpr int kSabotage = 0x16;
constexpr int kEspionage = 0x17;
constexpr int kRecruit = 0x18;
constexpr int kTable19 = 0x19;
constexpr int kIncite = 0x1a;
constexpr int kDSSabotage = 0x1b;
constexpr int kSubdue = 0x1c;
constexpr int kTable1D = 0x1d;
constexpr int kEscape = 0x2c;
} // namespace mission_table

// ---- 0x18xx formulas (mission_rules_18.cpp) ----
int DivideByMissionDivisor(int value);        // value / g_gnprtb1800
int NegatedDivideByMissionDivisor(int value); // -(value / g_gnprtb1800)
int GetMissionRuleValue1801();
int RollMissionRuleBonus1809(int* result);
int LookupDiplomacyMissionTable(int a, int b, int c, int* out);
int LookupEspionageMissionTable(int key, int* out);
int CopyMissionValue(int value, int* out);
int LookupRecruitMissionTable(int a, int b, int* out);
int LookupInciteMissionTable(int a, int b, int c, int* out);
int LookupSubdueMissionTable(int a, int b, int c, int* out);
int ComputeJediTrainingDifference(int a, int b, int* out);
int LookupRescueMissionTable(int key, int* out);
int LookupMissionTable19(int a, int b, int* out);
int LookupMissionTable1D(int a, int b, int* out);
int LookupSabotageMissionTable(int a, int b, int* out);
int LookupDSSabotageMissionTable(int a, int b, int* out);
int ComputePalaceMissionValue(int a, int b, int* out);
int ComputeEspionageOutcome(int a, int b, int c, int* amount, int* flagA, int* flagB);
int RollResearchBonus(int* out);
int ComputeJediTrainingGain(int a, int b, int* out);
int ComputeDagobahBonusA(int a, int* out);
int ComputeDagobahBonusB(int a, int b, int* out);
int ComputeDiplomacyBonus(int a, int b);
int ComputeSubdueBonus(int a, int b);

// ---- 0x10xx / 0x0exx (mission_rules_10_0e.cpp) ----
// Production time formula (cdecl): false if the work cannot be processed.
int ComputeProcessingDuration(int rate, const IntPair* work, int modifier, int notStartupCycle, int* duration);
void RollDecoyTable(int unused, int a, int b, int c, int foil, int* success, int* chance);
void RollFoilTable(int a, int b, int unused, int c, int d, int unused2, int* success, int* chance);
int ComputeDetectionChance(int unused, int a, int b, int* roll, int* chance);
int RollComplementChance(int unused, int value, int* success, int* chance);
int RollDirectChance(int unused, int chance, int* success, int* chanceOut);
int IsAtLeastGnprtb0E01(int value);
int IsAtLeastGnprtb0E02(int value);

// ---- 0x0cxx / 0x06xx / 0x02xx (mission_rules_0c_06_02.cpp) ----
int RollEscapeTable(int unused, int a, int b, int c, int d, int* success);
int ComputeEscapeBase(int* out);
int ComputeScaledByGnprtb0600(int a, int b, int* out);
int ComputeScaledByGnprtb0601(int a, int b, int* out);
int RollBelowRange0602(int value, unsigned* out);
void SplitDifference(int a, int b, int* positive, int* negative);
int RollChance0606(int* out);
int RollChanceFromGnprtb0200(int value, int* out);
int ComputeCompareFlags0200(int a, int b, int c, int d, unsigned* low, unsigned* high);
int CountSuccesses0205(int attempts, int* count);
int RollIdentifyOutcome(int a, int b, int c, int d, int* r1, int* r2, int* r3, int* r4);
