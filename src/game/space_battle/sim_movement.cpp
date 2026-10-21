#include <algorithm>
#include <cmath>

#include "space_battle/math2d.h"
#include "space_battle/sim_internal.h"

namespace reb::battle::sim
{
namespace
{
using namespace tuning;

struct MoveGoal {
  Vec2 point;
  float heading = 0.0f;
  float range = 0.0f; // the engagement range being held, for the intent line
  UnitIndex target = kNoUnit;
  bool hold = false;     // stay put and only turn
  bool approach = false; // closing on an enemy
  Vec2 secondPoint;      // Pincer: the other flank
  bool pincer = false;
};

Vec2 CentroidOf(const BattleState& state, const std::vector<UnitIndex>& units)
{
  Vec2 sum;
  int count = 0;
  for (UnitIndex unit : units) {
    if (state.InSpace(unit)) {
      sum += state.units[unit].position;
      ++count;
    }
  }
  return count > 0 ? sum * (1.0f / static_cast<float>(count)) : sum;
}

// The centroid of the side's ships that hold the line, and of the enemy's ships (fighters if it has none).
void LineAndEnemy(const BattleState& state, Side side, Vec2* line, Vec2* enemy)
{
  Vec2 lineSum;
  Vec2 enemySum;
  Vec2 fighterSum;
  int lineCount = 0;
  int enemyCount = 0;
  int fighterCount = 0;
  for (UnitIndex i = 0; i < static_cast<UnitIndex>(state.units.size()); ++i) {
    const UnitState& unit = state.units[i];
    if (!unit.inSpace) {
      continue;
    }
    if (unit.side != side) {
      if (unit.kind == UnitKind::CapitalShip) {
        enemySum += unit.position;
        ++enemyCount;
      } else if (unit.kind == UnitKind::Squadron) {
        fighterSum += unit.position;
        ++fighterCount;
      }
    } else if (unit.kind == UnitKind::CapitalShip) {
      const Role role = state.EffectiveShipDoctrine(i).role;
      if (role == Role::Line || role == Role::Strike || role == Role::Flank) {
        lineSum += unit.position;
        ++lineCount;
      }
    }
  }
  *line = lineCount > 0 ? lineSum * (1.0f / static_cast<float>(lineCount)) : DeploymentCenter(state, side);
  if (enemyCount > 0) {
    *enemy = enemySum * (1.0f / static_cast<float>(enemyCount));
  } else if (fighterCount > 0) {
    *enemy = fighterSum * (1.0f / static_cast<float>(fighterCount));
  } else {
    *enemy = DeploymentCenter(state, Opponent(side));
  }
}

UnitIndex NearestEnemy(const BattleState& state, Side side, Vec2 from, bool shipsOnly, float maxDistance)
{
  UnitIndex best = kNoUnit;
  float bestDistance = maxDistance * maxDistance;
  for (UnitIndex i = 0; i < static_cast<UnitIndex>(state.units.size()); ++i) {
    const UnitState& unit = state.units[i];
    const bool eligible = unit.kind == UnitKind::CapitalShip || (!shipsOnly && unit.kind == UnitKind::Squadron);
    if (unit.side == side || !unit.inSpace || !eligible) {
      continue;
    }
    const float distance = DistanceSq(from, unit.position);
    if (distance < bestDistance) {
      bestDistance = distance;
      best = i;
    }
  }
  return best;
}

UnitIndex HeaviestLineShip(const BattleState& state, Side side, UnitIndex except)
{
  UnitIndex best = kNoUnit;
  float bestHull = 0.0f;
  for (const ShipState& ship : state.ships) {
    const UnitState& unit = state.units[ship.unit];
    if (unit.side != side || !unit.inSpace || ship.unit == except) {
      continue;
    }
    const Archetype archetype = state.Profile(ship.unit).archetype;
    const float weight = state.Profile(ship.unit).hullMax
                         * (archetype == Archetype::BowLine || archetype == Archetype::BroadsideLine ? 2.0f : 1.0f);
    if (weight > bestHull) {
      bestHull = weight;
      best = ship.unit;
    }
  }
  return best;
}

float DesiredRange(const BattleState& state, const std::vector<UnitIndex>& members, EngagementRange range,
                   UnitIndex target)
{
  float close = 0.0f;
  float optimal = 0.0f;
  bool any = false;
  for (UnitIndex member : members) {
    if (!state.InSpace(member)) {
      continue;
    }
    const UnitProfile& profile = state.Profile(member);
    close = any ? std::min(close, profile.closeRange) : profile.closeRange;
    optimal = any ? std::min(optimal, profile.optimalRange) : profile.optimalRange;
    any = true;
  }
  float desired = optimal;
  if (range == EngagementRange::Close) {
    desired = close;
  } else if (range == EngagementRange::StandOff) {
    float reach = 0.0f;
    const Vec2 at = state.units[target].position;
    for (const ShipState& ship : state.ships) {
      const UnitState& unit = state.units[ship.unit];
      if (unit.side != state.units[target].side || !unit.inSpace || DistanceSq(unit.position, at) > kStandOffScan * kStandOffScan) {
        continue;
      }
      reach = std::max(reach, state.Profile(ship.unit).maxRange);
    }
    desired = std::max(optimal, reach + kStandOffMargin);
  }
  return std::max(desired, kMinEngagementRange) + state.Profile(target).radius;
}

float LargestRadius(const BattleState& state, const std::vector<UnitIndex>& members)
{
  float radius = 0.0f;
  for (UnitIndex member : members) {
    radius = std::max(radius, state.Profile(member).radius);
  }
  return radius;
}

// Under the guns (§6.13): pulled back inside the cover's edge, toward the planet.
Vec2 KeepInCover(const BattleState& state, Vec2 point, float radius)
{
  const Vec2 offset = point - state.planetCenter;
  const float outer = std::max(kPlanetRadius + radius, PlanetaryCoverRadius() - kCoverMargin - radius);
  if (LengthSq(offset) <= outer * outer) {
    return point;
  }
  return state.planetCenter + Normalized(offset, state.arrivalDirection) * outer;
}

// Stand-off keeps out of the enemy batteries' reach as it does out of their ships'.
Vec2 KeepOutOfCover(const BattleState& state, Side enemy, Vec2 point, float radius)
{
  const Vec2 offset = point - state.planetCenter;
  const float inner = PlanetaryCoverRadius() + kStandOffMargin + radius;
  if (!HasPlanetaryCover(state, enemy) || LengthSq(offset) >= inner * inner) {
    return point;
  }
  return state.planetCenter + Normalized(offset, state.arrivalDirection) * inner;
}

MoveGoal EngageGoal(BattleState& state, GroupState* group, Side side, const std::vector<UnitIndex>& members,
                    Vec2 centroid, const ShipDoctrine& doctrine, UnitIndex target, Approach approach)
{
  MoveGoal goal;
  goal.target = target;
  goal.approach = true;
  const Vec2 at = state.units[target].position;
  goal.range = DesiredRange(state, members, doctrine.range, target);
  Vec2 reference = Normalized(centroid - at);
  if (group != nullptr) {
    if (group->approachTarget != target) {
      group->approachTarget = target;
      group->approachReference = reference;
    }
    reference = group->approachReference;
  }
  if (doctrine.role == Role::Flank && approach == Approach::Direct) {
    approach = Approach::LeftHook;
  }
  switch (approach) {
    case Approach::Direct:
    case Approach::UnderGuns:
      goal.point = at + Normalized(centroid - at) * goal.range;
      break;
    case Approach::LeftHook:
      goal.point = at + Rotated(reference, -kHookAngle) * goal.range;
      break;
    case Approach::RightHook:
      goal.point = at + Rotated(reference, kHookAngle) * goal.range;
      break;
    case Approach::Pincer:
      goal.point = at + Rotated(reference, -kHookAngle) * goal.range;
      goal.secondPoint = at + Rotated(reference, kHookAngle) * goal.range;
      goal.pincer = true;
      break;
    case Approach::Hold:
      goal.point = centroid;
      goal.hold = true;
      break;
  }
  const float radius = LargestRadius(state, members);
  if (approach == Approach::UnderGuns && HasPlanetaryCover(state, side)) {
    goal.point = KeepInCover(state, goal.point, radius);
    goal.secondPoint = KeepInCover(state, goal.secondPoint, radius);
  } else if (doctrine.range == EngagementRange::StandOff && !goal.hold) {
    goal.point = KeepOutOfCover(state, Opponent(side), goal.point, radius);
    goal.secondPoint = KeepOutOfCover(state, Opponent(side), goal.secondPoint, radius);
  }
  goal.point = Clamped(goal.point, kBattleHalfExtent);
  goal.secondPoint = Clamped(goal.secondPoint, kBattleHalfExtent);
  goal.heading = AngleOf(at - goal.point);
  return goal;
}

MoveGoal ScreenGoal(const BattleState& state, Side side, UnitIndex protectedUnit)
{
  MoveGoal goal;
  goal.target = protectedUnit;
  const Vec2 at = state.units[protectedUnit].position;
  const UnitIndex threat = NearestEnemy(state, side, at, false, kScreenThreatRange);
  Vec2 toward;
  if (threat != kNoUnit) {
    toward = Normalized(state.units[threat].position - at);
  } else {
    Vec2 line;
    Vec2 enemy;
    LineAndEnemy(state, side, &line, &enemy);
    toward = Normalized(enemy - at);
  }
  goal.point = Clamped(at + toward * (kScreenDistance + state.Profile(protectedUnit).radius), kBattleHalfExtent);
  goal.heading = AngleOf(toward);
  return goal;
}

MoveGoal BehindLineGoal(const BattleState& state, Side side, float standoff)
{
  Vec2 line;
  Vec2 enemy;
  LineAndEnemy(state, side, &line, &enemy);
  MoveGoal goal;
  goal.point = Clamped(line + Normalized(line - enemy) * standoff, kBattleHalfExtent);
  goal.heading = AngleOf(enemy - goal.point);
  return goal;
}

UnitIndex MovementTarget(const BattleState& state, Side side, UnitIndex focus, const std::vector<ScoredTarget>* candidates,
                         Vec2 from)
{
  if (state.InSpace(focus) && state.units[focus].kind == UnitKind::CapitalShip) {
    return focus;
  }
  if (candidates != nullptr) {
    for (const ScoredTarget& target : *candidates) {
      if (state.InSpace(target.unit) && state.units[target.unit].kind == UnitKind::CapitalShip) {
        return target.unit;
      }
    }
  }
  const UnitIndex ship = NearestEnemy(state, side, from, true, 1.0e9f);
  return ship != kNoUnit ? ship : NearestEnemy(state, side, from, false, 1.0e9f); // nothing but fighters left
}

// Where a set of ships (a group, or one ship acting on its own orders) should be heading.
MoveGoal ComputeGoal(BattleState& state, GroupState* group, Side side, const std::vector<UnitIndex>& members,
                     const ShipDoctrine& doctrine, const std::vector<UnitOrder>& orders, UnitIndex focus)
{
  const Vec2 centroid = CentroidOf(state, members);
  const std::vector<ScoredTarget>* candidates = group != nullptr ? &group->candidates : nullptr;
  if (!orders.empty()) {
    const UnitOrder& order = orders.front();
    switch (order.kind) {
      case OrderKind::Move: {
        MoveGoal goal;
        goal.point = order.position;
        const bool arrived = Distance(centroid, order.position) < kMoveArrivalDistance * 2.0f;
        goal.heading = order.hasFacing ? order.facing
                       : arrived && group != nullptr ? group->goalHeading
                                                     : AngleOf(order.position - centroid);
        return goal;
      }
      case OrderKind::Hold: {
        MoveGoal goal;
        goal.point = order.position;
        goal.hold = true;
        goal.heading = group != nullptr ? group->goalHeading : state.units[members.front()].heading;
        return goal;
      }
      case OrderKind::Attack:
      case OrderKind::Approach:
        if (!order.targets.empty() && state.InSpace(order.targets.front())) {
          const UnitIndex target = order.targets.front();
          if (state.units[target].kind == UnitKind::CapitalShip) {
            Approach approach = order.kind == OrderKind::Approach ? order.approach : doctrine.approach;
            // An explicit attack goes after its target even out of the planetary batteries' cover.
            if (order.kind == OrderKind::Attack && approach == Approach::UnderGuns) {
              approach = Approach::Direct;
            }
            return EngageGoal(state, group, side, members, centroid, doctrine, target, approach);
          }
        }
        break;
      case OrderKind::Escort:
        if (!order.targets.empty() && state.InSpace(order.targets.front())) {
          return ScreenGoal(state, side, order.targets.front());
        }
        break;
      default:
        break;
    }
  }
  switch (doctrine.role) {
    case Role::Reserve:
      if (group == nullptr || !group->committed) {
        MoveGoal goal;
        goal.point = group == nullptr || !group->hasGoal ? centroid : group->goal;
        goal.hold = true;
        goal.heading = group != nullptr ? group->goalHeading : state.units[members.front()].heading;
        return goal;
      }
      break;
    case Role::Screen: {
      UnitIndex protectedUnit = doctrine.screenTarget;
      if (!state.InSpace(protectedUnit) || state.units[protectedUnit].side != side) {
        protectedUnit = HeaviestLineShip(state, side, members.size() == 1 ? members.front() : kNoUnit);
      }
      if (protectedUnit != kNoUnit && std::find(members.begin(), members.end(), protectedUnit) == members.end()) {
        return ScreenGoal(state, side, protectedUnit);
      }
      break;
    }
    case Role::Carrier:
      return BehindLineGoal(state, side, kCarrierStandoff);
    case Role::Picket:
      return BehindLineGoal(state, side, kPicketStandoff);
    default:
      break;
  }
  const UnitIndex target = MovementTarget(state, side, focus, candidates, centroid);
  if (target == kNoUnit && side == state.attacker && SidePresent(state, Opponent(side))) {
    // All that is left of the defence is docked at the planet, its garrison: go and draw it out.
    MoveGoal goal;
    goal.point = state.planetCenter + state.arrivalDirection * (kPlanetRadius + kGarrisonApproach);
    goal.heading = AngleOf(state.planetCenter - centroid);
    return goal;
  }
  if (target == kNoUnit) {
    MoveGoal goal;
    goal.point = centroid;
    goal.hold = true;
    goal.heading = state.units[members.front()].heading;
    return goal;
  }
  return EngageGoal(state, group, side, members, centroid, doctrine, target, doctrine.approach);
}

Vec2 SlotOffset(Formation formation, int slot, int count, float spacing, UnitIndex unit)
{
  const float center = 0.5f * static_cast<float>(count - 1);
  const float k = static_cast<float>(slot);
  switch (formation) {
    case Formation::LineAbreast:
      return {0.0f, (center - k) * spacing};
    case Formation::Column:
      return {(center - k) * spacing, 0.0f};
    case Formation::Wedge:
      return {-std::fabs(k - center) * 0.6f * spacing, (center - k) * spacing};
    case Formation::Ring: {
      const float angle = kTwoPi * k / static_cast<float>(std::max(1, count));
      return FromAngle(angle) * (spacing * std::max(1.0f, static_cast<float>(count) / 3.0f));
    }
    case Formation::Loose: {
      const float jitter = static_cast<float>((unit * 7919) % 5 - 2) * 0.2f * spacing;
      return {jitter, (center - k) * spacing * 1.5f};
    }
  }
  return {};
}

void AssignSlots(BattleState& state, const std::vector<UnitIndex>& members, Formation formation, Vec2 frameCenter,
                 float frameHeading, float spacing)
{
  std::vector<UnitIndex> ordered = members;
  const Vec2 forward = FromAngle(frameHeading);
  const Vec2 left = Rotated(forward, 0.5f * kPi);
  const Vec2 axis = formation == Formation::Column ? forward : left;
  std::stable_sort(ordered.begin(), ordered.end(), [&](UnitIndex a, UnitIndex b) {
    return Dot(state.units[a].position - frameCenter, axis) > Dot(state.units[b].position - frameCenter, axis);
  });
  const int count = static_cast<int>(ordered.size());
  for (int k = 0; k < count; ++k) {
    ShipState& ship = *state.Ship(ordered[static_cast<size_t>(k)]);
    ship.slotOffset = Rotated(SlotOffset(formation, k, count, spacing, ship.unit), frameHeading);
    ship.goal = Clamped(frameCenter + ship.slotOffset, kBattleHalfExtent);
  }
}

void PlaceFormation(BattleState& state, GroupState& group, const std::vector<UnitIndex>& members, Vec2 goalPoint,
                    float goalHeading, Formation formation)
{
  float radius = 0.0f;
  for (UnitIndex member : members) {
    radius = std::max(radius, state.Profile(member).radius);
  }
  const float spacing = kFormationSpacing + 2.0f * radius;
  const Vec2 centroid = CentroidOf(state, members);
  const float lookahead = std::max(group.speed, 50.0f) * kFormationLookaheadSeconds + spacing;
  Vec2 frameCenter = goalPoint;
  float frameHeading = goalHeading;
  if (Distance(centroid, goalPoint) > lookahead) {
    const Vec2 toward = Normalized(goalPoint - centroid);
    frameCenter = centroid + toward * lookahead;
    frameHeading = AngleOf(toward);
  }
  group.frameHeading = frameHeading;
  AssignSlots(state, members, formation, frameCenter, frameHeading, spacing);
}

std::vector<UnitIndex> FollowingMembers(const BattleState& state, const GroupState& group)
{
  std::vector<UnitIndex> members;
  for (UnitIndex member : group.members) {
    if (state.InSpace(member) && !state.Detached(member)) {
      members.push_back(member);
    }
  }
  return members;
}

// A withdrawing ship inside an enemy gravity well runs for the edge of the nearest one (§6.10).
bool EscapeGoal(const BattleState& state, UnitIndex unit, Vec2* goal)
{
  const UnitState& self = state.units[unit];
  float best = 0.0f;
  bool found = false;
  for (const ShipState& ship : state.ships) {
    const UnitState& projector = state.units[ship.unit];
    if (projector.side == self.side || !WellActive(state, ship.unit)) {
      continue;
    }
    const float distance = Distance(projector.position, self.position);
    if (distance <= kGravityWellRadius && (!found || distance < best)) {
      best = distance;
      found = true;
      *goal = Clamped(projector.position
                          + Normalized(self.position - projector.position, state.arrivalDirection)
                                * (kGravityWellRadius + kWellEscapeMargin),
                      kBattleHalfExtent);
    }
  }
  return found;
}

// Steers around the nearest hull on the path ahead, passing it on the side nearer the wanted heading.
float AvoidHulls(const BattleState& state, UnitIndex unit, float heading, float speed)
{
  const UnitState& self = state.units[unit];
  const float radius = state.Profile(unit).radius;
  const Vec2 forward = FromAngle(heading);
  float nearest = 0.0f;
  float avoided = heading;
  bool found = false;
  for (const ShipState& other : state.ships) {
    if (other.unit == unit || !state.InSpace(other.unit)) {
      continue;
    }
    const Vec2 delta = state.units[other.unit].position - self.position;
    const float clearance = radius + state.Profile(other.unit).radius + kSeparationPadding;
    const float distance = Length(delta);
    const float ahead = Dot(delta, forward);
    if (ahead <= 0.0f || distance > clearance + speed * kAvoidLookaheadSeconds || std::fabs(Cross(forward, delta)) >= clearance) {
      continue;
    }
    if (found && distance >= nearest) {
      continue;
    }
    found = true;
    nearest = distance;
    const float bearing = AngleOf(delta);
    const float offset = distance > clearance ? std::asin(std::min(1.0f, clearance / distance)) : 0.5f * kPi;
    const float left = WrapAngle(bearing + offset);
    const float right = WrapAngle(bearing - offset);
    avoided = std::fabs(AngleDelta(heading, left)) < std::fabs(AngleDelta(heading, right)) ? left : right;
  }
  return avoided;
}

void UpdateShipIntent(BattleState& state, UnitIndex unit, const MoveGoal* goal, float firepower)
{
  const ShipState& ship = *state.Ship(unit);
  Intent& intent = state.units[unit].intent;
  const std::vector<UnitOrder>& orders = state.ActiveOrders(unit);
  const OrderKind order = orders.empty() ? OrderKind::Attack : orders.front().kind;
  if (ship.withdrawing) {
    JumpBlocker blocker = JumpBlocker::None;
    const bool free = CanJump(state, unit, &blocker);
    intent.kind = free ? IntentKind::Withdrawing : IntentKind::Trapped;
    intent.blocker = blocker;
    intent.progress = ship.spoolTotal > 0 ? static_cast<float>(ship.spoolTicks) / static_cast<float>(ship.spoolTotal)
                                          : 0.0f;
    return;
  }
  if (ship.engineOffTicks > 0) {
    intent.kind = IntentKind::Disrupted;
    return;
  }
  const bool firing = std::find(ship.batteryTarget.begin(), ship.batteryTarget.end(), ship.primaryTarget)
                      != ship.batteryTarget.end();
  if (!orders.empty() && order == OrderKind::Tractor && ship.tractorTarget != kNoUnit) {
    intent.kind = IntentKind::Tractoring;
    intent.target = ship.tractorTarget;
    return;
  }
  if (!orders.empty() && order == OrderKind::Move && !firing) {
    intent.kind = IntentKind::Moving;
    return;
  }
  if (!orders.empty() && order == OrderKind::Escort && !firing && !orders.front().targets.empty()) {
    intent.kind = IntentKind::Escorting;
    intent.target = orders.front().targets.front();
    return;
  }
  const ShipDoctrine doctrine = state.EffectiveShipDoctrine(unit);
  if (ship.primaryTarget != kNoUnit) {
    intent.target = ship.primaryTarget;
    intent.kind = firing || firepower > 0.0f ? IntentKind::Engaging : IntentKind::Closing;
    const Side side = state.units[unit].side;
    if (intent.kind == IntentKind::Closing && doctrine.approach == Approach::UnderGuns && orders.empty()
        && HasPlanetaryCover(state, side)
        && !InPlanetaryCover(state, side, state.units[ship.primaryTarget].position,
                             state.Profile(ship.primaryTarget).radius)) {
      intent.kind = IntentKind::HoldingCover;
    }
    intent.arc = -1;
    const UnitState& self = state.units[unit];
    const float bearing = AngleDelta(self.heading, AngleOf(state.units[ship.primaryTarget].position - self.position));
    const UnitProfile& profile = state.Profile(unit);
    float best = 0.0f;
    for (int arc = 0; arc < kArcCount; ++arc) {
      if (InArc(static_cast<Arc>(arc), bearing) && profile.arcVsCapital[arc] > best) {
        best = profile.arcVsCapital[arc];
        intent.arc = static_cast<int8_t>(arc);
      }
    }
    return;
  }
  if (!orders.empty() && order == OrderKind::Hold) {
    intent.kind = IntentKind::Holding;
    return;
  }
  const GroupState* group = state.Group(state.units[unit].group);
  if (doctrine.role == Role::Reserve && (group == nullptr || !group->committed)) {
    intent.kind = IntentKind::Holding;
    return;
  }
  if (doctrine.role == Role::Screen && goal != nullptr && goal->target != kNoUnit && !goal->approach) {
    intent.kind = IntentKind::Screening;
    intent.target = goal->target;
    return;
  }
  intent.kind = IntentKind::NoTarget;
  intent.target = kNoUnit;
  intent.range = goal != nullptr ? goal->range : 0.0f;
}

} // namespace

void ThinkGroupMovement(BattleState& state, GroupState& group)
{
  if (group.flight) {
    return;
  }
  const std::vector<UnitIndex> members = FollowingMembers(state, group);
  if (members.empty()) {
    return;
  }
  float speed = 0.0f;
  bool first = true;
  for (UnitIndex member : members) {
    const float memberSpeed = MaxSpeed(state, member);
    speed = first ? memberSpeed : std::min(speed, memberSpeed);
    first = false;
  }
  group.speed = speed;
  const MoveGoal goal = ComputeGoal(state, &group, group.side, members, group.shipDoctrine, group.orders, group.focus);
  group.goal = goal.point;
  group.hasGoal = true;
  group.goalHeading = goal.heading;
  const Formation formation = group.shipDoctrine.formation;
  if (goal.hold) {
    for (UnitIndex member : members) {
      state.Ship(member)->goal = state.units[member].position;
    }
    if (!group.orders.empty() && group.orders.front().kind == OrderKind::Hold) {
      PlaceFormation(state, group, members, goal.point, goal.heading, formation);
    }
    return;
  }
  if (!goal.pincer) {
    PlaceFormation(state, group, members, goal.point, goal.heading, formation);
    return;
  }
  // Pincer: the left half of the group takes one flank, the right half the other.
  std::vector<UnitIndex> ordered = members;
  const Vec2 left = Rotated(FromAngle(goal.heading), 0.5f * kPi);
  const Vec2 centroid = CentroidOf(state, members);
  std::stable_sort(ordered.begin(), ordered.end(), [&](UnitIndex a, UnitIndex b) {
    return Dot(state.units[a].position - centroid, left) > Dot(state.units[b].position - centroid, left);
  });
  const auto half = static_cast<std::ptrdiff_t>((ordered.size() + 1) / 2);
  const std::vector<UnitIndex> first_half(ordered.begin(), ordered.begin() + half);
  const std::vector<UnitIndex> second_half(ordered.begin() + half, ordered.end());
  const Vec2 target = state.units[goal.target].position;
  const bool firstIsLeft = Dot(goal.point - centroid, left) >= Dot(goal.secondPoint - centroid, left);
  PlaceFormation(state, group, first_half, firstIsLeft ? goal.point : goal.secondPoint,
                 AngleOf(target - (firstIsLeft ? goal.point : goal.secondPoint)), formation);
  if (!second_half.empty()) {
    PlaceFormation(state, group, second_half, firstIsLeft ? goal.secondPoint : goal.point,
                   AngleOf(target - (firstIsLeft ? goal.secondPoint : goal.point)), formation);
  }
}

float ChooseHeading(const BattleState& state, UnitIndex unit, Facing facing, Vec2 goal, float* firepowerOut)
{
  const UnitState& self = state.units[unit];
  const ShipState& ship = *state.Ship(unit);
  const UnitProfile& profile = state.Profile(unit);
  struct Aim {
    float bearing; // absolute
    float weight;
    std::array<float, kArcCount> arcDps;
  };
  std::vector<Aim> aims;
  const auto addAim = [&](UnitIndex target, float weight) {
    if (!state.InSpace(target) || state.units[target].kind != UnitKind::CapitalShip) {
      return;
    }
    const Vec2 delta = state.units[target].position - self.position;
    const float distance = Length(delta) - state.Profile(target).radius;
    if (distance > profile.maxRange + kThreatScanRange) {
      return;
    }
    Aim aim{AngleOf(delta), weight, {}};
    for (int i = 0; i < profile.batteryCount; ++i) {
      const BatteryProfile& battery = profile.batteries[i];
      const float reach = distance <= battery.range ? 1.0f : distance <= battery.range + kFacingRangeSlack ? 0.5f : 0.0f;
      aim.arcDps[static_cast<size_t>(battery.arc)] += battery.dpsVsCapital * reach;
    }
    aims.push_back(aim);
  };
  addAim(ship.primaryTarget, 1.0f);
  if (const GroupState* group = state.Group(self.group)) {
    int extra = 0;
    for (const ScoredTarget& target : group->candidates) {
      if (extra >= kFacingSecondaryTargets) {
        break;
      }
      if (target.unit != ship.primaryTarget && state.units[target.unit].kind == UnitKind::CapitalShip) {
        const size_t before = aims.size();
        addAim(target.unit, kFacingSecondaryWeight);
        extra += aims.size() > before ? 1 : 0;
      }
    }
  }
  // Present the strongest arc to the strongest threat nearby.
  int strongestArc = static_cast<int>(std::max_element(profile.arcVsCapital.begin(), profile.arcVsCapital.end())
                                      - profile.arcVsCapital.begin());
  if (facing == Facing::StrongestToThreat) {
    UnitIndex threat = kNoUnit;
    float threatPower = 0.0f;
    for (const ShipState& other : state.ships) {
      const UnitState& enemy = state.units[other.unit];
      if (enemy.side == self.side || !enemy.inSpace
          || DistanceSq(enemy.position, self.position) > kStrongestThreatRange * kStrongestThreatRange) {
        continue;
      }
      const float power = state.Profile(other.unit).firepower * WeaponOutput(state, other.unit);
      if (power > threatPower) {
        threatPower = power;
        threat = other.unit;
      }
    }
    aims.clear();
    addAim(threat, 1.0f);
  }
  const float toGoal = AngleOf(goal - self.position);
  const float goalDistance = Distance(goal, self.position);
  float totalWeight = 0.0f;
  for (const Aim& aim : aims) {
    totalWeight += aim.weight;
  }
  if (aims.empty() || totalWeight <= 0.0f) {
    *firepowerOut = 0.0f;
    return goalDistance > kMoveArrivalDistance ? toGoal : ship.desiredHeading;
  }
  const auto arcAllowed = [&](int arc) {
    switch (facing) {
      case Facing::Auto:
        return true;
      case Facing::BowOn:
        return arc == static_cast<int>(Arc::Fore);
      case Facing::Broadside:
        return arc == static_cast<int>(Arc::Port) || arc == static_cast<int>(Arc::Starboard);
      case Facing::StrongestToThreat:
        return arc == strongestArc;
    }
    return true;
  };
  const float norm = std::max(1e-3f, totalWeight * profile.bestArcVsCapital);
  const float urgency = std::clamp(goalDistance / kGoalUrgencyDistance, 0.0f, 1.0f);
  const float turnRate = std::max(TurnRate(state, unit), 0.01f);
  std::vector<float> samples;
  samples.reserve(kFacingSamples + 4 + 2 * aims.size());
  samples.push_back(self.heading);
  samples.push_back(toGoal);
  for (const Aim& aim : aims) {
    samples.push_back(aim.bearing);
    samples.push_back(WrapAngle(aim.bearing + 0.5f * kPi));
    samples.push_back(WrapAngle(aim.bearing - 0.5f * kPi));
  }
  for (int i = 0; i < kFacingSamples; ++i) {
    samples.push_back(WrapAngle(kTwoPi * static_cast<float>(i) / kFacingSamples));
  }
  float bestScore = -1e9f;
  float bestHeading = self.heading;
  float bestFirepower = 0.0f;
  for (float heading : samples) {
    float firepower = 0.0f;
    for (const Aim& aim : aims) {
      const float relative = AngleDelta(heading, aim.bearing);
      for (int arc = 0; arc < kArcCount; ++arc) {
        if (!arcAllowed(arc)) {
          continue;
        }
        // A target held well inside an arc stays there while both ships move; the corner overlaps do not.
        const float inside = kArcHalfWidth - std::fabs(AngleDelta(ArcCenter(static_cast<Arc>(arc)), relative));
        const float hold = std::clamp(inside / kFacingArcMargin, 0.0f, 1.0f);
        firepower += aim.weight * aim.arcDps[static_cast<size_t>(arc)] * hold;
      }
    }
    firepower /= norm;
    const float movement = std::cos(AngleDelta(heading, toGoal)) * urgency;
    const float turnCost = std::fabs(AngleDelta(self.heading, heading)) / turnRate * kFacingTurnCostPerSecond;
    const float score = firepower + kFacingMovementWeight * movement - turnCost;
    if (score > bestScore) {
      bestScore = score;
      bestHeading = heading;
      bestFirepower = firepower;
    }
  }
  *firepowerOut = bestFirepower;
  return bestHeading;
}

void ThinkShip(BattleState& state, UnitIndex unit)
{
  ShipState& ship = *state.Ship(unit);
  UnitState& self = state.units[unit];
  const ShipDoctrine doctrine = state.EffectiveShipDoctrine(unit);
  GroupState* group = state.Group(self.group);
  MoveGoal goal;
  goal.point = ship.goal;
  goal.heading = group != nullptr ? group->goalHeading : self.heading;
  bool detached = state.Detached(unit);
  if (detached) {
    goal = ComputeGoal(state, nullptr, self.side, {unit}, doctrine, state.ActiveOrders(unit), ship.primaryTarget);
    ship.goal = goal.hold ? self.position : goal.point;
  } else if ((doctrine.breakFormation || doctrine.role == Role::Strike) && group->hasGoal) {
    // Breaking formation: straight for its place at the group's goal, at its own speed.
    ship.goal = Clamped(group->goal + ship.slotOffset, kBattleHalfExtent);
  }
  const std::vector<UnitOrder>& orders = state.ActiveOrders(unit);
  if (!orders.empty() && orders.front().kind == OrderKind::Tractor && !orders.front().targets.empty()
      && state.InSpace(orders.front().targets.front())) {
    const UnitIndex target = orders.front().targets.front();
    const Vec2 at = state.units[target].position;
    ship.goal = at + Normalized(self.position - at) * (kTractorStation * state.Profile(unit).tractorRange + state.Profile(target).radius);
    detached = true;
  }
  Vec2 escape;
  if (ship.withdrawing && EscapeGoal(state, unit, &escape)) {
    ship.goal = escape;
    detached = true;
  }
  const bool arrivedWithFacing = !orders.empty() && orders.front().kind == OrderKind::Move && orders.front().hasFacing
                                 && Distance(self.position, orders.front().position) < kMoveArrivalDistance;
  // An explicit move (or an escape from a gravity well) goes where it was sent; the guns fire at whatever bears.
  const bool mustMove = (!orders.empty() && orders.front().kind == OrderKind::Move && !arrivedWithFacing
                         && Distance(self.position, ship.goal) > kMoveArrivalDistance)
                        || (ship.withdrawing && EscapeGoal(state, unit, &escape));
  float firepower = 0.0f;
  if (arrivedWithFacing) {
    ship.desiredHeading = orders.front().facing;
  } else if (mustMove) {
    ship.desiredHeading = AngleOf(ship.goal - self.position);
  } else {
    ship.desiredHeading = ChooseHeading(state, unit, doctrine.facing, ship.goal, &firepower);
    if (firepower <= 0.0f && Distance(ship.goal, self.position) <= kMoveArrivalDistance) {
      ship.desiredHeading = goal.heading;
    }
  }
  // Speed: slow down near the goal, keep a raking pass going on a broadside, hold while the goal is behind.
  const float maxNow = MaxSpeed(state, unit);
  float cap = maxNow;
  const float goalDistance = Distance(ship.goal, self.position);
  if (!detached && !doctrine.breakFormation && doctrine.role != Role::Strike && group != nullptr) {
    // Station keeping: the formation's speed, plus or minus how far the ship lags its place around the centre.
    const float lag = Dot(CentroidOf(state, FollowingMembers(state, *group)) + ship.slotOffset - self.position,
                          FromAngle(group->frameHeading));
    cap = std::clamp(group->speed + lag * kStationKeepingGain, kStationKeepingFloor * group->speed, maxNow);
  }
  const float align = std::cos(AngleDelta(ship.desiredHeading, AngleOf(ship.goal - self.position)));
  float fraction = std::clamp(goalDistance / kSlowingDistance, 0.0f, 1.0f) * std::max(0.0f, align);
  if (goalDistance <= kMoveArrivalDistance) {
    fraction = 0.0f;
  }
  if (firepower > 0.0f && state.InSpace(ship.primaryTarget)) {
    const float relative
        = AngleDelta(ship.desiredHeading, AngleOf(state.units[ship.primaryTarget].position - self.position));
    const bool broadside = InArc(Arc::Port, relative) || InArc(Arc::Starboard, relative);
    if (broadside && align > kRakeMinAlignment) {
      fraction = std::max(fraction, kRakeSpeedFraction);
    } else if (align > 0.0f) {
      fraction = std::max(fraction, kStationSpeedFraction);
    }
  } else if (goalDistance > kMoveArrivalDistance && align <= 0.0f) {
    fraction = kTurningSpeedFraction;
  }
  ship.desiredSpeed = fraction * cap;
  if (ship.desiredSpeed > 0.0f) {
    ship.desiredHeading = AvoidHulls(state, unit, ship.desiredHeading, ship.desiredSpeed);
  }
  UpdateShipIntent(state, unit, &goal, firepower);
}

void MoveShips(BattleState& state)
{
  for (ShipState& ship : state.ships) {
    UnitState& unit = state.units[ship.unit];
    if (!unit.inSpace) {
      continue;
    }
    const UnitProfile& profile = state.Profile(ship.unit);
    const float rate = TurnRate(state, ship.unit) * kTickSeconds;
    unit.heading = WrapAngle(unit.heading + std::clamp(AngleDelta(unit.heading, ship.desiredHeading), -rate, rate));
    const float limit = MaxSpeed(state, ship.unit);
    const float target = std::min(ship.desiredSpeed, limit);
    const float accel = std::max(profile.acceleration, 1.0f) * kTickSeconds;
    if (ship.speed < target) {
      ship.speed = std::min(target, ship.speed + accel);
    } else {
      ship.speed = std::max(target, ship.speed - accel * kCapitalDecelerationFactor);
    }
    unit.velocity = FromAngle(unit.heading) * ship.speed;
    unit.position += unit.velocity * kTickSeconds;
  }
  // Keep hulls apart, bigger ships yielding less; and out of the Death Star and the planet.
  for (size_t a = 0; a < state.ships.size(); ++a) {
    UnitState& first = state.units[state.ships[a].unit];
    if (!first.inSpace) {
      continue;
    }
    const float firstRadius = state.Profile(state.ships[a].unit).radius;
    for (size_t b = a + 1; b < state.ships.size(); ++b) {
      UnitState& second = state.units[state.ships[b].unit];
      if (!second.inSpace) {
        continue;
      }
      const float secondRadius = state.Profile(state.ships[b].unit).radius;
      const float minimum = firstRadius + secondRadius + kSeparationPadding;
      const Vec2 delta = second.position - first.position;
      const float distanceSq = LengthSq(delta);
      if (distanceSq >= minimum * minimum) {
        continue;
      }
      const float distance = std::sqrt(distanceSq);
      const Vec2 axis = distance > 1e-3f ? delta * (1.0f / distance) : FromAngle(static_cast<float>(a + b));
      const float overlap = minimum - distance;
      // The ship driving into the other yields; otherwise the bigger hull moves less.
      const float firstClosing = std::max(0.0f, Dot(first.velocity, axis));
      const float secondClosing = std::max(0.0f, -Dot(second.velocity, axis));
      float firstShare = secondRadius / (firstRadius + secondRadius);
      if (firstClosing + secondClosing > 1.0f) {
        firstShare = firstClosing / (firstClosing + secondClosing);
      }
      first.position += axis * (-overlap * firstShare);
      second.position += axis * (overlap * (1.0f - firstShare));
    }
    for (const StationState& station : state.stations) {
      const UnitState& body = state.units[station.unit];
      if (!body.inSpace) {
        continue;
      }
      const float minimum = state.Profile(station.unit).radius + firstRadius + kSeparationPadding;
      const Vec2 delta = first.position - body.position;
      if (LengthSq(delta) < minimum * minimum) {
        first.position = body.position + Normalized(delta) * minimum;
      }
    }
    const float planetMinimum = kPlanetRadius + firstRadius;
    const Vec2 fromPlanet = first.position - state.planetCenter;
    if (LengthSq(fromPlanet) < planetMinimum * planetMinimum) {
      first.position = state.planetCenter + Normalized(fromPlanet) * planetMinimum;
    }
    first.position = Clamped(first.position, kBattleHalfExtent);
  }
}

} // namespace reb::battle::sim
