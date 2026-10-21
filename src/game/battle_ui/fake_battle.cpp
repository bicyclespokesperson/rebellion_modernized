#include "battle_ui/fake_battle.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "battle_ui/doctrine_edit.h"
#include "space_battle/battle.h"

namespace reb::battle_ui
{
namespace
{

using battle::kNoUnit;
using battle::Side;
using battle::UnitIndex;
using battle::Vec2;
using battle::ViewEffectKind;
using battle::ViewShotKind;
using battle::ViewUnitKind;

constexpr float kPiF = std::numbers::pi_v<float>;
constexpr float kDt = battle::kTickSeconds;
constexpr float kWellRadius = 7000.0f;
constexpr float kTractorRange = 3000.0f;

float Wrap(float angle)
{
  while (angle > kPiF) {
    angle -= 2.0f * kPiF;
  }
  while (angle < -kPiF) {
    angle += 2.0f * kPiF;
  }
  return angle;
}

float Bearing(Vec2 from, Vec2 to)
{
  return std::atan2(to.y - from.y, to.x - from.x);
}

float Dist(Vec2 a, Vec2 b)
{
  return std::hypot(a.x - b.x, a.y - b.y);
}

Vec2 Lerp(Vec2 a, Vec2 b, float t)
{
  return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

struct ClassSpec {
  float speed;
  float turn;
  float hull;
  float shield;
  bool broadside;
  float fireInterval;
  // fore, aft, port, starboard x turbolaser, ion, laser; ranges in range points
  battle::WeaponGrid weapons;
};

ClassSpec SpecFor(int32_t typeIndex)
{
  switch (typeIndex) {
    case 0: // Mon Calamari cruiser
      return {240,
              0.35f,
              1500,
              600,
              true,
              1.1f,
              {{{{60, 30, 10}, {20, 10, 10}, {110, 50, 10}, {110, 50, 10}}}, {55, 45, 20}}};
    case 2: // Assault Frigate
      return {300, 0.5f, 700, 500, true, 1.0f, {{{{40, 0, 20}, {0, 0, 0}, {80, 0, 20}, {80, 0, 20}}}, {45, 0, 20}}};
    case 3: // Nebulon-B
      return {300, 0.45f, 600, 200, false, 1.0f, {{{{60, 0, 30}, {0, 0, 30}, {10, 0, 30}, {10, 0, 30}}}, {50, 0, 20}}};
    case 4:  // Alliance Escort Carrier
    case 27: // Imperial Escort Carrier
      return {120, 0.2f, 600, 200, false, 1.6f, {{{{0, 0, 30}, {0, 0, 30}, {0, 0, 30}, {0, 0, 30}}}, {0, 0, 20}}};
    case 5: // Corellian Corvette
      return {480, 0.8f, 300, 100, false, 0.7f, {{{{0, 0, 40}, {0, 0, 40}, {0, 0, 40}, {0, 0, 40}}}, {0, 0, 22}}};
    case 16: // Lancer
      return {300, 0.6f, 400, 150, false, 0.6f, {{{{0, 0, 60}, {0, 0, 60}, {0, 0, 60}, {0, 0, 60}}}, {0, 0, 22}}};
    case 17: // Interdictor
      return {180, 0.3f, 900, 600, false, 1.5f, {{{{20, 0, 40}, {0, 0, 20}, {10, 0, 40}, {10, 0, 40}}}, {40, 0, 20}}};
    case 19: // Victory
      return {240, 0.4f, 1200, 200, false, 1.2f, {{{{90, 0, 0}, {0, 0, 0}, {80, 0, 0}, {80, 0, 0}}}, {50, 0, 0}}};
    case 20: // Imperial Star Destroyer
      return {240,
              0.35f,
              2200,
              400,
              false,
              1.0f,
              {{{{100, 100, 20}, {20, 0, 10}, {50, 30, 10}, {50, 30, 10}}}, {60, 50, 20}}};
    default:
      return {300, 0.5f, 600, 200, false, 1.2f, {{{{40, 0, 20}, {10, 0, 10}, {30, 0, 20}, {30, 0, 20}}}, {45, 0, 20}}};
  }
}

struct FighterSpec {
  float speed;
  float turn;
  float shieldPerCraft;
  bool torpedoes;
};

FighterSpec FighterFor(int32_t typeIndex)
{
  switch (typeIndex) {
    case 29: // X-wing
      return {800, 1.6f, 2, true};
    case 30: // Y-wing
      return {700, 1.2f, 3, true};
    case 31: // A-wing
      return {1100, 2.2f, 1, false};
    case 34: // TIE Interceptor
      return {1000, 2.2f, 0, false};
    case 35: // TIE Bomber
      return {650, 1.2f, 0, true};
    default: // TIE Fighter
      return {900, 1.9f, 0, false};
  }
}

float ShotSpeed(ViewShotKind kind)
{
  switch (kind) {
    case ViewShotKind::Turbolaser:
      return 3200.0f;
    case ViewShotKind::Ion:
      return 2800.0f;
    case ViewShotKind::Laser:
      return 3800.0f;
    case ViewShotKind::Torpedo:
      return 900.0f;
  }
  return 3000.0f;
}

} // namespace

FakeBattle::FakeBattle(uint32_t seed, int fleets)
    : m_rng(0x853c49e6748fea9bULL ^ (static_cast<uint64_t>(seed) << 17))
{
  for (int fleet = 0; fleet < fleets; ++fleet) {
    AddFleets(fleet, (static_cast<float>(fleet) - static_cast<float>(fleets - 1) * 0.5f) * 9000.0f);
  }
  for (const Unit& unit : m_units) {
    m_initialStrength[static_cast<int>(unit.view.side)] += unit.hullMax + unit.shieldMax;
  }
  m_zones = {battle::ViewDeployZone{{-4200.0f, 0.0f}, 3800.0f}, battle::ViewDeployZone{{5200.0f, 600.0f}, 4200.0f}};
}

void FakeBattle::AddFleets(int fleet, float dy)
{
  const auto group = [&](Side side, bool flight, const char* name) {
    AddGroup(side, flight, fleet == 0 ? name : std::string(name) + " " + std::to_string(fleet + 1));
    return static_cast<battle::GroupId>(m_groups.size() - 1);
  };
  const auto name = [fleet](const char* base) {
    return fleet == 0 ? std::string(base) : base + std::to_string(fleet + 1);
  };
  const battle::GroupId line = group(Side::Alliance, false, "Task Force 1");
  const battle::GroupId screen = group(Side::Alliance, false, "Task Force 2");
  const battle::GroupId carrier = group(Side::Alliance, false, "Task Force 3");
  const battle::GroupId red = group(Side::Alliance, true, "Red Squadron");
  const battle::GroupId gold = group(Side::Alliance, true, "Gold Squadron");
  const battle::GroupId green = group(Side::Alliance, true, "Green Squadron");
  const battle::GroupId hammer = group(Side::Empire, false, "Task Force 1");
  const battle::GroupId net = group(Side::Empire, false, "Task Force 2");
  const battle::GroupId carriers = group(Side::Empire, false, "Task Force 3");
  const battle::GroupId alpha = group(Side::Empire, true, "Alpha Flight");
  const battle::GroupId beta = group(Side::Empire, true, "Beta Flight");
  const battle::GroupId gamma = group(Side::Empire, true, "Gamma Flight");

  AddShip(Side::Alliance, 0, line, name("Liberty"), "Mon Calamari Cruiser", {-4200, dy + 1400}, 400);
  AddShip(Side::Alliance, 0, line, name("Defiance"), "Mon Calamari Cruiser", {-4400, dy - 1500}, 400);
  AddShip(Side::Alliance, 2, line, name("Redemption"), "Assault Frigate", {-3500, dy}, 220);
  AddShip(Side::Alliance, 3, screen, name("Salvation"), "Nebulon-B Frigate", {-3400, dy + 3000}, 180);
  AddShip(Side::Alliance, 5, screen, name("Tantive IV"), "Corellian Corvette", {-3000, dy - 3000}, 90);
  AddShip(Side::Alliance, 5, screen, name("Korolev"), "Corellian Corvette", {-2900, dy + 2300}, 90);
  AddShip(Side::Alliance, 4, carrier, name("Bright Hope"), "Alliance Escort Carrier", {-6500, dy}, 260);
  AddSquadron(Side::Alliance, 29, red, name("Red Squadron"), {-2200, dy + 900}, 12);
  AddSquadron(Side::Alliance, 30, gold, name("Gold Squadron"), {-2400, dy - 900}, 10);
  AddSquadron(Side::Alliance, 31, green, name("Green Squadron"), {-2000, dy - 2100}, 6);

  AddShip(Side::Empire, 20, hammer, name("Avenger"), "Imperial Star Destroyer", {4300, dy + 1200}, 450);
  AddShip(Side::Empire, 20, hammer, name("Devastator"), "Imperial Star Destroyer", {4600, dy - 1600}, 450);
  AddShip(Side::Empire, 19, hammer, name("Harrow"), "Victory Destroyer", {3600, dy - 100}, 320);
  AddShip(Side::Empire, 17, net, name("Hammerhead"), "Interdictor Cruiser", {7600, dy + 300}, 240);
  AddShip(Side::Empire, 16, net, name("Talon"), "Lancer Frigate", {3400, dy + 2900}, 120);
  AddShip(Side::Empire, 27, carriers, name("Warden"), "Imperial Escort Carrier", {8200, dy + 2600}, 260);
  AddSquadron(Side::Empire, 33, alpha, name("Alpha Flight"), {2400, dy + 800}, 12);
  AddSquadron(Side::Empire, 34, beta, name("Beta Flight"), {2600, dy - 1200}, 12);
  AddSquadron(Side::Empire, 35, gamma, name("Gamma Flight"), {3000, dy + 2000}, 8);
}

void FakeBattle::AddGroup(Side side, bool flight, std::string name)
{
  battle::ViewGroup group;
  group.id = static_cast<battle::GroupId>(m_groups.size());
  group.side = side;
  group.flight = flight;
  group.name = std::move(name);
  if (flight) {
    group.flightDoctrine.mission = battle::FlightMission::Strike;
  }
  m_groups.push_back(std::move(group));
}

void FakeBattle::AddShip(Side side, int32_t typeIndex, battle::GroupId group, std::string name, std::string className,
                         Vec2 position, float radius)
{
  const ClassSpec spec = SpecFor(typeIndex);
  Unit unit;
  unit.view.unit = static_cast<UnitIndex>(m_units.size());
  unit.view.kind = ViewUnitKind::CapitalShip;
  unit.view.side = side;
  unit.view.typeIndex = typeIndex;
  unit.view.group = group;
  unit.view.position = position;
  unit.view.heading = side == Side::Alliance ? 0.0f : kPiF;
  unit.view.radius = radius;
  unit.previous = position;
  unit.previousHeading = unit.view.heading;
  unit.name = std::move(name);
  unit.className = std::move(className);
  unit.weapons = spec.weapons;
  unit.speed = spec.speed;
  unit.turnRate = spec.turn;
  unit.hull = unit.hullMax = spec.hull;
  unit.shield = unit.shieldMax = spec.shield;
  unit.range = static_cast<float>(*std::ranges::max_element(spec.weapons.range)) * 100.0f;
  unit.preferredRange = unit.range * 0.75f;
  unit.fireInterval = spec.fireInterval;
  unit.cooldown = spec.fireInterval * Random();
  unit.broadside = spec.broadside;
  if (m_units.empty() || m_units.back().view.side != side) {
    unit.view.flags |= battle::kViewFlagship;
  }
  m_groups[group].members.push_back(unit.view.unit);
  m_units.push_back(std::move(unit));
}

void FakeBattle::AddSquadron(Side side, int32_t typeIndex, battle::GroupId group, std::string name, Vec2 position,
                             int craft)
{
  const FighterSpec spec = FighterFor(typeIndex);
  Unit unit;
  unit.view.unit = static_cast<UnitIndex>(m_units.size());
  unit.view.kind = ViewUnitKind::Squadron;
  unit.view.side = side;
  unit.view.typeIndex = typeIndex;
  unit.view.group = group;
  unit.view.position = position;
  unit.view.heading = side == Side::Alliance ? 0.0f : kPiF;
  unit.view.radius = 220.0f;
  unit.view.craft = unit.view.craftMax = craft;
  unit.previous = position;
  unit.previousHeading = unit.view.heading;
  unit.name = std::move(name);
  unit.className = side == Side::Alliance ? "Starfighter squadron" : "TIE squadron";
  unit.weapons.points = {{{0, 0, 8}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}};
  unit.weapons.range = {0, 0, 15};
  unit.speed = spec.speed;
  unit.turnRate = spec.turn;
  unit.hull = unit.hullMax = 8.0f * static_cast<float>(craft);
  unit.shield = unit.shieldMax = spec.shieldPerCraft * static_cast<float>(craft);
  unit.range = 1400.0f;
  unit.preferredRange = 500.0f;
  unit.fireInterval = 0.6f;
  unit.cooldown = 0.6f * Random();
  m_groups[group].members.push_back(unit.view.unit);
  m_units.push_back(std::move(unit));
}

float FakeBattle::Random()
{
  m_rng = m_rng * 6364136223846793005ULL + 1442695040888963407ULL;
  return static_cast<float>(static_cast<uint32_t>(m_rng >> 40)) / static_cast<float>(1U << 24);
}

float FakeBattle::Seconds() const
{
  return static_cast<float>(m_tick) * kDt;
}

bool FakeBattle::Active(const Unit& unit) const
{
  return (unit.view.flags & (battle::kViewDestroyed | battle::kViewWithdrawn)) == 0;
}

bool FakeBattle::Submit(const battle::Command& command)
{
  if (const std::optional<bool> handled = SubmitHudOrder(command)) {
    if (*handled) {
      m_commands.push_back(command);
    }
    return *handled;
  }
  std::vector<UnitIndex> units = command.target.units;
  for (battle::GroupId group : command.target.groups) {
    if (group >= 0 && group < static_cast<battle::GroupId>(m_groups.size())) {
      units.insert(units.end(), m_groups[group].members.begin(), m_groups[group].members.end());
    }
  }
  const bool valid = std::ranges::all_of(units, [&](UnitIndex u) {
    return u >= 0 && u < static_cast<UnitIndex>(m_units.size()) && m_units[u].view.side == command.side;
  });
  if (!valid || units.empty()) {
    return false;
  }
  for (UnitIndex index : units) {
    Unit& unit = m_units[index];
    if (const auto* attack = std::get_if<battle::OrderAttack>(&command.order)) {
      if (!command.queue) {
        unit.attackOrder.clear();
      }
      unit.attackOrder.insert(unit.attackOrder.end(), attack->targets.begin(), attack->targets.end());
      unit.moveTo.reset();
      unit.hold = false;
    } else if (const auto* move = std::get_if<battle::OrderMove>(&command.order)) {
      unit.moveTo = move->position;
      unit.moveFacing = move->facing;
      unit.attackOrder.clear();
      unit.hold = false;
    } else if (const auto* approach = std::get_if<battle::OrderApproach>(&command.order)) {
      unit.attackOrder = {approach->target};
      unit.moveTo.reset();
      unit.hold = false;
    } else if (std::holds_alternative<battle::OrderHold>(command.order)) {
      unit.moveTo.reset();
      unit.hold = true;
    } else if (std::holds_alternative<battle::OrderWithdraw>(command.order)) {
      unit.view.flags |= battle::kViewWithdrawing;
    }
  }
  m_commands.push_back(command);
  return true;
}

void FakeBattle::SetAiControlled(Side side, bool ai)
{
  m_ai[static_cast<int>(side)] = ai;
}

UnitIndex FakeBattle::ChooseTarget(UnitIndex self) const
{
  const Unit& unit = m_units[self];
  for (UnitIndex ordered : unit.attackOrder) {
    if (Active(m_units[ordered])) {
      return ordered;
    }
  }
  const bool laserOnly = unit.view.kind == ViewUnitKind::CapitalShip && unit.weapons.range[0] == 0;
  UnitIndex best = kNoUnit;
  float bestScore = 0.0f;
  for (const Unit& other : m_units) {
    if (other.view.side == unit.view.side || !Active(other) || other.view.kind == ViewUnitKind::DeathStar) {
      continue;
    }
    float score = Dist(unit.view.position, other.view.position);
    const bool otherIsSquadron = other.view.kind == ViewUnitKind::Squadron;
    if (unit.view.kind == ViewUnitKind::CapitalShip && otherIsSquadron != laserOnly) {
      score *= 3.0f;
    }
    if (best == kNoUnit || score < bestScore) {
      best = other.view.unit;
      bestScore = score;
    }
  }
  return best;
}

void FakeBattle::SteerAndMove(Unit& unit, UnitIndex target)
{
  float desired = unit.view.heading;
  float throttle = 1.0f;
  if (unit.moveTo) {
    const float distance = Dist(unit.view.position, *unit.moveTo);
    if (distance < std::max(150.0f, unit.speed * 0.6f)) {
      unit.view.position = Lerp(unit.view.position, *unit.moveTo, 0.05f);
      throttle = 0.0f;
      if (unit.moveFacing) {
        desired = *unit.moveFacing;
      }
    } else {
      desired = Bearing(unit.view.position, *unit.moveTo);
      throttle = std::min(1.0f, distance / 1500.0f + 0.3f);
    }
  } else if (unit.hold) {
    throttle = 0.0f;
  } else if (target != kNoUnit) {
    const Unit& enemy = m_units[target];
    const float bearing = Bearing(unit.view.position, enemy.view.position);
    const float distance = Dist(unit.view.position, enemy.view.position);
    if (distance > unit.preferredRange) {
      desired = bearing;
    } else if (unit.view.kind == ViewUnitKind::Squadron) {
      desired = bearing + 1.2f;
    } else if (unit.broadside) {
      const float left = Wrap(bearing + kPiF * 0.5f);
      const float right = Wrap(bearing - kPiF * 0.5f);
      desired = std::abs(Wrap(left - unit.view.heading)) < std::abs(Wrap(right - unit.view.heading)) ? left : right;
      throttle = 0.45f;
    } else {
      desired = bearing;
      throttle = 0.25f;
    }
  } else {
    throttle = 0.3f;
  }
  if (unit.view.flags & battle::kViewTractored) {
    throttle *= 0.3f;
  }
  const float turn = std::clamp(Wrap(desired - unit.view.heading), -unit.turnRate * kDt, unit.turnRate * kDt);
  unit.view.heading = Wrap(unit.view.heading + turn);
  const float step = unit.speed * throttle * kDt;
  unit.view.position.x += std::cos(unit.view.heading) * step;
  unit.view.position.y += std::sin(unit.view.heading) * step;
}

void FakeBattle::Fire(UnitIndex self, UnitIndex target)
{
  Unit& unit = m_units[self];
  const Unit& enemy = m_units[target];
  const float distance = Dist(unit.view.position, enemy.view.position);
  if (distance > unit.range) {
    return;
  }
  std::vector<ViewShotKind> kinds;
  if (unit.view.kind == ViewUnitKind::Squadron) {
    const int bolts = std::clamp(unit.view.craft / 3, 1, 4);
    kinds.assign(bolts, ViewShotKind::Laser);
    const FighterSpec spec = FighterFor(unit.view.typeIndex);
    if (spec.torpedoes && enemy.view.kind == ViewUnitKind::CapitalShip && distance < 1000.0f && Random() < 0.3f) {
      kinds.push_back(ViewShotKind::Torpedo);
    }
  } else {
    const auto& range = unit.weapons.range;
    if (range[0] * 100.0f >= distance) {
      kinds.insert(kinds.end(), 2, ViewShotKind::Turbolaser);
    }
    if (range[1] * 100.0f >= distance && Random() < 0.6f) {
      kinds.push_back(ViewShotKind::Ion);
    }
    if (range[2] * 100.0f >= distance) {
      kinds.insert(kinds.end(), enemy.view.kind == ViewUnitKind::Squadron ? 3 : 1, ViewShotKind::Laser);
    }
  }
  for (ViewShotKind kind : kinds) {
    Shot shot;
    shot.view.kind = kind;
    shot.view.side = unit.view.side;
    const float spread = unit.view.radius * 0.6f;
    shot.view.from
        = {unit.view.position.x + (Random() - 0.5f) * spread, unit.view.position.y + (Random() - 0.5f) * spread};
    const float scatter = enemy.view.radius * 0.7f;
    shot.view.to
        = {enemy.view.position.x + (Random() - 0.5f) * scatter, enemy.view.position.y + (Random() - 0.5f) * scatter};
    shot.view.hit = Random() < (enemy.view.kind == ViewUnitKind::Squadron ? 0.35f : 0.8f);
    if (!shot.view.hit) {
      shot.view.to.x += (Random() - 0.5f) * enemy.view.radius * 3.0f;
      shot.view.to.y += (Random() - 0.5f) * enemy.view.radius * 3.0f;
    }
    shot.view.shieldImpact = shot.view.hit && enemy.shield > 0.0f;
    shot.target = target;
    shot.damage = kind == ViewShotKind::Turbolaser ? 24.0f
                  : kind == ViewShotKind::Ion      ? 18.0f
                  : kind == ViewShotKind::Torpedo  ? 40.0f
                                                   : (enemy.view.kind == ViewUnitKind::Squadron ? 5.0f : 2.0f);
    shot.duration = Dist(shot.view.from, shot.view.to) / ShotSpeed(kind) + Random() * 0.1f;
    shot.age = -Random() * 0.4f;
    m_shots.push_back(shot);
  }
}

void FakeBattle::Damage(UnitIndex index, float amount, Vec2 at)
{
  Unit& unit = m_units[index];
  if (!Active(unit)) {
    return;
  }
  const bool squadron = unit.view.kind == ViewUnitKind::Squadron;
  if (unit.shield > 0.0f) {
    unit.shield = std::max(0.0f, unit.shield - amount);
    if (!squadron) {
      AddEffect(ViewEffectKind::ShieldFlash, at, unit.view.radius, 0.35f, index);
    }
    if (unit.shield <= 0.0f && !squadron) {
      Event(unit.view.side, unit.name + ": shields down", index);
    }
    return;
  }
  unit.hull -= amount;
  if (!squadron) {
    AddEffect(Random() < 0.08f ? ViewEffectKind::CriticalHit : ViewEffectKind::HullHit, at, unit.view.radius * 0.3f,
              0.5f, index);
  }
  if (squadron) {
    const int craft = std::max(0, static_cast<int>(std::ceil(unit.hull / 8.0f)));
    if (craft < unit.view.craft) {
      AddEffect(ViewEffectKind::Explosion, at, 60.0f, 0.8f, index);
      unit.view.craft = craft;
    }
  }
  if (unit.hull <= 0.0f) {
    unit.hull = 0.0f;
    unit.view.flags |= battle::kViewDestroyed;
    AddEffect(ViewEffectKind::Explosion, unit.view.position, squadron ? 150.0f : unit.view.radius * 2.2f,
              squadron ? 1.0f : 2.4f, index);
    Event(unit.view.side, unit.name + " destroyed", index);
  }
}

void FakeBattle::UpdateTractors()
{
  for (Unit& unit : m_units) {
    unit.view.flags &= ~(battle::kViewTractored | battle::kViewTractoring);
  }
  for (Unit& unit : m_units) {
    if (unit.view.typeIndex != 20 || !Active(unit)) {
      unit.tractorTarget = kNoUnit;
      continue;
    }
    const UnitIndex target = ChooseTarget(unit.view.unit);
    const bool inRange = target != kNoUnit && m_units[target].view.kind == ViewUnitKind::CapitalShip
                         && Dist(unit.view.position, m_units[target].view.position) < kTractorRange;
    unit.tractorTarget = inRange ? target : kNoUnit;
    if (inRange) {
      unit.view.flags |= battle::kViewTractoring;
      m_units[target].view.flags |= battle::kViewTractored;
    }
  }
}

void FakeBattle::UpdateWithdrawals()
{
  const auto well = std::ranges::find_if(m_units, [this](const Unit& u) {
    return u.view.typeIndex == 17 && Active(u);
  });
  for (Unit& unit : m_units) {
    if (!Active(unit) || unit.view.kind != ViewUnitKind::CapitalShip) {
      continue;
    }
    const bool inWell = well != m_units.end() && well->view.side != unit.view.side
                        && Dist(well->view.position, unit.view.position) < kWellRadius;
    unit.view.flags = inWell ? unit.view.flags | battle::kViewInEnemyWell : unit.view.flags & ~battle::kViewInEnemyWell;
    if (unit.hull < unit.hullMax * 0.35f && (unit.view.flags & battle::kViewWithdrawing) == 0) {
      unit.view.flags |= battle::kViewWithdrawing;
      Event(unit.view.side, unit.name + ": withdrawing", unit.view.unit);
    }
    if ((unit.view.flags & battle::kViewWithdrawing) == 0 || inWell || (unit.view.flags & battle::kViewTractored)) {
      continue;
    }
    unit.spoolSeconds += kDt;
    unit.view.spool = std::min(1.0f, unit.spoolSeconds / 12.0f);
    if (unit.view.spool >= 1.0f) {
      unit.view.flags = (unit.view.flags & ~battle::kViewWithdrawing) | battle::kViewWithdrawn;
      AddEffect(ViewEffectKind::Jump, unit.view.position, unit.view.radius * 2.0f, 1.0f, unit.view.unit);
      Event(unit.view.side, unit.name + " jumped to hyperspace", unit.view.unit);
    }
  }
}

void FakeBattle::AddEffect(ViewEffectKind kind, Vec2 position, float size, float duration, UnitIndex unit)
{
  Effect effect;
  effect.view.kind = kind;
  effect.view.position = position;
  effect.view.size = size;
  effect.view.unit = unit;
  effect.duration = duration;
  m_effects.push_back(effect);
}

void FakeBattle::Event(Side side, std::string text, UnitIndex unit)
{
  m_events.push_back({Seconds(), side, std::move(text), unit});
  if (m_events.size() > 40) {
    m_events.erase(m_events.begin());
  }
}

void FakeBattle::Step()
{
  if (Finished()) {
    return;
  }
  for (Unit& unit : m_units) {
    unit.previous = unit.view.position;
    unit.previousHeading = unit.view.heading;
  }
  for (Unit& unit : m_units) {
    if (!Active(unit)) {
      continue;
    }
    const UnitIndex target = ChooseTarget(unit.view.unit);
    SteerAndMove(unit, target);
    unit.view.velocity
        = {(unit.view.position.x - unit.previous.x) / kDt, (unit.view.position.y - unit.previous.y) / kDt};
    unit.view.target = target;
    unit.view.firing = target != kNoUnit && Dist(unit.view.position, m_units[target].view.position) <= unit.range;
    unit.cooldown -= kDt;
    if (target != kNoUnit && unit.cooldown <= 0.0f) {
      unit.cooldown = unit.fireInterval * (0.8f + 0.4f * Random());
      Fire(unit.view.unit, target);
    }
    unit.shield = std::min(unit.shieldMax, unit.shield + unit.shieldMax * 0.004f * kDt);
    const Unit* enemy = target == kNoUnit ? nullptr : &m_units[target];
    if (enemy) {
      const float distance = Dist(unit.view.position, enemy->view.position);
      unit.view.intent = "Targeting " + enemy->className + " " + enemy->name + ": "
                         + (unit.attackOrder.empty() ? "nearest" : "ordered") + "; "
                         + std::to_string(static_cast<int>(distance / 100.0f) / 10) + "."
                         + std::to_string(static_cast<int>(distance / 100.0f) % 10) + " km";
    } else {
      unit.view.intent = "No target";
    }
  }
  for (Shot& shot : m_shots) {
    shot.age += kDt;
    if (shot.age >= shot.duration && shot.view.hit) {
      Damage(shot.target, shot.damage, shot.view.to);
    }
  }
  std::erase_if(m_shots, [](const Shot& shot) {
    return shot.age >= shot.duration;
  });
  for (Effect& effect : m_effects) {
    effect.view.age += kDt;
  }
  std::erase_if(m_effects, [](const Effect& effect) {
    return effect.view.age >= effect.duration;
  });
  UpdateTractors();
  UpdateWithdrawals();
  if (m_superlaserCharge >= 0.0f && m_superlaserCharge < 1.0f) {
    m_superlaserCharge = std::min(1.0f, m_superlaserCharge + kDt / 300.0f);
  }
  ++m_tick;
}

bool FakeBattle::Finished() const
{
  std::array<bool, 2> present{};
  for (const Unit& unit : m_units) {
    present[static_cast<int>(unit.view.side)] |= Active(unit);
  }
  return !present[0] || !present[1] || m_tick >= 15 * 60 * battle::kTicksPerSecond;
}

void FakeBattle::BuildViewModel(float alpha, battle::ViewModel& out) const
{
  out.tick = m_tick;
  out.battleSeconds = (static_cast<float>(m_tick) + alpha) * kDt;
  out.boundsMin = {-20000.0f, -20000.0f};
  out.boundsMax = {20000.0f, 20000.0f};
  out.units.clear();
  out.craft.clear();
  out.shots.clear();
  out.effects.clear();
  out.tractors.clear();
  out.wells.clear();
  out.defenses.clear(); // the fabricated planet has no batteries
  out.coverRadius = 0.0f;
  out.groups = m_groups;
  out.sides = {};
  out.recentEvents = m_events;
  out.finished = Finished();
  out.superlaserCharge = m_superlaserCharge;
  out.deployZones = m_zones;
  std::array<int, 2> craft{};

  for (const Unit& unit : m_units) {
    if (unit.view.flags & battle::kViewWithdrawn) {
      continue;
    }
    battle::ViewUnit view = unit.view;
    view.position = Lerp(unit.previous, unit.view.position, alpha);
    view.heading = Wrap(unit.previousHeading + Wrap(unit.view.heading - unit.previousHeading) * alpha);
    view.hullFraction = unit.hull / unit.hullMax;
    view.shieldFraction = unit.shieldMax > 0.0f ? unit.shield / unit.shieldMax : 0.0f;
    if (unit.shield <= 0.0f) {
      view.flags |= battle::kViewShieldsDown;
    }
    out.units.push_back(view);
    const int side = static_cast<int>(unit.view.side);
    if (Active(unit) && unit.view.kind != ViewUnitKind::DeathStar) {
      out.sides[side].strength += unit.hull + unit.shield;
    }
    if (Active(unit) && unit.view.kind == ViewUnitKind::Squadron) {
      craft[side] += unit.view.craft;
    }
    if (unit.view.kind == ViewUnitKind::Squadron && Active(unit)) {
      const float t = out.battleSeconds;
      for (int i = 0; i < unit.view.craft; ++i) {
        const float row = static_cast<float>((i + 1) / 2);
        const float lateral = (i % 2 == 0 ? 1.0f : -1.0f) * row * 55.0f + std::sin(t * 1.3f + i) * 25.0f;
        const float back = -row * 70.0f + std::cos(t * 0.9f + i * 1.7f) * 25.0f;
        const float c = std::cos(view.heading);
        const float s = std::sin(view.heading);
        out.craft.push_back({view.unit,
                             {view.position.x + back * c - lateral * s, view.position.y + back * s + lateral * c},
                             view.heading + 0.25f * std::sin(t * 2.0f + i)});
      }
    }
    if (unit.tractorTarget != kNoUnit) {
      out.tractors.push_back({unit.view.unit, unit.tractorTarget});
    }
    if (unit.view.typeIndex == 17 && Active(unit)) {
      out.wells.push_back({unit.view.unit, view.position, kWellRadius, unit.view.side, true});
    }
  }
  for (int side = 0; side < 2; ++side) {
    out.sides[side].initialStrength = m_initialStrength[side];
    out.sides[side].signalsMax = 3;
    out.sides[side].signalsReady = m_signalsReady[side];
  }
  out.deathStarRunOpen = m_superlaserCharge >= 0.0f && craft[0] > craft[1];
  for (const Shot& shot : m_shots) {
    const float age = shot.age + alpha * kDt;
    if (age < 0.0f) {
      continue;
    }
    battle::ViewShot view = shot.view;
    view.progress = std::clamp(age / shot.duration, 0.0f, 1.0f);
    out.shots.push_back(view);
  }
  for (const Effect& effect : m_effects) {
    battle::ViewEffect view = effect.view;
    view.age += alpha * kDt;
    out.effects.push_back(view);
  }
}

battle::ViewUnitDetail FakeBattle::Detail(UnitIndex index) const
{
  battle::ViewUnitDetail detail;
  if (index < 0 || index >= static_cast<UnitIndex>(m_units.size())) {
    return detail;
  }
  const Unit& unit = m_units[index];
  detail.unit = index;
  detail.name = unit.name;
  detail.className = unit.className;
  detail.weapons = unit.weapons;
  detail.tractorRange = unit.view.typeIndex == 20 ? kTractorRange : 0.0f;
  if (unit.view.group != battle::kNoGroup) {
    detail.shipDoctrine = m_groups[unit.view.group].shipDoctrine;
    detail.flightDoctrine = m_groups[unit.view.group].flightDoctrine;
  }
  if (unit.view.kind == ViewUnitKind::Squadron) {
    CopyFields(detail.flightDoctrine, unit.flightOverride, unit.overrides);
  } else {
    CopyFields(detail.shipDoctrine, unit.shipOverride, unit.overrides);
  }
  detail.overrides = unit.overrides;
  detail.torpedoes = unit.view.kind == ViewUnitKind::Squadron && FighterFor(unit.view.typeIndex).torpedoes ? 4 : 0;
  detail.hangarCapacity = unit.view.typeIndex == 4 || unit.view.typeIndex == 27 ? 6 : 0;
  return detail;
}

void FakeBattle::AddDeathStar(Vec2 position)
{
  Unit unit;
  unit.view.unit = static_cast<UnitIndex>(m_units.size());
  unit.view.kind = ViewUnitKind::DeathStar;
  unit.view.side = Side::Empire;
  unit.view.typeIndex = 37;
  unit.view.position = position;
  unit.view.radius = 1600.0f;
  unit.previous = position;
  unit.name = "Death Star";
  unit.className = "Death Star";
  unit.hull = unit.hullMax = 1.0e6f;
  unit.fireInterval = 1.0e6f;
  unit.cooldown = 1.0e6f;
  m_units.push_back(std::move(unit));
  m_superlaserCharge = 1.0f;
}

std::vector<UnitIndex> FakeBattle::CommandUnits(const battle::Command& command) const
{
  std::vector<UnitIndex> units = command.target.units;
  for (battle::GroupId group : command.target.groups) {
    if (group >= 0 && group < static_cast<battle::GroupId>(m_groups.size())) {
      units.insert(units.end(), m_groups[group].members.begin(), m_groups[group].members.end());
    }
  }
  return units;
}

battle::GroupId FakeBattle::NewGroup(Side side, bool flight, battle::GroupId copyFrom)
{
  const auto count = std::ranges::count_if(m_groups, [&](const battle::ViewGroup& group) {
    return group.side == side && group.flight == flight;
  });
  AddGroup(side, flight, (flight ? "Flight " : "Task Force ") + std::to_string(count + 1));
  battle::ViewGroup& group = m_groups.back();
  if (copyFrom != battle::kNoGroup) {
    group.shipDoctrine = m_groups[copyFrom].shipDoctrine;
    group.flightDoctrine = m_groups[copyFrom].flightDoctrine;
  }
  return group.id;
}

std::optional<bool> FakeBattle::SubmitHudOrder(const battle::Command& command)
{
  const std::vector<UnitIndex> units = CommandUnits(command);
  const bool ownUnits = std::ranges::all_of(units, [&](UnitIndex u) {
    return u >= 0 && u < static_cast<UnitIndex>(m_units.size()) && m_units[u].view.side == command.side;
  });
  const bool ownGroups = std::ranges::all_of(command.target.groups, [&](battle::GroupId g) {
    return g >= 0 && g < static_cast<battle::GroupId>(m_groups.size()) && m_groups[g].side == command.side;
  });
  const bool valid = ownUnits && ownGroups;
  const bool groupTarget = !command.target.groups.empty();
  if (const auto* order = std::get_if<battle::OrderSetShipDoctrine>(&command.order)) {
    if (!valid || units.empty()) {
      return false;
    }
    for (battle::GroupId group : command.target.groups) {
      CopyFields(m_groups[group].shipDoctrine, order->doctrine, order->fields);
    }
    for (UnitIndex index : groupTarget ? std::vector<UnitIndex>{} : units) {
      Unit& unit = m_units[index];
      CopyFields(unit.shipOverride, order->doctrine, order->fields);
      unit.overrides = (unit.overrides | order->fields) & ~order->clear;
    }
    return true;
  }
  if (const auto* order = std::get_if<battle::OrderSetFlightDoctrine>(&command.order)) {
    if (!valid || units.empty()) {
      return false;
    }
    for (battle::GroupId group : command.target.groups) {
      CopyFields(m_groups[group].flightDoctrine, order->doctrine, order->fields);
    }
    for (UnitIndex index : groupTarget ? std::vector<UnitIndex>{} : units) {
      Unit& unit = m_units[index];
      CopyFields(unit.flightOverride, order->doctrine, order->fields);
      unit.overrides = (unit.overrides | order->fields) & ~order->clear;
    }
    return true;
  }
  if (const auto* order = std::get_if<battle::OrderSetPower>(&command.order)) {
    if (!valid || units.empty()) {
      return false;
    }
    battle::ShipDoctrine doctrine;
    doctrine.power = order->power;
    for (battle::GroupId group : command.target.groups) {
      CopyFields(m_groups[group].shipDoctrine, doctrine, battle::kShipPower);
    }
    for (UnitIndex index : groupTarget ? std::vector<UnitIndex>{} : units) {
      CopyFields(m_units[index].shipOverride, doctrine, battle::kShipPower);
      m_units[index].overrides |= battle::kShipPower;
    }
    return true;
  }
  if (const auto* order = std::get_if<battle::OrderAssignGroup>(&command.order)) {
    if (!valid || units.empty() || groupTarget) {
      return false;
    }
    const bool flight = m_units[units.front()].view.kind == ViewUnitKind::Squadron;
    const bool sameKind = std::ranges::all_of(units, [&](UnitIndex u) {
      return (m_units[u].view.kind == ViewUnitKind::Squadron) == flight;
    });
    battle::GroupId target = order->group;
    const bool targetValid = target == battle::kNoGroup
                             || (target >= 0 && target < static_cast<battle::GroupId>(m_groups.size())
                                 && m_groups[target].side == command.side && m_groups[target].flight == flight);
    if (!sameKind || !targetValid) {
      return false;
    }
    if (target == battle::kNoGroup) {
      target = NewGroup(command.side, flight, m_units[units.front()].view.group);
    }
    for (UnitIndex index : units) {
      Unit& unit = m_units[index];
      if (unit.view.group != battle::kNoGroup) {
        std::erase(m_groups[unit.view.group].members, index);
      }
      unit.view.group = target;
      unit.overrides = 0;
      m_groups[target].members.push_back(index);
    }
    return true;
  }
  if (const auto* order = std::get_if<battle::OrderSignal>(&command.order)) {
    int& ready = m_signalsReady[static_cast<int>(command.side)];
    if (ready <= 0) {
      return false;
    }
    --ready;
    static constexpr std::array<const char*, 4> kNames{"Concentrate fire", "Hold the line", "Scramble",
                                                       "Coordinated jump"};
    Event(command.side, std::string("Signal: ") + kNames[static_cast<int>(order->signal)], order->target);
    if (order->signal == battle::SignalKind::ConcentrateFire && order->target != kNoUnit) {
      for (Unit& unit : m_units) {
        if (unit.view.side == command.side && Active(unit) && unit.view.kind == ViewUnitKind::CapitalShip) {
          unit.attackOrder = {order->target};
        }
      }
    }
    return true;
  }
  if (const auto* order = std::get_if<battle::OrderFireSuperlaser>(&command.order)) {
    const bool target = order->target >= 0 && order->target < static_cast<UnitIndex>(m_units.size())
                        && m_units[order->target].view.side != command.side && Active(m_units[order->target])
                        && m_units[order->target].view.kind == ViewUnitKind::CapitalShip;
    if (command.side != Side::Empire || m_superlaserCharge < 1.0f || !target) {
      return false;
    }
    Unit& victim = m_units[order->target];
    AddEffect(ViewEffectKind::Superlaser, victim.view.position, victim.view.radius * 3.0f, 1.5f, order->target);
    Event(command.side, "Superlaser fired at " + victim.name, order->target);
    victim.shield = 0.0f;
    Damage(order->target, victim.hullMax * 2.0f, victim.view.position);
    m_superlaserCharge = 0.0f;
    return true;
  }
  if (std::holds_alternative<battle::OrderDeathStarRun>(command.order)) {
    if (!valid || units.empty() || command.side != Side::Alliance) {
      return false;
    }
    Event(command.side, m_units[units.front()].name + " begins the Death Star trench run", units.front());
    return true;
  }
  if (const auto* order = std::get_if<battle::OrderDeploy>(&command.order)) {
    if (!valid || units.empty() || m_tick != 0) {
      return false;
    }
    Vec2 centroid;
    for (UnitIndex index : units) {
      centroid.x += m_units[index].view.position.x / static_cast<float>(units.size());
      centroid.y += m_units[index].view.position.y / static_cast<float>(units.size());
    }
    const battle::ViewDeployZone& zone = m_zones[static_cast<int>(command.side)];
    Vec2 target = order->position;
    if (const float distance = Dist(target, zone.center); zone.radius > 0.0f && distance > zone.radius) {
      target = Lerp(zone.center, target, zone.radius / distance);
    }
    for (UnitIndex index : units) {
      Unit& unit = m_units[index];
      unit.view.position = {unit.view.position.x + target.x - centroid.x, unit.view.position.y + target.y - centroid.y};
      unit.previous = unit.view.position;
      if (order->facing) {
        unit.view.heading = unit.previousHeading = *order->facing;
      }
    }
    return true;
  }
  if (const auto* order = std::get_if<battle::OrderStrike>(&command.order)) {
    if (!valid || units.empty()) {
      return false;
    }
    for (UnitIndex index : units) {
      m_units[index].attackOrder = {order->target};
      m_units[index].moveTo.reset();
      m_units[index].hold = false;
    }
    return true;
  }
  return std::nullopt;
}

} // namespace reb::battle_ui
