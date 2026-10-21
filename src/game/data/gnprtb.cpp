#include "data/gnprtb.h"

#include <cstddef>

#include "data/game_math.h"
#include "data/param_table_entries.h"
#include "data/probability_table.h"
#include "data/random.h"

namespace
{

struct GnprtbBinding {
  int id;
  int* target;
};

struct SdprtbBinding {
  int id;
  int side;
  int* target;
};

// Binds every entry, in order, even after a failure; true if all succeeded.
template <size_t N>
int bindAll(const GnprtbBinding (&bindings)[N])
{
  int allBound = 1;
  for (const GnprtbBinding& binding : bindings) {
    allBound = BindGnprtbParameter(binding.id, binding.target) && allBound;
  }
  return allBound;
}

template <size_t N>
int bindAll(const SdprtbBinding (&bindings)[N])
{
  int allBound = 1;
  for (const SdprtbBinding& binding : bindings) {
    allBound = BindSdprtbParameter(binding.id, binding.side, binding.target) && allBound;
  }
  return allBound;
}

int recordHasId(const TableEntry* record, int id)
{
  return record && record->m_field20 == id;
}

int isSideInRange(int side)
{
  return side >= 1 && side <= 2;
}

// Rolls the chance, then draws `rand(chance) + rand(spread) + base`; 0 if the chance roll fails.
int rollDuration(int chance, int spread, int base)
{
  if (!PercentChance(chance)) {
    return 0;
  }
  const int first = GameRandom(chance);
  const int second = GameRandom(spread);
  return second + first + base;
}

} // namespace

int SetGnprtbDifficulty(int difficultyIndex)
{
  g_gnprtbDifficulty = difficultyIndex;
  return 1;
}

int BindGnprtbDefault()
{
  return BindGnprtbParameter(1, &g_gnprtbDefault) != 0;
}

int BindGnprtbParameter(int id, int* value)
{
  *value = 0;
  const TableEntry* record = FindTableRecord(kTableGnprtb, id);
  const int found = recordHasId(record, id);
  if (found) {
    *value = static_cast<const GeneralParamTableEntry*>(record)->GetValue(g_gnprtbDifficulty);
  }
  return found;
}

int BindSdprtbParameter(int id, int side, int* value)
{
  *value = 0;
  const TableEntry* record = FindTableRecord(kTableSdprtb, id);
  int usable = recordHasId(record, id);
  if (usable) {
    usable = isSideInRange(side);
  }
  if (usable) {
    *value = static_cast<const SideParamTableEntry*>(record)->GetValue(g_gnprtbDifficulty, side);
  }
  return usable;
}

int BindGeneralParameters()
{
  static const GnprtbBinding kGeneral[] = {
      {0xa00, &g_gnprtbA00}, {0xa01, &g_gnprtbA01}, {0xa02, &g_gnprtbA02}, {0xa03, &g_gnprtbA03}, {0xa04, &g_gnprtbA04},
      {0xa05, &g_gnprtbA05}, {0xa06, &g_gnprtbA06}, {0xa07, &g_gnprtbA07}, {0xa08, &g_gnprtbA08}, {0xa09, &g_gnprtbA09},
      {0xa0a, &g_gnprtbA0A}, {0xa0b, &g_gnprtbA0B}, {0xa0c, &g_gnprtbA0C}, {0xa0d, &g_gnprtbA0D}, {0xa0e, &g_gnprtbA0E},
      {0xa0f, &g_gnprtbA0F}, {0xa10, &g_gnprtbA10}, {0xa11, &g_gnprtbA11}, {0xa12, &g_gnprtbA12}, {0xa13, &g_gnprtbA13},
      {0xa14, &g_gnprtbA14}, {0xa15, &g_gnprtbA15}, {0xa16, &g_gnprtbA16}, {0xa17, &g_gnprtbA17}, {0xa18, &g_gnprtbA18},
      {0xa19, &g_gnprtbA19}, {0xa20, &g_gnprtbA20}, {0xa21, &g_gnprtbA21},
  };
  static const SdprtbBinding kPerSide[] = {
      {0xa00, 1, &g_sdprtbA00Side1}, {0xa00, 2, &g_sdprtbA00Side2}, {0xa01, 1, &g_sdprtbA01Side1},
      {0xa01, 2, &g_sdprtbA01Side2}, {0xa02, 1, &g_sdprtbA02Side1}, {0xa02, 2, &g_sdprtbA02Side2},
  };
  const int generalBound = bindAll(kGeneral);
  const int perSideBound = bindAll(kPerSide);
  return generalBound && perSideBound;
}

int BindGnprtb1C()
{
  static const GnprtbBinding kBindings[] = {
      {0x1c00, &g_gnprtb1C00},
      {0x1c01, &g_gnprtb1C01},
      {0x1c02, &g_gnprtb1C02},
  };
  return bindAll(kBindings);
}

int RulesScalePercent(int value, int percent)
{
  return ScalePercent(value, percent);
}

int ComputeSkillWithForceAndInjury(int skill, int forcePotential, int injury, int* result)
{
  *result = ClampNegativeToZero(ScalePercent(skill, forcePotential) + (skill - injury));
  return 1;
}

int ComputeSkillWithForce(int skill, int forcePotential, int* result)
{
  *result = ScalePercent(skill, forcePotential) + skill;
  return 1;
}

int HasForcePotentialA00(int forcePotential, int* result)
{
  *result = forcePotential >= g_gnprtbA00;
  return 1;
}

int HasForcePotentialA13(int forcePotential)
{
  return forcePotential >= g_gnprtbA13;
}

int HasForcePotentialA14(int forcePotential)
{
  return forcePotential >= g_gnprtbA14;
}

int HasForcePotentialA15(int forcePotential)
{
  return forcePotential >= g_gnprtbA15;
}

int RollInjuryDuration(int combatSkill, int* result)
{
  *result = 0;
  const int chance = MaxInt(g_gnprtbA05, 100 - combatSkill);
  *result = rollDuration(chance, g_gnprtbA07, g_gnprtbA06);
  return 1;
}

int RollInjuryDurationB(int combatSkill, int* result)
{
  *result = 0;
  const int chance = MaxInt(g_gnprtbA08, 100 - combatSkill);
  *result = rollDuration(chance, g_gnprtbA0A, g_gnprtbA09);
  return 1;
}

int RollDurationFromDifference(int a, int b, int* result)
{
  *result = 0;
  const int chance = MaxInt(g_gnprtbA0B, b - a);
  *result = rollDuration(chance, g_gnprtbA0D, g_gnprtbA0C);
  return 1;
}

int RollDurationUnconditional(int* result)
{
  *result = 0;
  *result = rollDuration(g_gnprtbA0E, g_gnprtbA10, g_gnprtbA0F);
  return 1;
}

int RollChanceA11(int* result)
{
  *result = PercentChance(g_gnprtbA11);
  return 1;
}

int MapTypeCodeToIndex(int typeCode, int* index)
{
  *index = 0;
  switch (typeCode) {
    case 0x260:
      *index = 0;
      return 1;
    case 0x261:
      *index = 1;
      return 1;
    case 0x262:
      *index = 2;
      return 1;
    case 0x263:
      *index = 3;
      return 1;
    default:
      return 0;
  }
}

int ComputeSideShare(int side, int a, int b)
{
  if (side == 1) {
    const int divisor = g_sdprtbA00Side1;
    return (b - a) / divisor + b / divisor;
  }
  if (side == 2) {
    const int divisor = g_sdprtbA00Side2;
    return (a - b) / divisor + a / divisor;
  }
  return 0;
}

int RollSideBonus(int side, int enabled)
{
  if (!enabled) {
    return 0;
  }
  if (side == 1) {
    return GameRandom(g_sdprtbA01Side1) + g_sdprtbA02Side1;
  }
  if (side == 2) {
    return GameRandom(g_sdprtbA01Side2) + g_sdprtbA02Side2;
  }
  return 0;
}

int RollBonusA20()
{
  return GameRandom(g_gnprtbA20) + g_gnprtbA21;
}
