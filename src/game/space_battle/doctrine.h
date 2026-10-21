#pragma once
#include <cstdint>

#include "space_battle/commands.h"
#include "space_battle/ship_profile.h"

// Doctrine defaults by archetype (tactical_combat.md §7.2) and the per-unit override masks of §5.1.
namespace reb::battle
{

// Every field of each doctrine, as override-mask bits (commands.h names them one by one).
inline constexpr DoctrineOverrideMask kAllShipFields = (kShipScreenTarget << 1) - 1;
inline constexpr DoctrineOverrideMask kAllFlightFields = (kFlightPatrolRadius << 1) - 1;

// Copies the fields selected by `mask` from `from` into `into`.
void ApplyShipFields(ShipDoctrine& into, const ShipDoctrine& from, DoctrineOverrideMask mask);
void ApplyFlightFields(FlightDoctrine& into, const FlightDoctrine& from, DoctrineOverrideMask mask);

// `ordinal` alternates the hook side between broadside groups of one side.
ShipDoctrine DefaultShipDoctrine(Archetype archetype, int ordinal);
FlightDoctrine DefaultFlightDoctrine(Archetype archetype);

TargetPriorities MakePriorities(std::initializer_list<TargetTag> tags, bool finishDamaged);

} // namespace reb::battle
