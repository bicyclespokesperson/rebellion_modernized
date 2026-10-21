#pragma once

#include <algorithm>

namespace ai
{

constexpr int RequiredOccupationTroops(int systemRequirement)
{
  return std::clamp(systemRequirement + 2, 4, 16);
}

constexpr bool CanLaunchOccupation(int fleetTroops, int systemRequirement, bool groundSuperiority)
{
  return fleetTroops >= RequiredOccupationTroops(systemRequirement) && groundSuperiority;
}

} // namespace ai
