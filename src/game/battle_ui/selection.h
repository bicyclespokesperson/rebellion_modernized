#pragma once
#include <array>
#include <vector>

#include "battle_ui/camera.h"
#include "space_battle/view_model.h"

// Picking units on screen and the player's selection (tactical_combat.md §5.1, §10.1).
namespace reb::battle_ui
{

// Units that can be picked and ordered: on the plane and still in the battle.
bool Selectable(const battle::ViewUnit& unit);

// The unit under `screen`, or kNoUnit. A unit is hit within its drawn radius, but never less than `minPickPixels`;
// the closest hit relative to its pick radius wins, ties to the lower unit index.
battle::UnitIndex PickUnit(const battle::ViewModel& view, const Camera& camera, Point screen, float minPickPixels);

// `side`'s units whose centre lies inside the screen rectangle, in unit order.
std::vector<battle::UnitIndex> UnitsInRect(const battle::ViewModel& view, const Camera& camera, const Rect& rect,
                                           battle::Side side);

const battle::ViewUnit* FindUnit(const battle::ViewModel& view, battle::UnitIndex unit);

// The selection holds either the player's own units, or a single other unit for inspection.
class Selection
{
  public:
  const std::vector<battle::UnitIndex>& Units() const
  {
    return m_units;
  }
  bool Empty() const
  {
    return m_units.empty();
  }
  bool Contains(battle::UnitIndex unit) const;
  void Clear()
  {
    m_units.clear();
  }
  void Set(std::vector<battle::UnitIndex> units);
  // Shift-click: adds or removes one unit. An enemy unit replaces the selection instead.
  void Toggle(battle::UnitIndex unit, bool own);
  void Add(const std::vector<battle::UnitIndex>& units);
  // Drops units that are gone (destroyed, withdrawn, docked).
  void Prune(const battle::ViewModel& view);
  // The selected units that `side` can order.
  std::vector<battle::UnitIndex> Orderable(const battle::ViewModel& view, battle::Side side) const;

  private:
  std::vector<battle::UnitIndex> m_units;
  bool m_foreign = false;
};

// Number-key groups (Ctrl+N stores the selection, N recalls it).
class HotkeyGroups
{
  public:
  static constexpr int kCount = 10;
  void Store(int key, std::vector<battle::UnitIndex> units);
  const std::vector<battle::UnitIndex>& Get(int key) const;
  // Seeds 1..9 with the side's task forces, then flights, in group order, for the keys that are still empty.
  void SeedFromGroups(const battle::ViewModel& view, battle::Side side);

  private:
  std::array<std::vector<battle::UnitIndex>, kCount> m_groups;
  bool m_seeded = false;
};

} // namespace reb::battle_ui
