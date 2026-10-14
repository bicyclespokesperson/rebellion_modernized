// The application shell: WinMain, the RebellionApplication window and its main loop.
//
// Start-up and run-time flow (see also docs/platform_layer.md):
//
//  WinMain (0x4165b0)
//    1. A named semaphore "STARWARS_REBELLION" is the single-instance guard; a failed CreateSemaphoreA or an
//       existing semaphore ends the process with 1. CheckDisplayRequirements (mouse, 640x480, 256 colours)
//       follows.
//    2. ChangeToInstalledLocation sets the drive and directory from the registry's "InstalledLocation";
//       InitCdAndVersionCheck loads the CD / version error texts; the string loader is installed.
//    3. The command line is scanned for -L (0x100), -N (0x10) and -W (0x1); 0x10 is always added. Unless
//       flag 0x1000 is set (never from the command line) InstalledLocationHasBigData must find
//       MDATA\MDATA.000 on a fixed drive, otherwise the "CD not found" box is shown and WinMain returns 0.
//    4. The RebellionApplication is created (0x160 bytes; the ctor 0x413000 registers the window class data and
//       the resource DLLs but creates no window yet), sized 640x480 and run. Run's result is WinMain's return
//       value; the application is deleted afterwards. (WinMain cannot be difftested: it is the process entry.)
//
//  RebellionApplication::Run (0x413560)
//    - Creates the native window (CoolWindow::Create) and shows it. WM_CREATE (WindowProc) sets up tooltips,
//      DirectDraw and the video manager and arms g_firstActivationPending.
//    - The loop drains the message queue (FilterMessage, TranslateAccelerator on the main window, then
//      TranslateMessage / DispatchMessage), then steps the game once through AppStepGame and lets the strategic
//      window run its idle virtual (slot 11). In a network game it polls the multiplayer layer and shows the
//      "opponent has left / is not responding" boxes. Finally ProcessPendingSaveLoad runs queued saves and loads.
//      The loop ends after WM_QUIT (0x12) has been seen and the pass has finished. There is no idle sleep.
//
//  WM_ACTIVATEAPP (WindowProc 0x1c) drives the front end: the first activation after WM_CREATE loads the
//  TEXTCOMM / COMMON resource DLLs and enters state 1 (intro movie) or, in a multiplayer session, state 5 (lobby).
//  Deactivation restores the display mode and minimizes the window (unless -W); reactivation switches DirectDraw
//  back to 640x480x8 and tells the active screen. Movies and child windows report their end with the custom
//  messages 0x405 / 0x465 (lParam: movie / window id), which select the next state through SetState.
#include "app/rebellion_application.h"
#include "port/native/game_context.h"
#include "port/minwin_impl/user/canvas.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <direct.h>
#include <new>

#include "app/app_cdcheck_version.h"
#include "app/app_dialogs.h"
#include "app/app_externals.h"
#include "app/app_strings.h"
#include "app/battle_flow.h"
#include "app/rebapp_globals.h"
#include "core/registry.h"
#include "events/game_clock_hooks.h"
#include "gfx/dib_bitmap.h"
#include "gfx/directdraw_host.h"
#include "multiplayer/multiplayer_window.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/game_ui_facade.h"
#include "tactical/comm2.h"
#include "ui/cool_environment.h"
#include "ui/cool_window.h"
#include "ui/game_window_object.h"
#include "video/movie_window.h"
#include "video/video_manager.h"

extern int g_firstActivationPending; // 0x6b1468
extern const int16_t g_resourceStringGroup; // 0x65d420
extern HCURSOR g_defaultCursor; // 0x6be5b4
extern HPALETTE g_palette; // 0x6be5a0

void SetResourceModule(void* module); // 0x5f49c0
HMODULE LoadResourceModule(const char* basePath, const char* fileName, uint32_t slot); // 0x5ff020
void SetLocaleFromEnvironment(); // 0x5f35a0
void ShutdownTooltips(); // 0x600e20

namespace
{

constexpr char kCompanyKey[] = "SOFTWARE\\LucasArts Entertainment Company LLC";
constexpr char kGameName[] = "Star Wars Rebellion";
constexpr char kBackslash[] = "\\";
constexpr int kScreenWidth = 0x280;
constexpr int kScreenHeight = 0x1e0;
constexpr uint32_t kWindowedStyleBits = 0xca0000;
constexpr uint32_t kPopupStyle = 0x86000000;
constexpr uint32_t kDeactivatedBit = 0x10000000;
constexpr int kTooltipDelayMs = 500;
constexpr int kDialogX = 500;
constexpr int kDialogY = 0x50;
constexpr UINT kWmQuit = 0x12;
constexpr uint32_t kSysCommandSizeMove = 0xf100;
constexpr uint32_t kFirstAcceleratorId = 3000;
constexpr uint32_t kEndAcceleratorId = 0xbd8;

int WindowedWidth()
{
  return GetSystemMetrics(SM_CXBORDER) * 2 + kScreenWidth;
}
int WindowedHeight()
{
  return GetSystemMetrics(SM_CYCAPTION) + kScreenHeight + GetSystemMetrics(SM_CYBORDER) * 2;
}

int WindowWidth(uint32_t flags)
{
  return (flags & kAppWindowed) ? WindowedWidth() : kScreenWidth;
}
int WindowHeight(uint32_t flags)
{
  return (flags & kAppWindowed) ? WindowedHeight() : kScreenHeight;
}
uint32_t WindowStyle(uint32_t flags)
{
  return ((flags & kAppWindowed) ? kWindowedStyleBits : 0) + kPopupStyle;
}

void DeleteWindowObject(CoolWindow* window)
{
  delete window;
}

void DestroyOverlayWindow(RebellionApplication* app)
{
  if (!app->m_overlayWindow)
    return;
  app->m_overlayWindow->Hide();
  DestroyWindow(app->m_overlayWindow->m_hwnd);
  if (app->m_overlayWindow)
    DeleteWindowObject(app->m_overlayWindow);
  app->m_overlayWindow = nullptr;
}

void SetFocusToActiveWindow(RebellionApplication* app)
{
  SetFocus(app->m_activeWindow->m_hwnd);
}

void InstallIdentityPaletteFrom(HDC dc, CoolWindow* window)
{
  auto* screen = static_cast<GameWindowObject*>(window);
  SetIdentityPalette(dc, screen->GetBackground()->CreatePaletteFromColorTable());
}

void RestoreCursorVisibility()
{
  while (ShowCursor(TRUE) < 0)
  {
  }
}

std::string BuildGameRegistryKey()
{
  std::string key;
  std::string game(kGameName);
  std::string separator(kBackslash);
  std::string company(kCompanyKey);
  key += company;
  key += separator;
  key += game;
  return key;
}

HACCEL AcceleratorTableOf(RebellionApplication* app)
{
  return reinterpret_cast<HACCEL>(app->m_unknown98);
}

} // namespace

RebellionApplication::RebellionApplication(HINSTANCE instance,
                                           uint32_t commandLineFlags,
                                           const std::string* commandLine,
                                           DirectDrawHost& display,
                                           reb::native::GameContext& context,
                                           UiResources& resources)
: CoolApp(instance,
          g_appClassName,
          WindowStyle(commandLineFlags),
          0,
          0,
          WindowWidth(commandLineFlags),
          WindowHeight(commandLineFlags),
          nullptr,
          static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)),
          LoadIconA(instance, MAKEINTRESOURCEA(0x3e8)),
          nullptr,
          nullptr,
          0,
          0,
          resources),
  m_display(display),
  m_context(context),
  m_activationFlags(0)
{
  m_param8 = 1;
  m_param10 = 0;
  m_paramF0 = 0;
  m_networkMode = 7;
  static const char* const kNetworkSettingNames[] = {
    "MinimumReceiveLatency", "MaximumReceiveLatency", "MinimumSendLatency",
    "MaximumSendLatency",    "DropReceivePercent",    "DropSendPercent",
    "DupeReceivePercent",    "DupeSendPercent",       "UseWaitForSingleObject",
  };
  for (const char* name : kNetworkSettingNames)
  {
    const std::string key(name);
    m_networkSettings.Add(&key, 0);
  }
  m_networkSettings.LoadValues(commandLine);
  m_syncState = 0;
  m_peersReady = 0;
  m_connected = 0;
  m_pauseRequested = 0;
  g_defaultCursor = LoadCursorA(m_instance, MAKEINTRESOURCEA(0x3e9));
  char* dllLocation = GetDllLocation();
  LoadResourceModule(dllLocation, g_rebdlogDllName, 0xc);
  LoadResourceModule(dllLocation, g_textcommDllName, 1);
  if (dllLocation)
    operator delete(dllLocation);
  SetResourceModule(instance);
  m_pendingLoadSlot = 0;
  m_pendingSaveSlot = 0;
  m_activeWindow = nullptr;
  m_strategicWindow = nullptr;
  m_tacticalWindow = nullptr;
  m_overlayWindow = nullptr;
  m_state = kStateNone;
  m_gameType = 0;
  m_side = 0;
  m_waitElapsed = 0;
  m_waitTimer.Reset();
  m_mismatchOptions = static_cast<int>(ReadRegistryDword(g_registryKeyPath, g_mismatchOptsName));
  m_commandLineFlags = commandLineFlags;
  m_unknown130 = -1;
  m_unknown134 = -1;
  m_displayOptions = ReadRegistryDword(g_registryKeyPath, g_displayOptsName);
  m_displayOptions2 = 0x13110000;
  m_volume = -static_cast<int>(ReadRegistryDword(g_registryKeyPath, g_volumeName));
  m_unknown158 = 0;
  m_defaultFontSlot = 4;
}

RebellionApplication::~RebellionApplication()
{
  if (VideoManager* videoManager = VideoManager::GetInstance())
    delete videoManager;
  if (m_strategicWindow && m_strategicWindow != m_activeWindow)
  {
    DeleteWindowObject(m_strategicWindow);
    m_strategicWindow = nullptr;
  }
  if (m_tacticalWindow && m_tacticalWindow != m_activeWindow)
  {
    DeleteWindowObject(m_tacticalWindow);
    m_tacticalWindow = nullptr;
  }
  if (m_activeWindow)
  {
    DeleteWindowObject(m_activeWindow);
    m_activeWindow = nullptr;
  }
  if (m_overlayWindow)
  {
    DeleteWindowObject(m_overlayWindow);
    m_overlayWindow = nullptr;
  }
  AppReleaseBattleFlow();
  ShutdownTooltips();
  m_context.SetBattleFlow(nullptr);
}

void RebellionApplication::PumpMessages(MSG* message, int* quit)
{
  while (PeekMessageA(message, nullptr, 0, 0, PM_REMOVE) && !*quit)
  {
    if (message->message == kWmQuit)
      *quit = 1;
    if (!FilterMessage(message))
      continue;
    HWND original = message->hwnd;
    if (m_hwnd && m_unknown98)
      message->hwnd = m_hwnd;
    if (!TranslateAcceleratorA(message->hwnd, AcceleratorTableOf(this), message))
    {
      message->hwnd = original;
      TranslateMessage(message);
      DispatchMessageA(message);
    }
  }
}

void RebellionApplication::PollMultiplayerSync(int quit)
{
  CommSyncState();
  m_syncState = CommGetPlayerCount();
  if (!m_connected)
    return;
  if (m_syncState == 2)
  {
    m_pauseRequested = 0;
    m_peersReady = 0;
  }
  else
  {
    m_peersReady = 1;
  }
  if (!m_peersReady || m_pauseRequested || quit)
    return;
  if (m_state < 8 || m_state >= 0x10)
    return;
  m_pauseRequested = 1;
  CommCleanUp();
  if (m_state == kStateStrategic)
  {
    if (m_strategicWindow->m_unknownAc != 0)
      return;
  }
  else if (m_state == kStateLobbyHost || m_state == kStateLobbyJoin)
  {
    static_cast<MultiPlayerWindow*>(m_activeWindow)->MultiPlayerWindow::UpdateStartButtons();
  }
  AppShowNoticeDialog(0x46d, 0, 0x1204, 0x190e, kDialogX, kDialogY);
  CommCleanUp();
}

void RebellionApplication::ReportOpponentTrouble()
{
  bool trigger = false;
  bool opponentLeft = false;
  if (GetManagerUnk38() && !GetManagerUnk3c())
  {
    trigger = true;
    opponentLeft = true;
  }
  if (!trigger && GetManagerUnk40() && !GetManagerUnk44())
  {
    trigger = true;
    opponentLeft = false;
  }
  if (!trigger && GetManagerStateDiffers() && !GetManagerUnk34())
  {
    trigger = true;
    opponentLeft = false;
  }
  if (!trigger)
    return;
  SetManagerUnk34(1);
  SetManagerUnk3c(1);
  SetManagerUnk44(1);
  UiBeginPause();
  if (m_state == kStateLobbyHost || m_state == kStateLobbyJoin)
    static_cast<MultiPlayerWindow*>(m_activeWindow)->MultiPlayerWindow::UpdateStartButtons();
  if (opponentLeft)
  {
    if (m_mismatchOptions)
      AppShowQuestionDialog(0x47d, 0x120b, 0x120c, kDialogX, kDialogY);
    else
      AppShowNoticeDialog(0x47d, 0, 0x120b, 0x1911, kDialogX, kDialogY);
  }
  else
  {
    if (m_mismatchOptions)
      AppShowQuestionDialog(0x47c, 0x1209, 0x120a, kDialogX, kDialogY);
    else
      AppShowNoticeDialog(0x47c, 0, 0x1209, 0x1910, kDialogX, kDialogY);
  }
}

WPARAM RebellionApplication::Run(int showCommand, int unused)
{
  (void)unused;
  int quit = 0;
  int stepped = 1;
  MSG message = {};
  CommQueryLobbyConnection();
  Create(0);
  Show(showCommand);
  for (;;)
  {
    PumpMessages(&message, &quit);
    stepped = 1;
    AppStepGame(&stepped);
    if (m_activeWindow && m_activeWindow == m_strategicWindow)
    {
      m_strategicWindow->TickIdleAnimations();
    }
    if (m_networkMode == 7 && m_gameType == 2)
      PollMultiplayerSync(quit);
    if (stepped)
    {
      ReportOpponentTrouble();
      ProcessPendingSaveLoad();
    }
    if (quit)
      return message.wParam;
  }
}

LRESULT RebellionApplication::OnCreate()
{
  InitCommonControls();
  CreateTooltipControl(kTooltipDelayMs);
  m_display.Initialize(m_hwnd, 1);
  if (m_commandLineFlags & kAppWindowed)
    m_display.SetNormalMode(0);
  const ResourceStringId titleId = {0x1000, g_resourceStringGroup};
  const std::string title = LoadStringResource(&titleId);
  SetWindowTitle(title.c_str());
  void* memory = operator new(sizeof(VideoManager));
  VideoManager* videoManager = memory ? new (memory) VideoManager(this) : nullptr;
  if (!videoManager)
    ShowErrorMessage(g_introMusicFailedText, g_soundManagerCaption);
  g_firstActivationPending = 1;
  return 0;
}

void RebellionApplication::EnterFrontEndAfterFirstActivation()
{
  int nextState = kStateIntroMovie;
  if (CommLaunchedFromLobby())
  {
    char* dllLocation = GetDllLocation();
    LoadResourceModule(dllLocation, g_textcommDllName, 1);
    LoadResourceModule(dllLocation, g_commonDllName, 8);
    if (dllLocation)
      operator delete(dllLocation);
    nextState = kStateLobbyJoin;
  }
  SetState(nextState);
  g_firstActivationPending = 0;
}

void RebellionApplication::OnDeactivated()
{
  m_activationFlags.m_value |= kDeactivatedBit;
  if (!(m_commandLineFlags & kAppWindowed))
  {
    SetFocus(m_hwnd);
    m_display.RestoreDisplayMode();
    ShowWindow(m_hwnd, SW_MINIMIZE);
  }
  if (m_activeWindow)
    SendMessageA(m_activeWindow->m_hwnd, kMsgDeactivated, 0, 0);
}

void RebellionApplication::OnActivated()
{
  m_activationFlags.m_value &= ~kDeactivatedBit;
  if (!(m_commandLineFlags & kAppWindowed))
  {
    if (m_activeWindow)
      m_activeWindow->Hide();
    m_display.SetExclusiveMode(0);
    m_display.SetDisplayMode(kScreenWidth, kScreenHeight, 8, 0);
    m_display.SetNormalMode(0);
    m_display.RestoreSurfaces();
  }
  if (RealizeCurrentPalette())
    InvalidateRect(m_hwnd, nullptr, FALSE);
  SetCaptureWindow(nullptr);
  if (!m_overlayWindow)
  {
    if (m_activeWindow)
      SetFocus(m_activeWindow->m_hwnd);
    if (m_activeWindow)
    {
      SendMessageA(m_activeWindow->m_hwnd, kMsgActivated, 0, 0);
      if (m_activeWindow)
        m_activeWindow->Show(SW_SHOW);
    }
  }
  else
  {
    SetFocus(m_overlayWindow->m_hwnd);
  }
  CenterAppDialog();
  if (g_firstActivationPending)
    EnterFrontEndAfterFirstActivation();
}

LRESULT RebellionApplication::OnActivateApp(WPARAM wParam, LPARAM lParam)
{
  HWND hwnd = m_hwnd;
  if (wParam == 0)
    OnDeactivated();
  else
    OnActivated();
  return DefWindowProcA(hwnd, WM_ACTIVATEAPP, wParam, lParam);
}

LRESULT RebellionApplication::OnQueryNewPalette(WPARAM wParam, LPARAM lParam)
{
  HDC dc = GetDC(m_hwnd);
  SetSystemPaletteUse(dc, SYSPAL_NOSTATIC);
  SetSystemPaletteUse(dc, SYSPAL_STATIC);
  if (m_activeWindow)
    m_activeWindow->WindowProc(WM_QUERYNEWPALETTE, wParam, lParam);
  InvalidateRect(m_hwnd, nullptr, FALSE);
  SetIdentityPalette(dc, g_palette);
  ReleaseDC(m_hwnd, dc);
  return 1;
}

void RebellionApplication::ReturnToActiveWindowAfterChild(LPARAM childId)
{
  if (!m_activeWindow)
    return;
  HDC dc = GetDC(m_hwnd);
  if (m_activeWindow == m_strategicWindow)
    InstallIdentityPaletteFrom(dc, m_strategicWindow);
  else if (m_activeWindow == m_tacticalWindow)
    InstallIdentityPaletteFrom(dc, m_tacticalWindow);
  ReleaseCapture();
  if (m_activeWindow)
    m_activeWindow->Show(SW_SHOW);
  ReleaseDC(m_hwnd, dc);
  if (!m_activeWindow)
    return;
  PostMessageA(m_activeWindow->m_hwnd, kMsgRequestFinished, 0, childId);
}

LRESULT RebellionApplication::OnChildDone(UINT message, WPARAM wParam, LPARAM lParam)
{
  const bool isChildDoneMessage = message == kMsgChildDone;
  if (isChildDoneMessage && wParam == 0xd)
  {
    DestroyAppDialog();
    return 0;
  }
  if (wParam != 0xf)
    return 0;
  DestroyOverlayWindow(this);
  switch (lParam)
  {
    case 0x65:
      if (isChildDoneMessage)
      {
        SetState(kStateIntroMovie2);
        return 0;
      }
      RestoreCursorVisibility();
      SetState(kStateMainMenu);
      return 0;
    case 0x66:
    case 0x6a:
    case 0x6b:
    case 0x6c:
    case 0x6d:
      RestoreCursorVisibility();
      SetState(kStateMainMenu);
      return 0;
    case 0x67:
      SetState(isChildDoneMessage ? kStateNewGameMovie : kStateStrategic);
      return 0;
    case 0x68:
    case 0x69:
      SetState(kStateStrategic);
      return 0;
  }
  ReturnToActiveWindowAfterChild(lParam);
  return 0;
}

LRESULT RebellionApplication::OnDialogAnswer(WPARAM wParam)
{
  switch (wParam)
  {
    case 0x46c:
      return OnSaveAnswer();
    case 0x46d:
    case 0x47c:
    case 0x47d:
      CommClearLobbyLaunch();
      SetState(kStateMainMenu);
      return 0;
    case 0x470:
      SetState(kStateQuit);
      return 0;
  }
  return 0;
}

LRESULT RebellionApplication::OnSaveAnswer()
{
  if (m_gameType != 2)
  {
    if (m_gameType != 1)
      return 0;
    UiAckSync(UiGetHumanSide());
    return 0;
  }
  if (m_state != kStateStrategic)
    return 0;
  UiAckSync(UiGetHumanSide());
  if (m_strategicWindow->m_unknownAc != 0)
    return 0;
  const bool ready = (!UiAreAllSynced() && UiIsPauseTick()) || UiGetHold();
  if (ready)
    AppShowAckDialog(0, 0x1206, kDialogX, kDialogY);
  return 0;
}

LRESULT RebellionApplication::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
  {
    case WM_CREATE:
      return OnCreate();
    case WM_DESTROY:
    case WM_CLOSE:
      if (!IsMoviePlaying())
        SetState(kStateConfirmQuit);
      return 0;
    case WM_SETFOCUS:
      if (!m_activeWindow)
        return 0;
      if (m_activationFlags.m_value & kDeactivatedBit)
        return 0;
      SetFocusToActiveWindow(this);
      return 0;
    case WM_KILLFOCUS:
    {
      if (!m_activeWindow)
        return 0;
      if (m_activationFlags.m_value & kDeactivatedBit)
        return 0;
      HWND newFocus = reinterpret_cast<HWND>(wParam);
      if (!IsChild(m_hwnd, newFocus))
        return 0;
      if (newFocus == m_activeWindow->m_hwnd)
        return 0;
      SetFocusToActiveWindow(this);
      return 0;
    }
    case WM_SYSKEYDOWN:
      return 0;
    case WM_ACTIVATEAPP:
      return OnActivateApp(wParam, lParam);
    case WM_QUERYNEWPALETTE:
      return OnQueryNewPalette(wParam, lParam);
    case WM_SIZE:
      if (minwin::GameplayCanvasEnabled() && m_strategicWindow && m_activeWindow == m_strategicWindow)
      {
        const int width = GetSystemMetrics(SM_CXSCREEN);
        const int height = GetSystemMetrics(SM_CYSCREEN);
        SetWindowPos(m_strategicWindow->m_hwnd, nullptr, m_unknownA0, m_unknownA4, width, height,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        if (m_overlayWindow)
          SetWindowPos(m_overlayWindow->m_hwnd, nullptr, m_unknownA0 + (width - m_screenWidth) / 2, m_unknownA4,
                       m_screenWidth, m_screenHeight, SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
      }
      break;
    case WM_COMMAND:
    {
      UINT forwarded = WM_COMMAND;
      const uint32_t id = wParam & 0xffff;
      if (id > kFirstAcceleratorId && id < kEndAcceleratorId)
        forwarded = kMsgAccelerator;
      if (!m_activeWindow)
        return 0;
      return m_activeWindow->WindowProc(forwarded, wParam, lParam);
    }
    case WM_SYSCOMMAND:
      if (wParam == kSysCommandSizeMove && m_activeWindow && m_activeWindow == m_tacticalWindow)
      {
        SendMessageA(m_tacticalWindow->m_hwnd, WM_KEYDOWN, 0x79, 0x79);
        return 0;
      }
      return CoolWindow::WindowProc(message, wParam, lParam);
    case WM_PALETTECHANGED:
      NotifyMoviePaletteChanged(reinterpret_cast<HWND>(wParam));
      if (RealizeCurrentPalette())
        InvalidateRect(m_hwnd, nullptr, FALSE);
      return 0;
    case kMsgChildDone:
    case kMsgMovieDone:
      return OnChildDone(message, wParam, lParam);
    case 0x40c:
      if (m_state != kStateLobbyHost && m_state != kStateLobbyJoin)
        return 0;
      if (!m_activeWindow)
        return 0;
      SendMessageA(m_activeWindow->m_hwnd, message, wParam, lParam);
      return 0;
    case kMsgDestroyWindow:
    {
      auto* window = reinterpret_cast<CoolWindow*>(lParam);
      if (!window)
        return 0;
      DestroyWindow(window->m_hwnd);
      DeleteWindowObject(window);
      return 0;
    }
    case kMsgDialogAnswer:
      return OnDialogAnswer(wParam);
    case kMsgDialogEnded:
      if (wParam == 0x47c || wParam == 0x47d)
        UiEndPause();
      return 0;
    case kMsgDeactivated:
    case kMsgActivated:
      if (!m_activeWindow)
        return 0;
      SendMessageA(m_activeWindow->m_hwnd, message, 0, 0);
      return 0;
  }
  return CoolWindow::WindowProc(message, wParam, lParam);
}

#ifdef REB_STANDALONE
bool RebSimRequested();
int RebSimRun(RebellionApplication* app);
#endif

int RunApplication(reb::native::GameContext& context,
                   HINSTANCE instance,
                   HINSTANCE previousInstance,
                   LPSTR commandLine,
                   int showCommand)
{
  WPARAM exitCode = 0;
  struct ApplicationSemaphore
  {
    HANDLE handle;
    ~ApplicationSemaphore()
    {
      if (handle)
        CloseHandle(handle);
    }
  };
  ApplicationSemaphore semaphore{CreateSemaphoreA(nullptr, 0, 1, g_appClassName)};
  const DWORD error = GetLastError();
  if (!semaphore.handle || error == ERROR_ALREADY_EXISTS || error == ERROR_INVALID_HANDLE ||
      !CheckDisplayRequirements(previousInstance, kScreenWidth, kScreenHeight, 0x100))
  {
    return 1;
  }
  ChangeToInstalledLocation();
  InitCdAndVersionCheck();
  SetLocaleFromEnvironment();
  InitStringLoader();
  uint32_t flags = 0;
  if (commandLine)
  {
    const int length = static_cast<int>(strlen(commandLine));
    for (int i = 0; i < length; ++i)
    {
      if (commandLine[i] == '-')
      {
        switch (commandLine[i + 1])
        {
          case 'L':
          case 'l':
            flags |= kAppFlag100;
            break;
          case 'N':
          case 'n':
            flags |= kAppNoAutosave;
            break;
          case 'W':
          case 'w':
            flags |= kAppWindowed;
            break;
        }
        ++i;
      }
    }
  }
  flags |= kAppNoAutosave;
  int dataFound = 1;
  if (!(flags & 0x1000))
    dataFound = InstalledLocationHasBigData();
  if (!dataFound)
  {
    ShowCdNotFoundError();
    CommCleanUp();
    return 0;
  }
  const std::string line = StringFromText(commandLine);
  RebellionApplication* app = &context.CreateApplication(instance, flags, line);
  app->SetScreenSize(kScreenWidth, kScreenHeight);
#ifdef REB_STANDALONE
  if (RebSimRequested())
  {
    const int result = RebSimRun(app);
    DestroyBattleFlow();
    context.ReleaseApplication();
    return result;
  }
#endif
  exitCode = app->Run(showCommand, 1);
  context.ReleaseApplication();
  return static_cast<int>(exitCode);
}

void ChangeToInstalledLocation()
{
  const std::string key = BuildGameRegistryKey();
  char* installed = reinterpret_cast<char*>(ReadRegistryValue(key.c_str(), g_installedLocationName));
  if (!installed)
    return;
  const int drive = toupper(static_cast<unsigned char>(*installed));
  if (static_cast<unsigned>(drive - 0x40) > 1)
    _chdrive(drive - 0x40);
  _chdir(installed + 2);
  operator delete(installed);
}

int InstalledLocationHasBigData()
{
  int found = 0;
  const std::string key = BuildGameRegistryKey();
  char* tooBig = reinterpret_cast<char*>(ReadRegistryValue(key.c_str(), g_tooBigLocationName));
  if (!tooBig)
    return found;
  char drive[0x104] = {};
  strncpy(drive, tooBig, 3);
  if (GetDriveTypeA(drive) == DRIVE_FIXED)
  {
    std::string path;
    const std::string dataFile("MDATA\\MDATA.000");
    const std::string separator(kBackslash);
    const std::string base = StringFromText(tooBig);
    path += base;
    path += separator;
    path += dataFile;
    WIN32_FIND_DATAA findData;
    HANDLE handle = FindFirstFileA(path.c_str(), &findData);
    found = handle != INVALID_HANDLE_VALUE;
    if (found)
      FindClose(handle);
  }
  operator delete(tooBig);
  return found;
}

const char* g_alreadyRunningMessage = nullptr;
const char* g_displayDriverName = nullptr;
const char* g_displayTooSmallMessage = nullptr;
const char* g_noMouseMessage = nullptr;
const int16_t g_resourceStringGroup = 1;
int g_firstActivationPending{};
int g_stringLoaderInstalled{};
