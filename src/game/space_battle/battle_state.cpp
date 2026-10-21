#include "space_battle/battle_state.h"

#include <algorithm>

#include "space_battle/doctrine.h"
#include "space_battle/tuning.h"

namespace reb::battle
{

Side Opponent(Side side)
{
  return side == Side::Alliance ? Side::Empire : Side::Alliance;
}

int SideIndex(Side side)
{
  return static_cast<int>(side);
}

bool BattleState::Present(UnitIndex unit) const
{
  if (!Valid(unit)) {
    return false;
  }
  const UnitState& state = units[unit];
  if (state.inSpace) {
    return true;
  }
  if (state.kind != UnitKind::Squadron || state.fate != UnitFate::Active) {
    return false;
  }
  const SquadronState& squadron = squadrons[state.slot];
  return squadron.place == SquadronPlace::Docked && (squadron.carrier == kNoUnit || InSpace(squadron.carrier));
}

const ShipState* BattleState::Ship(UnitIndex unit) const
{
  return Valid(unit) && units[unit].kind == UnitKind::CapitalShip ? &ships[units[unit].slot] : nullptr;
}

ShipState* BattleState::Ship(UnitIndex unit)
{
  return Valid(unit) && units[unit].kind == UnitKind::CapitalShip ? &ships[units[unit].slot] : nullptr;
}

const SquadronState* BattleState::Squadron(UnitIndex unit) const
{
  return Valid(unit) && units[unit].kind == UnitKind::Squadron ? &squadrons[units[unit].slot] : nullptr;
}

SquadronState* BattleState::Squadron(UnitIndex unit)
{
  return Valid(unit) && units[unit].kind == UnitKind::Squadron ? &squadrons[units[unit].slot] : nullptr;
}

const StationState* BattleState::Station(UnitIndex unit) const
{
  return Valid(unit) && units[unit].kind == UnitKind::DeathStar ? &stations[units[unit].slot] : nullptr;
}

const GroupState* BattleState::Group(GroupId id) const
{
  return id >= 0 && id < static_cast<GroupId>(groups.size()) ? &groups[id] : nullptr;
}

GroupState* BattleState::Group(GroupId id)
{
  return id >= 0 && id < static_cast<GroupId>(groups.size()) ? &groups[id] : nullptr;
}

ShipDoctrine BattleState::EffectiveShipDoctrine(UnitIndex unit) const
{
  const UnitState& state = units[unit];
  ShipDoctrine doctrine;
  if (const GroupState* group = Group(state.group)) {
    doctrine = group->shipDoctrine;
  }
  ApplyShipFields(doctrine, state.shipDoctrine, state.overrides);
  return doctrine;
}

FlightDoctrine BattleState::EffectiveFlightDoctrine(UnitIndex unit) const
{
  const UnitState& state = units[unit];
  FlightDoctrine doctrine;
  if (const GroupState* group = Group(state.group)) {
    doctrine = group->flightDoctrine;
  }
  ApplyFlightFields(doctrine, state.flightDoctrine, state.overrides);
  return doctrine;
}

const std::vector<UnitOrder>& BattleState::ActiveOrders(UnitIndex unit) const
{
  const UnitState& state = units[unit];
  if (!state.orders.empty()) {
    return state.orders;
  }
  if (const GroupState* group = Group(state.group)) {
    return group->orders;
  }
  return state.orders;
}

bool BattleState::Detached(UnitIndex unit) const
{
  constexpr DoctrineOverrideMask kMovement
      = kShipRole | kShipRange | kShipApproach | kShipFormation | kShipBreakFormation | kShipScreenTarget;
  const UnitState& state = units[unit];
  return !state.orders.empty() || Group(state.group) == nullptr
         || (state.kind == UnitKind::CapitalShip && (state.overrides & kMovement) != 0);
}

float BattleState::HullFraction(UnitIndex unit) const
{
  if (const ShipState* ship = Ship(unit)) {
    return std::clamp(ship->hull / profiles[unit].hullMax, 0.0f, 1.0f);
  }
  if (const SquadronState* squadron = Squadron(unit)) {
    return squadron->craftCount > 0 ? static_cast<float>(squadron->alive) / static_cast<float>(squadron->craftCount)
                                    : 0.0f;
  }
  if (const StationState* station = Station(unit)) {
    return std::clamp(station->hull / profiles[unit].hullMax, 0.0f, 1.0f);
  }
  return 0.0f;
}

float BattleState::ShieldFraction(UnitIndex unit) const
{
  const UnitProfile& profile = profiles[unit];
  if (const ShipState* ship = Ship(unit)) {
    return profile.shieldMax > 0.0f ? std::clamp(ship->shield / profile.shieldMax, 0.0f, 1.0f) : 0.0f;
  }
  if (const SquadronState* squadron = Squadron(unit)) {
    if (profile.craftShield <= 0.0f || squadron->alive == 0) {
      return 0.0f;
    }
    float total = 0.0f;
    for (int i = 0; i < squadron->craftCount; ++i) {
      const CraftState& craft = this->craft[squadron->firstCraft + i];
      total += craft.alive ? craft.shield : 0.0f;
    }
    return total / (profile.craftShield * static_cast<float>(squadron->alive));
  }
  return 1.0f;
}

bool BattleState::Withdrawing(UnitIndex unit) const
{
  if (const ShipState* ship = Ship(unit)) {
    return ship->withdrawing;
  }
  if (const SquadronState* squadron = Squadron(unit)) {
    return squadron->withdrawing;
  }
  if (const StationState* station = Station(unit)) {
    return station->withdrawing;
  }
  return false;
}

uint32_t BattleState::Tags(UnitIndex unit) const
{
  uint32_t tags = profiles[unit].staticTags | TagBit(TargetTag::Nearest);
  if (ShieldFraction(unit) <= 0.0f) {
    tags |= TagBit(TargetTag::ShieldsDown);
  }
  if (HullFraction(unit) < tuning::kDamagedHullFraction) {
    tags |= TagBit(TargetTag::Damaged);
  }
  if (Withdrawing(unit)) {
    tags |= TagBit(TargetTag::Withdrawing);
  }
  if (const ShipState* ship = Ship(unit); ship && ship->tractorHeld > 0.0f) {
    tags |= TagBit(TargetTag::Tractored);
  }
  return tags;
}

float BattleState::AdmiralTurnTerm(Side side) const
{
  const float rating = sides[SideIndex(side)].commandRating;
  return tuning::kAdmiralTurnTermMin + (tuning::kAdmiralTurnTermMax - tuning::kAdmiralTurnTermMin) * rating;
}

float BattleState::TransitionScale(Side side) const
{
  return 1.0f - tuning::kAdmiralTransitionReduction * sides[SideIndex(side)].commandRating;
}

} // namespace reb::battle
