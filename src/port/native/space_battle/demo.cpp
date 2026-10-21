#include "port/native/space_battle/demo.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "battle_ui/battle_screen.h"
#include "battle_ui/command_hud.h"
#include "battle_ui/fake_battle.h"
#include "battle_ui/live_battle.h"
#include "port/native/space_battle/presenter.h"
#include "space_battle/battle.h"

namespace reb::native::space_battle
{
namespace
{

using battle_ui::BattleScreen;
using battle_ui::Key;

constexpr double kReviewStep = 1.0 / 30.0;
constexpr double kEndLinger = 4.0; // seconds the result banner stays before the demo exits

const char* Env(const char* name)
{
  const char* value = std::getenv(name);
  return value && *value ? value : nullptr;
}

// The demo's stand-in for the campaign: Get Immediate Result runs the battle to its end at once with the AI on both
// sides, and observing hands the player's side to the AI.
class DemoHost : public battle_ui::BattleHost
{
  public:
  DemoHost(battle_ui::BattleSource& source, battle::Side player)
      : m_source(source)
      , m_player(player)
  {
  }
  void GetImmediateResult() override
  {
    m_source.SetAiControlled(battle::Side::Alliance, true);
    m_source.SetAiControlled(battle::Side::Empire, true);
    for (int64_t tick = 0; tick < 15 * 60 * battle::kTicksPerSecond && !m_source.Finished(); ++tick) {
      m_source.Step();
    }
  }
  void SetObserving(bool observing) override
  {
    m_source.SetAiControlled(m_player, observing);
  }

  private:
  battle_ui::BattleSource& m_source;
  battle::Side m_player;
};

// REB_SPACE_BATTLE_DEMO_SECONDS ends the live demo after that much real time (the live smoke).
int RunLive(BattleScreen& screen)
{
  const char* limit = Env("REB_SPACE_BATTLE_DEMO_SECONDS");
  const double stopAt = limit ? ClockSeconds() + std::atof(limit) : -1.0;
  double finishedAt = -1.0;
  int frames = 0;
  while (true) {
    PumpEvents();
    if (!DriveFrame(screen)) {
      break;
    }
    ++frames;
    const double now = ClockSeconds();
    if (screen.BattleFinished()) {
      finishedAt = finishedAt < 0.0 ? now : finishedAt;
      if (now - finishedAt > kEndLinger) {
        break;
      }
    }
    if (stopAt >= 0.0 && now >= stopAt) {
      std::printf("space battle demo: %d frames, battle time %.1f s\n", frames, screen.View().battleSeconds);
      return frames > 0 && screen.View().battleSeconds > 0.0f ? 0 : 1;
    }
  }
  return 0;
}

// Drives the screen through synthetic input, as a player would, and captures the frames worth reviewing.
class ReviewScript
{
  public:
  ReviewScript(BattleScreen& screen, std::filesystem::path directory, int width, int height)
      : m_screen(screen)
      , m_directory(std::move(directory))
      , m_width(width)
      , m_height(height)
  {
  }

  void Advance(double seconds)
  {
    for (double t = 0.0; t < seconds; t += kReviewStep) {
      m_screen.Update(kReviewStep);
      UpdateAudio(m_screen);
    }
  }
  void Press(Key key, uint8_t modifiers = 0)
  {
    m_screen.HandleInput(battle_ui::KeyEvent{key, true, false, modifiers});
    m_screen.HandleInput(battle_ui::KeyEvent{key, false, false, modifiers});
  }
  void Move(battle_ui::Point to)
  {
    m_screen.HandleInput(battle_ui::MouseMove{to, m_buttons, 0});
  }
  void Button(battle_ui::Point at, battle_ui::MouseButton button, bool down, uint8_t modifiers = 0)
  {
    m_buttons = down ? m_buttons | battle_ui::ButtonBit(button) : m_buttons & ~battle_ui::ButtonBit(button);
    m_screen.HandleInput(battle_ui::MouseButtonEvent{at, button, down, 1, modifiers});
  }
  void Click(battle_ui::Point at, battle_ui::MouseButton button, uint8_t modifiers = 0)
  {
    Move(at);
    Button(at, button, true, modifiers);
    Button(at, button, false, modifiers);
  }
  void Drag(battle_ui::Point from, battle_ui::Point to, battle_ui::MouseButton button)
  {
    Move(from);
    Button(from, button, true);
    for (int step = 1; step <= 8; ++step) {
      const float t = static_cast<float>(step) / 8.0f;
      Move({from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t});
    }
    Button(to, button, false);
  }
  void Wheel(battle_ui::Point at, float notches)
  {
    Move(at);
    m_screen.HandleInput(battle_ui::MouseWheel{at, 0.0f, notches, false, 0});
  }
  battle_ui::Point OnScreen(battle::UnitIndex unit) const
  {
    const battle::ViewUnit* found = battle_ui::FindUnit(m_screen.View(), unit);
    return found ? m_screen.GetCamera().WorldToScreen(found->position) : battle_ui::Point{};
  }
  battle::UnitIndex FirstUnit(battle::Side side, battle::ViewUnitKind kind) const
  {
    for (const battle::ViewUnit& unit : m_screen.View().units) {
      if (unit.side == side && unit.kind == kind && battle_ui::Selectable(unit)) {
        return unit.unit;
      }
    }
    return battle::kNoUnit;
  }
  bool Capture(const char* name)
  {
    battle_ui::DrawList frame;
    m_screen.Build(frame);
    const std::filesystem::path path = m_directory / (std::string(name) + ".png");
    const bool saved = space_battle::Capture(frame, m_width, m_height, path.string());
    std::printf("space battle review: %s %s (%zu draw commands)\n", saved ? "wrote" : "FAILED", path.string().c_str(),
                frame.Commands().size());
    m_failures += saved ? 0 : 1;
    return saved;
  }
  int Failures() const
  {
    return m_failures;
  }
  // Average cost of a frame over the next `frames` frames of battle at the current speed.
  void ReportTiming(int frames)
  {
    double build = 0.0;
    double render = 0.0;
    size_t commands = 0;
    for (int i = 0; i < frames; ++i) {
      Advance(kReviewStep);
      battle_ui::DrawList frame;
      const double start = ClockSeconds();
      m_screen.Build(frame);
      const double built = ClockSeconds();
      Render(frame, m_width, m_height);
      build += built - start;
      render += ClockSeconds() - built;
      commands += frame.Commands().size();
    }
    std::printf(
        "space battle review: %zu units, %zu craft: build %.2f ms, render %.2f ms, %zu draw commands per frame\n",
        m_screen.View().units.size(), m_screen.View().craft.size(), build * 1000.0 / frames, render * 1000.0 / frames,
        commands / static_cast<size_t>(frames));
  }

  private:
  BattleScreen& m_screen;
  std::filesystem::path m_directory;
  int m_width;
  int m_height;
  uint8_t m_buttons = 0;
  int m_failures = 0;
};

// Damage close up: the worst-hit capital ship, and a capital ship's explosion with its escape pods, waiting up to a
// minute of battle for one.
void ReviewDamage(BattleScreen& screen, ReviewScript& script)
{
  const auto damage = [](const battle::ViewUnit& unit) {
    const battle::CriticalHits& c = unit.criticals;
    return (1.0f - unit.hullFraction) + 0.3f * static_cast<float>(c.speed + c.shieldRecharge + c.weaponRecharge);
  };
  const battle::ViewUnit* worst = nullptr;
  for (const battle::ViewUnit& unit : screen.View().units) {
    if (battle_ui::Selectable(unit) && unit.kind == battle::ViewUnitKind::CapitalShip
        && (!worst || damage(unit) > damage(*worst))) {
      worst = &unit;
    }
  }
  if (worst && damage(*worst) > 0.0f) {
    screen.GetCamera().SetZoom(0.22f * screen.Screen().uiScale);
    screen.CenterOn(worst->unit);
    script.Advance(0.2);
    script.Capture("d1_damage");
  }
  for (int frame = 0; frame < 60 * 30; ++frame) {
    script.Advance(1.0 / 30.0);
    for (const battle::ViewEffect& effect : screen.View().effects) {
      const battle::ViewUnit* unit = battle_ui::FindUnit(screen.View(), effect.unit);
      if (effect.kind == battle::ViewEffectKind::Explosion && unit && unit->kind == battle::ViewUnitKind::CapitalShip
          && effect.age >= 0.9f) {
        screen.GetCamera().SetZoom(0.09f * screen.Screen().uiScale);
        screen.CenterAt(effect.position);
        script.Advance(0.0);
        script.Capture("d2_explosion");
        return;
      }
    }
  }
  std::printf("space battle review: no capital ship exploded within a minute\n");
}

int RunReview(BattleScreen& screen, const std::filesystem::path& directory)
{
  int width = 1920;
  int height = 1080;
  if (const char* size = Env("REB_SPACE_BATTLE_REVIEW_SIZE")) {
    std::sscanf(size, "%dx%d", &width, &height);
  }
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  battle_ui::ScreenInfo info{width, height, 1.0f, std::max(1.0f, static_cast<float>(height) / 1080.0f)};
  if (const char* scale = Env("REB_BATTLE_UI_SCALE")) {
    info.uiScale = static_cast<float>(std::atof(scale));
  }
  screen.HandleInput(battle_ui::ScreenChanged{info});
  // The presenter queued the real window's size on open; the capture size wins.
  (void)TakeInput();

  ReviewScript script(screen, directory, width, height);
  const battle::Side player = screen.PlayerSide();
  const battle::Side enemy = player == battle::Side::Alliance ? battle::Side::Empire : battle::Side::Alliance;
  script.Advance(0.4);
  script.Capture("01_opening");
  script.ReportTiming(10);
  script.Advance(7.0);
  script.Capture("02_engagement");

  script.Press(battle_ui::DigitKey(1));
  script.Press(Key::Tab);
  script.Advance(0.2);
  script.Capture("03_overlay_task_force");

  if (screen.GetSelection().Empty()) {
    screen.GetSelection().Set({script.FirstUnit(player, battle::ViewUnitKind::CapitalShip)});
  }
  const battle::UnitIndex flagship = screen.GetSelection().Units().front();
  script.Wheel(script.OnScreen(flagship), 7.0f);
  script.Advance(0.3);
  script.Capture("04_detailed_overlay");
  script.Press(Key::Tab);
  script.Advance(0.6);
  script.Capture("05_detailed");

  const battle::UnitIndex enemyShip = script.FirstUnit(enemy, battle::ViewUnitKind::CapitalShip);
  screen.CenterOn(enemyShip);
  script.Advance(0.1);
  script.Move(script.OnScreen(enemyShip));
  script.Advance(0.5);
  script.Capture("06_hover_intent");

  const battle::UnitIndex ownShip = script.FirstUnit(player, battle::ViewUnitKind::CapitalShip);
  screen.CenterOn(ownShip);
  script.Advance(0.1);
  script.Click(script.OnScreen(ownShip), battle_ui::MouseButton::Left);
  script.Move({screen.Layout().view.Right() - 40.0f, screen.Layout().view.y + 40.0f});
  script.Advance(0.3);
  script.Capture("07_selected_unit");
  script.Press(Key::F); // follow it: the top bar's Follow button lights and the banner names it
  script.Advance(1.0);
  script.Capture("07b_following");
  script.Press(Key::F);

  // The angled view (V): framed, close up with fighters above their shadows, with the overlay, rotated, and taking
  // orders, with picking through the projection.
  script.Press(Key::V);
  script.Press(Key::Home);
  script.Advance(0.2);
  script.Capture("a1_angled_overview");
  screen.GetCamera().SetZoom(0.16f * screen.Screen().uiScale);
  screen.CenterOn(ownShip);
  script.Advance(0.3);
  script.Capture("a2_angled_close");
  script.Click(script.OnScreen(ownShip), battle_ui::MouseButton::Left);
  script.Press(Key::Tab);
  screen.GetCamera().SetZoom(0.05f * screen.Screen().uiScale);
  script.Move(screen.Layout().battleArea.Center());
  script.Advance(0.2);
  script.Capture("a3_angled_overlay");
  script.Press(Key::Tab);
  script.Press(Key::Q);
  script.Press(Key::Q);
  script.Advance(0.2);
  script.Capture("a4_angled_rotated");
  script.Press(Key::E);
  script.Press(Key::E);
  script.Press(Key::Home);
  script.Advance(0.1);
  {
    const battle_ui::Rect area = screen.Layout().battleArea;
    script.Drag({area.x + area.w * 0.02f, area.y + area.h * 0.02f}, {area.Center().x, area.Bottom() - area.h * 0.02f},
                battle_ui::MouseButton::Left);
    const battle_ui::Point from = area.Center() + battle_ui::Point{area.w * 0.1f, 0.0f};
    script.Drag(from, from + battle_ui::Point{0.0f, -60.0f}, battle_ui::MouseButton::Right);
    script.Move(area.Center());
    script.Advance(0.2);
    script.Capture("a5_angled_orders");
  }
  script.Press(Key::V);

  // The system's planet in the detailed look.
  if (screen.View().planetRadius > 0.0f) {
    screen.GetCamera().SetZoom(battle_ui::Camera::kDetailedZoom * 1.1f * screen.Screen().uiScale);
    screen.CenterAt(screen.View().planetCenter);
    script.Advance(0.1);
    script.Capture("p1_planet");
  }

  script.Press(Key::Home);
  script.Advance(0.1);
  const battle_ui::Rect view = screen.Layout().view;
  script.Drag({view.x + view.w * 0.05f, view.y + view.h * 0.08f}, {view.Center().x, view.Bottom() - view.h * 0.08f},
              battle_ui::MouseButton::Left);
  script.Click(script.OnScreen(enemyShip), battle_ui::MouseButton::Right);
  script.Move(view.Center()); // away from the edges, which scroll the view
  script.Advance(0.25);
  script.Capture("08_box_select_attack");

  script.Press(Key::Equals);
  script.Press(Key::Equals);
  script.Advance(12.0);
  script.Capture("09_later_4x");

  script.Press(Key::Space);
  script.Advance(0.1);
  script.Capture("10_paused_overlay");
  script.Press(Key::Q);
  script.Advance(0.1);
  script.Capture("11_rotated");

  script.Press(Key::Space); // running again, for the damage close-ups
  ReviewDamage(screen, script);

  script.Press(Key::Home);
  script.ReportTiming(30);

  // Hand the window back to minwin and take it over again, as a campaign does between battles.
  Close();
  if (!Open()) {
    std::printf("space battle review: FAILED to reopen the battle screen\n");
    return 1;
  }
  (void)TakeInput();
  script.Capture("12_reopened");
  return script.Failures() == 0 ? 0 : 1;
}

// The demo's battle: the simulator on a snapshot file, or with REB_SPACE_BATTLE_DEMO=fabricated the fabricated
// stand-in (no installation or snapshot needed).
struct DemoBattle {
  std::optional<battle::BattleSnapshot> snapshot;
  std::unique_ptr<battle::Battle> simulator;
  std::unique_ptr<battle_ui::BattleSource> source;
};

int FleetsFromEnvironment();

DemoBattle MakeDemoBattle(const char* demo)
{
  DemoBattle battle;
  if (std::strcmp(demo, "fabricated") == 0) {
    auto fake = std::make_unique<battle_ui::FakeBattle>(1, FleetsFromEnvironment());
    if (Env("REB_SPACE_BATTLE_DEATH_STAR")) {
      fake->AddDeathStar({9800.0f, -3800.0f});
    }
    battle.source = std::move(fake);
    return battle;
  }
  std::ifstream file(demo, std::ios::binary);
  std::ostringstream text;
  text << file.rdbuf();
  battle.snapshot = file ? battle::ParseSnapshot(text.str()) : std::nullopt;
  if (!battle.snapshot) {
    std::fprintf(stderr, "space battle demo: cannot read a battle snapshot from %s\n", demo);
    return battle;
  }
  battle.simulator = std::make_unique<battle::Battle>(*battle.snapshot, battle::BattleOptions{});
  battle.source = std::make_unique<battle_ui::LiveBattle>(*battle.simulator);
  return battle;
}

// REB_SPACE_BATTLE_SIDE (alliance or empire), else the side a human voted for in the snapshot, else the Alliance.
battle::Side PlayerSide(const DemoBattle& battle)
{
  if (const char* side = Env("REB_SPACE_BATTLE_SIDE")) {
    return side[0] == 'e' || side[0] == 'E' ? battle::Side::Empire : battle::Side::Alliance;
  }
  if (battle.snapshot && battle.snapshot->params.sides[1].human && !battle.snapshot->params.sides[0].human) {
    return battle::Side::Empire;
  }
  return battle::Side::Alliance;
}

int FleetsFromEnvironment()
{
  const char* fleets = Env("REB_SPACE_BATTLE_DEMO_FLEETS");
  return fleets ? std::clamp(std::atoi(fleets), 1, 20) : 1;
}

DemoBattle FabricatedBattle(bool deathStar)
{
  DemoBattle battle;
  auto fake = std::make_unique<battle_ui::FakeBattle>(1, 1);
  if (deathStar) {
    fake->AddDeathStar({9800.0f, -3800.0f});
  }
  battle.source = std::move(fake);
  return battle;
}

// The command HUD's review: the briefing, every popup, the card for groups, ships and flights, roster regrouping,
// targeting, options and the Death Star controls, at several window sizes, each from a fresh battle.
class HudReview
{
  public:
  HudReview(DemoBattle battle, std::filesystem::path directory, int width, int height, float uiScale,
            battle::Side player, std::string prefix)
      : m_info{width, height, 1.0f, uiScale}
      , m_player(player)
      , m_prefix(std::move(prefix))
      , m_battle(std::move(battle))
  {
    battle_ui::BattleScreenOptions options;
    options.playerSide = player;
    options.startPaused = true;
    m_screen.emplace(*m_battle.source, Text(), Sprites(), options);
    m_host.emplace(*m_battle.source, player);
    m_hud = &battle_ui::InstallCommandHud(*m_screen, *m_host, m_store);
    m_screen->HandleInput(battle_ui::ScreenChanged{m_info});
    m_script.emplace(*m_screen, std::move(directory), width, height);
    Frame();
  }

  BattleScreen& Screen()
  {
    return *m_screen;
  }
  battle_ui::CommandHud& Hud()
  {
    return *m_hud;
  }
  ReviewScript& Script()
  {
    return *m_script;
  }
  void Frame()
  {
    battle_ui::DrawList frame;
    m_screen->Update(0.0);
    m_screen->Build(frame);
  }
  battle_ui::Point At(battle_ui::WidgetId id)
  {
    const std::optional<battle_ui::Rect> rect = m_hud->ControlRect(id);
    if (!rect) {
      std::printf("space battle review: control %llx not drawn\n", static_cast<unsigned long long>(id));
      ++m_missing;
      return {-100.0f, -100.0f};
    }
    return rect->Center();
  }
  // Clicks a HUD control; the HUD acts on it in the next frame and shows the result in the one after.
  void Click(battle_ui::WidgetId id, uint8_t modifiers = 0)
  {
    m_script->Click(At(id), battle_ui::MouseButton::Left, modifiers);
    Frame();
    Frame();
  }
  void ClickWorld(battle::UnitIndex unit, uint8_t modifiers = 0)
  {
    m_script->Click(m_script->OnScreen(unit), battle_ui::MouseButton::Left, modifiers);
    Frame();
    Frame();
  }
  // Presses at `from` and moves to `to` with a frame after each step; `release` ends the drag there.
  void Drag(battle_ui::Point from, battle_ui::Point to, bool release = true)
  {
    m_script->Move(from);
    m_script->Button(from, battle_ui::MouseButton::Left, true);
    Frame();
    for (int step = 1; step <= 8; ++step) {
      const float t = static_cast<float>(step) / 8.0f;
      m_script->Move({from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t});
      Frame();
    }
    if (release) {
      Release(to);
    }
  }
  void Release(battle_ui::Point at)
  {
    m_script->Button(at, battle_ui::MouseButton::Left, false);
    Frame();
    Frame();
  }
  void Key(battle_ui::Key key, uint8_t modifiers = 0)
  {
    m_script->Press(key, modifiers);
    Frame();
    Frame();
  }
  void Hover(battle_ui::Point at)
  {
    m_script->Move(at);
    Frame();
    m_script->Advance(0.5);
    Frame();
  }
  void Capture(const char* name)
  {
    m_script->Capture((m_prefix + name).c_str());
  }
  int Failures() const
  {
    return m_script->Failures() + m_missing;
  }

  private:
  battle_ui::ScreenInfo m_info;
  battle::Side m_player;
  std::string m_prefix;
  DemoBattle m_battle;
  battle_ui::MemorySettingsStore m_store;
  std::optional<BattleScreen> m_screen;
  std::optional<DemoHost> m_host;
  battle_ui::CommandHud* m_hud = nullptr;
  std::optional<ReviewScript> m_script;
  int m_missing = 0;
};

int ReviewCommandHud(const std::filesystem::path& directory, int width, int height, float uiScale)
{
  namespace id = battle_ui::hud_id;
  using battle_ui::DoctrineField;
  char prefix[48];
  std::snprintf(prefix, sizeof prefix, "hud_%dx%d_", width, height);
  HudReview review(FabricatedBattle(false), directory, width, height, uiScale, battle::Side::Alliance, prefix);
  ReviewScript& script = review.Script();
  BattleScreen& screen = review.Screen();
  for (battle::GroupId group = 0; group < 6; ++group) {
    review.Hud().FoldGroup(group, false); // small windows fold the larger groups; the script drags their ships
  }
  review.Frame();
  review.Capture("01_briefing");

  review.Key(battle_ui::DigitKey(1));
  review.Hover(review.At(id::DeployHandle(1)));
  review.Capture("02_briefing_group");
  const battle_ui::Point handle = review.At(id::DeployHandle(0));
  review.Drag(handle, {handle.x - 90.0f * uiScale, handle.y + 70.0f * uiScale}, false);
  review.Capture("03_deploying");
  review.Release({handle.x - 90.0f * uiScale, handle.y + 70.0f * uiScale});
  review.Capture("04_deployed");

  review.Hover(review.At(id::Chip(DoctrineField::Range)));
  review.Capture("05_chip_tooltip");
  review.Click(id::Chip(DoctrineField::Range));
  review.Capture("06_range_options");
  review.Click(id::FieldOption(0));
  review.Click(id::Chip(DoctrineField::Priorities));
  review.Click(id::PriorityTag(battle::TargetTag::Carrier));
  review.Click(id::PriorityTag(battle::TargetTag::Interdictor));
  review.Click(id::PriorityTag(battle::TargetTag::Damaged));
  review.Capture("07_priority_editor");
  const battle_ui::Point from = review.At(id::PrioritySlot(3));
  const battle_ui::Point to = review.At(id::PrioritySlot(0));
  review.Drag(from, {from.x + 30.0f * uiScale, to.y - 4.0f * uiScale}, false);
  review.Capture("08_priority_drag");
  review.Release({from.x + 30.0f * uiScale, to.y - 4.0f * uiScale});
  review.Capture("09_priorities_reordered");

  review.Key(battle_ui::Key::Escape);
  review.Click(id::Presets());
  review.Capture("10_presets");
  review.Click(id::SavePreset());
  screen.HandleInput(battle_ui::TextInput{"Line breakers"});
  review.Frame();
  review.Capture("11_preset_naming");
  review.Key(battle_ui::Key::Return);
  review.Frame();
  review.Capture("12_presets_saved");
  review.Key(battle_ui::Key::Escape);

  review.Click(id::Engage());
  script.Advance(8.0);
  review.Frame();
  review.Capture("13_battle");

  review.Click(id::RosterUnit(3));
  review.Click(id::Chip(DoctrineField::Facing));
  review.Click(id::FieldOption(1));
  review.Hover(review.At(id::RosterUnit(3)));
  review.Capture("14_ship_override");

  const battle_ui::Point grab = review.At(id::RosterUnit(4));
  const battle_ui::Point drop = review.At(id::RosterGroup(0));
  review.Drag(grab, drop, false);
  review.Capture("15_roster_drag");
  review.Release(drop);
  review.Capture("16_regrouped");

  review.Key(battle_ui::DigitKey(1));
  review.Click(id::Order(battle_ui::OrderButton::Attack));
  const battle::UnitIndex enemy = script.FirstUnit(battle::Side::Empire, battle::ViewUnitKind::CapitalShip);
  review.Hover(script.OnScreen(enemy));
  review.Capture("17_attack_pick");
  review.ClickWorld(enemy, battle_ui::kModCtrl);
  review.Key(battle_ui::Key::Escape);

  review.Click(id::Signal(battle::SignalKind::ConcentrateFire));
  review.Hover(script.OnScreen(enemy));
  review.Capture("18_signal_pick");
  review.Key(battle_ui::Key::Escape);

  review.Click(id::Options());
  review.Hover(review.At(id::CommandPhase(20)));
  review.Capture("19_options");
  review.Click(id::Palette(true));
  review.Click(id::HudScale(2));
  review.Key(battle_ui::Key::Escape);
  review.Capture("20_accessibility");
  review.Click(id::Options());
  review.Click(id::Palette(false));
  review.Click(id::HudScale(1));
  review.Key(battle_ui::Key::Escape);

  review.Click(id::WithdrawFleet());
  review.Capture("21_confirm_withdraw");
  review.Key(battle_ui::Key::Escape);

  review.Key(battle_ui::DigitKey(4));
  review.Click(id::Chip(DoctrineField::Mission));
  review.Capture("22_flight_card");
  review.Key(battle_ui::Key::Escape);

  review.Click(id::CollapseRoster());
  review.Click(id::CollapseCommand());
  review.Click(id::CollapseEvents());
  review.Click(id::CollapseCard());
  review.Capture("23_collapsed");
  review.Click(id::CollapseRoster());
  review.Click(id::CollapseCommand());
  review.Click(id::CollapseEvents());
  review.Click(id::CollapseCard());

  review.Click(id::Observe());
  script.Advance(2.0);
  review.Frame();
  review.Capture("24_observing");
  return review.Failures();
}

int ReviewDeathStar(const std::filesystem::path& directory, int width, int height, float uiScale)
{
  namespace id = battle_ui::hud_id;
  int failures = 0;
  {
    HudReview review(FabricatedBattle(true), directory, width, height, uiScale, battle::Side::Empire, "hud_deathstar_");
    review.Click(id::Engage());
    review.Script().Advance(3.0);
    review.Frame();
    review.Capture("01_empire");
    review.Click(id::FireSuperlaser());
    const battle::UnitIndex target
        = review.Script().FirstUnit(battle::Side::Alliance, battle::ViewUnitKind::CapitalShip);
    review.Hover(review.Script().OnScreen(target));
    review.Capture("02_superlaser_pick");
    review.ClickWorld(target);
    review.Script().Advance(1.0);
    review.Frame();
    review.Capture("03_fired");
    if (const battle::UnitIndex station = review.Script().FirstUnit(battle::Side::Empire, battle::ViewUnitKind::DeathStar);
        station != battle::kNoUnit) {
      review.Screen().GetCamera().SetZoom(0.04f * uiScale);
      review.Screen().CenterOn(station);
      review.Frame();
      review.Capture("03a_station_top_close");
      review.Key(battle_ui::Key::V);
      review.Frame();
      review.Capture("03b_station_angled_close");
    }
    failures += review.Failures();
  }
  {
    HudReview review(FabricatedBattle(true), directory, width, height, uiScale, battle::Side::Alliance,
                     "hud_deathstar_");
    review.Click(id::Engage());
    review.Key(battle_ui::DigitKey(4));
    review.Script().Advance(2.0);
    review.Frame();
    review.Hover(review.At(id::Order(battle_ui::OrderButton::DeathStarRun)));
    review.Capture("04_alliance_flight");
    failures += review.Failures();
  }
  return failures;
}

// The HUD on the simulator, from the demo's snapshot: whatever its fleets are, the main panels with real data.
int ReviewSimulatorHud(const char* demo, const std::filesystem::path& directory, int width, int height, float uiScale)
{
  namespace id = battle_ui::hud_id;
  DemoBattle battle = MakeDemoBattle(demo);
  if (!battle.source) {
    return 1;
  }
  const battle::Side player = PlayerSide(battle);
  char prefix[48];
  std::snprintf(prefix, sizeof prefix, "hud_sim_%dx%d_", width, height);
  HudReview review(std::move(battle), directory, width, height, uiScale, player, prefix);
  review.Capture("01_briefing");
  review.Key(battle_ui::DigitKey(1));
  review.Capture("02_task_force");
  // Doctrine edits and deployment must show while the briefing is paused.
  review.Click(id::Chip(battle_ui::DoctrineField::Range));
  review.Click(id::FieldOption(static_cast<int>(battle::EngagementRange::StandOff)));
  if (const std::optional<battle_ui::Rect> handle = review.Hud().ControlRect(id::DeployHandle(0))) {
    const battle_ui::Point from = handle->Center();
    review.Drag(from, {from.x + 60.0f * uiScale, from.y - 40.0f * uiScale});
  }
  review.Capture("03_briefing_edited");
  review.Click(id::Engage());
  review.Script().Advance(20.0);
  review.Frame();
  review.Capture("04_battle");
  // The angled view with the HUD, framed and then close up on a ship of the player's.
  review.Key(battle_ui::Key::V);
  review.Capture("04a_angled");
  if (const battle::UnitIndex near = review.Script().FirstUnit(player, battle::ViewUnitKind::CapitalShip);
      near != battle::kNoUnit) {
    review.Screen().GetCamera().SetZoom(0.12f * uiScale);
    review.Screen().CenterOn(near);
    review.Frame();
    review.Capture("04b_angled_close");
  }
  if (const battle::UnitIndex flight = review.Script().FirstUnit(player, battle::ViewUnitKind::Squadron);
      flight != battle::kNoUnit) {
    review.Screen().GetCamera().SetZoom(0.6f * uiScale);
    review.Screen().CenterOn(flight);
    review.Frame();
    review.Capture("04c_fighter_angled_close");
    review.Key(battle_ui::Key::V);
    review.Frame();
    review.Capture("04d_fighter_top_close");
    review.Key(battle_ui::Key::V);
  }
  review.Key(battle_ui::Key::V);
  review.Key(battle_ui::Key::Home);
  const battle::UnitIndex ship = review.Script().FirstUnit(player, battle::ViewUnitKind::CapitalShip);
  if (ship != battle::kNoUnit && review.Hud().ControlRect(id::RosterUnit(ship))) {
    review.Click(id::RosterUnit(ship));
    review.Click(id::Chip(battle_ui::DoctrineField::Priorities));
    review.Capture("05_ship_priorities");
    review.Key(battle_ui::Key::Escape);
  }
  review.Key(battle_ui::Key::Equals);
  review.Script().Advance(40.0);
  review.Frame();
  review.Capture("06_later");
  // Planetary batteries (§6.13): the whole cover ring, from the planet toward the battle.
  const battle::ViewModel& view = review.Screen().View();
  if (!view.defenses.empty() && view.coverRadius > 0.0f) {
    const battle_ui::Rect area = review.Screen().Layout().battleArea;
    review.Screen().GetCamera().SetZoom(std::min(area.w, area.h) * 0.85f / (2.0f * view.coverRadius));
    review.Screen().CenterAt({view.planetCenter.x * 0.6f, view.planetCenter.y * 0.6f});
    review.Frame();
    review.Capture("07_planetary_cover");
  }
  return review.Failures();
}

// REB_SPACE_BATTLE_REVIEW_SIZE limits the HUD passes to that size (the smoke); otherwise three window sizes.
int RunHudReview(const std::filesystem::path& directory, const char* demo)
{
  struct Size {
    int width;
    int height;
    float uiScale;
  };
  std::vector<Size> sizes{{1920, 1080, 1.0f}, {1280, 720, 1.0f}, {2560, 1600, 2.0f}};
  if (const char* size = Env("REB_SPACE_BATTLE_REVIEW_SIZE")) {
    Size only{1920, 1080, 1.0f};
    std::sscanf(size, "%dx%d", &only.width, &only.height);
    only.uiScale = std::max(1.0f, static_cast<float>(only.height) / 1080.0f);
    sizes = {only};
  }
  int failures = 0;
  for (const Size& size : sizes) {
    failures += ReviewCommandHud(directory, size.width, size.height, size.uiScale);
  }
  failures += ReviewDeathStar(directory, sizes.front().width, sizes.front().height, sizes.front().uiScale);
  if (std::strcmp(demo, "fabricated") != 0) {
    failures += ReviewSimulatorHud(demo, directory, sizes.front().width, sizes.front().height, sizes.front().uiScale);
  }
  return failures == 0 ? 0 : 1;
}

} // namespace

std::optional<int> RunDemoIfRequested()
{
  const char* demo = Env("REB_SPACE_BATTLE_DEMO");
  if (!demo) {
    return std::nullopt;
  }
  const char* review = Env("REB_SPACE_BATTLE_REVIEW_DIR");
  if (review) {
#if defined(_WIN32)
    _putenv_s("REB_MINWIN_HIDDEN", "1");
#else
    setenv("REB_MINWIN_HIDDEN", "1", 0);
#endif
  }
  DemoBattle battle = MakeDemoBattle(demo);
  if (!battle.source) {
    return 1;
  }
  if (!Open()) {
    std::fprintf(stderr, "space battle demo: cannot open the battle screen\n");
    return 1;
  }
  battle_ui::BattleScreenOptions options;
  options.playerSide = PlayerSide(battle);
  if (battle.snapshot) {
    options.systemPicture = battle.snapshot->params.systemPictureId;
  }
  // A live battle opens in the paused briefing (tactical_combat.md §4), except in the timed live smoke, which checks
  // that battle time advances. The screen's own review runs without the HUD.
  options.startPaused = !review && !Env("REB_SPACE_BATTLE_DEMO_SECONDS");
  int result = 0;
  {
    DemoHost host(*battle.source, options.playerSide);
    battle_ui::MemorySettingsStore settings;
    BattleScreen screen(*battle.source, Text(), Sprites(), options);
    if (review) {
      result = RunReview(screen, review);
    } else {
      battle_ui::InstallCommandHud(screen, host, settings);
      result = RunLive(screen);
    }
  }
  if (review && result == 0 && !Env("REB_SPACE_BATTLE_REVIEW_SKIP_HUD")) {
    result = RunHudReview(review, demo);
  }
  Close();
  return result;
}

} // namespace reb::native::space_battle
