// Draw-list contents for a small fabricated view model: the top bar and info box, silhouettes against sprites and
// tactical symbols, shots, the overlay, and the widget kit's click, toggle, list and tooltip behaviour.
#include <algorithm>
#include <string_view>

#include "battle_ui/draw_list.h"
#include "battle_ui/unit_names.h"
#include "battle_ui/widgets.h"
#include "battle_ui/world_painter.h"
#include "battle_ui_screen_driver.h"

using namespace reb::battle_ui;
using namespace reb::battle_ui::test;

namespace
{

bool HasText(const DrawList& list, std::string_view text)
{
  return std::ranges::any_of(list.Commands(), [text](const DrawCommand& command) {
    const auto* drawn = std::get_if<DrawText>(&command);
    return drawn && drawn->text == text;
  });
}

template <class T, class Predicate>
size_t CountIf(const DrawList& list, Predicate predicate)
{
  return static_cast<size_t>(std::ranges::count_if(list.Commands(), [&](const DrawCommand& command) {
    const T* drawn = std::get_if<T>(&command);
    return drawn && predicate(*drawn);
  }));
}

class SomeSprites : public SpriteCatalog
{
  public:
  bool Has(int32_t typeIndex, SpriteView view) const override
  {
    return typeIndex == 0 && view == SpriteView::TopDown;
  }
};

void TopBarAndInfoBox()
{
  ScreenDriver d;
  const DrawList& frame = d.Frame();
  REQUIRE(HasText(frame, "00:00"));
  REQUIRE(HasText(frame, "ALLIANCE") && HasText(frame, "EMPIRE"));
  REQUIRE(HasText(frame, "80%") && HasText(frame, "50%"));
  REQUIRE(HasText(frame, "Signals 1/3"));
  REQUIRE(HasText(frame, "PAUSED"));
  REQUIRE(HasText(frame, "Unit 0") && !HasText(frame, "Unit 4")); // destroyed units are not labelled
  for (const char* speed : {"0.5x", "1x", "2x", "4x", "8x"}) {
    REQUIRE(HasText(frame, speed));
  }

  d.Click(d.At(0));
  const DrawList& selected = d.Frame();
  REQUIRE(HasText(selected, "Mon Calamari Cruiser") && HasText(selected, "Shields"));
  d.Press(DigitKey(1));
  REQUIRE(HasText(d.Frame(), "2 selected"));

  // Clips are balanced: the frame ends with no clip set.
  const auto lastClip
      = std::ranges::find_if(d.frame.Commands().rbegin(), d.frame.Commands().rend(), [](const DrawCommand& c) {
          return std::holds_alternative<DrawClip>(c);
        });
  REQUIRE(lastClip != d.frame.Commands().rend() && !std::get<DrawClip>(*lastClip).rect);
}

void ArtSilhouettesAndSymbols()
{
  ScreenDriver d;
  d.screen.SetOverlay(false);         // the overlay (on while paused) uses the tactical look
  d.screen.GetCamera().SetZoom(0.2f); // detailed: art for the classes the pack has, silhouettes for the rest
  d.screen.CenterOn(0);
  REQUIRE(d.Frame().Count<DrawSprite>() == 0);
  REQUIRE(d.frame.Count<DrawPolygon>() > 10);

  RecordingSource source;
  FixedTextMeasurer text;
  SomeSprites sprites;
  BattleScreen screen(source, text, sprites);
  screen.HandleInput(ScreenChanged{ScreenInfo{1280, 720, 1.0f, 1.0f}});
  screen.SetOverlay(false);
  screen.GetCamera().SetZoom(0.2f);
  screen.CenterOn(0);
  DrawList frame;
  screen.Update(0.0);
  screen.Build(frame);
  REQUIRE(CountIf<DrawSprite>(frame,
                              [](const DrawSprite& s) {
                                return s.typeIndex == 0 && s.length > 100.0f;
                              })
          >= 1);
  REQUIRE(CountIf<DrawSprite>(frame,
                              [](const DrawSprite& s) {
                                return s.typeIndex != 0;
                              })
          == 0);

  // Zoomed out the same ships become tactical symbols: no art at all.
  screen.GetCamera().SetZoom(Camera::kMinZoom);
  frame.Clear();
  screen.Build(frame);
  REQUIRE(frame.Count<DrawSprite>() == 0);
  REQUIRE(frame.Count<DrawPolygon>() > 0);
}

void ShotsUseTheColourCode()
{
  ScreenDriver d;
  reb::battle::ViewShot alliance;
  alliance.side = reb::battle::Side::Alliance;
  alliance.from = {0.0f, 0.0f};
  alliance.to = {6000.0f, 0.0f};
  alliance.progress = 0.5f;
  reb::battle::ViewShot ion = alliance;
  ion.kind = reb::battle::ViewShotKind::Ion;
  ion.side = reb::battle::Side::Empire;
  d.source.View().shots = {alliance, ion};
  const DrawList& frame = d.Frame();
  const auto additive = [](const DrawPolyline& line) {
    return line.blend == Blend::Additive;
  };
  REQUIRE(CountIf<DrawPolyline>(frame, additive) == 4); // a glow and a core per bolt
  REQUIRE(CountIf<DrawPolyline>(frame,
                                [](const DrawPolyline& l) {
                                  return l.blend == Blend::Additive && l.color.r > 200 && l.color.g < 120;
                                })
          == 1); // red Alliance glow
  REQUIRE(CountIf<DrawPolyline>(frame,
                                [](const DrawPolyline& l) {
                                  return l.blend == Blend::Additive && l.color.b > 200 && l.color.r < 120;
                                })
          == 1); // blue ion glow
  REQUIRE(BoltColor(DefaultTheme(), reb::battle::Side::Empire, reb::battle::ViewShotKind::Laser).g > 200);
  Theme accessible;
  accessible.ionBolt = Rgb(0xc89cff);
  REQUIRE(BoltColor(accessible, reb::battle::Side::Empire, reb::battle::ViewShotKind::Ion) == Rgb(0xc89cff));
}

void OverlayDrawsArcsAndTargetLines()
{
  ScreenDriver d;
  d.source.View().units[0].target = 2;
  d.source.View().units[0].firing = true;
  d.Click(d.At(0));
  const auto arcs = [](const DrawList& list) {
    return CountIf<DrawSector>(list, [](const DrawSector& s) {
      return Near(s.sweep, 100.0f * kPi / 180.0f, 1e-4f);
    });
  };
  d.screen.SetOverlay(false);
  REQUIRE(arcs(d.Frame()) == 0);
  d.screen.SetOverlay(true);
  REQUIRE(d.screen.OverlayVisible());
  d.Frame();
  // The fixture's detail has fore and port batteries: two arcs, and a solid target line to the enemy.
  REQUIRE(arcs(d.frame) == 2);
  const Point from = d.At(0);
  const Point to = d.At(2);
  REQUIRE(CountIf<DrawPolyline>(d.frame,
                                [&](const DrawPolyline& l) {
                                  return l.points.size() == 2 && Near(l.points[0], from, 0.5f)
                                         && Near(l.points[1], to, 0.5f) && l.stroke.dash == 0.0f;
                                })
          == 1);
  // Pausing turns the overlay on by default; Tab overrides it until the pause state changes.
  d.screen.SetOverlay(false);
  d.Press(Key::Space);
  REQUIRE(!d.screen.Paused() && !d.screen.OverlayVisible());
  d.Press(Key::Space);
  REQUIRE(d.screen.OverlayVisible());
  d.Press(Key::Tab);
  REQUIRE(!d.screen.OverlayVisible());
}

void WidgetClicks()
{
  Ui ui;
  DrawList draw;
  FixedTextMeasurer text;
  const ScreenInfo screen{800, 600, 1.0f, 1.0f};
  const Rect button{10.0f, 10.0f, 80.0f, 24.0f};
  const WidgetId id = Id("test.button");
  const auto frame = [&](double seconds = 0.0) {
    draw.Clear();
    ui.BeginFrame(draw, text, screen, seconds);
    ui.Panel({0.0f, 0.0f, 300.0f, 200.0f});
    const bool clicked = ui.Button(id, button, "Go");
    ui.EndFrame();
    return clicked;
  };
  REQUIRE(!frame());
  REQUIRE(ui.ProcessEvent(MouseButtonEvent{{20.0f, 20.0f}, MouseButton::Left, true}));
  REQUIRE(ui.MouseCaptured());
  REQUIRE(ui.ProcessEvent(MouseButtonEvent{{25.0f, 22.0f}, MouseButton::Left, false}));
  REQUIRE(frame());
  REQUIRE(!frame());
  // Pressed on the button but released elsewhere: no click.
  ui.ProcessEvent(MouseButtonEvent{{20.0f, 20.0f}, MouseButton::Left, true});
  ui.ProcessEvent(MouseButtonEvent{{200.0f, 150.0f}, MouseButton::Left, false});
  REQUIRE(!frame());
  // The panel absorbs clicks on its empty area; outside it nothing is consumed.
  REQUIRE(ui.ProcessEvent(MouseButtonEvent{{200.0f, 150.0f}, MouseButton::Right, true}));
  REQUIRE(!ui.ProcessEvent(MouseButtonEvent{{500.0f, 400.0f}, MouseButton::Left, true}));
  REQUIRE(!ui.ProcessEvent(MouseMove{{500.0f, 400.0f}}));
  // A tooltip shows after the hover delay.
  ui.ProcessEvent(MouseMove{{30.0f, 20.0f}});
  const auto tooltipFrame = [&](double seconds) {
    draw.Clear();
    ui.BeginFrame(draw, text, screen, seconds);
    ui.Button(id, button, "Go");
    ui.Tooltip(id, "Launch");
    ui.EndFrame();
    return HasText(draw, "Launch");
  };
  REQUIRE(!tooltipFrame(1.0));
  REQUIRE(!tooltipFrame(1.1));
  REQUIRE(tooltipFrame(1.5));
}

void WidgetToggleAndList()
{
  Ui ui;
  DrawList draw;
  FixedTextMeasurer text;
  const ScreenInfo screen{800, 600, 1.0f, 1.0f};
  bool value = false;
  int clickedRow = -1;
  const auto frame = [&] {
    draw.Clear();
    ui.BeginFrame(draw, text, screen, 0.0);
    ui.Toggle(Id("toggle"), {10.0f, 10.0f, 120.0f, 20.0f}, "Auto-pause", value);
    clickedRow
        = ui.List(Id("list"), {10.0f, 40.0f, 200.0f, 100.0f}, 30, 20.0f, -1, [](int, const Rect&, bool, bool) {});
    ui.EndFrame();
  };
  const auto click = [&](Point at) {
    ui.ProcessEvent(MouseButtonEvent{at, MouseButton::Left, true});
    ui.ProcessEvent(MouseButtonEvent{at, MouseButton::Left, false});
    frame();
  };
  frame();
  click({20.0f, 20.0f});
  REQUIRE(value);
  click({20.0f, 20.0f});
  REQUIRE(!value);
  click({50.0f, 40.0f + 3.0f * 20.0f + 5.0f});
  REQUIRE(clickedRow == 3);
  REQUIRE(ui.ProcessEvent(MouseWheel{{50.0f, 100.0f}, 0.0f, -2.0f})); // scroll down two rows
  frame();
  click({50.0f, 40.0f + 3.0f * 20.0f + 5.0f});
  REQUIRE(clickedRow == 5);
}

void UnnamedUnitsAreNamedByClass()
{
  RecordingSource source;
  source.anonymous = true;
  source.View().units.push_back(
      Unit(5, reb::battle::Side::Empire, reb::battle::ViewUnitKind::CapitalShip, 20, {6000.0f, 2000.0f}, 450.0f, 2));
  FixedTextMeasurer text;
  NoSprites sprites;
  BattleScreen screen(source, text, sprites);
  REQUIRE(screen.Label(0).name == "Mon Calamari Cruiser 1" && !screen.Label(0).ownName);
  REQUIRE(screen.Label(1).name == "Nebulon-B Frigate 1");
  REQUIRE(screen.Label(2).name == "Imperial Star Destroyer 1" && screen.Label(5).name == "Imperial Star Destroyer 2");
  REQUIRE(screen.Label(3).name == "X-wing Squadron 1");
  REQUIRE(screen.Label(4).name == "Victory Destroyer 1" && screen.Label(4).className == "Victory Destroyer");
  REQUIRE(ClassName(36) == "TIE Defender" && ClassName(13) == "Liberator Cruiser" && ClassName(38).empty());
  reb::battle::ViewUnitDetail numbered;
  numbered.name = " 4";
  REQUIRE(NameUnit(source.View().units[2], numbered, 1).name == "Imperial Star Destroyer 4");
  numbered.name = " Executor ";
  REQUIRE(NameUnit(source.View().units[2], numbered, 1).name == "Executor");

  RecordingSource named;
  BattleScreen namedScreen(named, text, sprites);
  REQUIRE(namedScreen.Label(2).name == "Unit 2" && namedScreen.Label(2).ownName);
}

void TextWrapping()
{
  FixedTextMeasurer text; // 5.5 px per character at size 10
  const auto lines = WrapText(text, "Targeting Nebulon-B Redemption: priority one", 10.0f, 120.0f);
  REQUIRE(lines.size() == 3);
  for (const std::string& line : lines) {
    REQUIRE(text.Width(line, 10.0f) <= 120.0f);
  }
  const auto clipped = WrapText(text, "one two three four five six seven eight nine ten", 10.0f, 60.0f, 2);
  REQUIRE(clipped.size() == 2 && clipped.back().ends_with("..."));
  REQUIRE(WrapText(text, "", 10.0f, 60.0f).empty());
}

const DrawText& FindText(const DrawList& list, std::string_view text)
{
  for (const DrawCommand& command : list.Commands()) {
    if (const auto* drawn = std::get_if<DrawText>(&command); drawn && drawn->text == text) {
      return *drawn;
    }
  }
  REQUIRE(false);
  std::abort();
}

void OutroBannersUseThePlayersPerspective()
{
  using reb::battle::Side;
  using reb::battle::ViewEndReason;
  constexpr std::array<std::string_view, 6> messages{
      "VICTORY · ALL ENEMY SHIPS DESTROYED", "VICTORY · THE ENEMY FLEET HAS WITHDRAWN",
      "DEFEAT · OUR FLEET HAS BEEN DESTROYED", "WITHDRAWAL COMPLETE",
      "STALEMATE · NEITHER SIDE CAN BREAK THE OTHER", "TIME LIMIT REACHED"};
  for (Side player : {Side::Alliance, Side::Empire}) {
    for (int ending = 0; ending < 6; ++ending) {
      BattleScreenOptions options;
      options.playerSide = player;
      ScreenDriver h(options);
      const float pausedSize = FindText(h.Frame(), "PAUSED").size;
      auto& view = h.source.View();
      view.decided = true;
      view.endReason = ending < 4 ? ViewEndReason::Elimination
                                  : ending == 4 ? ViewEndReason::Stalemate : ViewEndReason::TimeLimit;
      for (auto& unit : view.units) {
        const bool absent = ending < 2 ? unit.side != player : ending < 4 && unit.side == player;
        unit.flags = absent ? ((ending == 1 || ending == 3) ? reb::battle::kViewWithdrawn
                                                                        : reb::battle::kViewDestroyed)
                            : 0;
      }
      const DrawList& frame = h.Frame();
      REQUIRE(HasText(frame, messages[ending]));
      REQUIRE(HasText(frame, "Continue"));
      REQUIRE(!HasText(frame, "PAUSED"));
      REQUIRE(FindText(frame, messages[ending]).size > pausedSize);
      REQUIRE(!h.source.Finished());
      view.finished = true;
      REQUIRE(!HasText(h.Frame(), "Continue"));
      REQUIRE(CountIf<DrawText>(h.frame, [](const DrawText& text) { return text.text.starts_with("BATTLE OVER"); }) == 1);
    }
  }
}

void OutroPlaysAtNormalSpeed()
{
  ScreenDriver h;
  h.Frame();
  h.screen.SetSpeedIndex(4);
  h.screen.SetPaused(false);
  h.Frame(0.1);
  REQUIRE(h.screen.SpeedIndex() == 4);
  h.source.View().decided = true;
  h.source.View().endReason = reb::battle::ViewEndReason::Elimination;
  h.Frame(0.1);
  REQUIRE(h.screen.SpeedIndex() == BattleScreen::kNormalSpeed);
  h.screen.SetSpeedIndex(3);
  h.Frame(0.1);
  REQUIRE(h.screen.SpeedIndex() == 3);
}

void OutroContinueButtonAndKeysSkip()
{
  for (Key key : {Key::Return, Key::Space}) {
    ScreenDriver h;
    h.source.View().decided = true;
    h.source.View().endReason = reb::battle::ViewEndReason::TimeLimit;
    h.Frame();
    REQUIRE(h.screen.Paused());
    const int64_t pausedTick = h.source.Tick();
    h.Frame(1.0);
    REQUIRE(h.source.Tick() == pausedTick && !h.source.Finished());
    h.Press(key);
    REQUIRE(h.source.skippedOutros == 1 && h.source.Finished());
    REQUIRE(h.screen.Paused());
  }
  ScreenDriver h;
  h.source.View().decided = true;
  h.source.View().endReason = reb::battle::ViewEndReason::TimeLimit;
  const Point button = FindText(h.Frame(), "Continue").position;
  h.Click(button);
  h.Frame();
  REQUIRE(h.source.skippedOutros == 1 && h.source.Finished());
}

void SpaceStillPausesBeforeDecision()
{
  BattleScreenOptions options;
  options.startPaused = false;
  ScreenDriver h(options);
  REQUIRE(!h.screen.Paused());
  h.Press(Key::Return);
  REQUIRE(h.source.skippedOutros == 0 && !h.source.Finished() && !h.screen.Paused());
  h.Press(Key::Space);
  REQUIRE(h.screen.Paused() && h.source.skippedOutros == 0);
  const int64_t pausedTick = h.source.Tick();
  h.Frame(1.0);
  REQUIRE(h.source.Tick() == pausedTick);
  h.Press(Key::Space);
  REQUIRE(!h.screen.Paused() && h.source.skippedOutros == 0);
}

} // namespace

int main()
{
  TopBarAndInfoBox();
  ArtSilhouettesAndSymbols();
  ShotsUseTheColourCode();
  OverlayDrawsArcsAndTargetLines();
  WidgetClicks();
  WidgetToggleAndList();
  UnnamedUnitsAreNamedByClass();
  TextWrapping();
  OutroBannersUseThePlayersPerspective();
  OutroContinueButtonAndKeysSkip();
  OutroPlaysAtNormalSpeed();
  SpaceStillPausesBeforeDecision();
  std::puts("battle_ui draw-list tests passed");
}
