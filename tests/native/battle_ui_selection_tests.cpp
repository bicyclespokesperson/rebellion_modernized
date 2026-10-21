// Hit-testing and selection: picking, box selection, the selection's rules, number-key groups, and the screen's
// click, shift-click, box and hotkey gestures.
#include "battle_ui/selection.h"
#include "battle_ui_screen_driver.h"

using namespace reb::battle_ui;
using namespace reb::battle_ui::test;
using reb::battle::kNoUnit;
using reb::battle::Side;

namespace
{

Camera MakeCamera(float zoom)
{
  Camera camera;
  camera.SetViewport({0.0f, 0.0f, 1280.0f, 720.0f});
  camera.SetZoom(zoom);
  return camera;
}

void Picking()
{
  const reb::battle::ViewModel view = SmallBattle();
  const Camera camera = MakeCamera(0.05f);
  const auto at = [&](reb::battle::UnitIndex unit) {
    return camera.WorldToScreen(view.units[unit].position);
  };
  REQUIRE(PickUnit(view, camera, at(0), 12.0f) == 0);
  REQUIRE(PickUnit(view, camera, at(0) + Point{15.0f, 0.0f}, 12.0f) == 0); // within the drawn radius (20 px)
  REQUIRE(PickUnit(view, camera, at(2), 12.0f) == 2);
  REQUIRE(PickUnit(view, camera, at(3), 12.0f) == 3);
  REQUIRE(PickUnit(view, camera, {1270.0f, 10.0f}, 12.0f) == kNoUnit);
  REQUIRE(PickUnit(view, camera, at(4), 12.0f) == kNoUnit); // destroyed

  // A ship smaller than the minimum pick radius on screen is still hit within that radius.
  const Camera far = MakeCamera(0.005f);
  const Point small = far.WorldToScreen(view.units[1].position);
  REQUIRE(PickUnit(view, far, small + Point{10.0f, -5.0f}, 12.0f) == 1);
  REQUIRE(PickUnit(view, far, small + Point{10.0f, -5.0f}, 6.0f) == kNoUnit);

  // Overlapping units: the closer one relative to its pick radius, then the lower index.
  reb::battle::ViewModel stacked = view;
  stacked.units[1].position = stacked.units[0].position;
  stacked.units[1].radius = stacked.units[0].radius;
  REQUIRE(PickUnit(stacked, camera, at(0), 12.0f) == 0);
}

void RectSelection()
{
  const reb::battle::ViewModel view = SmallBattle();
  const Camera camera = MakeCamera(0.05f);
  const Rect everything{0.0f, 0.0f, 1280.0f, 720.0f};
  REQUIRE((UnitsInRect(view, camera, everything, Side::Alliance) == std::vector<reb::battle::UnitIndex>{0, 1, 3}));
  REQUIRE((UnitsInRect(view, camera, everything, Side::Empire) == std::vector<reb::battle::UnitIndex>{2}));
  const Point squadron = camera.WorldToScreen(view.units[3].position);
  REQUIRE((UnitsInRect(view, camera, {squadron.x - 5.0f, squadron.y - 5.0f, 10.0f, 10.0f}, Side::Alliance)
           == std::vector<reb::battle::UnitIndex>{3}));
}

void SelectionRules()
{
  const reb::battle::ViewModel view = SmallBattle();
  Selection selection;
  selection.Toggle(0, true);
  selection.Toggle(1, true);
  REQUIRE((selection.Units() == std::vector<reb::battle::UnitIndex>{0, 1}));
  selection.Toggle(0, true);
  REQUIRE((selection.Units() == std::vector<reb::battle::UnitIndex>{1}));
  selection.Toggle(2, false); // an enemy replaces the selection, for inspection only
  REQUIRE((selection.Units() == std::vector<reb::battle::UnitIndex>{2}));
  REQUIRE(selection.Orderable(view, Side::Alliance).empty());
  selection.Toggle(3, true); // and is dropped as soon as an own unit is added
  REQUIRE((selection.Units() == std::vector<reb::battle::UnitIndex>{3}));
  selection.Set({3, 0, 4});
  selection.Prune(view);
  REQUIRE((selection.Units() == std::vector<reb::battle::UnitIndex>{3, 0}));
  REQUIRE((selection.Orderable(view, Side::Alliance) == std::vector<reb::battle::UnitIndex>{0, 3}));
}

void HotkeyGroupsSeedFromTaskForces()
{
  const reb::battle::ViewModel view = SmallBattle();
  HotkeyGroups groups;
  groups.SeedFromGroups(view, Side::Alliance);
  REQUIRE((groups.Get(1) == std::vector<reb::battle::UnitIndex>{0, 1}));
  REQUIRE((groups.Get(2) == std::vector<reb::battle::UnitIndex>{3}));
  REQUIRE(groups.Get(3).empty());
  groups.Store(1, {1});
  groups.SeedFromGroups(view, Side::Alliance);
  REQUIRE((groups.Get(1) == std::vector<reb::battle::UnitIndex>{1}));
}

void ScreenClicks()
{
  ScreenDriver d;
  d.Click(d.At(0));
  REQUIRE((d.Selected() == std::vector<reb::battle::UnitIndex>{0}));
  d.Click(d.At(1), MouseButton::Left, kModShift);
  REQUIRE((d.Selected() == std::vector<reb::battle::UnitIndex>{0, 1}));
  d.Click(d.At(0), MouseButton::Left, kModShift);
  REQUIRE((d.Selected() == std::vector<reb::battle::UnitIndex>{1}));
  d.Click(d.At(2));
  REQUIRE((d.Selected() == std::vector<reb::battle::UnitIndex>{2}));
  const Point empty = d.screen.GetCamera().WorldToScreen({-1500.0f, 2500.0f});
  d.Click(empty);
  REQUIRE(d.Selected().empty());

  // Double-click centres the unit in the battle area between the HUD's panels.
  d.Click(d.At(2), MouseButton::Left, 0, 2);
  REQUIRE(Near(d.At(2), d.screen.Layout().battleArea.Center(), 0.5f));
}

void ScreenBoxSelection()
{
  ScreenDriver d;
  Point lo = d.At(0);
  Point hi = d.At(0);
  for (const reb::battle::UnitIndex unit : {0, 1, 2, 3}) {
    const Point p = d.At(unit);
    lo = {std::min(lo.x, p.x), std::min(lo.y, p.y)};
    hi = {std::max(hi.x, p.x), std::max(hi.y, p.y)};
  }
  d.Drag(lo - Point{10.0f, 10.0f}, hi + Point{10.0f, 10.0f});
  REQUIRE((d.Selected() == std::vector<reb::battle::UnitIndex>{0, 1, 3})); // own units only

  // Shift adds to the selection; a box around one unit replaces it otherwise.
  d.screen.GetSelection().Clear();
  d.Frame();
  const Point squadron = d.At(3);
  d.Drag(squadron - Point{8.0f, 8.0f}, squadron + Point{8.0f, 8.0f});
  REQUIRE((d.Selected() == std::vector<reb::battle::UnitIndex>{3}));
  const Point flagship = d.At(0);
  d.Drag(flagship - Point{8.0f, 8.0f}, flagship + Point{8.0f, 8.0f}, MouseButton::Left, kModShift);
  REQUIRE((d.Selected() == std::vector<reb::battle::UnitIndex>{3, 0}));
}

void ScreenHotkeys()
{
  ScreenDriver d;
  d.Press(DigitKey(1));
  REQUIRE((d.Selected() == std::vector<reb::battle::UnitIndex>{0, 1}));
  d.Press(DigitKey(2));
  REQUIRE((d.Selected() == std::vector<reb::battle::UnitIndex>{3}));
  d.Press(DigitKey(1), kModShift);
  REQUIRE((d.Selected() == std::vector<reb::battle::UnitIndex>{3, 0, 1}));
  d.Press(DigitKey(7), kModCtrl); // store
  d.screen.GetSelection().Clear();
  d.Press(DigitKey(7));
  REQUIRE((d.Selected() == std::vector<reb::battle::UnitIndex>{0, 1, 3}));
  d.Press(DigitKey(9)); // empty group: the selection stays
  REQUIRE(d.Selected().size() == 3);
}

} // namespace

int main()
{
  Picking();
  RectSelection();
  SelectionRules();
  HotkeyGroupsSeedFromTaskForces();
  ScreenClicks();
  ScreenBoxSelection();
  ScreenHotkeys();
  std::puts("battle_ui selection tests passed");
}
