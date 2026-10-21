#include "rules/loyalty_params.h"
#include "data/seed_table_entries.h"

#include "data/game_math.h"
#include "data/gnprtb.h"
#include "data/probability_table.h"
#include "data/random.h"

// The parameter values live in the original image during the hybrid phase (extern declarations only).

namespace
{

constexpr int kBothSides = 3;
constexpr int kReserveTableAlliance = 3;
constexpr int kReserveTableEmpire = 4;
constexpr int kReserveRollLow = 1;
constexpr int kReserveRollHigh = 100;
constexpr uint32_t kCoreFamiliesFirst = 0x90;
constexpr uint32_t kCoreFamiliesEnd = 0x92;
constexpr uint32_t kOuterFamiliesEnd = 0x94;
constexpr uint32_t kSystemFamiliesFirst = 0x90;
constexpr uint32_t kSystemFamiliesEnd = 0x98;
constexpr uint32_t kControllerMask = 3;
constexpr uint32_t kGarrisonMask = 0x30;

int isSideNumber(int side)
{
  return side >= 1 && side <= 3;
}

// The side that is not `side`; "both" stays both.
int otherSide(int side)
{
  return side == kBothSides ? kBothSides : 2 - (side != 1);
}

void resetRule(LoyaltyRule* rule)
{
  LoyaltyRule fresh;
  *rule = fresh;
}

int pickReserveUnits(int tableType, int* count, ObjectRefList* list)
{
  TableEntry* record = FindTableRecord(tableType, GameRandomRange(kReserveRollLow, kReserveRollHigh));
  int result = 0;
  if (record != nullptr) {
    *count = TableRecordFirstValue(record);
    *list = static_cast<SeedFamilyTableEntry*>(record)->m_items;
    result = list->m_ok;
  }
  return result;
}

int getRows(int type, int firstRow, int lastRow, int* tableType, int* first, int* last)
{
  *tableType = type;
  *first = firstRow;
  *last = lastRow;
  return 1;
}

int scaleReserveBudget(int value, int galaxySize, int* out, int percentSize1, int percentSize2, int percentSize3)
{
  *out = 0;
  const int valid = galaxySize >= 1 && galaxySize <= 3;
  if (valid) {
    int percent;
    if (galaxySize == 1) {
      percent = percentSize1;
    } else if (galaxySize == 2) {
      percent = percentSize2;
    } else {
      percent = percentSize3;
    }
    *out = ScalePercent(value, percent);
  }
  return valid;
}

// The sides a pair of side fields (1, 2 or 3 = both) is resolved to: "both" takes the side the other one is not.
void resolveSides(uint32_t* from, uint32_t* to)
{
  if (*from == kBothSides) {
    *from = (*to == *from) ? kBothSides : 2 - (*to != 1);
  }
  if (*to == kBothSides) {
    *to = (*from == *to) ? kBothSides : 2 - (*from != 1);
  }
}

} // namespace

void BindGnprtb14()
{
  BindGnprtbParameter(0x1400, &g_gnprtb1400);
  BindGnprtbParameter(0x1401, &g_gnprtb1401);
  BindGnprtbParameter(0x1402, &g_gnprtb1402);
  BindGnprtbParameter(0x1403, &g_gnprtb1403);
  BindGnprtbParameter(0x1404, &g_gnprtb1404);
  BindGnprtbParameter(0x1405, &g_gnprtb1405);
  BindGnprtbParameter(0x1406, &g_gnprtb1406);
  BindGnprtbParameter(0x1407, &g_gnprtb1407);
  BindGnprtbParameter(0x1408, &g_gnprtb1408);
  BindGnprtbParameter(0x1409, &g_gnprtb1409);
  BindGnprtbParameter(0x140a, &g_gnprtb140A);
  BindGnprtbParameter(0x140b, &g_gnprtb140B);
  BindGnprtbParameter(0x140c, &g_gnprtb140C);
  BindGnprtbParameter(0x140d, &g_gnprtb140D);
  BindGnprtbParameter(0x140e, &g_gnprtb140E);
  BindGnprtbParameter(0x140f, &g_gnprtb140F);
  BindGnprtbParameter(0x1410, &g_gnprtb1410);
  BindGnprtbParameter(0x1411, &g_gnprtb1411);
  BindGnprtbParameter(0x1412, &g_gnprtb1412);
  BindGnprtbParameter(0x1413, &g_gnprtb1413);
  BindGnprtbParameter(0x1414, &g_gnprtb1414);
  BindGnprtbParameter(0x1415, &g_gnprtb1415);
  BindGnprtbParameter(0x1416, &g_gnprtb1416);
  BindGnprtbParameter(0x1417, &g_gnprtb1417);
  BindGnprtbParameter(0x1420, &g_gnprtb1420);
  BindSdprtbParameter(0x1400, 1, &g_sdprtb1400Side1);
  BindSdprtbParameter(0x1400, 2, &g_sdprtb1400Side2);
  BindSdprtbParameter(0x1401, 1, &g_sdprtb1401Side1);
  BindSdprtbParameter(0x1401, 2, &g_sdprtb1401Side2);
  BindSdprtbParameter(0x1402, 1, &g_sdprtb1402Side1);
  BindSdprtbParameter(0x1402, 2, &g_sdprtb1402Side2);
  BindSdprtbParameter(0x1408, 1, &g_sdprtb1408Side1);
  BindSdprtbParameter(0x1408, 2, &g_sdprtb1408Side2);
  BindSdprtbParameter(0x1409, 1, &g_sdprtb1409Side1);
  BindSdprtbParameter(0x1409, 2, &g_sdprtb1409Side2);
  BindSdprtbParameter(0x140a, 1, &g_sdprtb140ASide1);
  BindSdprtbParameter(0x140a, 2, &g_sdprtb140ASide2);
  BindSdprtbParameter(0x140b, 1, &g_sdprtb140BSide1);
  BindSdprtbParameter(0x140b, 2, &g_sdprtb140BSide2);
  BindSdprtbParameter(0x1410, 1, &g_sdprtb1410Side1);
  BindSdprtbParameter(0x1410, 2, &g_sdprtb1410Side2);
  BindSdprtbParameter(0x1411, 1, &g_sdprtb1411Side1);
  BindSdprtbParameter(0x1411, 2, &g_sdprtb1411Side2);
  BindSdprtbParameter(0x1412, 1, &g_sdprtb1412Side1);
  BindSdprtbParameter(0x1412, 2, &g_sdprtb1412Side2);
  BindSdprtbParameter(0x1413, 1, &g_sdprtb1413Side1);
  BindSdprtbParameter(0x1413, 2, &g_sdprtb1413Side2);
  BindSdprtbParameter(0x1414, 1, &g_sdprtb1414Side1);
  BindSdprtbParameter(0x1414, 2, &g_sdprtb1414Side2);
  BindSdprtbParameter(0x1415, 1, &g_sdprtb1415Side1);
  BindSdprtbParameter(0x1415, 2, &g_sdprtb1415Side2);
  BindSdprtbParameter(0x1420, 1, &g_sdprtb1420Side1);
  BindSdprtbParameter(0x1420, 2, &g_sdprtb1420Side2);
  BindSdprtbParameter(0x1421, 1, &g_sdprtb1421Side1);
  BindSdprtbParameter(0x1421, 2, &g_sdprtb1421Side2);
  BindSdprtbParameter(0x1422, 1, &g_sdprtb1422Side1);
  BindSdprtbParameter(0x1422, 2, &g_sdprtb1422Side2);
  BindSdprtbParameter(0x1423, 1, &g_sdprtb1423Side1);
  BindSdprtbParameter(0x1423, 2, &g_sdprtb1423Side2);
  BindSdprtbParameter(0x1430, 1, &g_sdprtb1430Side1);
  BindSdprtbParameter(0x1430, 2, &g_sdprtb1430Side2);
  BindSdprtbParameter(0x1431, 1, &g_sdprtb1431Side1);
  BindSdprtbParameter(0x1431, 2, &g_sdprtb1431Side2);
  BindSdprtbParameter(0x1432, 1, &g_sdprtb1432Side1);
  BindSdprtbParameter(0x1432, 2, &g_sdprtb1432Side2);
  BindSdprtbParameter(0x1440, 1, &g_sdprtb1440Side1);
  BindSdprtbParameter(0x1440, 2, &g_sdprtb1440Side2);
  BindSdprtbParameter(0x1441, 1, &g_sdprtb1441Side1);
  BindSdprtbParameter(0x1441, 2, &g_sdprtb1441Side2);
  BindSdprtbParameter(0x1442, 1, &g_sdprtb1442Side1);
  BindSdprtbParameter(0x1442, 2, &g_sdprtb1442Side2);
  BindSdprtbParameter(0x1443, 1, &g_sdprtb1443Side1);
  BindSdprtbParameter(0x1443, 2, &g_sdprtb1443Side2);
  BindSdprtbParameter(0x1444, 1, &g_sdprtb1444Side1);
  BindSdprtbParameter(0x1444, 2, &g_sdprtb1444Side2);
  BindSdprtbParameter(0x1445, 1, &g_sdprtb1445Side1);
  BindSdprtbParameter(0x1445, 2, &g_sdprtb1445Side2);
}

// ---- new-game seeding parameters --------------------------------------------------------------------------------

int RollStartingFleetAtYavin(int* atYavin)
{
  *atYavin = PercentChance(g_gnprtb1409);
  return 1;
}

int GetYavinForceRows(int* tableType, int* first, int* last)
{
  return getRows(0x50, g_gnprtb140A, g_gnprtb140B, tableType, first, last);
}

int GetKey154ForceRows(int* tableType, int* first, int* last)
{
  return getRows(0x51, g_gnprtb140C, g_gnprtb140D, tableType, first, last);
}

int GetCoruscantForceRows(int* tableType, int* first, int* last)
{
  return getRows(0x52, g_gnprtb140E, g_gnprtb140F, tableType, first, last);
}

int GetStartingFleetRows(int* tableType, int* first, int* last)
{
  return getRows(0x53, g_gnprtb1410, g_gnprtb1411, tableType, first, last);
}

int GetCoruscantFleetRows(int* tableType, int* first, int* last)
{
  return getRows(0x54, g_gnprtb1412, g_gnprtb1413, tableType, first, last);
}

int GetCoreSystemRows(int* tableType, int* first, int* last)
{
  return getRows(0x55, g_gnprtb1414, g_gnprtb1415, tableType, first, last);
}

int GetCoruscantSystemRows(int* tableType, int* first, int* last)
{
  return getRows(0x56, g_gnprtb1416, g_gnprtb1417, tableType, first, last);
}

int ScaleAllianceReserveBudget(int value, int galaxySize, int* out)
{
  return scaleReserveBudget(value, galaxySize, out, g_sdprtb1430Side1, g_sdprtb1431Side1, g_sdprtb1432Side1);
}

int ScaleEmpireReserveBudget(int value, int galaxySize, int* out)
{
  return scaleReserveBudget(value, galaxySize, out, g_sdprtb1430Side2, g_sdprtb1431Side2, g_sdprtb1432Side2);
}

int PickAllianceReserveUnits(int* count, ObjectRefList* list)
{
  return pickReserveUnits(kReserveTableAlliance, count, list);
}

int PickEmpireReserveUnits(int* count, ObjectRefList* list)
{
  return pickReserveUnits(kReserveTableEmpire, count, list);
}

// ---- distances and travel time ----------------------------------------------------------------------------------

int GetStartingCharacterCount(int side, int galaxySize, int* count)
{
  *count = 0;
  if (side == 1) {
    if (galaxySize == 1) {
      *count = g_sdprtb1400Side1;
    } else if (galaxySize == 2) {
      *count = g_sdprtb1401Side1;
    } else if (galaxySize == 3) {
      *count = g_sdprtb1402Side1;
    }
  } else if (side == 2) {
    if (galaxySize == 1) {
      *count = g_sdprtb1400Side2;
    } else if (galaxySize == 2) {
      *count = g_sdprtb1401Side2;
    } else if (galaxySize == 3) {
      *count = g_sdprtb1402Side2;
    }
  }
  return 1;
}

int MapDistance(const MapPosition* from, const MapPosition* to)
{
  const int64_t dx = static_cast<int64_t>(to->m_x) - from->m_x;
  const int64_t dy = static_cast<int64_t>(to->m_y) - from->m_y;
  const auto squaredDistance = static_cast<uint64_t>(dx * dx + dy * dy);
  return static_cast<int>(IntegerSqrt(squaredDistance));
}

int TravelDays(const MapPosition* from, const MapPosition* to)
{
  return TravelDaysAtSpeed(from, to, g_gnprtbDefault);
}

int TravelDaysAtSpeed(const MapPosition* from, const MapPosition* to, int speed)
{
  int days = 0;
  const int distance = MapDistance(from, to);
  if (distance != 0) {
    days = ScalePercent(IntDivide(distance, g_gnprtb1400), speed);
    if (days == 0) {
      days = 1;
    }
  }
  return days;
}

// ---- loyalty rules ----------------------------------------------------------------------------------------------

int BuildCaptureRule(int side, int inner, LoyaltyRule* rule)
{
  resetRule(rule);
  int built = isSideNumber(side);
  if (built) {
    built = side != kBothSides;
  }
  if (!built) {
    return built;
  }
  rule->m_controlSide = 0;
  if (inner != 0) {
    rule->SetFamilyRange(kCoreFamiliesFirst, kCoreFamiliesEnd);
  } else {
    rule->SetFamilyRange(kCoreFamiliesEnd, kOuterFamiliesEnd);
  }
  rule->m_unitSide = side;
  rule->m_direction = side;
  int threshold;
  int amount;
  if (inner != 0) {
    threshold = side == 1 ? g_sdprtb140BSide1 : g_sdprtb140BSide2;
    amount = side == 1 ? g_sdprtb140ASide1 : g_sdprtb140ASide2;
  } else {
    threshold = side == 1 ? g_sdprtb1409Side1 : g_sdprtb1409Side2;
    amount = side == 1 ? g_sdprtb1408Side1 : g_sdprtb1408Side2;
  }
  rule->m_threshold = threshold;
  rule->m_amount = amount;
  return built;
}

int BuildCaptureRuleAlt(int side, int inner, LoyaltyRule* rule)
{
  resetRule(rule);
  int built = isSideNumber(side);
  if (built) {
    built = side != kBothSides;
  }
  if (!built) {
    return built;
  }
  rule->m_controlSide = 0;
  if (inner != 0) {
    rule->SetFamilyRange(kCoreFamiliesFirst, kCoreFamiliesEnd);
  } else {
    rule->SetFamilyRange(kCoreFamiliesEnd, kOuterFamiliesEnd);
  }
  rule->m_unitSide = side;
  rule->m_direction = side;
  rule->m_threshold = inner != 0 ? g_gnprtb1404 : g_gnprtb1402;
  rule->m_amount = inner != 0 ? g_gnprtb1403 : g_gnprtb1401;
  return built;
}

int BuildStrengthImbalanceRule(int totalSide1, int totalSide2, int side, int valueSide1, int valueSide2,
                               LoyaltyRule* rule)
{
  resetRule(rule);
  int built = isSideNumber(side);
  if (built) {
    built = side != kBothSides;
  }
  if (!built) {
    return built;
  }
  rule->m_controlSide = 0;
  rule->SetFamilyRange(kSystemFamiliesFirst, kSystemFamiliesEnd);

  const int side1Larger = (totalSide1 <= totalSide2) + 1;
  const int larger = side1Larger == 1 ? totalSide1 : totalSide2;
  const int smaller = side1Larger == 1 ? totalSide2 : totalSide1;
  int baseLarger;
  int baseSmaller;
  int ratioLarger;
  int ratioSmaller;
  int largerValue;
  int smallerValue;
  if (side1Larger == 1) {
    baseLarger = g_sdprtb1412Side1;
    baseSmaller = g_sdprtb1414Side2;
    ratioLarger = g_sdprtb1413Side1;
    ratioSmaller = g_sdprtb1415Side2;
    largerValue = valueSide1;
    smallerValue = valueSide2;
  } else {
    baseLarger = g_sdprtb1412Side2;
    baseSmaller = g_sdprtb1414Side1;
    ratioLarger = g_sdprtb1413Side2;
    ratioSmaller = g_sdprtb1415Side1;
    largerValue = valueSide2;
    smallerValue = valueSide1;
  }

  int largerShare = 0;
  int smallerShare = 0;
  if (larger != 0) {
    const int difference = larger - smaller;
    largerShare = ScaleRatio(ratioLarger, difference, larger) + baseLarger;
    smallerShare = ratioSmaller + baseSmaller - ScaleRatio(ratioSmaller, difference, larger);
  }
  const int scaledLarger = ScalePercent(largerValue, largerShare);
  const int scaledSmaller = ScalePercent(smallerValue, smallerShare);

  int direction = side;
  int balance;
  if (side1Larger == direction) {
    balance = 2 * scaledSmaller - scaledLarger;
  } else {
    balance = 2 * scaledLarger - scaledSmaller;
  }
  int amount = balance / g_gnprtb1420;
  if (amount < 0) {
    amount = -amount;
    direction = otherSide(direction);
  }
  const int limit = direction == 1 ? g_sdprtb1411Side1 : g_sdprtb1411Side2;
  amount = Clamp(amount, g_percentMin, limit);
  rule->m_direction = direction;
  rule->m_amount = amount;
  const int unitSide = otherSide(direction);
  rule->m_unitSide = unitSide;
  rule->m_threshold = unitSide == 1 ? g_sdprtb1410Side1 : g_sdprtb1410Side2;
  return built;
}

int BuildControlChangeRule(SideBits* from, SideBits* to, LoyaltyRule* rule)
{
  resetRule(rule);
  if (((from->m_value ^ to->m_value) & kControllerMask) != 0) {
    uint32_t fromSide = from->m_value & kControllerMask;
    uint32_t toSide = to->m_value & kControllerMask;
    resolveSides(&fromSide, &toSide);
    rule->m_controlSide = 0;
    rule->SetFamilyRange(kSystemFamiliesFirst, kSystemFamiliesEnd);
    rule->m_unitSide = fromSide;
    rule->m_direction = toSide;
    int amount = g_percentMin;
    int threshold;
    const uint32_t fromBits = from->m_value;
    const uint32_t toBits = to->m_value;
    if ((fromBits & kGarrisonMask) == 0) {
      if ((toBits & kGarrisonMask) == 0) {
        threshold = toSide == 1 ? g_sdprtb1421Side1 : g_sdprtb1421Side2;
        const uint32_t fromController = fromBits & kControllerMask;
        if (fromController == 1 || (toBits & kControllerMask) == 1) {
          amount += g_sdprtb1420Side1;
        }
        if (fromController == 2 || (toBits & kControllerMask) == 2) {
          amount += g_sdprtb1420Side2;
        }
      } else {
        threshold = toSide == 1 ? g_sdprtb1441Side1 : g_sdprtb1441Side2;
        amount = toSide == 1 ? g_sdprtb1440Side1 : g_sdprtb1440Side2;
      }
    } else if ((toBits & kGarrisonMask) == 0) {
      threshold = toSide == 1 ? g_sdprtb1443Side1 : g_sdprtb1443Side2;
      amount = toSide == 1 ? g_sdprtb1442Side1 : g_sdprtb1442Side2;
    } else {
      threshold = toSide == 1 ? g_sdprtb1445Side1 : g_sdprtb1445Side2;
      amount = toSide == 1 ? g_sdprtb1444Side1 : g_sdprtb1444Side2;
    }
    rule->m_threshold = threshold;
    rule->m_amount = amount;
  }
  return 1;
}

int BuildControlChangeRuleSimple(SideBits* from, SideBits* to, LoyaltyRule* rule)
{
  resetRule(rule);
  if (((from->m_value ^ to->m_value) & kControllerMask) != 0) {
    uint32_t fromSide = from->m_value & kControllerMask;
    uint32_t toSide = to->m_value & kControllerMask;
    resolveSides(&fromSide, &toSide);
    rule->m_controlSide = 0;
    rule->SetFamilyRange(kSystemFamiliesFirst, kSystemFamiliesEnd);
    rule->m_unitSide = fromSide;
    rule->m_direction = toSide;
    int amount = g_percentMin;
    const int threshold = toSide == 1 ? g_sdprtb1423Side1 : g_sdprtb1423Side2;
    const uint32_t fromController = from->m_value & kControllerMask;
    if (fromController == 1 || (to->m_value & kControllerMask) == 1) {
      amount += g_sdprtb1422Side1;
    }
    if (fromController == 2 || (to->m_value & kControllerMask) == 2) {
      amount += g_sdprtb1422Side2;
    }
    rule->m_threshold = threshold;
    rule->m_amount = amount;
  }
  return 1;
}

int BuildEnrouteRule(LoyaltyRule* rule)
{
  resetRule(rule);
  rule->m_controlSide = 1;
  rule->SetFamilyRange(kSystemFamiliesFirst, kSystemFamiliesEnd);
  rule->m_unitSide = 1;
  rule->m_direction = 1;
  rule->m_threshold = g_gnprtb1406;
  rule->m_amount = g_gnprtb1405;
  return 1;
}

int BuildDepartureRule(LoyaltyRule* rule)
{
  resetRule(rule);
  rule->m_controlSide = 0;
  rule->SetFamilyRange(kSystemFamiliesFirst, kSystemFamiliesEnd);
  rule->m_unitSide = 1;
  rule->m_threshold = g_gnprtb1408;
  rule->m_direction = 1;
  rule->m_amount = g_gnprtb1407;
  return 1;
}
