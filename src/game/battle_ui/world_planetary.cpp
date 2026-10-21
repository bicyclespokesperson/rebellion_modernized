// Planetary batteries (tactical_combat.md §6.13): the cover a side's batteries reach, drawn as a fortified ring around
// the planet with its danger zone faintly tinted, and the emplacements on the planet's limb with their muzzle flashes.
// The ring's inward ticks set it apart from a gravity well's plain circle.
#include <algorithm>

#include "battle_ui/world_painter_internal.h"
#include "space_battle/math2d.h"

namespace reb::battle_ui::painter
{
namespace
{

constexpr float kMuzzleSeconds = 0.4f;
constexpr float kTickSpacingPx = 14.0f;
constexpr float kTickLengthPx = 7.0f;
constexpr int kMaxTicks = 720;

bool SideHasVisibleBattery(const Painter& p, Side side, bool* firing)
{
  bool any = false;
  for (const battle::ViewDefense& defense : p.scene.view.defenses) {
    if (defense.side == side && DefenseVisible(defense, p.scene.playerSide)) {
      any = true;
      *firing = *firing || (defense.sinceFired >= 0.0f && defense.sinceFired < 3.5f);
    }
  }
  return any;
}

void PaintCover(const Painter& p, Side side, bool firing)
{
  const battle::ViewModel& view = p.scene.view;
  const Color color = FactionColor(p.scene.theme, side);
  const float pulse = firing ? 0.5f + 0.5f * static_cast<float>(std::sin(p.scene.seconds * 2.5)) : 0.0f;
  PlaneSector(p.out, p.camera, view.planetCenter, view.planetRadius, view.coverRadius, 0.0f, kTwoPi,
              color.WithAlpha(p.detailed ? 9 : 15));
  PlaneCircle(p.out, p.camera, view.planetCenter, view.coverRadius,
              color.WithAlpha(static_cast<uint8_t>(120.0f + 70.0f * pulse)), Stroke{1.5f * p.ui});
  // Ticks pointing in toward the planet, only where the ring is on screen.
  const float radiusPx = std::max(1.0f, p.Px(view.coverRadius));
  const int ticks = std::clamp(static_cast<int>(kTwoPi * radiusPx / (kTickSpacingPx * p.ui)), 24, kMaxTicks);
  const float inset = p.camera.MetersFor(kTickLengthPx * p.ui);
  const Rect visible = p.camera.Viewport().Inset(-20.0f * p.ui);
  const Color tick = color.WithAlpha(static_cast<uint8_t>(90.0f + 60.0f * pulse));
  for (int i = 0; i < ticks; ++i) {
    const Vec2 direction = battle::FromAngle(kTwoPi * static_cast<float>(i) / static_cast<float>(ticks));
    const Point outer = p.Screen(view.planetCenter + direction * view.coverRadius);
    if (!visible.Contains(outer)) {
      continue;
    }
    p.out.Line(outer, p.Screen(view.planetCenter + direction * (view.coverRadius - inset)), tick, Stroke{1.0f * p.ui});
  }
}

// A turret on the limb: a dark base ringed in its weapon's colour, the barrel laid on its target (or straight out),
// and a flash at the muzzle as a salvo leaves.
void PaintEmplacement(const Painter& p, const battle::ViewDefense& defense)
{
  const battle::ViewModel& view = p.scene.view;
  const Point at = p.Screen(defense.site);
  if (!p.camera.Viewport().Inset(-30.0f * p.ui).Contains(at)) {
    return;
  }
  const Color color = defense.weapon == battle::WeaponKind::Ion ? p.scene.theme.ion : FactionColor(p.scene.theme, defense.side);
  Vec2 aim = defense.site - view.planetCenter;
  if (const ViewUnit* target = defense.target == battle::kNoUnit ? nullptr : FindUnit(view, defense.target)) {
    aim = target->position - defense.site;
  }
  Point barrel = p.Screen(defense.site + battle::Normalized(aim) * 1000.0f) - at;
  const float barrelLength = std::max(0.001f, std::hypot(barrel.x, barrel.y));
  barrel = barrel * (1.0f / barrelLength);
  const float size = 6.0f * p.ui;
  const Point muzzle = at + barrel * (size * 2.3f);
  p.out.FillCircle(at, size * 2.2f, color.WithAlpha(28), Blend::Additive);
  p.out.Line(at, muzzle, color, Stroke{2.6f * p.ui});
  p.out.FillCircle(at, size, Rgb(0x0a1220, 235));
  p.out.Circle(at, size, color, Stroke{1.5f * p.ui});
  p.out.FillCircle(at, size * 0.35f, color);
  if (defense.weapon == battle::WeaponKind::Ion) {
    p.out.Circle(at, size * 1.6f, color.WithAlpha(110), Stroke{1.0f * p.ui, 2.0f * p.ui, 2.0f * p.ui});
  }
  if (defense.sinceFired >= 0.0f && defense.sinceFired < kMuzzleSeconds) {
    const float t = defense.sinceFired / kMuzzleSeconds;
    const Color flash = Mix(color, Rgb(0xffffff), 0.55f);
    p.out.FillCircle(muzzle, (4.0f + 9.0f * t) * p.ui, flash.WithAlpha(static_cast<uint8_t>(210.0f * (1.0f - t))),
                     Blend::Additive);
    p.out.FillCircle(at, (7.0f + 6.0f * t) * p.ui, color.WithAlpha(static_cast<uint8_t>(90.0f * (1.0f - t))),
                     Blend::Additive);
  }
}

} // namespace

void PaintPlanetaryCover(const Painter& p)
{
  if (p.scene.view.coverRadius <= 0.0f) {
    return;
  }
  for (const Side side : {Side::Alliance, Side::Empire}) {
    bool firing = false;
    if (SideHasVisibleBattery(p, side, &firing)) {
      PaintCover(p, side, firing);
    }
  }
}

void PaintEmplacements(const Painter& p)
{
  for (const battle::ViewDefense& defense : p.scene.view.defenses) {
    if (DefenseVisible(defense, p.scene.playerSide)) {
      PaintEmplacement(p, defense);
    }
  }
}

} // namespace reb::battle_ui::painter

namespace reb::battle_ui
{

bool DefenseVisible(const battle::ViewDefense& defense, battle::Side viewer)
{
  return defense.side == viewer || defense.briefingKnown[static_cast<size_t>(viewer)] || defense.sinceFired >= 0.0f;
}

std::string DescribeBatteries(const std::vector<const battle::ViewDefense*>& defenses)
{
  struct Kind {
    std::string name;
    bool ion;
    int count;
  };
  std::vector<Kind> kinds;
  for (const battle::ViewDefense* defense : defenses) {
    const bool ion = defense->weapon == battle::WeaponKind::Ion;
    const std::string& name = defense->name;
    const auto found = std::find_if(kinds.begin(), kinds.end(),
                                    [&](const Kind& kind) { return kind.name == name && kind.ion == ion; });
    if (found == kinds.end()) {
      kinds.push_back({name, ion, 1});
    } else {
      ++found->count;
    }
  }
  std::string text;
  for (size_t i = 0; i < kinds.size(); ++i) {
    if (i > 0) {
      text += i + 1 == kinds.size() ? " and " : ", ";
    }
    const Kind& kind = kinds[i];
    const bool several = kind.count > 1;
    text += std::to_string(kind.count) + (kind.name.empty() ? "" : " " + kind.name) + " "
            + (kind.ion ? (several ? "ion cannons" : "ion cannon")
                        : (several ? "turbolaser batteries" : "turbolaser battery"));
  }
  return text;
}

} // namespace reb::battle_ui
