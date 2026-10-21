#pragma once
#include <cstdint>
#include <string>
#include <string_view>

#include "space_battle/view_model.h"

// Shared battle text and screen labels, with class/ordinal fallbacks for empty or number-only names.
namespace reb::battle
{

// The class name for a snapshot ship-type index; empty when unknown.
// Indices 8, 13 and 36 identify the Corellian Gunship, Liberator Cruiser and TIE Defender.
std::string_view ClassName(int32_t typeIndex);

struct UnitNaming {
  std::string name;      // what labels show
  std::string className; // from the type index; else the detail's (the simulator reports the archetype there)
  bool ownName = false;  // `name` is the unit's own, not made from its class
};

// `ordinal` counts the units of the same class and side, from 1, in unit order.
UnitNaming NameUnit(const ViewUnit& unit, const ViewUnitDetail& detail, int ordinal);

// Counts ordinals in snapshot order, independently for each side and class.
UnitNaming NameUnit(const BattleSnapshot& snapshot, UnitIndex unit, std::string_view fallbackClassName = {});

} // namespace reb::battle
