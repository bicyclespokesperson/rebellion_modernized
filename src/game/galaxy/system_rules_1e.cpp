#include "galaxy/system_rules_1e.h"

#include "data/game_math.h"
#include "data/gnprtb.h"
#include "data/probability_table.h"
#include "data/random.h"

namespace
{
constexpr uint32_t kFixedUnitKey = 0x2c000001;
constexpr int kUnitTableA = 1;
constexpr int kUnitTableB = 2;

int rollStartingUnit(int a, int b, int chancePerPoint, int table, int* garrisoned, NullableKey* outKey)
{
  *garrisoned = 0;
  outKey->m_value = 0;
  if (PercentChance((a - b) * chancePerPoint)) {
    *garrisoned = 1;
    outKey->m_value = kFixedUnitKey;
    return 1;
  }
  const int roll = GameRandom(100);
  const TableEntry* record = FindTableRecord(table, roll);
  if (record) {
    outKey->m_value = static_cast<uint32_t>(TableRecordFirstValue(record));
    *garrisoned = outKey->IsSet();
  }
  return record != nullptr;
}

int clampedRawMaterial(int roll, int base, int cap)
{
  const int value = Clamp(roll + base, g_gnprtb1E1F, g_gnprtb1E20);
  return Clamp(value, g_gnprtb1E1F, cap);
}
} // namespace

int BindGnprtb1E()
{
  int ok = 1;
  ok = BindGnprtbParameter(0x1e00, &g_gnprtb1E00) && ok;
  ok = BindGnprtbParameter(0x1e01, &g_gnprtb1E01) && ok;
  ok = BindGnprtbParameter(0x1e02, &g_gnprtb1E02) && ok;
  ok = BindGnprtbParameter(0x1e03, &g_gnprtb1E03) && ok;
  ok = BindGnprtbParameter(0x1e04, &g_gnprtb1E04) && ok;
  ok = BindGnprtbParameter(0x1e05, &g_gnprtb1E05) && ok;
  ok = BindGnprtbParameter(0x1e06, &g_gnprtb1E06) && ok;
  ok = BindGnprtbParameter(0x1e07, &g_gnprtb1E07) && ok;
  ok = BindGnprtbParameter(0x1e08, &g_gnprtb1E08) && ok;
  ok = BindGnprtbParameter(0x1e09, &g_gnprtb1E09) && ok;
  ok = BindGnprtbParameter(0x1e0a, &g_gnprtb1E0A) && ok;
  ok = BindGnprtbParameter(0x1e0b, &g_gnprtb1E0B) && ok;
  ok = BindGnprtbParameter(0x1e0c, &g_gnprtb1E0C) && ok;
  ok = BindGnprtbParameter(0x1e0d, &g_gnprtb1E0D) && ok;
  ok = BindGnprtbParameter(0x1e0e, &g_systemBlockadeStatLevelFleetSide) && ok;
  ok = BindGnprtbParameter(0x1e0f, &g_gnprtb1E0F) && ok;
  ok = BindGnprtbParameter(0x1e10, &g_systemBlockadeStatLevelOther) && ok;
  ok = BindGnprtbParameter(0x1e11, &g_gnprtb1E11) && ok;
  ok = BindGnprtbParameter(0x1e12, &g_gnprtb1E12) && ok;
  ok = BindGnprtbParameter(0x1e13, &g_gnprtb1E13) && ok;
  ok = BindGnprtbParameter(0x1e14, &g_gnprtb1E14) && ok;
  ok = BindGnprtbParameter(0x1e15, &g_gnprtb1E15) && ok;
  ok = BindGnprtbParameter(0x1e16, &g_gnprtb1E16) && ok;
  ok = BindGnprtbParameter(0x1e17, &g_gnprtb1E17) && ok;
  ok = BindGnprtbParameter(0x1e18, &g_gnprtb1E18) && ok;
  ok = BindGnprtbParameter(0x1e19, &g_gnprtb1E19) && ok;
  ok = BindGnprtbParameter(0x1e1a, &g_gnprtb1E1A) && ok;
  ok = BindGnprtbParameter(0x1e1b, &g_gnprtb1E1B) && ok;
  ok = BindGnprtbParameter(0x1e1c, &g_gnprtb1E1C) && ok;
  ok = BindGnprtbParameter(0x1e1d, &g_gnprtb1E1D) && ok;
  ok = BindGnprtbParameter(0x1e1e, &g_gnprtb1E1E) && ok;
  ok = BindGnprtbParameter(0x1e1f, &g_gnprtb1E1F) && ok;
  ok = BindGnprtbParameter(0x1e20, &g_gnprtb1E20) && ok;
  ok = BindGnprtbParameter(0x1e21, &g_gnprtb1E21) && ok;
  ok = BindGnprtbParameter(0x1e22, &g_gnprtb1E22) && ok;
  ok = BindGnprtbParameter(0x1e23, &g_gnprtb1E23) && ok;
  ok = BindGnprtbParameter(0x1e24, &g_gnprtb1E24) && ok;
  ok = BindGnprtbParameter(0x1e25, &g_gnprtb1E25) && ok;
  ok = BindGnprtbParameter(0x1e26, &g_gnprtb1E26) && ok;
  ok = BindGnprtbParameter(0x1e27, &g_gnprtb1E27) && ok;
  ok = BindGnprtbParameter(0x1e28, &g_gnprtb1E28) && ok;
  ok = BindGnprtbParameter(0x1e29, &g_gnprtb1E29) && ok;
  ok = BindGnprtbParameter(0x1e2a, &g_gnprtb1E2A) && ok;
  ok = BindGnprtbParameter(0x1e2b, &g_gnprtb1E2B) && ok;
  ok = BindGnprtbParameter(0x1e2c, &g_gnprtb1E2C) && ok;
  ok = BindGnprtbParameter(0x1e2d, &g_gnprtb1E2D) && ok;
  ok = BindGnprtbParameter(0x1e2e, &g_gnprtb1E2E) && ok;
  ok = BindGnprtbParameter(0x1e2f, &g_gnprtb1E2F) && ok;
  ok = BindGnprtbParameter(0x1e30, &g_gnprtb1E30) && ok;
  ok = BindGnprtbParameter(0x1e31, &g_gnprtb1E31) && ok;
  ok = BindGnprtbParameter(0x1e32, &g_gnprtb1E32) && ok;
  ok = BindGnprtbParameter(0x1e33, &g_gnprtb1E33) && ok;
  ok = BindGnprtbParameter(0x1e34, &g_gnprtb1E34) && ok;
  ok = BindGnprtbParameter(0x1e35, &g_gnprtb1E35) && ok;
  ok = BindGnprtbParameter(0x1e36, &g_gnprtb1E36) && ok;
  ok = BindGnprtbParameter(0x1e37, &g_gnprtb1E37) && ok;
  ok = BindGnprtbParameter(0x1e38, &g_gnprtb1E38) && ok;
  ok = BindGnprtbParameter(0x1e39, &g_gnprtb1E39) && ok;
  ok = BindGnprtbParameter(0x1e50, &g_gnprtb1E50) && ok;
  ok = BindGnprtbParameter(0x1e51, &g_gnprtb1E51) && ok;
  ok = BindGnprtbParameter(0x1e52, &g_gnprtb1E52) && ok;
  ok = BindGnprtbParameter(0x1e53, &g_gnprtb1E53) && ok;
  ok = BindGnprtbParameter(0x1e54, &g_gnprtb1E54) && ok;
  ok = BindGnprtbParameter(0x1e55, &g_gnprtb1E55) && ok;
  ok = BindGnprtbParameter(0x1e56, &g_gnprtb1E56) && ok;
  ok = BindGnprtbParameter(0x1e57, &g_gnprtb1E57) && ok;
  ok = BindSdprtbParameter(0x1e00, 1, &g_sdprtb1E00Side1) && ok;
  ok = BindSdprtbParameter(0x1e00, 2, &g_sdprtb1E00Side2) && ok;
  ok = BindSdprtbParameter(0x1e01, 1, &g_sdprtb1E01Side1) && ok;
  ok = BindSdprtbParameter(0x1e01, 2, &g_sdprtb1E01Side2) && ok;
  ok = BindSdprtbParameter(0x1e02, 1, &g_sdprtb1E02Side1) && ok;
  ok = BindSdprtbParameter(0x1e02, 2, &g_sdprtb1E02Side2) && ok;
  ok = BindSdprtbParameter(0x1e03, 1, &g_sdprtb1E03Side1) && ok;
  ok = BindSdprtbParameter(0x1e03, 2, &g_sdprtb1E03Side2) && ok;
  ok = BindSdprtbParameter(0x1e04, 1, &g_sdprtb1E04Side1) && ok;
  ok = BindSdprtbParameter(0x1e04, 2, &g_sdprtb1E04Side2) && ok;
  ok = BindSdprtbParameter(0x1e05, 1, &g_sdprtb1E05Side1) && ok;
  ok = BindSdprtbParameter(0x1e05, 2, &g_sdprtb1E05Side2) && ok;
  return ok;
}

int RollSystemEnergy()
{
  const int roll = GameRandom(g_gnprtb1E2A);
  return Clamp(roll + g_gnprtb1E29, g_gnprtb1E21, g_gnprtb1E22);
}

int RollSystemRawMaterial(int cap)
{
  const int roll = GameRandom(g_gnprtb1E2C);
  return clampedRawMaterial(roll, g_gnprtb1E2B, cap);
}

int RollSystemPopulated()
{
  return PercentChance(g_gnprtb1E32);
}

int RollInitialLoyalty(int side, int kind)
{
  if (side == 1) {
    if (kind == 0) {
      return GameRandom(g_sdprtb1E03Side1) + g_sdprtb1E02Side1;
    }
    return GameRandom(g_sdprtb1E05Side1) + g_sdprtb1E04Side1;
  }
  if (side == 2) {
    if (kind == 0) {
      return 100 - (GameRandom(g_sdprtb1E03Side2) + g_sdprtb1E02Side2);
    }
    return 100 - (GameRandom(g_sdprtb1E05Side2) + g_sdprtb1E04Side2);
  }
  if (side == 3) {
    const int roll = GameRandom(g_gnprtb1E54);
    return roll + (50 - g_gnprtb1E54 / 2);
  }
  return 0;
}

int SideSharePercent(int side, int kind)
{
  if (side == 1) {
    return kind == 0 ? g_sdprtb1E00Side1 : g_sdprtb1E01Side1;
  }
  if (side == 2) {
    return kind == 0 ? g_sdprtb1E00Side2 : g_sdprtb1E01Side2;
  }
  if (side != 3) {
    return kind;
  }
  if (kind == 0) {
    return (((100 - g_sdprtb1E00Side1) - g_sdprtb1E00Side2) - g_sdprtb1E01Side1) - g_sdprtb1E01Side2;
  }
  return 0;
}

int RollSystemEnergyB()
{
  const int first = GameRandom(g_gnprtb1E2E);
  const int second = GameRandom(g_gnprtb1E2F);
  return Clamp(second + g_gnprtb1E2D + first, g_gnprtb1E21, g_gnprtb1E22);
}

int RollSystemRawMaterialB(int cap)
{
  const int roll = GameRandom(g_gnprtb1E31);
  return clampedRawMaterial(roll, g_gnprtb1E30, cap);
}

int RollSystemPopulatedB()
{
  return PercentChance(g_gnprtb1E33);
}

int RollCenteredPercent()
{
  const int roll = GameRandom(g_gnprtb1E55);
  return roll + (50 - g_gnprtb1E55 / 2);
}

int RollStartingUnitA(int a, int b, int* garrisoned, NullableKey* outKey)
{
  return rollStartingUnit(a, b, g_gnprtb1E56, kUnitTableA, garrisoned, outKey);
}

int RollStartingUnitB(int a, int b, int* garrisoned, NullableKey* outKey)
{
  return rollStartingUnit(a, b, g_gnprtb1E57, kUnitTableB, garrisoned, outKey);
}

int ScaleForEmpire(int value, int side, int flag)
{
  if (flag != 0 && side == 2) {
    return value * g_gnprtb1E00;
  }
  return value;
}

int SelectRandomChild(ChildIterator* it)
{
  int ok = 1;
  const int count = it->Count();
  if (count != 0) {
    const int steps = GameRandom(count - 1);
    it->Begin();
    for (int i = 0; it->m_current != nullptr && i < steps; ++i) {
      it->Advance();
    }
    ok = it->m_current != nullptr;
  }
  return ok;
}

int DampenFavourableShift(int value, int side, int flag)
{
  if (flag != 0 && ((side == 1 && value > 0) || (side == 2 && value < 0))) {
    return value / g_gnprtb1E01;
  }
  return value;
}

int ScaleByRatio1E53(int value)
{
  if (value == 0) {
    value = 1;
  }
  return ScaleRatio(g_gnprtb1E53, 100, value);
}

int ComputeUprisingPercent(int side, int loyalty, int scaleFlag, int a, int b, int c, int suppressed)
{
  int percent = 0;
  const int scaled = ScaleForEmpire(c, side, scaleFlag);
  if (loyalty <= g_gnprtb1E34 && suppressed == 0) {
    int base = g_gnprtb1E39;
    if (g_gnprtb1E38 < loyalty) {
      base = g_gnprtb1E35;
      if (loyalty <= g_gnprtb1E36) {
        base = g_gnprtb1E37;
      }
    }
    percent = ClampPercent(((base - a * g_gnprtb1E06) - b * g_gnprtb1E07) - scaled * g_gnprtb1E08);
  }
  if (side == 2) {
    percent = -percent;
  }
  return percent;
}

void RollUprisingIncident(int side, int shortfallBase, int scaleFlag, int a, int b, int c, int d, int* outA, int* outB)
{
  *outA = 0;
  *outB = 0;
  const int first = GameRandom(g_gnprtb1E1C) + g_gnprtb1E1B;
  const int second = GameRandom(g_gnprtb1E1C) + g_gnprtb1E1B;
  const int multiplier = (scaleFlag == 0 || side != 2) ? 1 : g_gnprtb1E00;
  const int shortfall = ComputeShortfallUnits(shortfallBase);
  const int total = first + second + (shortfall - multiplier * a) + ((d + c) - b);
  int value;
  if (LookupTableValue(0x28, total, &value)) {
    *outA = value;
    if (LookupTableValue(0x29, total, &value)) {
      *outB = value;
    }
  }
}

int RollInformantIncident(int* outValue)
{
  *outValue = 0;
  const int roll = GameRandom(g_gnprtb1E1E) + g_gnprtb1E1D;
  int value;
  const int found = LookupTableValue(0x2a, roll, &value);
  if (found) {
    *outValue = value;
  }
  return found;
}

int RollSupportChance(int* success)
{
  *success = PercentChance(g_gnprtb1E24);
  return 1;
}

int ResolveMutualLosses(int* a, int* b)
{
  const int rounds = MaxInt(*a, *b);
  int lossesA = 0;
  int lossesB = 0;
  for (int i = 0; i < rounds; ++i) {
    if (i < *a) {
      if (PercentChance((((*b - lossesA) - lossesB) + *a) * g_gnprtb1E23)) {
        ++lossesA;
      }
    }
    if (i < *b) {
      if (PercentChance((((*a - lossesA) - lossesB) + *b) * g_gnprtb1E23)) {
        ++lossesB;
      }
    }
  }
  if (lossesA == 0 && lossesB == 0) {
    if (*a == 0) {
      if (*b != 0) {
        lossesB = 1;
      }
    } else {
      lossesA = 1;
    }
  }
  *a = *a - lossesA;
  *b = *b - lossesB;
  if (*b < *a) {
    *a = *b;
  }
  return 1;
}

int ApplyRandomCounterShift(int* a, int* b, int flagA, int flagB)
{
  int outcome;
  const int found = LookupTableValue(0x2b, RollPercent(), &outcome);
  if (!found) {
    return found;
  }
  switch (outcome) {
    case 1:
      if (flagA != 0 && g_gnprtb1E1F < *a) {
        --*a;
      }
      break;
    case 2:
      if (flagB != 0 && g_gnprtb1E21 < *b) {
        --*b;
      }
      break;
    case 3:
      if (*a < g_gnprtb1E20 && *a < *b) {
        ++*a;
      }
      break;
    case 4:
      if (*b < g_gnprtb1E22) {
        ++*b;
      }
      break;
  }
  return found;
}

int RollFailure(int percent, int* result)
{
  *result = !PercentChance(percent);
  return 1;
}

int ComputeRemainingSupport(int side, int x, int flag1, int flag2, int flag3, int total)
{
  return total - ComputeSupportCost(side, x, flag1, flag2, flag3);
}

int ComputeSupportCost(int side, int x, int flag1, int flag2, int flag3)
{
  int cost = ComputeShortfallUnits(x);
  if (flag1 != 0 && side == 2) {
    cost = cost / g_gnprtb1E00;
  }
  if (flag2 != 0 && flag3 != 0) {
    cost = cost * g_gnprtb1E02;
  }
  return cost;
}

int ComputeResidualPercent(int a, int b)
{
  return ClampNegativeToZero((100 - a * g_gnprtb1E04) - b * g_gnprtb1E05);
}

int ComputeShortfallUnits(int x)
{
  int units = 0;
  if (x < g_gnprtb1E51) {
    units = DivideRoundUp(g_gnprtb1E51 - x, -g_gnprtb1E52);
  }
  return units;
}

int ClassifyPercent(int x)
{
  if (g_gnprtb1E50 <= x) {
    return 1;
  }
  return (100 - x < g_gnprtb1E50) + 2;
}

int GetParameter1E03()
{
  return g_gnprtb1E03;
}

int ThresholdBonus(int x)
{
  return x >= g_gnprtb1E0A ? 0 : g_gnprtb1E09;
}

int GetParameter1E19()
{
  return g_gnprtb1E19;
}

int SignedParameter1E1A(int x)
{
  return x == 0 ? -g_gnprtb1E1A : g_gnprtb1E1A;
}
