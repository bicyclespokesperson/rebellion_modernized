#include "data/game_math.h"
#include "data/probability_table.h"
#include "data/random.h"
#include "mission/mission_rules.h"

// Mission formulas over GNPRTB 0x18xx and the *MSTB tables (0x55c620-0x55cb60), called by each mission class.

namespace
{

// The originals leave LookupTableValue's found flag in EAX, and the mission slot-157 callbacks return it.
int lookupMissionTable(int tableType, int key, int* out)
{
  *out = 0;
  int value = 0;
  const int found = LookupTableValue(tableType, key, &value);
  if (found)
    *out = value;
  return found;
}

} // namespace

int DivideByMissionDivisor(int value)
{
  return value / g_gnprtb1800;
}

int NegatedDivideByMissionDivisor(int value)
{
  return -(value / g_gnprtb1800);
}

int GetMissionRuleValue1801()
{
  return g_gnprtb1801;
}

int RollMissionRuleBonus1809(int* result)
{
  *result = GameRandom(g_gnprtb1809) + g_gnprtb1808;
  return 1;
}

int LookupDiplomacyMissionTable(int a, int b, int c, int* out)
{
  return lookupMissionTable(mission_table::kDiplomacy, (c - b) + a, out);
}

int LookupEspionageMissionTable(int key, int* out)
{
  return lookupMissionTable(mission_table::kEspionage, key, out);
}

int CopyMissionValue(int value, int* out)
{
  *out = value;
  return 1;
}

int LookupRecruitMissionTable(int a, int b, int* out)
{
  return lookupMissionTable(mission_table::kRecruit, a - b, out);
}

int LookupInciteMissionTable(int a, int b, int c, int* out)
{
  return lookupMissionTable(mission_table::kIncite, (a - b) - c, out);
}

int LookupSubdueMissionTable(int a, int b, int c, int* out)
{
  return lookupMissionTable(mission_table::kSubdue, (c - b) + a, out);
}

int ComputeJediTrainingDifference(int a, int b, int* out)
{
  *out = b - a;
  return 1;
}

int LookupRescueMissionTable(int key, int* out)
{
  return lookupMissionTable(mission_table::kRescue, key, out);
}

int LookupMissionTable19(int a, int b, int* out)
{
  return lookupMissionTable(mission_table::kTable19, a - b, out);
}

int LookupMissionTable1D(int a, int b, int* out)
{
  return lookupMissionTable(mission_table::kTable1D, a - b, out);
}

int LookupSabotageMissionTable(int a, int b, int* out)
{
  return lookupMissionTable(mission_table::kSabotage, (b + a) / 2, out);
}

int LookupDSSabotageMissionTable(int a, int b, int* out)
{
  return lookupMissionTable(mission_table::kDSSabotage, (b + a) / 2, out);
}

int ComputePalaceMissionValue(int a, int b, int* out)
{
  *out = a / g_gnprtb180A + b / g_gnprtb180B;
  return 1;
}

int ComputeEspionageOutcome(int a, int b, int c, int* amount, int* flagA, int* flagB)
{
  *amount = 0;
  *flagA = 0;
  *flagB = 0;
  if (a == 0)
    return 1;
  if (c == 0)
  {
    if (b == 0)
    {
      *amount = GameRandom(g_gnprtb1821) + g_gnprtb1820;
      *flagA = 1;
      *flagB = 0;
      return 1;
    }
    *amount = GameRandom(g_gnprtb1823) + g_gnprtb1822;
  }
  else
  {
    *amount = GameRandom(g_gnprtb1825) + g_gnprtb1824;
  }
  *flagA = 1;
  *flagB = 1;
  return 1;
}

int RollResearchBonus(int* out)
{
  *out = GameRandom(g_gnprtb181C) + g_gnprtb181B;
  return 1;
}

int ComputeJediTrainingGain(int a, int b, int* out)
{
  *out = a;
  if (a < b)
    *out += GameRandom(ScalePercent(b - a, g_gnprtb181D));
  return 1;
}

int ComputeDagobahBonusA(int a, int* out)
{
  *out = ScalePercent(a, g_gnprtb181E) + a;
  return 1;
}

int ComputeDagobahBonusB(int a, int b, int* out)
{
  *out = ScalePercent(a, b / g_gnprtb181F) + a;
  return 1;
}

int ComputeDiplomacyBonus(int a, int b)
{
  int bonus = 0;
  if (a == b)
    return GameRandom(g_gnprtb1828) + g_gnprtb1827;
  if (b == 3)
    bonus = GameRandom(g_gnprtb182A) + g_gnprtb1829;
  return bonus;
}

int ComputeSubdueBonus(int a, int b)
{
  int bonus = 0;
  if (a == b)
    return GameRandom(g_gnprtb182C) + g_gnprtb182B;
  if (b == 3)
    bonus = GameRandom(g_gnprtb182E) + g_gnprtb182D;
  return bonus;
}
