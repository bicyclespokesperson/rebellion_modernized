#include <cstddef>
#include <memory>
#include <new>

#include "app/app_externals.h"
#include "app/app_strings.h"
#include "app/battle_flow.h"
#include "app/battle_live.h"
#include "app/game_session.h"
#include "app/rebapp_globals.h"
#include "app/rebellion_application.h"
#include "audio/sound_manager.h"
#include "events/game_clock_hooks.h"
#include "events/game_step_driver.h"
#include "gameobj/game_settings.h"
#include "gfx/dib_bitmap.h"
#include "gfx/directdraw_host.h"
#include "multiplayer/multiplayer_window.h"
#include "multiplayer/lan_lobby.h"
#include "port/native/game_context.h"
#include "port/native/lan_game.h"
#include "port/minwin_impl/user/canvas.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/game_options_window.h"
#include "strategy_ui/game_ui_facade.h"
#include "strategy_ui/main_menu_window.h"
#include "strategy_ui/object_menu.h"
#include "multiplayer/comm_helpers.h"
#include "ui/custom_dialog_box.h"
#include "ui/game_window_object.h"
#include "video/movie_window.h"

namespace
{

constexpr UINT kBlackness = 0x42;
constexpr int kShowNormal = 5;
constexpr uint32_t kTextCommSlot = 1;
constexpr uint32_t kCommonSlot = 8;
constexpr uint32_t kTextStraSlot = 2;
constexpr uint32_t kFrontEndBackdropId = 0x4e21;
constexpr int kBattleFlowTactical = 7;
constexpr int kBattleFlowStrategic = 0x16;
constexpr int kNetworkModeConnected = 7;
constexpr uint32_t kModalChildStrategic = 7;
constexpr uint32_t kModalChildStrategicAlt = 8;
constexpr int kMovieFirstIntro = 0x65;
constexpr int kMovieSecondIntro = 0x66;
constexpr int kMovieNewGameEmpire = 0x68;
constexpr int kMovieVictoryAlliance = 0x6a;
constexpr int kMovieVictoryEmpire = 0x6b;
constexpr int kMovieDefeatAlliance = 0x6c;
constexpr int kMovieDefeatEmpire = 0x6d;
constexpr int kQuitDialogMessage = 0x470;
constexpr int kQuitDialogText = 0x1202;
constexpr int kQuitDialogX = 500;
constexpr int kQuitDialogY = 0x50;

struct WindowStorageDeleter {
  void operator()(void* storage) const noexcept
  {
    ::operator delete(storage);
  }
};

template <class Window, class Construct>
Window* ConstructWindow(Construct construct)
{
  std::unique_ptr<void, WindowStorageDeleter> storage(::operator new(sizeof(Window), std::nothrow));
  if (!storage) {
    return nullptr;
  }
  auto* window = static_cast<Window*>(construct(storage.get()));
  storage.release();
  return window;
}

void ShowCursorCounter(bool visible)
{
  int count = reb::compat::ShowCursor(visible);
  if (visible) {
    while (count < 0) {
      count = reb::compat::ShowCursor(TRUE);
    }
  } else {
    while (count >= 0) {
      count = reb::compat::ShowCursor(FALSE);
    }
  }
}

void DeleteHeapString(char* text)
{
  if (text) {
    ::operator delete(text);
  }
}

void ShowNewWindow(CoolWindow* window)
{
  window->Create(0);
  window->Show(kShowNormal);
  reb::compat::SetFocus(window->m_hwnd);
}

} // namespace

void RebellionApplication::ReleaseOverlayWindow()
{
  if (!m_overlayWindow) {
    return;
  }
  m_overlayWindow->Hide();
  reb::compat::DestroyWindow(m_overlayWindow->m_hwnd);
  delete m_overlayWindow;
  m_overlayWindow = nullptr;
}

void RebellionApplication::DetachActiveWindow()
{
  if (!m_activeWindow) {
    return;
  }
  m_activeWindow->Hide();
  CoolWindow* window = m_activeWindow;
  if (m_strategicWindow == window) {
    m_strategicWindow = nullptr;
  }
  reb::compat::PostMessageA(m_hwnd, kMsgDestroyWindow, 0, reinterpret_cast<LPARAM>(window));
  m_activeWindow = nullptr;
}

void RebellionApplication::FillWindowBlack()
{
  HDC dc = reb::compat::GetDC(m_hwnd);
  reb::compat::PatBlt(dc, 0, 0, m_clientWidth, m_clientHeight, kBlackness);
  ReleasePalette(dc);
  reb::compat::ReleaseDC(m_hwnd, dc);
}

void RebellionApplication::SetGameParams(int side, int gameType, int param8, int paramF0, int connectionKind,
                                         int networkMode)
{
  m_side = side;
  m_param8 = param8;
  m_gameType = gameType;
  m_networkMode = networkMode;
  m_paramF0 = paramF0;
  m_param10 = 0;
  if (connectionKind == 1) {
    m_param10 = 0;
  } else if (connectionKind == 2) {
    m_param10 = 1;
  }
}

void RebellionApplication::SetState(int state)
{
  m_previousState = m_state;
  m_state = state;
  for (;;) {
    switch (state) {
      case kStateIntroMovie:
      case kStateIntroMovie2:
      case kStateNewGameMovie:
        m_connected = 0;
        m_pauseRequested = 0;
        m_peersReady = 0;
        SetIdentityPaletteForDisplay();
        BeginMovieState();
        return;
      case kStateMainMenu:
        if (m_context.GetLanGame().Playing() && m_context.GetLanGame().IsHost()) {
          m_context.GetLanGame().SaveBeforeExit(*this);
        }
        m_connected = 0;
        m_pauseRequested = 0;
        m_peersReady = 0;
        m_networkMode = 0;
        m_gameType = 0;
        CommClearLobbyLaunch();
        CommCleanUp();
        [[fallthrough]];
      case kStateLobbyHost:
      case kStateLobbyJoin:
      case kStateOptionsMenu:
      case kStateOptionsInGame:
        SetIdentityPaletteForDisplay();
        DoMainChildWindow(m_previousState);
        return;
      case kStateStrategic:
        m_connected = 1;
        SetIdentityPaletteForDisplay();
        ShowStrategicWindow(1);
        return;
      case kStateTactical:
        SetIdentityPaletteForDisplay();
        ShowTacticalWindow();
        return;
      case kStateBattleSwitch:
        {
          int battleFlowState = AppGetBattleFlowState();
          int next;
          if (battleFlowState == kBattleFlowTactical) {
            next = kStateTactical;
          } else if (battleFlowState == kBattleFlowStrategic) {
            next = kStateStrategic;
          } else {
            return;
          }
          if (m_strategicWindow) m_strategicWindow->Hide();
          m_activeWindow = nullptr;
          m_previousState = m_state;
          m_state = next;
          state = next;
          continue;
        }
      case kStateConfirmQuit:
        SetCaptureWindow(nullptr);
        reb::compat::SetFocus(nullptr);
        ReleaseOverlayWindow();
        AppShowQuestionDialog(kQuitDialogMessage, 0, kQuitDialogText, kQuitDialogX, kQuitDialogY);
        m_state = m_previousState;
        return;
      case kStateQuit:
        if (m_context.GetLanGame().Playing() && m_context.GetLanGame().IsHost()) {
          m_context.GetLanGame().SaveBeforeExit(*this);
        }
        reb::compat::WinHelpA(m_hwnd, nullptr, HELP_QUIT, 0);
        CommTrace("RebellionApplication::QuitRebellion(): REBAPP_UI_PHASE_COCKPIT_SCREEN: About to call "
                  "PlayerQuitSession()!\n");
        CommSendPlayerQuit();
        ReleaseOverlayWindow();
        DestroyAllWindows();
        m_context.GetLanGame().Close();
        CommShutdown();
        DestroyBattleFlow();
        SoundManager::Shutdown(0);
        m_display.ReleaseAll();
        reb::compat::PostQuitMessage(0);
        return;
      case kStateVictoryMovieEmpire:
      case kStateVictoryMovieAlliance:
      case kStateDefeatMovieEmpire:
      case kStateDefeatMovieAlliance:
        m_context.GetLanGame().FinishGame(state);
        SetIdentityPaletteForDisplay();
        DestroyAppDialog();
        DestroyStrategicWindow();
        m_context.GetLanGame().Close();
        CommCleanUp();
        DestroyBattleFlow();
        BeginMovieState();
        return;
      default:
        m_state = kStateNone;
        return;
    }
  }
}

void RebellionApplication::PlayMovie(int movieId, const std::string* caption, void (*callback)(uint32_t, uint32_t, int))
{
  if (m_activeWindow) {
    m_activeWindow->Hide();
  }
  ReleaseOverlayWindow();
  FillWindowBlack();
  const bool widescreenGameplay = minwin::GameplayCanvasEnabled();
  const int movieWidth = m_screenWidth;
  const int movieHeight = m_screenHeight;
  const int movieX = widescreenGameplay ? m_unknownA0 + (reb::compat::GetSystemMetrics(SM_CXSCREEN) - movieWidth) / 2 : m_unknownA0;
  m_overlayWindow = ConstructWindow<MovieWindow>([&](void* memory) {
    return new (memory)
        MovieWindow(movieId, callback, m_instance, movieX, m_unknownA4, movieWidth, movieHeight, this, caption);
  });
  if (m_overlayWindow) {
    m_overlayWindow->Create(0);
    m_overlayWindow->Show(kShowNormal);
    reb::compat::SetFocus(m_overlayWindow->m_hwnd);
  }
}

void RebellionApplication::BeginMovieState()
{
  DetachActiveWindow();
  ShowCursorCounter(false);
  ReleaseOverlayWindow();
  FillWindowBlack();
  int movieId;
  switch (m_state) {
    case kStateIntroMovie:
      movieId = kMovieFirstIntro;
      break;
    case kStateIntroMovie2:
      movieId = kMovieSecondIntro;
      break;
    case kStateNewGameMovie:
      {
        m_unknown130 = 1;
        m_unknown134 = 0;
        char* dllLocation = GetDllLocation();
        CoolRegisterResourceDll(dllLocation, g_textstraDllName, kTextStraSlot);
        DeleteHeapString(dllLocation);
        CreateBattleFlow();
        GameSettings setup;
        setup.m_field08 = m_param10;
        setup.m_field04 = m_param8;
        setup.m_field10 = m_paramF0;
        setup.m_field0c = m_networkMode;
        g_isNetworkGame = m_networkMode == kNetworkModeConnected;
        AppStartGame(&setup, m_gameType, m_side);
        PlayMovie((m_side != 1) + kMovieNewGameEmpire, nullptr, nullptr);
        return;
      }
    case kStateVictoryMovieEmpire:
      movieId = kMovieVictoryEmpire;
      break;
    case kStateVictoryMovieAlliance:
      movieId = kMovieVictoryAlliance;
      break;
    case kStateDefeatMovieEmpire:
      movieId = kMovieDefeatEmpire;
      break;
    case kStateDefeatMovieAlliance:
      movieId = kMovieDefeatAlliance;
      break;
    default:
      return;
  }
  PlayMovie(movieId, nullptr, nullptr);
}

void RebellionApplication::DoMainChildWindow(int previousState)
{
  minwin::SetGameplayCanvasEnabled(false);
  ReleaseOverlayWindow();
  if (m_strategicWindow) {
    if (m_activeWindow == m_strategicWindow) {
      m_activeWindow = nullptr;
    }
    m_strategicWindow->Hide();
  }
  DetachActiveWindow();
  FillWindowBlack();

  char* dllLocation = GetDllLocation();
  if (!FindResourceModule(kTextCommSlot)) {
    LoadResourceModule(dllLocation, g_textcommDllName, kTextCommSlot);
  }
  if (!FindResourceModule(kCommonSlot)) {
    LoadResourceModule(dllLocation, g_commonDllName, kCommonSlot);
  }
  HMODULE commonModule = FindResourceModule(kCommonSlot);
  DeleteHeapString(dllLocation);

  int x = m_unknownA0;
  int y = m_unknownA4;
  int width = m_screenWidth;
  int height = m_screenHeight;
  switch (m_state) {
    case kStateMainMenu:
      {
        ShowCursorCounter(true);
        CommTrace("RebellionApplication::DoMainChildWindow(): REBAPP_UI_PHASE_COCKPIT_SCREEN: About to call "
                  "PlayerQuitSession()!\n");
        CommSendPlayerQuit();
        DestroyStrategicWindow();
        m_context.GetLanGame().Close();
        CommCleanUp();
        DestroyBattleFlow();
        UiSetPausePending(0);
        DibBitmap* backdrop = nullptr;
        if (void* memory = ::operator new(sizeof(DibBitmap))) {
          backdrop = new (memory) DibBitmap(commonModule, kFrontEndBackdropId, 2u);
        }
        if (backdrop) {
          HDC dc = reb::compat::GetDC(m_hwnd);
          SetIdentityPalette(dc, backdrop->CreatePaletteFromColorTable());
          reb::compat::ReleaseDC(m_hwnd, dc);
          backdrop->~DibBitmap();
          ::operator delete(backdrop);
        }
        m_activeWindow = ConstructWindow<MainMenuWindow>([&](void* memory) {
          return new (memory) MainMenuWindow(m_instance, x, y, width, height, this);
        });
        break;
      }
    case kStateLobbyHost:
    case kStateLobbyJoin:
      m_activeWindow = ConstructWindow<LanLobbyWindow>([&](void* memory) {
        return new (memory) LanLobbyWindow(m_instance, x, y, width, height, this);
      });
      m_networkMode = 0;
      break;
    case kStateOptionsMenu:
    case kStateOptionsInGame:
      m_activeWindow = ConstructWindow<GameOptionsWindow>([&](void* memory) {
        return new (memory) GameOptionsWindow(m_instance, x, y, width, height, this, previousState);
      });
      break;
    default:
      break;
  }
  if (m_activeWindow) {
    m_activeWindow->Create(0);
    m_activeWindow->Show(kShowNormal);
    reb::compat::SetFocus(m_activeWindow->m_hwnd);
  }
}

void RebellionApplication::ShowStrategicWindow(int unused)
{
  minwin::SetGameplayCanvasEnabled(true);
  ReleaseOverlayWindow();
  DetachActiveWindow();
  FillWindowBlack();
  ShowCursorCounter(true);
  if (!m_strategicWindow) {
    int x = m_unknownA0;
    int y = m_unknownA4;
    int width = reb::compat::GetSystemMetrics(SM_CXSCREEN);
    int height = reb::compat::GetSystemMetrics(SM_CYSCREEN);
    if (m_side != 0) {
      int firstGame = m_previousState == kStateBattleSwitch || m_previousState == kStateTactical;
      m_strategicWindow = ConstructWindow<GalaxyWindow>([&](void* memory) {
        return new (memory) GalaxyWindow(m_instance, x, y, width, height, this, m_side, firstGame);
      });
    }
    if (m_strategicWindow) {
      ShowNewWindow(m_strategicWindow);
    }
  } else {
    reb::compat::SetWindowPos(m_strategicWindow->m_hwnd, nullptr, m_unknownA0, m_unknownA4, reb::compat::GetSystemMetrics(SM_CXSCREEN),
                 reb::compat::GetSystemMetrics(SM_CYSCREEN), SWP_NOZORDER | SWP_NOACTIVATE);
    HDC dc = reb::compat::GetDC(m_hwnd);
    DibBitmap* background = static_cast<GameWindowObject*>(m_strategicWindow)->GetBackground();
    SetIdentityPalette(dc, background->CreatePaletteFromColorTable());
    m_strategicWindow->Show(kShowNormal);
    reb::compat::ReleaseDC(m_hwnd, dc);
  }
  m_activeWindow = m_strategicWindow;
}

void RebellionApplication::DestroyStrategicWindow()
{
  if (!m_strategicWindow) {
    minwin::SetGameplayCanvasEnabled(false);
    return;
  }
  if (m_activeWindow == m_strategicWindow) {
    m_activeWindow = nullptr;
  }
  BOOL destroyed = reb::compat::DestroyWindow(m_strategicWindow->m_hwnd);
  if (destroyed && m_strategicWindow) {
    delete m_strategicWindow;
  }
  m_strategicWindow = nullptr;
  minwin::SetGameplayCanvasEnabled(false);
}

void RebellionApplication::RequestSwitchSides()
{
  if (m_state == kStateStrategic && m_gameType == 1) {
    m_switchSidesRequested = true;
  }
}

void RebellionApplication::ProcessDeveloperActions()
{
  if (!m_switchSidesRequested) {
    return;
  }
  if (m_state != kStateStrategic || m_gameType != 1) {
    m_switchSidesRequested = false;
    return;
  }
  GameStepDriver* driver = GetGameStepDriver();
  BattleFlow* flow = GetBattleFlow();
  if (!flow || flow->m_state != kFlowStrategic) {
    return;
  }
  if (!driver || driver->m_setupStage != 0 || (driver->m_phase != 0 && driver->m_phase != 2)) {
    return;
  }
  if (!m_strategicWindow || m_strategicWindow->m_inModalLoop || m_strategicWindow->m_mouseMode != 1
      || !g_popupStack.empty()) {
    return;
  }
  GameSession* session = driver->Session();
  m_switchSidesRequested = false;
  if (!session || !session->SwitchHumanSide()) {
    return;
  }
  DismissMenu();
  DestroyStrategicWindow();
  m_side = session->GetHumanSide();
  ShowStrategicWindow(0);
  m_strategicWindow->InitializeGameDisplay();
  m_strategicWindow->SetDateAndSpeed(GameHookDay(), session->m_human->m_speed, 1);
  session->HumanRequestUpdate();
}

void RebellionApplication::ShowTacticalWindow()
{
  minwin::SetGameplayCanvasEnabled(false);
  if (BattleFlow* flow = GetBattleFlow(); flow && flow->LiveSession()) {
    ReleaseOverlayWindow();
    if (m_strategicWindow) m_strategicWindow->Hide();
    m_activeWindow = nullptr;
    ShowCursorCounter(true);
    flow->LiveSession()->Open();
    return;
  }
}

void RebellionApplication::DestroyAllWindows()
{
  DetachActiveWindow();
  DestroyStrategicWindow();
}

void RebellionApplication::ForwardToStrategicWindow()
{
  CoolWindow* window = m_activeWindow;
  if (!window) {
    return;
  }
  if (window->m_childId == kModalChildStrategic || window->m_childId == kModalChildStrategicAlt) {
    static_cast<GalaxyWindow*>(window)->GalaxyWindow::InitializeGameDisplay();
  }
}

GalaxyWindow* g_galaxyWindow{};
HWND g_previousCapture{};
MainMenuWindow* g_mainMenuWindow{};
const char* g_coolDragListClassName{};
