// MultiPlayerWindow, part 1: construction, the window procedure, the command handler and the creation of the pages'
// texts and controls. The pages and the network handling are in multiplayer_window_pages.cpp and
// multiplayer_lobby.cpp.
#include "multiplayer/multiplayer_window.h"

#include "app/app_control_cmds.h"
#include "app/rebapp_globals.h"
#include "core/checked_new.h"
#include "core/utility_classes.h"
#include "gfx/dib_bitmap.h"
#include "multiplayer/comm_mgr.h"
#include "tactical/comm2.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_string_field.h"
#include "ui/text_scroll_field.h"

void RegisterStrategicFactoriesOnce(); // 0x41e550

extern uint32_t g_lobbyProviderItemWidth; // 0x6b1244 set to 0x15f by the static initializer 0x40ec40
extern int g_flowRangeSpan; // 0x6b1240 width of the session list items (computed by 0x40d780)

extern const RECT g_rectProviderList; // 0x658668
extern const RECT g_rectSideAClick; // 0x6586a8
extern const RECT g_rectSideBClick; // 0x6586b8

namespace
{

constexpr uint32_t kLobbyStyle = 0x44000000;
constexpr uint32_t kApplicationGuid[4] = {0x8dec0d67, 0x11d0dcf5, 0x2000edaf, 0x7e7c9daf};
constexpr char kCancelTrace[] = "MultiPlayerWindow::DoCancelMultiPlayer(): About to call PlayerQuitSession()!\n";
constexpr UINT kStartedMessage = 0x40c;
constexpr int kNoticeDialogMessage = 0x46e;
constexpr uint32_t kOverlayColor = 0x200ff00;

constexpr uint32_t kPlayerStateConnected = 7;
constexpr int kStateMainMenu = 3;
constexpr int kStateLobbyLaunched = 0xe;
constexpr int kStateLoadGame = 0xc;
constexpr int kStateStartGame = 7;

constexpr RECT kProviderIntro = {0x8a, 0x4f, 500, 0x73};
constexpr RECT kProviderQuestion = {0x8a, 0x119, 500, 0x130};
constexpr RECT kJoinIntro = {0x79, 0x58, 0x1d9, 0x88};
constexpr RECT kJoinNames = {0x79, 0xf8, 0x1d9, 0x112};
constexpr RECT kHostNames = {0x79, 0xf8, 0x1d9, 0x129};
constexpr RECT kSetupTitle = {0x8e, 0x4a, 0x179, 0x67};
constexpr RECT kSetupSubtitle = {0x8e, 0x89, 0x178, 0xa6};
constexpr RECT kSetupSideA = {0xb2, 0xcd, 0x11e, 0xe7};
constexpr RECT kSetupSideB = {0x162, 0xcd, 0x1ce, 0xe7};
constexpr RECT kSetupChatLabel = {0x8f, 0x107, 0xd7, 0x11c};

CoolStrobeButton* MultiPlayerWindow::* const kButtonMembers[] = {
  &MultiPlayerWindow::m_backButton,     &MultiPlayerWindow::m_nextButton,     &MultiPlayerWindow::m_startButton,
  &MultiPlayerWindow::m_joinModeButton, &MultiPlayerWindow::m_hostModeButton, &MultiPlayerWindow::m_sideButtonA,
  &MultiPlayerWindow::m_sideButtonB,    &MultiPlayerWindow::m_sizeButton1,    &MultiPlayerWindow::m_sizeButton2,
  &MultiPlayerWindow::m_sizeButton3,    &MultiPlayerWindow::m_loadButton,     &MultiPlayerWindow::m_typeButton1,
  &MultiPlayerWindow::m_typeButton2,
};

void DeleteProviderData(CoolDragList* list)
{
  for (CoolListItem* item = list->m_items->First(); item; item = item->Next())
    if (item->m_userPointer)
      delete reinterpret_cast<ServiceProviderInfo*>(item->m_userPointer);
}

void DeleteSessionData(CoolDragList* list)
{
  for (CoolListItem* item = list->m_items->First(); item; item = item->Next())
    if (item->m_userPointer)
      delete reinterpret_cast<SessionInfo*>(item->m_userPointer);
}

void DrawPageText(MultiPlayerWindow* window, UINT stringId, const RECT& area, UINT format)
{
  window->DrawTextResource(stringId, area, 6, kOverlayColor, format);
}

void LoadAndDraw(MultiPlayerWindow* window, uint32_t overlayId, void (*draw)(MultiPlayerWindow*))
{
  DibBitmap* bitmap = window->LoadBitmapCopy(overlayId);
  if (window->AddOverlay(bitmap, overlayId) != overlayId)
    return;
  if (window->BeginOffscreen(bitmap) == 0)
    return;
  draw(window);
  window->EndOffscreen();
}

void DrawProviderPage(MultiPlayerWindow* window)
{
  DrawPageText(window, 0x1106, kProviderIntro, 0x15);
  DrawPageText(window, 0x1107, kProviderQuestion, 0x15);
}

void DrawJoinPage(MultiPlayerWindow* window)
{
  DrawPageText(window, 0x110b, kJoinIntro, 0x25);
  DrawPageText(window, 0x110d, kJoinNames, 0x25);
}

void DrawHostPage(MultiPlayerWindow* window)
{
  DrawPageText(window, 0x110b, kJoinIntro, 0x25);
  DrawPageText(window, 0x110f, kHostNames, 0x25);
}

void DrawSetupPage(MultiPlayerWindow* window)
{
  DrawPageText(window, 0x1113, kSetupTitle, 0x25);
  DrawPageText(window, 0x1114, kSetupSubtitle, 0x25);
  DrawPageText(window, 0x1117, kSetupSideA, 4);
  DrawPageText(window, 0x1118, kSetupSideB, 4);
  DrawPageText(window, 0x1119, kSetupChatLabel, 0x22);
}

} // namespace

// ---- construction -----------------------------------------------------------------------------------------------

MultiPlayerWindow::MultiPlayerWindow(HINSTANCE instance,
                                     int x,
                                     int y,
                                     int width,
                                     int height,
                                     CoolWindow* parent,
                                     uint32_t stringSlot,
                                     uint32_t bitmapSlot,
                                     uint32_t backgroundId,
                                     uint32_t childId,
                                     uint32_t style,
                                     uint32_t flags,
                                     int side,
                                     int galaxySize)
: CoolGameWindow(instance,
                 x,
                 y,
                 width,
                 height,
                 parent,
                 stringSlot,
                 bitmapSlot,
                 backgroundId,
                 childId,
                 style,
                 flags,
                 0,
                 0)
{
  m_style = kLobbyStyle;
  m_lastPollTime = timeGetTime();
  m_lastPlayerCount = 0;
  if (CommLaunchedFromLobby() == 0)
  {
    m_isHost = 1;
  }
  else
  {
    m_isHost = CommLobbyIsHost();
  }
  AppSetUnknown138(m_isHost);
  for (int slot = 1; slot <= 6; ++slot)
    g_lobbySlotTable[slot] = 0;
  g_lobbySlotPollTime = timeGetTime();
  m_ready = 0;
  m_closing = 0;
  m_side = side;
  m_gameSeed = 0;
  m_gameNameField = nullptr;
  m_hostNameField = nullptr;
  m_joinNameField = nullptr;
  m_chatInput = nullptr;
  m_chatLog = nullptr;
  m_providerList = nullptr;
  m_sessionList = nullptr;
  m_selectedProvider = nullptr;
  m_selectedSession = nullptr;
  for (auto member : kButtonMembers)
    this->*member = nullptr;
  m_galaxySize = galaxySize;
  m_gameType = 1;
  m_page = kLobbyPageProvider;
  g_isNetworkGame = 1;
}

MultiPlayerWindow::~MultiPlayerWindow()
{
  m_ready = 0;
  m_closing = 1;
  KillTimer(m_hwnd, kLobbyTimerSessions);
  KillTimer(m_hwnd, kLobbyTimerNetwork);
  if (m_parent)
    CommSetNotifyWindow(m_parent->m_hwnd);
  delete m_gameNameField;
  delete m_hostNameField;
  delete m_joinNameField;
  delete m_chatInput;
  delete m_chatLog;
  if (m_providerList)
  {
    DeleteProviderData(m_providerList);
    m_providerList->RemoveAll();
    delete m_providerList;
  }
  if (m_sessionList)
  {
    DeleteSessionData(m_sessionList);
    m_sessionList->RemoveAll();
    delete m_sessionList;
  }
  delete m_selectedProvider;
  delete m_selectedSession;
}

// ---- window procedure and commands ------------------------------------------------------------------------------

LRESULT MultiPlayerWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
  LRESULT result = 0;
  if (m_ready == 1)
  {
    ProcessNetwork();
    if (m_page == kLobbyPageSetup && m_isHost == 1)
      PollSlotTable();
  }
  if (message < 0x202)
  {
    if (message == WM_LBUTTONDOWN)
    {
      if (m_page == kLobbyPageProvider)
        return result;
      if (m_page != kLobbyPageSetup)
        return result;
      if (m_isHost == 0)
        return result;
      POINT point = {static_cast<short>(lParam), static_cast<short>(lParam >> 16)};
      if (PtInRect(&g_rectSideAClick, point))
      {
        SetGameType(1, 0, 1);
        return result;
      }
      POINT second = {static_cast<short>(lParam), static_cast<short>(lParam >> 16)};
      if (PtInRect(&g_rectSideBClick, second))
        SetGameType(2, 0, 1);
      return result;
    }
    if (message == WM_TIMER)
    {
      if (wParam == kLobbyTimerNetwork)
      {
        if (m_page != kLobbyPageSetup)
          return result;
        if ((g_lobbyFlags & 1) != 0)
          return result;
        if (m_isHost == 0)
          return result;
        MpSlotTableMsg* ping = NewOrNull<MpSlotTableMsg>();
        if (!ping)
          return result;
        SendToPeer(ping);
        return result;
      }
      if (wParam != kLobbyTimerSessions)
        return result;
      if (m_selectedSession || m_page != kLobbyPageJoin)
      {
        KillTimer(m_hwnd, kLobbyTimerSessions);
        return result;
      }
      if (m_closing != 0)
        return result;
      if (CommIsEnumerating() != 0)
        return result;
      RefreshSessionList();
      return result;
    }
    return CoolGameWindow::WindowProc(message, wParam, lParam);
  }
  if (message == kFieldReturnMessage)
  {
    PrintChatLine(nullptr);
    return result;
  }
  if (message != kStartedMessage)
    return CoolGameWindow::WindowProc(message, wParam, lParam);
  switch (wParam)
  {
    case kCommPlayerLeft:
    case 2:
      UpdateStartButtons();
      return result;
    case kCommPlayerLost:
      UpdateStartButtons();
      break;
    case kCommSessionEnded:
      if (CommGetPlayerCount() > 1)
        return result;
      break;
    default:
      return result;
  }
  UpdateStartButtons();
  CommClearLobbyLaunch();
  CommCleanUp();
  OnCreate1();
  if (CommLaunchedFromLobby() == 0)
  {
    ReturnToProviderPage();
  }
  else
  {
    CommTrace(kCancelTrace);
    CommSendPlayerQuit();
    m_closing = 1;
    KillTimer(m_hwnd, kLobbyTimerSessions);
    KillTimer(m_hwnd, kLobbyTimerNetwork);
    CommCleanUp();
    g_isNetworkGame = 0;
    const int launched = CommLaunchedFromLobby();
    m_closing = 1;
    AppSetState(launched == 0 ? kStateMainMenu : kStateLobbyLaunched);
  }
  AppShowNoticeDialog(kNoticeDialogMessage, 0, 0x1205, 0x190f, 500, 0x50);
  return result;
}

void MultiPlayerWindow::OnCommand(int a, int b, int c)
{
  ProcessNetwork();
  switch (static_cast<int16_t>(a))
  {
    case 100:
      if (m_page == kLobbyPageProvider)
      {
        m_closing = 1;
        AppSetState(kStateMainMenu);
        return;
      }
      if (m_page > kLobbyPageProvider && m_page < kLobbyPageSetup + 1)
        ReturnToProviderPage();
      break;
    case 0x65:
      if (m_page == kLobbyPageProvider)
      {
        RememberSelectedProvider();
        if (m_isHost != 0)
        {
          ShowHostPage();
          return;
        }
        ShowJoinPage();
        return;
      }
      if (m_page == kLobbyPageJoin || m_page == kLobbyPageHost)
        ShowSetupPage();
      break;
    case 0x66:
    {
      m_closing = 1;
      KillTimer(m_hwnd, kLobbyTimerSessions);
      AppSetGameParams(m_side, 2, m_galaxySize, m_gameSeed, m_gameType, kPlayerStateConnected);
      AppSetUnknown138(m_isHost);
      CommCloseSessionToNewPlayers(this);
      if (m_isHost != 0)
      {
        SendSettings(1);
        const DWORD start = m_lastPollTime;
        DWORD elapsed = timeGetTime() - start;
        while (elapsed < 7000 && CommAllPacketsAcknowledged() == 0)
          elapsed = timeGetTime() - start;
      }
      KillTimer(m_hwnd, kLobbyTimerNetwork);
      m_closing = 1;
      AppSetState(kStateStartGame);
      break;
    }
    case 0x67:
      CommTrace(kCancelTrace);
      CommSendPlayerQuit();
      m_closing = 1;
      KillTimer(m_hwnd, kLobbyTimerSessions);
      KillTimer(m_hwnd, kLobbyTimerNetwork);
      CommCleanUp();
      g_isNetworkGame = 0;
      {
        const int launched = CommLaunchedFromLobby();
        m_closing = 1;
        AppSetState(launched != 0 ? kStateLobbyLaunched : kStateMainMenu);
      }
      return;
    case 0x68:
      AppSetUnknown138(0);
      if (m_isHost != 0)
      {
        m_isHost = 0;
        if (m_hostModeButton && m_joinModeButton)
        {
          m_hostModeButton->ClearStateFlags(kButtonChecked);
          m_joinModeButton->SetStateFlags(kButtonChecked);
        }
      }
      break;
    case 0x69:
      AppSetUnknown138(1);
      if (m_isHost != 1)
      {
        m_isHost = 1;
        if (m_hostModeButton && m_joinModeButton)
        {
          m_hostModeButton->SetStateFlags(kButtonChecked);
          m_joinModeButton->ClearStateFlags(kButtonChecked);
        }
      }
      break;
    case 0x6a:
      SetSide(2, 0, 1);
      return;
    case 0x6b:
      SetSide(1, 0, 1);
      return;
    case 0x6c:
      SetGalaxySize(1, 0, 1);
      return;
    case 0x6d:
      SetGalaxySize(2, 0, 1);
      return;
    case 0x6e:
      SetGalaxySize(3, 0, 1);
      return;
    case 0x6f:
      m_closing = 1;
      AppSetState(kStateLoadGame);
      return;
    case 0x70:
      SetGameType(1, 0, 1);
      return;
    case 0x71:
      SetGameType(2, 0, 1);
      return;
  }
}

// ---- the first page's session, the page texts and the controls ---------------------------------------------------

void MultiPlayerWindow::OnCreate1()
{
  for (int slot = 1; slot <= 6; ++slot)
    g_lobbySlotTable[slot] = 0;
  g_lobbySlotPollTime = timeGetTime();
  {
    RandomGenerator generator;
    m_gameSeed = generator.Seed(0);
  }
  RegisterStrategicFactoriesOnce();
  StringMap settings = GetNetworkSettings();
  CommInit(kApplicationGuid[0], kApplicationGuid[1], kApplicationGuid[2], kApplicationGuid[3], &settings,
           m_parent->m_hwnd, 2, 1, 1, 1, 10000, 0);
  m_closing = 0;
  CommSetSessionFlags(0);
}

void MultiPlayerWindow::OnCreate2()
{
  LoadAndDraw(this, kLobbyPageProvider, DrawProviderPage);
  LoadAndDraw(this, kLobbyPageJoin, DrawJoinPage);
  LoadAndDraw(this, kLobbyPageHost, DrawHostPage);
  LoadAndDraw(this, kLobbyPageSetup, DrawSetupPage);
}

const RECT g_rectProviderList = {144, 125, 500, 222};
const RECT g_rectSideAClick = {178, 205, 286, 231};
const RECT g_rectSideBClick = {354, 205, 462, 231};
uint32_t g_lobbyProviderItemWidth{0x15f};
