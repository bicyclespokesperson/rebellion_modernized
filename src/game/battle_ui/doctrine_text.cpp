#include "battle_ui/doctrine_text.h"

#include <algorithm>
#include <array>

namespace reb::battle_ui
{
namespace
{

using battle::FlightDoctrine;
using battle::ShipDoctrine;

constexpr std::array kRoles{
    OptionText{"Line", "Line", "Fight in the battle line, engaging enemy capital ships at the chosen range."},
    OptionText{"Strike", "Strike", "Drive at the priority targets and accept exposure to reach them."},
    OptionText{"Flank", "Flank", "Work round the enemy's side to bring guns to bear where its own are weakest."},
    OptionText{"Screen", "Screen", "Stay between the threat and the escorted ship and cover it with point defence."},
    OptionText{"Carrier", "Carrier", "Stand off out of reach and keep launching and recovering squadrons."},
    OptionText{"Picket", "Picket", "Hold an outlying station, such as the enemy's escape side."},
    OptionText{"Reserve", "Reserve", "Hold back out of the fight until committed."},
};
constexpr std::array kRanges{
    OptionText{"Close", "Close", "Close in so that every weapon fires, lasers included."},
    OptionText{"Optimal", "Optimal", "Hold the range at which this group's own mix of batteries does the most damage."},
    OptionText{"Stand-off", "Stand-off", "Stay outside the enemy's best range; needs a speed advantage."},
};
constexpr std::array kApproaches{
    OptionText{"Direct", "Direct", "Head straight for the target."},
    OptionText{"Left Hook", "Left Hook", "Swing round the target group's left flank before engaging."},
    OptionText{"Right Hook", "Right Hook", "Swing round the target group's right flank before engaging."},
    OptionText{"Pincer", "Pincer", "Split and take both flanks at once."},
    OptionText{"Hold", "Hold", "Stay in position and let the enemy come."},
    OptionText{"Under the guns", "Under guns",
               "Fight inside your planetary batteries' cover: engage whatever comes in, but never leave the ring. "
               "Without batteries, the same as Direct."},
};
constexpr std::array kFacings{
    OptionText{"Auto", "Auto", "Turn to bring the most firepower to bear on the current targets."},
    OptionText{"Bow-on", "Bow-on", "Keep the bow pointed at the target."},
    OptionText{"Broadside", "Broadside", "Turn side-on and rake the target with a full broadside."},
    OptionText{"Strongest arc", "Strongest", "Present the strongest arc to the strongest threat."},
};
constexpr std::array kAllocations{
    OptionText{"Auto", "Auto", "Split fire between targets without wasting shots on ships that are already dying."},
    OptionText{"Concentrate", "Concentrate", "The whole group fires at one target."},
    OptionText{"Spread", "Spread", "Each ship picks its own target."},
};
constexpr std::array kSubsystems{
    OptionText{"Hull", "Hull", "Aim for the hull: full damage and no system focus."},
    OptionText{"Shields", "Shields", "Aim at the shield generator to slow the target's shield recharge."},
    OptionText{"Engines", "Engines", "Aim at the engines so the line can outmanoeuvre a slowed target."},
    OptionText{"Weapons", "Weapons", "Aim at the weapon generator to blunt a ship you cannot kill in time."},
    OptionText{"Hyperdrive", "Hyperdrive", "Aim at the hyperdrive so that the target cannot escape."},
    OptionText{"Tractor beam", "Tractor", "Aim at the tractor beam to break or prevent its lock."},
};
constexpr std::array kPowers{
    OptionText{"Balanced", "Balanced", "Normal weapon recharge, shield recharge and speed."},
    OptionText{"Weapons", "Weapons", "Weapons recharge 30% faster, shields at half rate, speed 10% lower."},
    OptionText{"Shields", "Shields", "Shields recharge 80% faster, weapons 30% slower, speed 10% lower."},
    OptionText{"Engines", "Engines", "Speed and turn rate 40% higher, weapons and shields recharge 20% slower."},
};
constexpr std::array kWithdrawRules{
    OptionText{"Never", "Never", "Fight on whatever the damage."},
    OptionText{"Hull below 25%", "Hull < 25%", "Jump out once the hull falls below 25%."},
    OptionText{"Hull below 50%", "Hull < 50%", "Jump out once the hull falls below 50%."},
    OptionText{"Hull below 75%", "Hull < 75%", "Jump out once the hull falls below 75%."},
    OptionText{"Shields down", "Shields down", "Jump out as soon as the shields collapse."},
    OptionText{"On order only", "On order", "Withdraw only when ordered to."},
};
constexpr std::array kFormations{
    OptionText{"Line abreast", "Abreast", "Side by side across the line of advance."},
    OptionText{"Column", "Column", "One behind another along the line of advance."},
    OptionText{"Wedge", "Wedge", "An arrowhead behind the leading ship."},
    OptionText{"Ring", "Ring", "Around the ship the group escorts."},
    OptionText{"Loose", "Loose", "No fixed formation: each ship keeps its own station."},
};
constexpr std::array kMissions{
    OptionText{"Intercept", "Intercept", "Hunt enemy fighters and bombers in the area."},
    OptionText{"Escort", "Escort", "Fly cover for a ship or another flight."},
    OptionText{"Strike", "Strike", "Attack enemy capital ships in priority order."},
    OptionText{"Patrol", "Patrol", "Patrol an area and engage whatever enters it."},
    OptionText{"Hold in hangar", "Hangar", "Stay docked until ordered out."},
};
constexpr std::array kDogfights{
    OptionText{"Engage", "Engage", "Fight enemy fighters freely."},
    OptionText{"Avoid", "Avoid", "Ignore enemy fighters unless attacked and press on to the strike target."},
};
constexpr std::array kRearmRules{
    OptionText{"Torpedoes spent", "No torpedoes", "Return to a carrier to rearm once the torpedoes are gone."},
    OptionText{"Half losses", "50% losses", "Return to a carrier once half the craft are lost."},
    OptionText{"Never", "Never", "Stay in the fight."},
};
constexpr std::array kLaunchRules{
    OptionText{"Immediately", "At once", "Launch as soon as the battle starts."},
    OptionText{"When enemy fighters commit", "Enemy launch", "Launch when enemy fighters commit to an attack."},
    OptionText{"On order", "On order", "Stay in the hangar until given the Launch order."},
};

constexpr std::array<FieldText, 14> kFields{{
    {"Role", "What the group is for; it shapes movement, formation and default targets."},
    {"Range", "The distance at which the group fights."},
    {"Approach", "How the group closes with its target."},
    {"Facing", "Which side of each ship turns toward the enemy; Rebellion's ships carry different guns in each arc."},
    {"Targets", "The ranked target tags each ship scores enemies by; click to reorder or change them."},
    {"Fire", "How the group shares its fire between targets."},
    {"Aim", "The system that hull hits favour; anything but Hull costs 30% of the damage and triples critical hits."},
    {"Power", "Where the ships route power; a switch takes about five seconds."},
    {"Withdraw", "When a ship spools its hyperdrive and leaves the battle on its own."},
    {"Formation", "How the ships hold station around each other."},
    {"Mission", "What the flight does when it has no direct order."},
    {"Dogfight", "Whether the flight fights enemy fighters or avoids them."},
    {"Rearm", "When the flight returns to a carrier to rearm."},
    {"Launch", "When the flight launches from its carrier."},
}};

constexpr std::array kShipFields{DoctrineField::Role,      DoctrineField::Range,      DoctrineField::Approach,
                                 DoctrineField::Facing,    DoctrineField::Priorities, DoctrineField::Allocation,
                                 DoctrineField::Subsystem, DoctrineField::Power,      DoctrineField::Withdraw,
                                 DoctrineField::Formation};
constexpr std::array kFlightFields{DoctrineField::Mission, DoctrineField::Priorities, DoctrineField::Dogfight,
                                   DoctrineField::Rearm, DoctrineField::Launch};

constexpr std::array<OptionText, kTargetTagCount> kTags{{
    {"Fighters", "Fighters", "Starfighter squadrons."},
    {"Light ships", "Light", "Corvettes and frigates."},
    {"Medium ships", "Medium", "Cruisers and Victory Destroyers."},
    {"Heavy ships", "Heavy", "Star Destroyers and Mon Calamari cruisers."},
    {"Super ships", "Super", "Super Star Destroyers and Bulwark battlecruisers."},
    {"Death Star", "Death Star", "The battle station."},
    {"Carriers", "Carriers", "Ships with hangars; losing one strands its fighters."},
    {"Transports", "Transports", "Troop carriers: the weak point of an invasion."},
    {"Interdictors", "Interdictors", "Ships with a gravity well projector, which stops enemy jumps."},
    {"Tractor ships", "Tractors", "Ships with a tractor beam."},
    {"Pickets", "Pickets", "Laser-heavy anti-fighter ships."},
    {"Bombers", "Bombers", "Fighters with ion cannons: Y-wings, B-wings and TIE Bombers."},
    {"Flagships", "Flagships", "Ships with a commander aboard."},
    {"Shields down", "Shields down", "Targets whose shields have collapsed."},
    {"Damaged", "Damaged", "Targets with less than half their hull."},
    {"Withdrawing", "Withdrawing", "Targets spooling their hyperdrive to escape."},
    {"Tractored", "Tractored", "Targets held by a tractor beam."},
    {"Threatening", "Threatening", "Ships firing at a ship this group escorts."},
    {"Nearest", "Nearest", "Whatever is closest."},
}};

constexpr std::array kSignals{
    OptionText{"Concentrate fire", "Concentrate",
               "Every ship in range turns its fire on one target for 20 s, with an accuracy bonus."},
    OptionText{"Hold the line", "Hold the line", "No automatic withdrawal and double damage-control rate for 30 s."},
    OptionText{"Scramble", "Scramble", "Every hangar launches at three times the normal rate."},
    OptionText{"Coordinated jump", "Coord. jump",
               "Ships that start withdrawing within 15 s spool their hyperdrives 40% faster."},
};

std::span<const OptionText> OptionsOf(DoctrineField field)
{
  switch (field) {
    case DoctrineField::Role:
      return kRoles;
    case DoctrineField::Range:
      return kRanges;
    case DoctrineField::Approach:
      return kApproaches;
    case DoctrineField::Facing:
      return kFacings;
    case DoctrineField::Priorities:
      return {};
    case DoctrineField::Allocation:
      return kAllocations;
    case DoctrineField::Subsystem:
      return kSubsystems;
    case DoctrineField::Power:
      return kPowers;
    case DoctrineField::Withdraw:
      return kWithdrawRules;
    case DoctrineField::Formation:
      return kFormations;
    case DoctrineField::Mission:
      return kMissions;
    case DoctrineField::Dogfight:
      return kDogfights;
    case DoctrineField::Rearm:
      return kRearmRules;
    case DoctrineField::Launch:
      return kLaunchRules;
  }
  return {};
}

template <class E>
E To(int index)
{
  return static_cast<E>(index);
}

template <class E>
int From(E value)
{
  return static_cast<int>(value);
}

} // namespace

const FieldText& Describe(DoctrineField field)
{
  return kFields[static_cast<size_t>(field)];
}

bool IsFlightField(DoctrineField field)
{
  return field >= DoctrineField::Mission;
}

battle::DoctrineOverrideMask FieldMask(DoctrineField field, bool flight)
{
  switch (field) {
    case DoctrineField::Role:
      return battle::kShipRole;
    case DoctrineField::Range:
      return battle::kShipRange;
    case DoctrineField::Approach:
      return battle::kShipApproach;
    case DoctrineField::Facing:
      return battle::kShipFacing;
    case DoctrineField::Priorities:
      return flight ? battle::DoctrineOverrideMask{battle::kFlightPriorities}
                    : battle::DoctrineOverrideMask{battle::kShipPriorities};
    case DoctrineField::Allocation:
      return battle::kShipAllocation;
    case DoctrineField::Subsystem:
      return battle::kShipSubsystem;
    case DoctrineField::Power:
      return battle::kShipPower;
    case DoctrineField::Withdraw:
      return battle::kShipWithdraw;
    case DoctrineField::Formation:
      return battle::kShipFormation;
    case DoctrineField::Mission:
      return battle::kFlightMission;
    case DoctrineField::Dogfight:
      return battle::kFlightDogfight;
    case DoctrineField::Rearm:
      return battle::kFlightRearm;
    case DoctrineField::Launch:
      return battle::kFlightLaunch;
  }
  return 0;
}

std::span<const DoctrineField> ShipFields()
{
  return kShipFields;
}

std::span<const DoctrineField> FlightFields()
{
  return kFlightFields;
}

int OptionCount(DoctrineField field)
{
  return static_cast<int>(OptionsOf(field).size());
}

const OptionText& Option(DoctrineField field, int index)
{
  static const OptionText kNone{"", "", ""};
  const std::span<const OptionText> options = OptionsOf(field);
  return index >= 0 && index < static_cast<int>(options.size()) ? options[index] : kNone;
}

int GetOption(DoctrineField field, const ShipDoctrine& ship, const FlightDoctrine& flight)
{
  switch (field) {
    case DoctrineField::Role:
      return From(ship.role);
    case DoctrineField::Range:
      return From(ship.range);
    case DoctrineField::Approach:
      return From(ship.approach);
    case DoctrineField::Facing:
      return From(ship.facing);
    case DoctrineField::Priorities:
      return -1;
    case DoctrineField::Allocation:
      return From(ship.allocation);
    case DoctrineField::Subsystem:
      return From(ship.subsystem);
    case DoctrineField::Power:
      return From(ship.power);
    case DoctrineField::Withdraw:
      return From(ship.withdraw);
    case DoctrineField::Formation:
      return From(ship.formation);
    case DoctrineField::Mission:
      return From(flight.mission);
    case DoctrineField::Dogfight:
      return From(flight.dogfight);
    case DoctrineField::Rearm:
      return From(flight.rearm);
    case DoctrineField::Launch:
      return From(flight.launch);
  }
  return -1;
}

void SetOption(DoctrineField field, int index, ShipDoctrine& ship, FlightDoctrine& flight)
{
  switch (field) {
    case DoctrineField::Role:
      ship.role = To<battle::Role>(index);
      break;
    case DoctrineField::Range:
      ship.range = To<battle::EngagementRange>(index);
      break;
    case DoctrineField::Approach:
      ship.approach = To<battle::Approach>(index);
      break;
    case DoctrineField::Facing:
      ship.facing = To<battle::Facing>(index);
      break;
    case DoctrineField::Priorities:
      break;
    case DoctrineField::Allocation:
      ship.allocation = To<battle::FireAllocation>(index);
      break;
    case DoctrineField::Subsystem:
      ship.subsystem = To<battle::Subsystem>(index);
      break;
    case DoctrineField::Power:
      ship.power = To<battle::PowerRouting>(index);
      break;
    case DoctrineField::Withdraw:
      ship.withdraw = To<battle::WithdrawRule>(index);
      break;
    case DoctrineField::Formation:
      ship.formation = To<battle::Formation>(index);
      break;
    case DoctrineField::Mission:
      flight.mission = To<battle::FlightMission>(index);
      break;
    case DoctrineField::Dogfight:
      flight.dogfight = To<battle::DogfightRule>(index);
      break;
    case DoctrineField::Rearm:
      flight.rearm = To<battle::RearmRule>(index);
      break;
    case DoctrineField::Launch:
      flight.launch = To<battle::LaunchRule>(index);
      break;
  }
}

bool FieldDiffers(DoctrineField field, bool flight, const ShipDoctrine& shipA, const FlightDoctrine& flightA,
                  const ShipDoctrine& shipB, const FlightDoctrine& flightB)
{
  if (field != DoctrineField::Priorities) {
    return GetOption(field, shipA, flightA) != GetOption(field, shipB, flightB);
  }
  const battle::TargetPriorities& a = flight ? flightA.priorities : shipA.priorities;
  const battle::TargetPriorities& b = flight ? flightB.priorities : shipB.priorities;
  return a.count != b.count || a.finishDamaged != b.finishDamaged
         || !std::equal(a.tags.begin(), a.tags.begin() + a.count, b.tags.begin());
}

const OptionText& Describe(battle::TargetTag tag)
{
  return kTags[static_cast<size_t>(tag)];
}

TagGroup GroupOf(battle::TargetTag tag)
{
  if (tag <= battle::TargetTag::DeathStar) {
    return TagGroup::Size;
  }
  if (tag <= battle::TargetTag::Flagship) {
    return TagGroup::Function;
  }
  if (tag <= battle::TargetTag::Threatening) {
    return TagGroup::State;
  }
  return TagGroup::Fallback;
}

std::string_view Name(TagGroup group)
{
  switch (group) {
    case TagGroup::Size:
      return "Size";
    case TagGroup::Function:
      return "Role";
    case TagGroup::State:
      return "State";
    case TagGroup::Fallback:
      return "Other";
  }
  return "";
}

std::string PriorityText(const battle::TargetPriorities& priorities)
{
  std::string text;
  for (int i = 0; i < priorities.count; ++i) {
    text += (i ? ", " : "") + std::string(Describe(priorities.tags[i]).shortName);
  }
  if (priorities.finishDamaged) {
    text += text.empty() ? "Finish damaged" : " + finish";
  }
  return text;
}

const OptionText& Describe(battle::SignalKind signal)
{
  return kSignals[static_cast<size_t>(signal)];
}

} // namespace reb::battle_ui
