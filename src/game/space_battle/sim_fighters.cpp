#include <algorithm>
#include <cmath>

#include "space_battle/math2d.h"
#include "space_battle/sim_internal.h"

namespace reb::battle::sim
{
namespace
{
using namespace tuning;

Vec2 HangarPosition(const BattleState& state, UnitIndex carrier, Side side)
{
  if (carrier == kNoUnit) {
    return side == state.attacker ? state.arrivalDirection * kAttackerDeployDistance
                                  : state.planetCenter + state.arrivalDirection * kPlanetRadius;
  }
  return state.units[carrier].position;
}

float HangarRadius(const BattleState& state, UnitIndex carrier)
{
  return carrier == kNoUnit ? 0.0f : state.Profile(carrier).radius;
}

Vec2 SlotOffset(int craft)
{
  return {-kSquadronSpacing * static_cast<float>(craft / 4), kSquadronSpacing * (static_cast<float>(craft % 4) - 1.5f)};
}

// Where on a ship's hull each craft aims, so a squadron spreads over its target.
Vec2 AimOffset(int craft, float radius)
{
  return FromAngle(static_cast<float>(craft) * 2.4f) * (radius * 0.5f);
}

Vec2 FleetCenter(const BattleState& state, Side side, Vec2 fallback)
{
  Vec2 sum;
  int count = 0;
  for (const ShipState& ship : state.ships) {
    if (state.units[ship.unit].side == side && state.InSpace(ship.unit)) {
      sum += state.units[ship.unit].position;
      ++count;
    }
  }
  return count > 0 ? sum * (1.0f / static_cast<float>(count)) : fallback;
}

int TorpedoesLeft(const BattleState& state, const SquadronState& squadron)
{
  int total = 0;
  for (int i = 0; i < squadron.craftCount; ++i) {
    const CraftState& craft = state.craft[squadron.firstCraft + i];
    total += craft.alive ? craft.torpedoes : 0;
  }
  return total;
}

float CraftDistance(const BattleState& state, const CraftState& craft, UnitIndex target)
{
  return Distance(craft.position, state.units[target].position) - state.Profile(target).radius;
}

// The best enemy unit of one kind for this squadron, by its priorities, how close it is to dying and distance.
UnitIndex BestTarget(const BattleState& state, const SquadronState& squadron, const FlightDoctrine& doctrine,
                     UnitKind kind, Vec2 center, float radius, int8_t* rank, TargetTag* tag)
{
  const Side side = state.units[squadron.unit].side;
  UnitIndex best = kNoUnit;
  float bestScore = 0.0f;
  for (UnitIndex i = 0; i < static_cast<UnitIndex>(state.units.size()); ++i) {
    const UnitState& unit = state.units[i];
    if (unit.side == side || !unit.inSpace || unit.kind != kind) {
      continue;
    }
    const float distance = Distance(center, unit.position);
    if (distance > radius) {
      continue;
    }
    int8_t candidateRank = -1;
    TargetTag candidateTag = TargetTag::Nearest;
    const float weight = PriorityWeight(doctrine.priorities, state.Tags(i), &candidateRank, &candidateTag);
    const float health = EffectiveHealth(state, i) / std::max(1.0f, MaxEffectiveHealth(state, i));
    const float score = weight * (1.5f - 0.5f * health) / (1.0f + distance / kFighterTargetDistance);
    if (score > bestScore) {
      bestScore = score;
      best = i;
      *rank = candidateRank;
      *tag = candidateTag;
    }
  }
  return best;
}

UnitIndex NearestEnemySquadron(const BattleState& state, Side side, Vec2 from, float radius)
{
  UnitIndex best = kNoUnit;
  float bestDistance = radius;
  for (const SquadronState& other : state.squadrons) {
    if (state.units[other.unit].side == side || other.place != SquadronPlace::InSpace) {
      continue;
    }
    const float distance = Distance(from, other.centroid) - other.spread;
    if (distance < bestDistance) {
      bestDistance = distance;
      best = other.unit;
    }
  }
  return best;
}

// Own carrier first (its slot stays reserved while the squadron flies), then the planet for system-based
// squadrons, then the nearest friendly ship with room; a departing squadron prefers ships that are leaving.
UnitIndex ChooseHangar(const BattleState& state, const SquadronState& squadron, bool* found)
{
  *found = true;
  if (squadron.carrier != kNoUnit && state.InSpace(squadron.carrier)) {
    return squadron.carrier;
  }
  const Side side = state.units[squadron.unit].side;
  if (squadron.carrier == kNoUnit && side != state.attacker && !squadron.withdrawing) {
    return kNoUnit;
  }
  UnitIndex best = kNoUnit;
  float bestScore = 0.0f;
  for (const ShipState& ship : state.ships) {
    if (state.units[ship.unit].side != side || FreeHangarSlots(state, ship.unit) <= 0) {
      continue;
    }
    const float distance = Distance(state.units[ship.unit].position, squadron.centroid);
    const float score = (squadron.withdrawing && ship.withdrawing ? 2.0f : 1.0f) / (1.0f + distance);
    if (score > bestScore) {
      bestScore = score;
      best = ship.unit;
    }
  }
  *found = best != kNoUnit;
  return best;
}

struct Plan {
  SquadronTask task = SquadronTask::Patrol;
  UnitIndex target = kNoUnit;
  Vec2 anchor;
  IntentKind intent = IntentKind::Patrolling;
  int8_t rank = -1;
  TargetTag tag = TargetTag::Nearest;
  bool fromOrder = false;
};

Plan Engage(const BattleState& state, UnitIndex target, bool fromOrder)
{
  Plan plan;
  plan.target = target;
  plan.fromOrder = fromOrder;
  if (state.units[target].kind == UnitKind::Squadron) {
    plan.task = SquadronTask::Intercept;
    plan.intent = IntentKind::Intercepting;
  } else {
    plan.task = SquadronTask::Strike;
    plan.intent = IntentKind::Striking;
  }
  plan.anchor = state.units[target].position;
  return plan;
}

Plan Loiter(Vec2 center, IntentKind intent)
{
  Plan plan;
  plan.task = SquadronTask::Patrol;
  plan.anchor = center;
  plan.intent = intent;
  return plan;
}

bool WasAttacked(const BattleState& state, const SquadronState& squadron)
{
  return state.tick - squadron.lastAttackedTick <= Ticks(kAttackedMemorySeconds);
}

Plan ChooseMission(const BattleState& state, SquadronState& squadron, const FlightDoctrine& doctrine)
{
  const UnitIndex self = squadron.unit;
  const Side side = state.units[self].side;
  const Vec2 fleet = FleetCenter(state, side, squadron.centroid);
  const Vec2 enemyFleet = FleetCenter(state, Opponent(side), fleet);
  const Vec2 front = fleet + Normalized(enemyFleet - fleet) * kFighterScreenAhead;
  const bool hasZone = doctrine.patrolRadius > 0.0f;
  const Vec2 zoneCenter = hasZone ? doctrine.patrolCenter : front;
  Plan plan;
  switch (doctrine.mission) {
    case FlightMission::Escort: {
      const UnitIndex escorted = doctrine.escortTarget;
      if (state.InSpace(escorted) && state.units[escorted].side == side) {
        const Vec2 at = state.units[escorted].position;
        const UnitIndex threat = NearestEnemySquadron(state, side, at, kEscortEngageRange);
        if (threat != kNoUnit) {
          plan = Engage(state, threat, false);
          plan.intent = IntentKind::Intercepting;
          return plan;
        }
        plan = Loiter(at, IntentKind::Escorting);
        plan.task = SquadronTask::Escort;
        plan.target = escorted;
        return plan;
      }
      break;
    }
    case FlightMission::Intercept:
    case FlightMission::Patrol: {
      const float radius = hasZone ? doctrine.patrolRadius
                           : doctrine.mission == FlightMission::Intercept ? kInterceptZone
                                                                          : kPatrolDefaultRadius;
      UnitIndex target
          = BestTarget(state, squadron, doctrine, UnitKind::Squadron, zoneCenter, radius, &plan.rank, &plan.tag);
      if (target == kNoUnit && doctrine.mission == FlightMission::Patrol) {
        target = BestTarget(state, squadron, doctrine, UnitKind::CapitalShip, zoneCenter, radius, &plan.rank, &plan.tag);
      }
      if (target != kNoUnit) {
        const Plan engaged = Engage(state, target, false);
        plan.task = engaged.task;
        plan.target = engaged.target;
        plan.anchor = engaged.anchor;
        plan.intent = engaged.intent;
        return plan;
      }
      return Loiter(zoneCenter, IntentKind::Patrolling);
    }
    case FlightMission::Strike:
    case FlightMission::HoldInHangar:
      break;
  }
  // Strike, and the fallback for a lost escort: fighters met on the way are fought by the dogfight rule.
  const bool dogfight = doctrine.dogfight == DogfightRule::Engage || WasAttacked(state, squadron);
  if (dogfight) {
    const UnitIndex threat = NearestEnemySquadron(state, side, squadron.centroid, kDogfightEngageRange);
    if (threat != kNoUnit) {
      return Engage(state, threat, false);
    }
  }
  UnitIndex target
      = BestTarget(state, squadron, doctrine, UnitKind::CapitalShip, squadron.centroid, 1.0e9f, &plan.rank, &plan.tag);
  if (target == kNoUnit) {
    target = BestTarget(state, squadron, doctrine, UnitKind::Squadron, squadron.centroid, 1.0e9f, &plan.rank, &plan.tag);
  }
  if (target != kNoUnit) {
    const Plan engaged = Engage(state, target, false);
    plan.task = engaged.task;
    plan.target = engaged.target;
    plan.anchor = engaged.anchor;
    plan.intent = engaged.intent;
    return plan;
  }
  return Loiter(zoneCenter, IntentKind::Patrolling);
}

void ThinkSquadronInSpace(BattleState& state, SquadronState& squadron)
{
  const UnitIndex self = squadron.unit;
  const FlightDoctrine doctrine = state.EffectiveFlightDoctrine(self);
  const std::vector<UnitOrder>& orders = state.ActiveOrders(self);
  const UnitProfile& profile = state.Profile(self);
  const OrderKind order = orders.empty() ? OrderKind::Hold : orders.front().kind;
  const bool hasOrder = !orders.empty();

  // Returning to a hangar: by order, by doctrine, or with nothing left to fight with.
  bool wantReturn = false;
  bool stay = false;
  if (hasOrder && (order == OrderKind::Recall || order == OrderKind::Recover)) {
    wantReturn = true;
    stay = order == OrderKind::Recall || squadron.withdrawing;
  } else if (!hasOrder && doctrine.mission == FlightMission::HoldInHangar) {
    wantReturn = true;
    stay = true;
  } else if (!hasOrder && doctrine.rearm == RearmRule::WhenTorpedoesSpent && profile.torpedoesPerCraft > 0
             && TorpedoesLeft(state, squadron) == 0) {
    wantReturn = true;
  } else if (!hasOrder && doctrine.rearm == RearmRule::AtHalfLosses && squadron.alive * 2 < squadron.initialAlive) {
    wantReturn = true;
    stay = true;
  }
  Plan plan;
  bool planned = false;
  // Docking with nothing else of its side in space would concede the battle (§6.11), so it fights on.
  if (wantReturn && !squadron.noHangar && CoveredWhenDocked(state, self)) {
    bool found = false;
    const UnitIndex hangar = ChooseHangar(state, squadron, &found);
    if (found) {
      squadron.carrier = hangar;
      squadron.stayAfterDock = stay;
      plan.task = SquadronTask::Return;
      plan.target = hangar;
      plan.anchor = HangarPosition(state, hangar, state.units[self].side);
      plan.intent = IntentKind::Returning;
      planned = true;
    } else {
      squadron.noHangar = true; // stays behind and fights on
    }
  }
  if (!planned && hasOrder) {
    const UnitOrder& front = orders.front();
    const UnitIndex target = front.targets.empty() ? kNoUnit : front.targets.front();
    switch (order) {
      case OrderKind::Attack:
      case OrderKind::Strike:
      case OrderKind::Approach:
        if (state.InSpace(target)) {
          plan = Engage(state, target, true);
          planned = true;
        }
        break;
      case OrderKind::Escort:
        if (state.InSpace(target)) {
          plan = Loiter(state.units[target].position, IntentKind::Escorting);
          plan.task = SquadronTask::Escort;
          plan.target = target;
          const UnitIndex threat = NearestEnemySquadron(state, state.units[self].side, plan.anchor, kEscortEngageRange);
          if (threat != kNoUnit) {
            plan = Engage(state, threat, true);
          }
          planned = true;
        }
        break;
      case OrderKind::Move:
        plan = Loiter(front.position, IntentKind::Moving);
        planned = true;
        break;
      case OrderKind::Hold:
        plan = Loiter(front.position, IntentKind::Holding);
        planned = true;
        break;
      case OrderKind::Run:
        if (state.InSpace(target)) {
          plan.task = SquadronTask::TrenchRun;
          plan.target = target;
          plan.anchor = state.units[target].position;
          plan.intent = IntentKind::TrenchRun;
          plan.fromOrder = true;
          planned = true;
        }
        break;
      default:
        break;
    }
  }
  if (!planned) {
    plan = ChooseMission(state, squadron, doctrine);
  }
  if (plan.target != squadron.target) {
    for (int i = 0; i < squadron.craftCount; ++i) {
      CraftState& craft = state.craft[squadron.firstCraft + i];
      craft.targetCraft = -1;
      if (craft.phase == CraftPhase::Formation) {
        craft.phase = CraftPhase::Approach;
      }
    }
  }
  squadron.task = plan.task;
  squadron.target = plan.target;
  squadron.anchor = plan.anchor;
  Intent& intent = state.units[self].intent;
  intent.kind = squadron.withdrawing && squadron.spoolTotal > 0 ? IntentKind::Withdrawing : plan.intent;
  intent.target = plan.target;
  intent.priorityRank = plan.rank;
  intent.tag = plan.tag;
  intent.fromOrder = plan.fromOrder;
  intent.progress = squadron.spoolTotal > 0 ? static_cast<float>(squadron.spoolTicks) / static_cast<float>(squadron.spoolTotal)
                                            : 0.0f;
}

void Steer(CraftState& craft, Vec2 aim, float turnRate, float speed)
{
  const float step = turnRate * kTickSeconds;
  craft.heading = WrapAngle(craft.heading + std::clamp(AngleDelta(craft.heading, AngleOf(aim - craft.position)), -step, step));
  craft.position += FromAngle(craft.heading) * (speed * kTickSeconds);
}

void FlyAttackRun(const BattleState& state, CraftState& craft, int index, UnitIndex target, float turnRate, float speed)
{
  const float radius = state.Profile(target).radius;
  const Vec2 aim = state.units[target].position + AimOffset(index, radius);
  if (craft.phase == CraftPhase::BreakOff || craft.phase == CraftPhase::Extend) {
    Steer(craft, craft.position + FromAngle(craft.breakHeading) * 1000.0f, turnRate, speed);
    --craft.phaseTicks;
    if (craft.phaseTicks <= 0 || CraftDistance(state, craft, target) > kExtendDistance) {
      craft.phase = CraftPhase::Approach;
    }
    return;
  }
  craft.phase = CraftPhase::Approach;
  Steer(craft, aim, turnRate, speed);
  if (CraftDistance(state, craft, target) < kBreakOffDistance) {
    craft.phase = CraftPhase::Extend;
    craft.breakHeading = WrapAngle(craft.heading + (index % 2 == 0 ? kBreakOffAngle : -kBreakOffAngle));
    craft.phaseTicks = static_cast<int16_t>(Ticks(kExtendSeconds));
  }
}

// The trench run: straight for the station, then along its surface until the port (§6.12).
void FlyTrenchRun(const BattleState& state, const SquadronState& squadron, CraftState& craft, int index, float turnRate,
                  float speed)
{
  const Vec2 center = state.units[squadron.target].position;
  if (squadron.runTicks <= 0) {
    craft.phase = CraftPhase::Approach;
    Steer(craft, center + AimOffset(index, state.Profile(squadron.target).radius), turnRate, speed);
    return;
  }
  const float lane = state.Profile(squadron.target).radius + kTrenchDepth + kSquadronSpacing * static_cast<float>(index % 3);
  const float angle = AngleOf(craft.position - center) + 0.3f;
  Steer(craft, center + FromAngle(angle) * lane, 2.0f * turnRate, speed);
}

int32_t NearestCraft(const BattleState& state, Vec2 from, const SquadronState& squadron)
{
  int32_t best = -1;
  float bestDistance = 0.0f;
  for (int i = 0; i < squadron.craftCount; ++i) {
    const CraftState& craft = state.craft[squadron.firstCraft + i];
    if (!craft.alive) {
      continue;
    }
    const float distance = DistanceSq(from, craft.position);
    if (best < 0 || distance < bestDistance) {
      best = squadron.firstCraft + i;
      bestDistance = distance;
    }
  }
  return best;
}

bool CraftOf(const SquadronState& squadron, int32_t craft)
{
  return craft >= squadron.firstCraft && craft < squadron.firstCraft + squadron.craftCount;
}

void FlyDogfight(const BattleState& state, CraftState& craft, const SquadronState& enemy, float turnRate, float speed)
{
  if (craft.targetCraft < 0 || !CraftOf(enemy, craft.targetCraft) || !state.craft[craft.targetCraft].alive
      || craft.phaseTicks <= 0) {
    craft.targetCraft = NearestCraft(state, craft.position, enemy);
    craft.phaseTicks = static_cast<int16_t>(Ticks(kDogfightRetargetSeconds));
  }
  --craft.phaseTicks;
  if (craft.targetCraft < 0) {
    return;
  }
  const CraftState& other = state.craft[craft.targetCraft];
  if (craft.phase == CraftPhase::Extend) {
    Steer(craft, craft.position + FromAngle(craft.breakHeading) * 1000.0f, turnRate, speed);
    if (craft.phaseTicks <= Ticks(kOvershootSeconds - 1.0f)) {
      craft.phase = CraftPhase::Pursue;
    }
    return;
  }
  craft.phase = CraftPhase::Pursue;
  const float distance = Distance(craft.position, other.position);
  const float lead = std::min(2.0f, distance / std::max(speed, 1.0f));
  const float otherSpeed = state.Profile(enemy.unit).maxSpeed;
  Steer(craft, other.position + FromAngle(other.heading) * (otherSpeed * lead), turnRate, speed);
  if (distance < kOvershootDistance
      && std::fabs(AngleDelta(craft.heading, AngleOf(other.position - craft.position))) < 0.5f * kPi) {
    craft.phase = CraftPhase::Extend;
    craft.breakHeading = WrapAngle(craft.heading + kOvershootTurn);
    craft.phaseTicks = static_cast<int16_t>(Ticks(kOvershootSeconds));
  }
}

void FlyFormation(const BattleState& state, const SquadronState& squadron, CraftState& craft, int index, float turnRate,
                  float speed)
{
  // Fighters cannot hover: they circle their anchor in formation.
  const float orbit = squadron.task == SquadronTask::Escort ? kEscortStation : kLoiterOrbit;
  const float angular = kLoiterSpeedFraction * speed / orbit;
  const float angle = static_cast<float>(state.tick) * kTickSeconds * angular + static_cast<float>(squadron.unit);
  const Vec2 leader = squadron.anchor + FromAngle(angle) * orbit;
  const Vec2 slot = leader + Rotated(SlotOffset(index), angle + 0.5f * kPi);
  const float distance = Distance(craft.position, slot);
  craft.phase = CraftPhase::Formation;
  Steer(craft, slot, turnRate, distance < 2.5f * kSquadronSpacing ? 0.75f * speed : speed);
}

float CraftAccuracy(const BattleState& state, UnitIndex shooter, UnitIndex target, float base)
{
  return base * (1.0f + CommanderBonus(state, shooter, kCommanderAccuracyBonus))
         * (1.0f - CommanderBonus(state, target, kCommanderEvasionBonus));
}

void CraftShot(BattleState& state, const SquadronState& squadron, const CraftState& craft, UnitIndex target,
               int32_t targetCraft, ShotKind kind, float damage, float accuracy, float speed)
{
  PendingShot shot;
  shot.fireTick = state.tick;
  shot.shooter = squadron.unit;
  shot.target = target;
  shot.targetCraft = targetCraft;
  shot.side = state.units[squadron.unit].side;
  shot.kind = kind;
  shot.damage = damage;
  shot.hit = state.rng.Chance(std::min(kMaxHitChance, accuracy));
  shot.from = craft.position;
  if (targetCraft >= 0) {
    const CraftState& other = state.craft[targetCraft];
    shot.to = other.position;
  } else {
    shot.to = state.units[target].position;
    const ShipState* ship = state.Ship(target);
    shot.shieldImpact = ship != nullptr && ship->shield > 0.0f;
    const Vec2 toward = Normalized(shot.from - shot.to);
    shot.to = shot.to + toward * (state.Profile(target).radius * 0.6f);
  }
  if (!shot.hit) {
    shot.to = shot.to + Rotated(Normalized(shot.to - shot.from), 0.5f * kPi) * kCraftMissOffset;
  }
  QueueShot(state, shot, speed);
}

bool WantsLaunch(const BattleState& state, const SquadronState& squadron)
{
  const UnitState& unit = state.units[squadron.unit];
  if (squadron.place != SquadronPlace::Docked || squadron.stayDocked || squadron.withdrawing || squadron.rearmTicks > 0
      || unit.fate != UnitFate::Active || squadron.alive <= 0) {
    return false;
  }
  const FlightDoctrine doctrine = state.EffectiveFlightDoctrine(squadron.unit);
  if (doctrine.mission == FlightMission::HoldInHangar && !squadron.launchOrdered) {
    return false;
  }
  switch (doctrine.launch) {
    case LaunchRule::Immediately:
      return true;
    case LaunchRule::WhenEnemyFightersCommit:
      return squadron.launchOrdered
             || NearestEnemySquadron(state, unit.side, HangarPosition(state, squadron.carrier, unit.side), kCommitRange)
                    != kNoUnit;
    case LaunchRule::OnOrder:
      return squadron.launchOrdered;
  }
  return false;
}

// One squadron leaves a hangar per interval: 5 s plus 0-7 s, three times as fast under Scramble (§6.8).
void RunLaunchBay(BattleState& state, UnitIndex carrier, Side side, int32_t& timer)
{
  timer -= state.sides[SideIndex(side)].scrambleTicks > 0 ? static_cast<int32_t>(kScrambleRate) : 1;
  if (timer > 0) {
    return;
  }
  timer = 0;
  for (SquadronState& squadron : state.squadrons) {
    if (squadron.carrier == carrier && state.units[squadron.unit].side == side && WantsLaunch(state, squadron)) {
      LaunchSquadron(state, squadron.unit);
      timer = Ticks(kLaunchBaseSeconds) + state.rng.Range(0, kLaunchRandomSeconds * kTicksPerSecond);
      return;
    }
  }
}

void ClearReturnOrders(BattleState& state, UnitIndex unit)
{
  const auto isReturn = [](const UnitOrder& order) {
    return order.kind == OrderKind::Recover || order.kind == OrderKind::Recall;
  };
  std::erase_if(state.units[unit].orders, isReturn);
  if (GroupState* group = state.Group(state.units[unit].group)) {
    const bool allAboard = std::none_of(group->members.begin(), group->members.end(),
                                        [&](UnitIndex member) { return state.InSpace(member); });
    if (allAboard) {
      std::erase_if(group->orders, isReturn);
    }
  }
}

} // namespace

int32_t FreeHangarSlots(const BattleState& state, UnitIndex carrier)
{
  if (!state.InSpace(carrier)) {
    return 0;
  }
  int32_t used = 0;
  for (const SquadronState& squadron : state.squadrons) {
    const bool holdsSlot = squadron.place == SquadronPlace::Docked || squadron.place == SquadronPlace::InSpace;
    used += squadron.carrier == carrier && holdsSlot ? 1 : 0;
  }
  return std::max(0, state.Profile(carrier).hangarCapacity - used);
}

void RehomeSquadrons(BattleState& state, UnitIndex lostCarrier)
{
  int stranded = 0;
  const Side side = state.units[lostCarrier].side;
  for (SquadronState& squadron : state.squadrons) {
    if (squadron.carrier != lostCarrier || squadron.place != SquadronPlace::InSpace) {
      continue;
    }
    UnitIndex best = kNoUnit;
    float bestDistance = 0.0f;
    for (const ShipState& ship : state.ships) {
      if (state.units[ship.unit].side != side || ship.unit == lostCarrier || FreeHangarSlots(state, ship.unit) <= 0) {
        continue;
      }
      const float distance = DistanceSq(state.units[ship.unit].position, squadron.centroid);
      if (best == kNoUnit || distance < bestDistance) {
        best = ship.unit;
        bestDistance = distance;
      }
    }
    squadron.carrier = best;
    squadron.noHangar = best == kNoUnit;
    stranded += best == kNoUnit ? 1 : 0;
  }
  if (stranded > 0) {
    LogEvent(state, side,
             std::to_string(stranded) + " squadrons of " + UnitName(state, lostCarrier) + " have no hangar to return to",
             lostCarrier);
  }
}

void LaunchSquadron(BattleState& state, UnitIndex unit)
{
  SquadronState& squadron = *state.Squadron(unit);
  UnitState& self = state.units[unit];
  const Vec2 origin = HangarPosition(state, squadron.carrier, self.side);
  const float heading = squadron.carrier != kNoUnit ? state.units[squadron.carrier].heading
                                                    : AngleOf(state.arrivalDirection * (self.side == state.attacker ? -1.0f : 1.0f));
  const Vec2 exit = origin + FromAngle(heading + 0.5f * kPi) * (HangarRadius(state, squadron.carrier) + kLaunchOffset);
  squadron.place = SquadronPlace::InSpace;
  squadron.launched = true;
  squadron.anchor = exit;
  squadron.task = SquadronTask::Idle;
  squadron.target = kNoUnit;
  squadron.launchOrdered = false;
  squadron.nextThinkTick = static_cast<int32_t>(state.tick);
  self.inSpace = true;
  self.position = self.previousPosition = exit;
  self.heading = self.previousHeading = heading;
  for (int i = 0; i < squadron.craftCount; ++i) {
    CraftState& craft = state.craft[squadron.firstCraft + i];
    craft.position = exit + Rotated(SlotOffset(i), heading);
    craft.previousPosition = craft.position;
    craft.heading = heading;
    craft.previousHeading = heading;
    craft.phase = CraftPhase::Formation;
    craft.targetCraft = -1;
  }
  squadron.centroid = exit;
  LogEvent(state, self.side, UnitName(state, unit) + " launched", unit);
}

void DockSquadron(BattleState& state, UnitIndex unit, UnitIndex carrier, bool rearm)
{
  SquadronState& squadron = *state.Squadron(unit);
  UnitState& self = state.units[unit];
  squadron.place = SquadronPlace::Docked;
  squadron.carrier = carrier;
  squadron.target = kNoUnit;
  squadron.task = SquadronTask::Idle;
  squadron.rearmTicks = rearm ? Ticks(kRearmSeconds) : 0;
  self.inSpace = false;
  self.intent.kind = rearm ? IntentKind::Rearming : IntentKind::Docked;
  self.intent.target = carrier;
}

void UpdateSquadronBounds(BattleState& state)
{
  for (SquadronState& squadron : state.squadrons) {
    UnitState& self = state.units[squadron.unit];
    if (squadron.place != SquadronPlace::InSpace) {
      if (squadron.place == SquadronPlace::Docked) {
        self.position = HangarPosition(state, squadron.carrier, self.side);
        squadron.centroid = self.position;
        squadron.spread = 0.0f;
      }
      continue;
    }
    Vec2 sum;
    Vec2 headings;
    int count = 0;
    for (int i = 0; i < squadron.craftCount; ++i) {
      const CraftState& craft = state.craft[squadron.firstCraft + i];
      if (craft.alive) {
        sum += craft.position;
        headings += FromAngle(craft.heading);
        ++count;
      }
    }
    if (count == 0) {
      continue;
    }
    squadron.centroid = sum * (1.0f / static_cast<float>(count));
    float spread = 0.0f;
    for (int i = 0; i < squadron.craftCount; ++i) {
      const CraftState& craft = state.craft[squadron.firstCraft + i];
      if (craft.alive) {
        spread = std::max(spread, Distance(craft.position, squadron.centroid));
      }
    }
    squadron.spread = spread + kCraftRadius;
    self.velocity = (squadron.centroid - self.position) * static_cast<float>(kTicksPerSecond);
    self.position = squadron.centroid;
    self.heading = AngleOf(headings);
  }
}

void ThinkSquadrons(BattleState& state)
{
  for (SquadronState& squadron : state.squadrons) {
    if (state.tick < squadron.nextThinkTick) {
      continue;
    }
    squadron.nextThinkTick = static_cast<int32_t>(state.tick) + Ticks(kSquadronThinkSeconds);
    UnitState& unit = state.units[squadron.unit];
    if (squadron.place == SquadronPlace::Docked) {
      unit.intent.kind = squadron.rearmTicks > 0 ? IntentKind::Rearming : IntentKind::Docked;
      unit.intent.target = squadron.carrier;
      continue;
    }
    if (squadron.place == SquadronPlace::InSpace) {
      ThinkSquadronInSpace(state, squadron);
    }
  }
}

void FlyCraft(BattleState& state)
{
  for (SquadronState& squadron : state.squadrons) {
    if (squadron.place != SquadronPlace::InSpace) {
      continue;
    }
    const UnitIndex unit = squadron.unit;
    const float turnRate = TurnRate(state, unit);
    const float speed = MaxSpeed(state, unit);
    const bool targetUp = state.InSpace(squadron.target);
    if (squadron.task == SquadronTask::Escort && targetUp) {
      squadron.anchor = state.units[squadron.target].position;
    }
    if (squadron.task == SquadronTask::Return) {
      squadron.anchor = HangarPosition(state, squadron.carrier, state.units[unit].side);
    }
    for (int i = 0; i < squadron.craftCount; ++i) {
      CraftState& craft = state.craft[squadron.firstCraft + i];
      if (!craft.alive) {
        continue;
      }
      if (state.endReason != EndReason::None) {
        FlyFormation(state, squadron, craft, i, turnRate, speed);
        craft.position = Clamped(craft.position, kBattleHalfExtent);
        continue;
      }
      switch (squadron.task) {
        case SquadronTask::TrenchRun:
          if (targetUp) {
            FlyTrenchRun(state, squadron, craft, i, turnRate, speed);
          } else {
            FlyFormation(state, squadron, craft, i, turnRate, speed);
          }
          break;
        case SquadronTask::Strike:
          if (targetUp) {
            FlyAttackRun(state, craft, i, squadron.target, turnRate, speed);
          } else {
            FlyFormation(state, squadron, craft, i, turnRate, speed);
          }
          break;
        case SquadronTask::Intercept:
          if (const SquadronState* enemy = state.Squadron(squadron.target); enemy && targetUp) {
            FlyDogfight(state, craft, *enemy, turnRate, speed);
          } else {
            FlyFormation(state, squadron, craft, i, turnRate, speed);
          }
          break;
        case SquadronTask::Return:
          Steer(craft, squadron.anchor + Rotated(SlotOffset(i), craft.heading), turnRate, speed);
          break;
        default:
          FlyFormation(state, squadron, craft, i, turnRate, speed);
          break;
      }
      craft.position = Clamped(craft.position, kBattleHalfExtent);
    }
  }
}

void FireCraft(BattleState& state)
{
  for (SquadronState& squadron : state.squadrons) {
    if (squadron.place != SquadronPlace::InSpace) {
      continue;
    }
    const UnitIndex unit = squadron.unit;
    const UnitProfile& profile = state.Profile(unit);
    const UnitIndex target = squadron.target;
    const bool engaging = (squadron.task == SquadronTask::Strike || squadron.task == SquadronTask::Intercept)
                          && state.InSpace(target);
    const SquadronState* enemy = engaging ? state.Squadron(target) : nullptr;
    const float turn = TurnPoints(state, unit);
    for (int i = 0; i < squadron.craftCount; ++i) {
      CraftState& craft = state.craft[squadron.firstCraft + i];
      if (!craft.alive) {
        continue;
      }
      craft.gunTicks = static_cast<int16_t>(std::max(0, craft.gunTicks - 1));
      craft.torpedoTicks = static_cast<int16_t>(std::max(0, craft.torpedoTicks - 1));
      if (!engaging) {
        continue;
      }
      if (enemy != nullptr) {
        if (craft.targetCraft < 0 || !CraftOf(*enemy, craft.targetCraft) || !state.craft[craft.targetCraft].alive
            || craft.gunTicks > 0 || profile.craftGunVsFighter <= 0.0f) {
          continue;
        }
        const CraftState& other = state.craft[craft.targetCraft];
        const float distance = Distance(craft.position, other.position);
        const float off = std::fabs(AngleDelta(craft.heading, AngleOf(other.position - craft.position)));
        if (distance <= profile.craftGunRange && off <= kCraftGunCone) {
          const float accuracy = CraftAccuracy(state, unit, target, kCraftVsCraftAccuracy * Evasion(turn, TurnPoints(state, target)));
          CraftShot(state, squadron, craft, target, craft.targetCraft, ShotKind::Laser,
                    profile.craftGunVsFighter * kCraftGunSeconds, accuracy, kBoltSpeed[2]);
          craft.gunTicks = static_cast<int16_t>(Ticks(kCraftGunSeconds));
        }
        continue;
      }
      if (craft.phase != CraftPhase::Approach) {
        continue;
      }
      const float distance = CraftDistance(state, craft, target);
      const float off = std::fabs(AngleDelta(craft.heading, AngleOf(state.units[target].position - craft.position)));
      const float accuracy = CraftAccuracy(state, unit, target, kCraftVsShipAccuracy);
      if (craft.torpedoes > 0 && craft.torpedoTicks <= 0 && distance <= profile.torpedoRange && off <= kCraftTorpedoCone) {
        const int salvo = std::min<int>(craft.torpedoes, kTorpedoSalvo);
        for (int t = 0; t < salvo; ++t) {
          CraftShot(state, squadron, craft, target, -1, ShotKind::Torpedo, kTorpedoDamage,
                    CraftAccuracy(state, unit, target, kTorpedoAccuracy), kTorpedoSpeed);
        }
        craft.torpedoes = static_cast<int16_t>(craft.torpedoes - salvo);
        craft.torpedoTicks = static_cast<int16_t>(Ticks(kTorpedoSeconds));
      }
      if (craft.gunTicks <= 0 && off <= kCraftGunCone) {
        bool fired = false;
        if (profile.craftGunVsCapital > 0.0f && distance <= profile.craftGunRange) {
          CraftShot(state, squadron, craft, target, -1, ShotKind::Laser, profile.craftGunVsCapital * kCraftGunSeconds,
                    accuracy, kBoltSpeed[2]);
          fired = true;
        }
        if (profile.craftIonVsCapital > 0.0f && distance <= profile.craftIonRange) {
          CraftShot(state, squadron, craft, target, -1, ShotKind::Ion, profile.craftIonVsCapital * kCraftGunSeconds,
                    accuracy, kBoltSpeed[1]);
          fired = true;
        }
        if (fired) {
          craft.gunTicks = static_cast<int16_t>(Ticks(kCraftGunSeconds));
        }
      }
    }
  }
}

void UpdateHangars(BattleState& state)
{
  for (ShipState& ship : state.ships) {
    if (state.InSpace(ship.unit) && state.Profile(ship.unit).hangarCapacity > 0) {
      RunLaunchBay(state, ship.unit, state.units[ship.unit].side, ship.launchTicks);
    }
  }
  for (StationState& station : state.stations) {
    if (state.InSpace(station.unit)) {
      RunLaunchBay(state, station.unit, state.units[station.unit].side, station.launchTicks);
    }
  }
  const Side defender = Opponent(state.attacker);
  RunLaunchBay(state, kNoUnit, defender, state.sides[SideIndex(defender)].planetLaunchTicks);

  for (SquadronState& squadron : state.squadrons) {
    if (squadron.place == SquadronPlace::Docked && squadron.rearmTicks > 0 && --squadron.rearmTicks == 0) {
      const UnitProfile& profile = state.Profile(squadron.unit);
      for (int i = 0; i < squadron.craftCount; ++i) {
        CraftState& craft = state.craft[squadron.firstCraft + i];
        if (craft.alive) {
          craft.torpedoes = static_cast<int16_t>(profile.torpedoesPerCraft);
          craft.shield = profile.craftShield;
        }
      }
    }
    if (squadron.place != SquadronPlace::InSpace || squadron.task != SquadronTask::Return) {
      continue;
    }
    const UnitIndex carrier = squadron.carrier;
    const Side side = state.units[squadron.unit].side;
    if ((carrier != kNoUnit && !state.InSpace(carrier)) || !CoveredWhenDocked(state, squadron.unit)) {
      squadron.task = SquadronTask::Idle;
      squadron.nextThinkTick = static_cast<int32_t>(state.tick);
      continue;
    }
    const float reach = kDockDistance + HangarRadius(state, carrier) + squadron.spread;
    if (DistanceSq(squadron.centroid, HangarPosition(state, carrier, side)) <= reach * reach) {
      const bool stay = squadron.stayAfterDock || squadron.withdrawing;
      DockSquadron(state, squadron.unit, carrier, !stay);
      squadron.stayDocked = stay;
      ClearReturnOrders(state, squadron.unit);
      LogEvent(state, side, UnitName(state, squadron.unit) + (stay ? " docked" : " docked to rearm"), squadron.unit);
    }
  }
}

} // namespace reb::battle::sim
