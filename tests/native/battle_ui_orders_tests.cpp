// Input -> Command: the right-click and right-drag orders, Hold, modifiers, who may be ordered, the AI handover on the
// first order, and the clock that turns real time into 20 Hz ticks.
#include "battle_ui/orders.h"
#include "battle_ui_screen_driver.h"
#include "space_battle/battle.h"

using namespace reb::battle_ui;
using namespace reb::battle_ui::test;
using reb::battle::Command;
using reb::battle::Side;

namespace
{

template <class T>
const T& Order(const Command& command)
{
  const T* order = std::get_if<T>(&command.order);
  REQUIRE(order != nullptr);
  return *order;
}

void AttackMoveEscort()
{
  ScreenDriver d;
  REQUIRE(d.source.aiControlled[0] && d.source.aiControlled[1]); // both sides under the AI until the first order
  d.Click(d.At(0));
  d.source.steps = 7;
  d.Click(d.At(2), MouseButton::Right);
  REQUIRE(d.source.commands.size() == 1);
  const Command& attack = d.source.commands.back();
  REQUIRE(attack.side == Side::Alliance && attack.tick == 7 && !attack.queue);
  REQUIRE((attack.target.units == std::vector<reb::battle::UnitIndex>{0}));
  REQUIRE((Order<reb::battle::OrderAttack>(attack).targets == std::vector<reb::battle::UnitIndex>{2}));
  REQUIRE(!d.source.aiControlled[0] && d.source.aiControlled[1]); // the player took command

  d.Click(d.At(2), MouseButton::Right, kModCtrl); // Ctrl adds a further target after the current ones
  REQUIRE(d.source.commands.back().queue);
  REQUIRE(std::holds_alternative<reb::battle::OrderAttack>(d.source.commands.back().order));

  const Point plane = d.screen.GetCamera().WorldToScreen({-1500.0f, 2500.0f});
  d.Click(plane, MouseButton::Right);
  const auto& move = Order<reb::battle::OrderMove>(d.source.commands.back());
  REQUIRE(Near(move.position.x, -1500.0f, 30.0f) && Near(move.position.y, 2500.0f, 30.0f));
  REQUIRE(!move.facing.has_value() && !d.source.commands.back().queue);
  d.Click(plane, MouseButton::Right, kModShift);
  REQUIRE(d.source.commands.back().queue);

  d.Click(d.At(1), MouseButton::Right); // a friendly unit outside the selection
  REQUIRE(Order<reb::battle::OrderEscort>(d.source.commands.back()).target == 1);
}

void DragOrders()
{
  ScreenDriver d;
  d.Click(d.At(0));
  const Camera& camera = d.screen.GetCamera();
  const Point from = camera.WorldToScreen({-1000.0f, -500.0f});
  d.Drag(from, from + Point{0.0f, -60.0f}, MouseButton::Right); // up the screen: face +y
  const auto& move = Order<reb::battle::OrderMove>(d.source.commands.back());
  REQUIRE(Near(move.position.x, -1000.0f, 30.0f) && Near(move.position.y, -500.0f, 30.0f));
  REQUIRE(move.facing && Near(*move.facing, kPi * 0.5f, 0.01f));

  // From the enemy at +x of the selection: dragging up the screen hooks left (port), down hooks right.
  const Point enemy = d.At(2);
  d.Drag(enemy, enemy + Point{0.0f, -80.0f}, MouseButton::Right);
  REQUIRE(Order<reb::battle::OrderApproach>(d.source.commands.back()).approach == reb::battle::Approach::LeftHook);
  REQUIRE(Order<reb::battle::OrderApproach>(d.source.commands.back()).target == 2);
  d.Drag(enemy, enemy + Point{0.0f, 80.0f}, MouseButton::Right);
  REQUIRE(Order<reb::battle::OrderApproach>(d.source.commands.back()).approach == reb::battle::Approach::RightHook);
  d.Drag(enemy, enemy + Point{80.0f, 0.0f}, MouseButton::Right);
  REQUIRE(Order<reb::battle::OrderApproach>(d.source.commands.back()).approach == reb::battle::Approach::Direct);
}

void WhoCanBeOrdered()
{
  ScreenDriver d;
  d.Click(d.At(2), MouseButton::Right); // nothing selected
  REQUIRE(d.source.commands.empty());
  d.Click(d.At(2)); // an enemy, selected for inspection
  d.Click(d.At(0), MouseButton::Right);
  REQUIRE(d.source.commands.empty());
  REQUIRE(d.source.aiControlled[0]);

  d.Press(DigitKey(1));
  d.Press(DigitKey(2), kModShift);
  d.Press(Key::H);
  REQUIRE(d.source.commands.size() == 1);
  REQUIRE(std::holds_alternative<reb::battle::OrderHold>(d.source.commands.back().order));
  REQUIRE((d.source.commands.back().target.units == std::vector<reb::battle::UnitIndex>{0, 1, 3}));
}

void PlayerSideEmpire()
{
  BattleScreenOptions options;
  options.playerSide = Side::Empire;
  options.aiUntilFirstOrder = false;
  ScreenDriver d(options);
  REQUIRE(d.source.aiControlled[0] && !d.source.aiControlled[1]);
  d.Click(d.At(2));
  d.Click(d.At(0), MouseButton::Right);
  REQUIRE(d.source.commands.size() == 1 && d.source.commands.back().side == Side::Empire);
  REQUIRE(Order<reb::battle::OrderAttack>(d.source.commands.back()).targets.front() == 0);
}

void OrderTranslationWithoutTheScreen()
{
  const reb::battle::ViewModel view = SmallBattle();
  const OrderContext context{Side::Alliance, 3, {0, 3}, false};
  const auto attack = ClickOrder(context, view, 2, {}, false);
  REQUIRE(attack && attack->tick == 3 && (attack->target.units == std::vector<reb::battle::UnitIndex>{0, 3}));
  const auto own = ClickOrder(context, view, 0, {10.0f, 20.0f}, false); // a selected unit: move onto it
  REQUIRE(own && std::holds_alternative<reb::battle::OrderMove>(own->order));
  REQUIRE(!SelectionOrder({Side::Alliance, 0, {}, false}, reb::battle::OrderWithdraw{}));
}

void ClockTurnsRealTimeIntoTicks()
{
  ScreenDriver d;
  REQUIRE(d.screen.Paused());
  d.screen.Update(1.0);
  REQUIRE(d.source.steps == 0);
  d.Press(Key::Space);
  REQUIRE(!d.screen.Paused());
  d.screen.Update(1.0);
  REQUIRE(d.source.steps == 20);
  d.screen.Update(0.025); // half a tick: interpolated, not stepped
  REQUIRE(d.source.steps == 20 && Near(d.source.lastAlpha, 0.5f, 1e-3f));
  d.screen.Update(0.025);
  REQUIRE(d.source.steps == 21);

  d.Press(Key::Equals);
  d.Press(Key::Equals);
  REQUIRE(d.screen.SpeedIndex() == 3); // 4x
  d.screen.Update(0.5);
  REQUIRE(d.source.steps == 61);
  d.Press(Key::Equals);
  d.Press(Key::Equals);
  REQUIRE(d.screen.SpeedIndex() == 4); // 8x is the top
  d.screen.Update(1.0);
  REQUIRE(d.source.steps == 61 + BattleScreen::kMaxTicksPerUpdate); // a long frame drops time instead of spiralling
  for (int i = 0; i < 6; ++i) {
    d.Press(Key::Minus);
  }
  REQUIRE(d.screen.SpeedIndex() == 0);
  d.screen.Update(1.0);
  REQUIRE(d.source.steps == 61 + BattleScreen::kMaxTicksPerUpdate + 10);

  d.source.View().finished = true;
  d.screen.Update(1.0);
  REQUIRE(d.source.steps == 61 + BattleScreen::kMaxTicksPerUpdate + 10);
  REQUIRE(d.screen.BattleFinished());
}

void TopBarButtons()
{
  ScreenDriver d;
  d.Click(d.At(0));
  d.Frame();
  // The 2x button: pause button, then 0.5x, 1x, 2x from the left of the top bar.
  const float pad = 6.0f;
  const Point twoX{pad + 30.0f + 4.0f + 2.0f * 42.0f + 20.0f, 17.0f};
  d.Click(twoX);
  d.Frame();
  REQUIRE(d.screen.SpeedIndex() == 2 && !d.screen.Paused());
  REQUIRE((d.Selected() == std::vector<reb::battle::UnitIndex>{0})); // the click did not reach the battle view
  d.Click({pad + 10.0f, 17.0f});                                     // pause
  d.Frame();
  REQUIRE(d.screen.Paused());
}

void CameraControls()
{
  ScreenDriver d;
  Camera& camera = d.screen.GetCamera();
  const reb::battle::Vec2 start = camera.Center();
  d.Move({640.0f, 400.0f});
  d.screen.HandleInput(KeyEvent{Key::D, true, false, 0}); // pan right while held
  d.Frame(0.5);
  d.screen.HandleInput(KeyEvent{Key::D, false, false, 0});
  const reb::battle::Vec2 panned = camera.Center();
  REQUIRE(panned.x > start.x + 100.0f && Near(panned.y, start.y, 1.0f));
  d.Frame(0.5);
  REQUIRE(Near(camera.Center().x, panned.x, 0.01f));

  d.Move({1279.0f, 400.0f}); // the right edge scrolls the same way
  d.Frame(0.5);
  REQUIRE(camera.Center().x > panned.x + 100.0f);
  const reb::battle::Vec2 edged = camera.Center();
  d.Move({-1.0f, -1.0f}); // the mouse left the window: no scrolling
  d.Frame(0.5);
  REQUIRE(Near(camera.Center().x, edged.x, 0.01f) && Near(camera.Center().y, edged.y, 0.01f));

  d.Press(Key::F1, kModCtrl); // bookmark this view
  const float zoom = camera.Zoom();
  d.Press(Key::Q);
  REQUIRE(camera.RotationStep() == 1);
  d.Press(Key::Home);
  d.Press(Key::F1);
  REQUIRE(Near(camera.Center().x, edged.x, 0.01f) && Near(camera.Zoom(), zoom, 1e-6f) && camera.RotationStep() == 0);
  d.Press(Key::F2); // nothing stored there
  REQUIRE(Near(camera.Center().x, edged.x, 0.01f));

  d.Click(d.At(2));
  d.Press(Key::F); // follow the selected unit as it moves
  d.source.View().units[2].position = {7000.0f, 500.0f};
  d.Move({640.0f, 400.0f});
  d.Frame(0.1);
  const Point areaCenter = d.screen.Layout().battleArea.Center();
  REQUIRE(Near(d.At(2), areaCenter, 0.5f));
  d.Move({1279.0f, 400.0f}); // edge scrolling does not release the followed unit
  d.Frame(0.5);
  REQUIRE(d.screen.Following() == 2 && Near(d.At(2), areaCenter, 0.5f));
  d.screen.HandleInput(MouseWheel{{900.0f, 300.0f}, 0.0f, 1.0f}); // the wheel zooms on the followed unit
  REQUIRE(Near(d.At(2), areaCenter, 0.5f));
  d.Move({640.0f, 400.0f});
  d.screen.GetSelection().Set({3});
  d.Press(Key::F); // a different selection switches the follow instead of releasing it
  REQUIRE(d.screen.Following() == 3 && Near(d.At(3), areaCenter, 0.5f));
  d.Press(Key::F);
  REQUIRE(d.screen.Following() == reb::battle::kNoUnit);
}

void SavedScreenRoundTrip()
{
  ScreenDriver original;
  original.Click(original.At(0));
  original.Click(original.At(2), MouseButton::Right);
  original.Press(Key::Num3, kModCtrl);
  original.screen.CenterAt({1234.0f, -5678.0f});
  original.screen.GetCamera().SetZoom(0.12f);
  original.screen.GetCamera().Rotate(3);
  original.screen.GetCamera().SetView(CameraView::Angled);
  original.Press(Key::F1, kModCtrl);
  original.screen.SetSpeedIndex(4);
  original.screen.SetOverlay(false);
  const auto saved = original.screen.SaveUiState();
  ScreenDriver restored;
  REQUIRE(restored.screen.RestoreUiState(saved));
  REQUIRE(restored.screen.SaveUiState() == saved);
  REQUIRE(restored.Selected() == original.Selected());
  REQUIRE(restored.screen.Hotkeys().Get(3) == original.screen.Hotkeys().Get(3));
  REQUIRE(!restored.screen.RestoreUiState("REB_BATTLE_UI 99"));
  REQUIRE(!restored.screen.RestoreUiState(saved + "garbage"));
  REQUIRE(restored.screen.SaveUiState() == saved);
}

void EscapeAndClose()
{
  ScreenDriver d;
  d.Press(Key::Escape);
  REQUIRE(d.screen.QuitRequested());
  ScreenDriver e;
  e.screen.HandleInput(CloseRequested{});
  REQUIRE(e.screen.QuitRequested());
}

} // namespace

int main()
{
  AttackMoveEscort();
  DragOrders();
  WhoCanBeOrdered();
  PlayerSideEmpire();
  OrderTranslationWithoutTheScreen();
  ClockTurnsRealTimeIntoTicks();
  TopBarButtons();
  CameraControls();
  EscapeAndClose();
  SavedScreenRoundTrip();
  std::puts("battle_ui orders tests passed");
}
