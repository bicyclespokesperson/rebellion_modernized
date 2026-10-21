#include <algorithm>
#include <array>
#include <cmath>

#include "space_battle/doctrine.h"
#include "space_battle/unit_names.h"
#include "space_battle/math2d.h"
#include "space_battle/sim_internal.h"

namespace reb::battle::sim
{
namespace
{
using namespace tuning;

constexpr std::array<Archetype, 4> kFrontRow{Archetype::BowLine, Archetype::BroadsideLine, Archetype::EscortFrigate,
                                             Archetype::Picket};
constexpr std::array<const char*, 8> kFlightColours{"Red", "Blue", "Green", "Gold", "Rogue", "Gray", "Black", "Silver"};

const UnitIdentity& IdentityOf(const UnitEntry& entry)
{
  return std::visit([](const auto& unit) -> const UnitIdentity& { return unit.identity; }, entry);
}

struct SideLayout {
  Vec2 center;
  Vec2 forward; // toward the enemy
  Vec2 left;
};

SideLayout LayoutFor(const BattleState& state, Side side)
{
  const Vec2 u = state.arrivalDirection;
  SideLayout layout;
  if (side == state.attacker) {
    layout.center = u * kAttackerDeployDistance;
    layout.forward = -u;
  } else {
    layout.center = u * -kDefenderDeployDistance;
    layout.forward = u;
  }
  layout.left = Rotated(layout.forward, 0.5f * kPi);
  return layout;
}

void InitShip(BattleState& state, UnitIndex index, const CapitalShipEntry& entry)
{
  const UnitProfile& profile = state.profiles[index];
  ShipState ship;
  ship.unit = index;
  ship.hull = std::clamp(static_cast<float>(entry.hull), 0.0f, profile.hullMax);
  ship.shield = profile.shieldMax;
  ship.criticals = entry.criticals;
  auto clampHits = [](int32_t& hits, int32_t max) { hits = std::clamp(hits, 0, max); };
  clampHits(ship.criticals.speed, kCritSystemMax);
  clampHits(ship.criticals.shieldRecharge, kCritSystemMax);
  clampHits(ship.criticals.weaponRecharge, kCritSystemMax);
  clampHits(ship.criticals.tractor, kCritSystemMax);
  clampHits(ship.criticals.hyperdrivePrimary, 1);
  clampHits(ship.criticals.hyperdriveBackup, 1);
  ship.batteryTarget.fill(kNoUnit);
  for (int i = 0; i < profile.batteryCount; ++i) {
    ship.batteryCooldown[i] = state.rng.Range(0, Ticks(kSalvoSeconds[static_cast<size_t>(profile.batteries[i].kind)]));
  }
  ship.damageControlTicks = state.rng.Range(1, Ticks(kDamageControlSeconds));
  ship.launchTicks = Ticks(1.0f) + state.rng.Range(0, kLaunchRandomSeconds * kTicksPerSecond);
  ship.nextThinkTick = index % Ticks(kShipThinkSeconds);
  UnitState& unit = state.units[index];
  unit.slot = static_cast<int32_t>(state.ships.size());
  unit.inSpace = ship.hull > 0.0f;
  unit.fate = unit.inSpace ? UnitFate::Active : UnitFate::Destroyed;
  state.ships.push_back(ship);
}

void InitSquadron(BattleState& state, UnitIndex index, const SquadronEntry& entry)
{
  const UnitProfile& profile = state.profiles[index];
  SquadronState squadron;
  squadron.unit = index;
  squadron.firstCraft = static_cast<int32_t>(state.craft.size());
  squadron.craftCount = profile.craftMax;
  squadron.alive = std::clamp(entry.currentSize, 0, profile.craftMax);
  squadron.initialAlive = squadron.alive;
  squadron.nextThinkTick = index % Ticks(kSquadronThinkSeconds);
  const bool carried = state.Valid(entry.carrier) && state.units[entry.carrier].kind != UnitKind::Passive
                       && state.units[entry.carrier].side == entry.identity.side
                       && state.profiles[entry.carrier].hangarCapacity > 0;
  squadron.carrier = carried ? entry.carrier : kNoUnit;
  for (int i = 0; i < squadron.craftCount; ++i) {
    CraftState craft;
    craft.alive = i < squadron.alive;
    craft.hull = craft.alive ? kCraftHull : 0.0f;
    craft.shield = craft.alive ? profile.craftShield : 0.0f;
    craft.torpedoes = static_cast<int16_t>(craft.alive ? profile.torpedoesPerCraft : 0);
    state.craft.push_back(craft);
  }
  UnitState& unit = state.units[index];
  unit.slot = static_cast<int32_t>(state.squadrons.size());
  unit.fate = squadron.alive > 0 ? UnitFate::Active : UnitFate::Destroyed;
  squadron.place = squadron.alive > 0 ? SquadronPlace::Docked : SquadronPlace::Destroyed;
  state.squadrons.push_back(squadron);
}

void InitStation(BattleState& state, UnitIndex index, const DeathStarEntry& entry)
{
  StationState station;
  station.unit = index;
  station.hull = std::clamp(static_cast<float>(entry.hull), 0.0f, state.profiles[index].hullMax);
  UnitState& unit = state.units[index];
  unit.slot = static_cast<int32_t>(state.stations.size());
  unit.inSpace = station.hull > 0.0f;
  unit.fate = unit.inSpace ? UnitFate::Active : UnitFate::Destroyed;
  state.stations.push_back(station);
}

void InitCommanders(BattleState& state)
{
  for (UnitIndex i = 0; i < static_cast<UnitIndex>(state.snapshot.units.size()); ++i) {
    const auto* character = std::get_if<CharacterEntry>(&state.snapshot.units[i]);
    if (character == nullptr || !state.Valid(character->ship) || state.units[character->ship].kind == UnitKind::Passive
        || state.units[character->ship].side != character->identity.side) {
      continue;
    }
    SideState& side = state.sides[SideIndex(character->identity.side)];
    const auto leadership = static_cast<float>(character->leadership);
    const auto combat = static_cast<float>(character->combat);
    if (character->admiral && (side.admiral == kNoUnit || leadership > side.leadership)) {
      side.admiral = i;
      side.leadership = leadership;
    }
    if (character->commander && (side.commander == kNoUnit || combat > side.commanderCombat)) {
      side.commander = i;
      side.commanderCombat = combat;
    }
  }
  for (SideState& side : state.sides) {
    if (side.admiral != kNoUnit) {
      side.signalsMax = std::min(kSignalMaxCharges,
                                 1 + static_cast<int32_t>(std::max(0.0f, side.leadership) / kSignalLeadershipPerCharge));
      side.signalsReady = side.signalsMax;
    }
  }
}

float GroupSpacing(const BattleState& state, const GroupState& group)
{
  float radius = 0.0f;
  for (UnitIndex member : group.members) {
    radius = std::max(radius, state.profiles[member].radius);
  }
  return kFormationSpacing + 2.0f * radius;
}

void PlaceRow(BattleState& state, const std::vector<GroupId>& row, const SideLayout& layout, Vec2 rowCenter)
{
  float width = 0.0f;
  for (GroupId id : row) {
    width += GroupSpacing(state, state.groups[id]) * static_cast<float>(state.groups[id].members.size());
  }
  float cursor = -0.5f * width;
  const float heading = AngleOf(layout.forward);
  for (GroupId id : row) {
    const float spacing = GroupSpacing(state, state.groups[id]);
    for (UnitIndex member : state.groups[id].members) {
      UnitState& unit = state.units[member];
      unit.position = rowCenter + layout.left * (cursor + 0.5f * spacing);
      unit.heading = heading;
      cursor += spacing;
    }
  }
}

bool HullFits(const BattleState& state, Side side, Vec2 position, float radius)
{
  const SideState& zone = state.sides[SideIndex(side)];
  return Distance(position, zone.deploymentCenter) + radius <= zone.deploymentRadius
         && std::abs(position.x) + radius <= kBattlePlaneHalfExtent
         && std::abs(position.y) + radius <= kBattlePlaneHalfExtent
         && Distance(position, state.planetCenter) >= kPlanetRadius + radius;
}

struct RankSpan {
  float low;
  float high;
};

std::vector<RankSpan> RankSpans(const BattleState& state, Side side, const SideLayout& layout,
                                float depth, float radius)
{
  const SideState& zone = state.sides[SideIndex(side)];
  const Vec2 origin = layout.center - layout.forward * depth;
  const Vec2 relative = origin - zone.deploymentCenter;
  const float along = Dot(relative, layout.forward);
  const float usable = zone.deploymentRadius - radius;
  if (usable <= std::abs(along)) {
    return {};
  }
  const float half = std::sqrt(usable * usable - along * along);
  RankSpan span{-half, half};
  for (int axis = 0; axis < 2; ++axis) {
    const float p = axis == 0 ? origin.x : origin.y;
    const float direction = axis == 0 ? layout.left.x : layout.left.y;
    const float extent = kBattlePlaneHalfExtent - radius;
    if (std::abs(direction) < 0.000001f) {
      if (std::abs(p) > extent) return {};
    } else {
      const float a = (-extent - p) / direction;
      const float b = (extent - p) / direction;
      span.low = std::max(span.low, std::min(a, b));
      span.high = std::min(span.high, std::max(a, b));
    }
  }
  if (span.low > span.high) return {};
  const Vec2 planet = state.planetCenter - origin;
  const float planetDepth = Dot(planet, layout.forward);
  const float clearance = kPlanetRadius + radius;
  if (std::abs(planetDepth) >= clearance) return {span};
  const float cut = std::sqrt(clearance * clearance - planetDepth * planetDepth);
  const float lateral = Dot(planet, layout.left);
  std::vector<RankSpan> spans;
  if (span.low < lateral - cut) spans.push_back({span.low, std::min(span.high, lateral - cut)});
  if (span.high > lateral + cut) spans.push_back({std::max(span.low, lateral + cut), span.high});
  return spans;
}

bool PlaceWrappedRow(BattleState& state, Side side, const std::vector<GroupId>& row,
                     const SideLayout& layout, float radius, float& depth)
{
  size_t groupIndex = 0;
  size_t memberIndex = 0;
  const float step = std::max(kDeploymentRowSpacing, 2.0f * radius + kFormationSpacing);
  const float maxDepth = 2.0f * state.sides[SideIndex(side)].deploymentRadius - kDeployZoneRadius;
  while (groupIndex < row.size() && depth <= maxDepth) {
    const auto spans = RankSpans(state, side, layout, depth, radius);
    float widest = 0.0f;
    for (const auto& span : spans) widest = std::max(widest, span.high - span.low);
    for (const auto& span : spans) {
      std::vector<std::pair<UnitIndex, float>> rank;
      float cursor = span.low;
      while (groupIndex < row.size()) {
        const GroupState& group = state.groups[row[groupIndex]];
        const float spacing = GroupSpacing(state, group);
        const size_t remaining = group.members.size() - memberIndex;
        const float wholeWidth = spacing * static_cast<float>(remaining - 1);
        if (memberIndex == 0 && wholeWidth <= widest && cursor + wholeWidth > span.high) break;
        if (cursor > span.high) break;
        rank.push_back({group.members[memberIndex++], cursor});
        cursor += spacing;
        if (memberIndex == group.members.size()) {
          ++groupIndex;
          memberIndex = 0;
        }
      }
      if (!rank.empty()) {
        const float shift = 0.5f * (span.high - rank.back().second);
        for (const auto& [member, lateral] : rank) {
          UnitState& unit = state.units[member];
          unit.position = layout.center - layout.forward * depth + layout.left * (lateral + shift);
          unit.heading = AngleOf(layout.forward);
          if (!HullFits(state, side, unit.position, state.profiles[member].radius)) return false;
        }
      }
    }
    depth += step;
  }
  return groupIndex == row.size();
}

void FitTaskForces(BattleState& state, Side side, const SideLayout& layout,
                   const std::vector<GroupId>& front, const std::vector<GroupId>& back)
{
  float radius = 0.0f;
  bool fits = true;
  for (const ShipState& ship : state.ships) {
    if (state.units[ship.unit].side != side || !state.units[ship.unit].inSpace) continue;
    radius = std::max(radius, state.profiles[ship.unit].radius);
    fits &= HullFits(state, side, state.units[ship.unit].position, state.profiles[ship.unit].radius);
  }
  if (fits) return;
  radius += 1.0f; // Leave room for floating-point transforms at the boundary.
  SideState& zone = state.sides[SideIndex(side)];
  auto place = [&](float growth) {
    zone.deploymentRadius = kDeployZoneRadius + growth;
    zone.deploymentCenter = layout.center - layout.forward * growth;
    float depth = 0.0f;
    if (!PlaceWrappedRow(state, side, front, layout, radius, depth)) return false;
    depth = std::max(depth, kDeploymentRowSpacing);
    return PlaceWrappedRow(state, side, back, layout, radius, depth);
  };
  if (place(0.0f)) return;
  float low = 0.0f;
  float high = kDeploymentRowSpacing;
  while (!place(high)) {
    if (high > kMaxDeployZoneGrowth) {
      // More ships than the battle plane holds: keep the unwrapped lines rather than search forever.
      zone.deploymentRadius = kDeployZoneRadius;
      zone.deploymentCenter = layout.center;
      PlaceRow(state, front, layout, layout.center);
      PlaceRow(state, back, layout, layout.center - layout.forward * kDeploymentRowSpacing);
      return;
    }
    high *= 2.0f;
  }
  while (high - low > 0.1f) {
    const float middle = 0.5f * (low + high);
    if (place(middle)) high = middle;
    else low = middle;
  }
  place(high);
}

void FormTaskForces(BattleState& state, Side side)
{
  std::array<std::vector<UnitIndex>, 7> buckets;
  constexpr std::array<Archetype, 7> kOrder{Archetype::BowLine,   Archetype::BroadsideLine, Archetype::EscortFrigate,
                                            Archetype::Picket,    Archetype::Interdictor,   Archetype::Carrier,
                                            Archetype::Transport};
  for (const ShipState& ship : state.ships) {
    const UnitState& unit = state.units[ship.unit];
    if (unit.side != side || !unit.inSpace) {
      continue;
    }
    const auto it = std::find(kOrder.begin(), kOrder.end(), state.profiles[ship.unit].archetype);
    buckets[static_cast<size_t>(it - kOrder.begin())].push_back(ship.unit);
  }
  const SideLayout layout = LayoutFor(state, side);
  std::vector<GroupId> front;
  std::vector<GroupId> back;
  int taskForce = 0;
  int broadsideOrdinal = 0;
  for (size_t b = 0; b < kOrder.size(); ++b) {
    const std::vector<UnitIndex>& ships = buckets[b];
    const int groupCount = (static_cast<int>(ships.size()) + kMaxTaskForceShips - 1) / kMaxTaskForceShips;
    for (int g = 0; g < groupCount; ++g) {
      const GroupId id = CreateGroup(state, side, false, "Task Force " + std::to_string(++taskForce));
      GroupState& group = state.groups[id];
      group.shipDoctrine = DefaultShipDoctrine(kOrder[b], kOrder[b] == Archetype::BroadsideLine ? broadsideOrdinal++ : 0);
      // A defender with planetary batteries starts its battle line under their guns (§6.13).
      const bool line = kOrder[b] == Archetype::BowLine || kOrder[b] == Archetype::BroadsideLine;
      if (line && side != state.attacker && HasPlanetaryCover(state, side)) {
        group.shipDoctrine.approach = Approach::UnderGuns;
      }
      const size_t begin = ships.size() * static_cast<size_t>(g) / static_cast<size_t>(groupCount);
      const size_t end = ships.size() * static_cast<size_t>(g + 1) / static_cast<size_t>(groupCount);
      for (size_t i = begin; i < end; ++i) {
        SetGroup(state, ships[i], id);
      }
      const bool frontRow = std::find(kFrontRow.begin(), kFrontRow.end(), kOrder[b]) != kFrontRow.end();
      (frontRow ? front : back).push_back(id);
    }
  }
  PlaceRow(state, front, layout, layout.center);
  PlaceRow(state, back, layout, layout.center - layout.forward * kDeploymentRowSpacing);
  FitTaskForces(state, side, layout, front, back);
}

void FormFlights(BattleState& state, Side side)
{
  struct Kind {
    uint32_t typeBits;
    Archetype archetype;
    std::vector<UnitIndex> squadrons;
  };
  std::vector<Kind> kinds;
  for (const SquadronState& squadron : state.squadrons) {
    const UnitIndex index = squadron.unit;
    if (state.units[index].side != side || squadron.place == SquadronPlace::Destroyed) {
      continue;
    }
    const uint32_t typeBits = IdentityOf(state.snapshot.units[index]).typeBits;
    const Archetype archetype = state.profiles[index].archetype;
    auto it = std::find_if(kinds.begin(), kinds.end(),
                           [&](const Kind& kind) { return kind.typeBits == typeBits && kind.archetype == archetype; });
    if (it == kinds.end()) {
      kinds.push_back({typeBits, archetype, {}});
      it = kinds.end() - 1;
    }
    it->squadrons.push_back(index);
  }
  int flight = 0;
  for (const Kind& kind : kinds) {
    const int groupCount = (static_cast<int>(kind.squadrons.size()) + kMaxFlightSquadrons - 1) / kMaxFlightSquadrons;
    for (int g = 0; g < groupCount; ++g) {
      const std::string name = flight < static_cast<int>(kFlightColours.size())
                                   ? std::string(kFlightColours[static_cast<size_t>(flight)]) + " Flight"
                                   : "Flight " + std::to_string(flight + 1);
      ++flight;
      const GroupId id = CreateGroup(state, side, true, name);
      state.groups[id].flightDoctrine = DefaultFlightDoctrine(kind.archetype);
      const size_t begin = kind.squadrons.size() * static_cast<size_t>(g) / static_cast<size_t>(groupCount);
      const size_t end = kind.squadrons.size() * static_cast<size_t>(g + 1) / static_cast<size_t>(groupCount);
      for (size_t i = begin; i < end; ++i) {
        SetGroup(state, kind.squadrons[i], id);
      }
    }
  }
}

void PlaceSquadrons(BattleState& state)
{
  for (SquadronState& squadron : state.squadrons) {
    UnitState& unit = state.units[squadron.unit];
    const SideLayout layout = LayoutFor(state, unit.side);
    Vec2 origin;
    if (squadron.carrier != kNoUnit) {
      origin = state.units[squadron.carrier].position;
    } else if (unit.side == state.attacker) {
      // System-based squadrons belong to the defender; an attacker's arrive with its fleet, already flying.
      origin = layout.center - layout.forward * kDeploymentRowSpacing;
      if (squadron.place == SquadronPlace::Docked) {
        squadron.place = SquadronPlace::InSpace;
        squadron.launched = true;
        unit.inSpace = true;
      }
    } else {
      origin = state.planetCenter + state.arrivalDirection * kPlanetRadius;
    }
    unit.position = origin;
    unit.heading = AngleOf(layout.forward);
    squadron.anchor = origin;
    squadron.centroid = origin;
    for (int i = 0; i < squadron.craftCount; ++i) {
      CraftState& craft = state.craft[squadron.firstCraft + i];
      const Vec2 offset{-kSquadronSpacing * static_cast<float>(i / 4), kSquadronSpacing * static_cast<float>(i % 4 - 1)};
      craft.position = origin + Rotated(offset, unit.heading);
      craft.previousPosition = craft.position;
      craft.heading = unit.heading;
      craft.previousHeading = unit.heading;
    }
  }
}

void PlaceStations(BattleState& state)
{
  for (const StationState& station : state.stations) {
    UnitState& unit = state.units[station.unit];
    const SideLayout layout = LayoutFor(state, unit.side);
    unit.position
        = layout.center - layout.forward * (kDeploymentRowSpacing + state.profiles[station.unit].radius + kDeathStarStandoff);
    unit.heading = AngleOf(layout.forward);
  }
}

} // namespace

GroupId CreateGroup(BattleState& state, Side side, bool flight, std::string name)
{
  GroupState group;
  group.id = static_cast<GroupId>(state.groups.size());
  group.side = side;
  group.flight = flight;
  group.name = std::move(name);
  group.nextThinkTick = group.id % Ticks(kGroupThinkSeconds);
  state.groups.push_back(std::move(group));
  return state.groups.back().id;
}

void SetGroup(BattleState& state, UnitIndex unit, GroupId id)
{
  UnitState& self = state.units[unit];
  if (GroupState* old = state.Group(self.group)) {
    std::erase(old->members, unit);
  }
  self.group = id;
  if (GroupState* group = state.Group(id)) {
    group->members.insert(std::lower_bound(group->members.begin(), group->members.end(), unit), unit);
  }
}

std::string UnitName(const BattleState& state, UnitIndex unit)
{
  return NameUnit(state.snapshot, unit, ArchetypeName(state.Profile(unit).archetype)).name;
}

void Initialize(BattleState& state)
{
  state.profiles = BuildProfiles(state.snapshot);
  state.rng = Pcg32(state.snapshot.params.seed, 0x5ba771eULL);
  state.attacker = Opponent(state.snapshot.params.heldFirstBy);
  state.arrivalDirection = FromAngle(state.options.attackerBearing);
  state.planetCenter = state.arrivalDirection * -kPlanetDistance;
  state.units.resize(state.snapshot.units.size());
  for (UnitIndex i = 0; i < static_cast<UnitIndex>(state.snapshot.units.size()); ++i) {
    const UnitEntry& entry = state.snapshot.units[i];
    UnitState& unit = state.units[i];
    unit.kind = state.profiles[i].kind;
    unit.side = IdentityOf(entry).side;
    if (const auto* ship = std::get_if<CapitalShipEntry>(&entry)) {
      InitShip(state, i, *ship);
    } else if (const auto* station = std::get_if<DeathStarEntry>(&entry)) {
      InitStation(state, i, *station);
    }
  }
  // Squadrons after every ship, so their carriers are known.
  for (UnitIndex i = 0; i < static_cast<UnitIndex>(state.snapshot.units.size()); ++i) {
    if (const auto* squadron = std::get_if<SquadronEntry>(&state.snapshot.units[i])) {
      InitSquadron(state, i, *squadron);
    }
  }
  for (Side side : {Side::Alliance, Side::Empire}) {
    state.sides[SideIndex(side)].deploymentCenter = LayoutFor(state, side).center;
    state.sides[SideIndex(side)].deploymentRadius = kDeployZoneRadius;
  }
  InitCommanders(state);
  InitDefenses(state);
  for (Side side : {Side::Alliance, Side::Empire}) {
    FormTaskForces(state, side);
    FormFlights(state, side);
  }
  PlaceSquadrons(state);
  PlaceStations(state);
  for (UnitState& unit : state.units) {
    unit.previousPosition = unit.position;
    unit.previousHeading = unit.heading;
  }
  for (ShipState& ship : state.ships) {
    UnitState& unit = state.units[ship.unit];
    ship.desiredHeading = unit.heading;
    ship.goal = unit.position;
    if (unit.side == state.attacker) {
      ship.speed = kArrivalSpeedFraction * state.profiles[ship.unit].maxSpeed; // dropping out of hyperspace
      unit.velocity = FromAngle(unit.heading) * ship.speed;
    }
  }
  UpdateSides(state);
  for (SideState& side : state.sides) {
    side.initialStrength = side.strength;
  }
  for (Side side : {Side::Alliance, Side::Empire}) {
    state.sides[SideIndex(side)].durableLow = DurableTotal(state, side);
  }
}

} // namespace reb::battle::sim
