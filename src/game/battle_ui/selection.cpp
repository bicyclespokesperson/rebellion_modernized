#include "battle_ui/selection.h"

#include <algorithm>

namespace reb::battle_ui
{

using battle::kNoUnit;
using battle::UnitIndex;

bool Selectable(const battle::ViewUnit& unit)
{
  return (unit.flags & (battle::kViewDestroyed | battle::kViewWithdrawn | battle::kViewDocked)) == 0;
}

const battle::ViewUnit* FindUnit(const battle::ViewModel& view, UnitIndex unit)
{
  const auto it = std::ranges::find(view.units, unit, &battle::ViewUnit::unit);
  return it == view.units.end() ? nullptr : &*it;
}

namespace
{

// Where a unit appears: fighters fly above the plane in the angled view, and are picked where they are drawn.
Point Appears(const Camera& camera, const battle::ViewUnit& unit)
{
  return camera.WorldToScreen(unit.position,
                              unit.kind == battle::ViewUnitKind::Squadron ? Camera::kFlightHeight : 0.0f);
}

} // namespace

UnitIndex PickUnit(const battle::ViewModel& view, const Camera& camera, Point screen, float minPickPixels)
{
  UnitIndex best = kNoUnit;
  float bestScore = 0.0f;
  for (const battle::ViewUnit& unit : view.units) {
    if (!Selectable(unit)) {
      continue;
    }
    const float pickRadius = std::max(camera.PixelsFor(unit.radius), minPickPixels);
    const float score = Distance(Appears(camera, unit), screen) / pickRadius;
    if (score > 1.0f) {
      continue;
    }
    if (best == kNoUnit || score < bestScore || (score == bestScore && unit.unit < best)) {
      best = unit.unit;
      bestScore = score;
    }
  }
  return best;
}

std::vector<UnitIndex> UnitsInRect(const battle::ViewModel& view, const Camera& camera, const Rect& rect,
                                   battle::Side side)
{
  std::vector<UnitIndex> units;
  for (const battle::ViewUnit& unit : view.units) {
    const Point p = Appears(camera, unit);
    if (Selectable(unit) && unit.side == side && p.x >= rect.x && p.y >= rect.y && p.x <= rect.Right()
        && p.y <= rect.Bottom()) {
      units.push_back(unit.unit);
    }
  }
  std::ranges::sort(units);
  return units;
}

bool Selection::Contains(UnitIndex unit) const
{
  return std::ranges::find(m_units, unit) != m_units.end();
}

void Selection::Set(std::vector<UnitIndex> units)
{
  m_units = std::move(units);
  m_foreign = false;
}

void Selection::Toggle(UnitIndex unit, bool own)
{
  if (!own) {
    m_units = {unit};
    m_foreign = true;
    return;
  }
  if (m_foreign) {
    m_units.clear();
    m_foreign = false;
  }
  if (const auto it = std::ranges::find(m_units, unit); it != m_units.end()) {
    m_units.erase(it);
  } else {
    m_units.push_back(unit);
  }
}

void Selection::Add(const std::vector<UnitIndex>& units)
{
  if (m_foreign) {
    m_units.clear();
    m_foreign = false;
  }
  for (UnitIndex unit : units) {
    if (!Contains(unit)) {
      m_units.push_back(unit);
    }
  }
}

void Selection::Prune(const battle::ViewModel& view)
{
  std::erase_if(m_units, [&view](UnitIndex unit) {
    const battle::ViewUnit* found = FindUnit(view, unit);
    return !found || !Selectable(*found);
  });
}

std::vector<UnitIndex> Selection::Orderable(const battle::ViewModel& view, battle::Side side) const
{
  std::vector<UnitIndex> units;
  for (UnitIndex unit : m_units) {
    const battle::ViewUnit* found = FindUnit(view, unit);
    if (found && found->side == side && Selectable(*found)) {
      units.push_back(unit);
    }
  }
  std::ranges::sort(units);
  return units;
}

void HotkeyGroups::Store(int key, std::vector<UnitIndex> units)
{
  if (key >= 0 && key < kCount) {
    m_groups[key] = std::move(units);
  }
}

const std::vector<UnitIndex>& HotkeyGroups::Get(int key) const
{
  static const std::vector<UnitIndex> kEmpty;
  return key >= 0 && key < kCount ? m_groups[key] : kEmpty;
}

void HotkeyGroups::SeedFromGroups(const battle::ViewModel& view, battle::Side side)
{
  if (m_seeded) {
    return;
  }
  m_seeded = true;
  int key = 1;
  for (const bool flights : {false, true}) {
    for (const battle::ViewGroup& group : view.groups) {
      if (group.side != side || group.flight != flights || group.members.empty() || key >= kCount) {
        continue;
      }
      if (m_groups[key].empty()) {
        m_groups[key] = group.members;
      }
      ++key;
    }
  }
}

} // namespace reb::battle_ui
