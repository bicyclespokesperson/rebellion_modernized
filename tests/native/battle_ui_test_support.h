// Shared fixtures for the battle UI tests: a requirement macro that names the failed check, a small fabricated view
// model, and a battle source that records what the screen does to it.
#pragma once
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "battle_ui/battle_source.h"
#include "battle_ui/ui_types.h"

#define REQUIRE(condition)                                                                                             \
  do {                                                                                                                 \
    if (!(condition)) {                                                                                                \
      std::fprintf(stderr, "%s:%d: requirement failed: %s\n", __FILE__, __LINE__, #condition);                         \
      std::abort();                                                                                                    \
    }                                                                                                                  \
  } while (false)

namespace reb::battle_ui::test
{

inline bool Near(float a, float b, float tolerance = 0.01f)
{
  return std::abs(a - b) <= tolerance;
}

inline bool Near(Point a, Point b, float tolerance = 0.01f)
{
  return Near(a.x, b.x, tolerance) && Near(a.y, b.y, tolerance);
}

inline battle::ViewUnit Unit(battle::UnitIndex index, battle::Side side, battle::ViewUnitKind kind, int32_t typeIndex,
                             battle::Vec2 position, float radius, battle::GroupId group)
{
  battle::ViewUnit unit;
  unit.unit = index;
  unit.side = side;
  unit.kind = kind;
  unit.typeIndex = typeIndex;
  unit.position = position;
  unit.radius = radius;
  unit.group = group;
  return unit;
}

// Unit 0 and 1: Alliance capital ships (task force 0); 2: an Empire Star Destroyer; 3: an Alliance X-wing squadron
// (flight 1); 4: a destroyed Empire ship.
inline battle::ViewModel SmallBattle()
{
  using battle::Side;
  using battle::ViewUnitKind;
  battle::ViewModel view;
  view.boundsMin = {-20000.0f, -20000.0f};
  view.boundsMax = {20000.0f, 20000.0f};
  view.units = {Unit(0, Side::Alliance, ViewUnitKind::CapitalShip, 0, {0.0f, 0.0f}, 400.0f, 0),
                Unit(1, Side::Alliance, ViewUnitKind::CapitalShip, 3, {2000.0f, 1000.0f}, 180.0f, 0),
                Unit(2, Side::Empire, ViewUnitKind::CapitalShip, 20, {6000.0f, 0.0f}, 450.0f, 2),
                Unit(3, Side::Alliance, ViewUnitKind::Squadron, 29, {1000.0f, -1500.0f}, 200.0f, 1),
                Unit(4, Side::Empire, ViewUnitKind::CapitalShip, 19, {3000.0f, 3000.0f}, 300.0f, 2)};
  view.units[2].heading = 3.14159265f;
  view.units[3].craft = 10;
  view.units[3].craftMax = 12;
  view.units[4].flags = battle::kViewDestroyed;
  view.craft = {{3, {1000.0f, -1500.0f}, 0.0f}, {3, {950.0f, -1450.0f}, 0.0f}};
  battle::ViewGroup taskForce;
  taskForce.id = 0;
  taskForce.name = "Task Force 1";
  taskForce.members = {0, 1};
  battle::ViewGroup flight;
  flight.id = 1;
  flight.flight = true;
  flight.name = "Red Squadron";
  flight.members = {3};
  battle::ViewGroup enemy;
  enemy.id = 2;
  enemy.side = Side::Empire;
  enemy.members = {2, 4};
  view.groups = {taskForce, flight, enemy};
  view.sides[0] = {800.0f, 1000.0f, 1, 3, false};
  view.sides[1] = {500.0f, 1000.0f, 0, 0, false};
  return view;
}

class RecordingSource : public BattleSource
{
  public:
  explicit RecordingSource(battle::ViewModel view = SmallBattle())
      : m_view(std::move(view))
  {
  }

  bool Submit(const battle::Command& command) override
  {
    commands.push_back(command);
    return true;
  }
  void Step() override
  {
    ++steps;
    m_view.tick = steps;
  }
  void SkipOutro() override
  {
    ++skippedOutros;
    if (m_view.decided) {
      m_view.finished = true;
    }
  }
  bool Finished() const override
  {
    return m_view.finished;
  }
  int64_t Tick() const override
  {
    return steps;
  }
  void SetAiControlled(battle::Side side, bool ai) override
  {
    aiControlled[static_cast<int>(side)] = ai;
  }
  void BuildViewModel(float alpha, battle::ViewModel& out) const override
  {
    lastAlpha = alpha;
    out = m_view;
  }
  battle::ViewUnitDetail Detail(battle::UnitIndex unit) const override
  {
    battle::ViewUnitDetail detail;
    detail.unit = unit;
    if (!anonymous) {
      detail.name = "Unit " + std::to_string(unit);
      detail.className = "Class";
    }
    detail.weapons.points[0] = {100, 50, 10};
    detail.weapons.points[2] = {40, 0, 10};
    detail.weapons.range = {60, 50, 20};
    return detail;
  }
  battle::ViewModel& View()
  {
    return m_view;
  }

  std::vector<battle::Command> commands;
  int64_t steps = 0;
  int skippedOutros = 0;
  bool anonymous = false; // details without names, as native campaign snapshots have
  std::array<bool, 2> aiControlled{false, false};
  mutable float lastAlpha = -1.0f;

  private:
  battle::ViewModel m_view;
};

} // namespace reb::battle_ui::test
