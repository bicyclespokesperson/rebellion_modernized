#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "battle_ui/camera.h"
#include "battle_ui/draw_list.h"
#include "battle_ui/selection.h"
#include "battle_ui/sprite_catalog.h"
#include "battle_ui/widgets.h"
#include "space_battle/view_model.h"

// Draws the battle plane and everything on it (tactical_combat.md §10.1, §10.3, §10.4): background, grid, wells,
// the tactical overlay, units as art, silhouettes or symbols, craft, bolts, effects and markers, in the top-down or
// the angled view.
namespace reb::battle_ui
{

struct WorldScene {
  const battle::ViewModel& view;
  const Camera& camera;
  const SpriteCatalog& sprites;
  const Theme& theme;
  double seconds = 0.0; // real time, for cosmetic animation
  const Selection& selection;
  battle::UnitIndex hovered = battle::kNoUnit;
  bool overlay = false;
  battle::Side playerSide = battle::Side::Alliance;
  const std::vector<battle::ViewUnitDetail>& selectedDetails; // the selection's arcs and ranges, for the overlay
  const std::vector<std::string>& names;                      // by unit index; may be shorter than the unit list
  uint32_t systemPicture = 0;         // BattleParams::systemPictureId: the planet type drawn behind the defender
  const TextMeasurer* text = nullptr; // sizes labels so that overlapping ones can be dropped; estimated without one
};

void PaintWorld(DrawList& out, const WorldScene& scene);

// Faction colours: art tint and symbols; and the bolt colour code (red Alliance, green Empire, blue ion by default),
// from the theme so that an accessible palette recolours bolts too.
Color FactionColor(const Theme& theme, battle::Side side);
Color BoltColor(const Theme& theme, battle::Side side, battle::ViewShotKind kind);

// A planetary battery the viewer knows of (§6.13): its own, one its intelligence reported, or one that has opened fire.
bool DefenseVisible(const battle::ViewDefense& defense, battle::Side viewer);
// Counted by class and weapon: "1 LNR Series II turbolaser battery and 2 KDY-150 ion cannons".
std::string DescribeBatteries(const std::vector<const battle::ViewDefense*>& defenses);

} // namespace reb::battle_ui
