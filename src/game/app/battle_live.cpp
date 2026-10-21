#include "app/battle_live.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

#include "app/app_dialogs.h"
#include "app/rebapp_globals.h"
#include "app/rebellion_application.h"
#include "battle_ui/live_battle.h"
#include "port/native/space_battle/presenter.h"
#include "port/native/space_battle/settings_store.h"
#include "ui/cool_app.h"

namespace ui = reb::battle_ui;
namespace battle = reb::battle;
namespace presenter = reb::native::space_battle;

namespace
{
constexpr float kLiveOutroSeconds = 5.0f;
}

struct LiveBattleSession::Impl {
  struct PauseMenu final : ui::HudPanel {
    explicit PauseMenu(Impl& session) : session(session) {}
    void Draw(ui::Ui& ui, ui::BattleScreen& screen) override
    {
      if (!session.menu) return;
      ui.Region(ui::Id("campaign.modal"),
                {0, 0, static_cast<float>(screen.Screen().width), static_cast<float>(screen.Screen().height)});
      const float width = ui.S(300);
      ui::Rect panel{(screen.Screen().width - width) / 2, screen.Screen().height * 0.25f, width, ui.S(220)};
      ui::Rect content = ui.Panel(panel, "Battle paused");
      ui::Rect row{content.x + ui.S(8), content.y + ui.S(8), content.w - ui.S(16), ui.S(32)};
      if (ui.Button(ui::Id("campaign.resume"), row, "Resume")) {
        session.menu = false;
        screen.SetPaused(session.wasPaused);
      }
      row.y += ui.S(40);
      if (ui.Button(ui::Id("campaign.save"), row, "Save / load game")) session.action = Action::Options;
      row.y += ui.S(40);
      if (ui.Button(ui::Id("campaign.immediate"), row, "Get Immediate Result")) session.immediate = true;
      row.y += ui.S(40);
      if (ui.Button(ui::Id("campaign.quit"), row, "Quit game")) session.action = Action::Quit;
    }
    Impl& session;
  };
  enum class Action { None, Options, Quit };

  Impl(battle::Battle& simulator, battle::Side player, LiveBattleSession& host, bool restoring)
      : simulator(simulator), source(simulator),
        screen(source, presenter::Text(), presenter::Sprites(),
               {.playerSide = player,
                .startPaused = true,
                .aiUntilFirstOrder = true,
                .preserveAiControl = restoring,
                .systemPicture = simulator.Snapshot().params.systemPictureId})
  {
    simulator.SetOutroSeconds(kLiveOutroSeconds);
    ui::InstallCommandHud(screen, host, settings);
    screen.AddPanel(std::make_unique<PauseMenu>(*this));
  }

  battle::Battle& simulator;
  ui::LiveBattle source;
  presenter::ConfigSettingsStore settings;
  ui::BattleScreen screen;
  bool immediate = false;
  bool menu = false;
  bool wasPaused = true;
  Action action = Action::None;
};

LiveBattleSession::LiveBattleSession(battle::Battle& simulator, battle::Side player, bool restoring)
    : m_impl(std::make_unique<Impl>(simulator, player, *this, restoring))
{
}

LiveBattleSession::~LiveBattleSession()
{
  presenter::Close();
}

bool LiveBattleSession::Open()
{
  return presenter::Open();
}

bool LiveBattleSession::Advance()
{
  auto& session = *m_impl;
  if (session.immediate) {
    session.simulator.SetOutroSeconds(0);
    session.simulator.SkipOutro();
    session.simulator.SetAiControlled(battle::Side::Alliance, true);
    session.simulator.SetAiControlled(battle::Side::Empire, true);
    return session.simulator.RunHeadless(battle::kTicksPerSecond * 10);
  }
  auto* application = static_cast<RebellionApplication*>(g_app);
  if (application && application->m_state != kStateTactical) return false;
  // The quit question (and any other app dialog) is an ordinary game window that leaves the state tactical: the
  // battle screen gives up the window until it is answered, or it covers the dialog and the game cannot quit.
  if (g_appDialog) {
    if (presenter::IsOpen()) presenter::Close();
    return false;
  }
  if (!presenter::IsOpen() && !Open()) {
    std::fprintf(stderr, "battle: the battle screen did not reopen; resolving headless\n");
    GetImmediateResult();
    return false;
  }
  // Keep Escape inside this session; a window close is handed back to the normal quit confirmation by Close.
  for (const auto& event : presenter::TakeInput()) {
    if (std::holds_alternative<ui::CloseRequested>(event)) {
      session.screen.SetPaused(true);
      presenter::Close();
      return false;
    }
    if (const auto* key = std::get_if<ui::KeyEvent>(&event);
        session.menu && key && key->down && key->key == ui::Key::Escape) {
      session.menu = false;
      session.screen.SetPaused(session.wasPaused);
      continue;
    }
    if (!session.menu || std::holds_alternative<ui::MouseMove>(event)
        || std::holds_alternative<ui::MouseButtonEvent>(event) || std::holds_alternative<ui::ScreenChanged>(event)) {
      session.screen.HandleInput(event);
    }
  }
  if (session.screen.QuitRequested()) {
    session.screen.ClearQuitRequest();
    session.screen.HandleInput(ui::FocusLost{});
    session.wasPaused = session.screen.Paused();
    session.screen.SetPaused(true);
    session.menu = true;
  }
  presenter::DriveFrame(session.screen);
  if (session.action != Impl::Action::None) {
    const auto action = std::exchange(session.action, Impl::Action::None);
    session.menu = false;
    presenter::Close();
    AppSetState(action == Impl::Action::Options ? kStateOptionsInGame : kStateConfirmQuit);
  }
  return session.simulator.Finished();
}

void LiveBattleSession::GetImmediateResult()
{
  m_impl->simulator.SetOutroSeconds(0);
  m_impl->simulator.SkipOutro();
  m_impl->immediate = true;
  SetObserving(true);
  m_impl->simulator.SetAiControlled(battle::Side::Alliance, true);
  m_impl->simulator.SetAiControlled(battle::Side::Empire, true);
  m_impl->simulator.RunHeadless(m_impl->simulator.Options().tickLimit);
}

void LiveBattleSession::SetObserving(bool observing)
{
  m_impl->simulator.SetAiControlled(m_impl->screen.PlayerSide(), observing);
}

ui::BattleScreen& LiveBattleSession::Screen() { return m_impl->screen; }
battle::Battle& LiveBattleSession::Simulator() { return m_impl->simulator; }

bool LiveBattlePresenterAvailable()
{
  const char* smoke = std::getenv("REB_LIVE_BATTLE_SMOKE");
  return presenter::Available() || (smoke && std::strcmp(smoke, "1") == 0);
}
