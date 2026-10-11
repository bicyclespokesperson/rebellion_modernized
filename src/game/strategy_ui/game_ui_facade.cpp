#include "strategy_ui/game_ui_facade.h"
#include <string>

#include "app/game_session.h"
#include "app/rebapp_globals.h"
#include "core/utility_classes.h"
#include "events/game_clock_hooks.h"
#include "events/game_step_driver.h"
#include "orders/order_list.h"
#include "strategy_ui/advisor_droid.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/manufacture_order_window.h"

// 0x4fcf20: the menu commands that apply to `selection` for `player`.
void BuildAvailableOrdersViaManager(int side, ObjectKeyValueSet* team, EventKindSet* orders);

// ---- Game session ------------------------------------------------------------------------------------------

void UiCreateGameSession(int opponentType, int humanSide)
{
  CreateGameSession(opponentType, humanSide);
}

int UiAdvanceTurn(int* roundComplete)
{
  int result = 1;
  GameSession* session = GetGameSession();
  if (session)
    result = session->AdvanceTurn(roundComplete);
  return result;
}

int UiReadSession(istream* in)
{
  int result = 0;
  GameSession* session = GetGameSession();
  if (session)
    return session->Read(in);
  CreateEmptyGameSession();
  if (GetGameSession())
    result = UiReadSession(in);
  return result;
}

int UiWriteSession(ostream* out)
{
  int result = 0;
  GameSession* session = GetGameSession();
  if (session)
    result = session->Write(out);
  return result;
}

void UiSetSessionViewReady(int ready)
{
  GameSession* session = GetGameSession();
  if (session)
    session->m_viewReady = ready;
}

int UiSessionRandom(int limit)
{
  int result = 0;
  if (GetGameSession())
    result = g_sessionRandom.Next(limit);
  return result;
}

int UiGetHumanSide()
{
  int result = 0;
  GameSession* session = GetGameSession();
  if (session)
    result = session->GetHumanSide();
  return result;
}

int UiIsOpponentAI()
{
  int result = 0;
  GameSession* session = GetGameSession();
  if (session)
    result = session->IsOpponentAI();
  return result;
}

void UiHumanCommand(int command, void* selection, void* key, const void* argument)
{
  GameSession* session = GetGameSession();
  if (session)
    session->HumanCommand(command, selection, key, argument);
}

void UiHumanReceiveMessage(PlayerMessage* message, int flag)
{
  GameSession* session = GetGameSession();
  if (session)
    session->HumanReceiveMessage(message, flag);
}

void UiHumanAdvisorNotify(const ObjectKey* key)
{
  GameSession* session = GetGameSession();
  if (session)
    session->HumanAdvisorNotify(key);
}

void UiHumanSetVote(int choice, int timed, PlayerMessage* reply)
{
  GameSession* session = GetGameSession();
  if (session)
    session->HumanSetVote(choice, timed, reply);
}

void UiHumanConfirmVote(PlayerMessage* reply)
{
  GameSession* session = GetGameSession();
  if (session)
    session->HumanConfirmVote(reply);
}

void UiRequestRedraw(int value)
{
  GameSession* session = GetGameSession();
  if (session)
    session->RequestRedraw(value);
}

void UiRequestRedrawAlt(int value)
{
  GameSession* session = GetGameSession();
  if (session)
    session->RequestRedrawAlt(value);
}

void UiRouteMessage(MessageBase* message, int senderSide)
{
  GameSession* session = GetGameSession();
  if (session)
    session->RouteMessage(message, senderSide);
  else
    delete message;
}

void UiDeliverQueuedMessage(int id)
{
  GameSession* session = GetGameSession();
  if (session)
    session->DeliverQueuedMessage(id);
}

int UiAdvancePhase()
{
  int result = 0;
  GameSession* session = GetGameSession();
  if (session)
    result = session->AdvancePhase();
  return result;
}

int UiProcessAllMessages()
{
  int result = 1;
  GameSession* session = GetGameSession();
  if (session)
    result = session->ProcessAllMessages();
  return result;
}

void UiUpdateSession()
{
  GameSession* session = GetGameSession();
  if (session)
    session->Update();
}

int UiAreAllSynced()
{
  int result = 0;
  GameSession* session = GetGameSession();
  if (session)
    result = session->AreAllSynced();
  return result;
}

void UiBeginVoteAll(int id, ObjectKey* key)
{
  GameSession* session = GetGameSession();
  if (session)
    session->BeginVoteAll(id, key);
}

void UiExchangeVotesTimed(int* allVoted, int* unanimous, GameTime* timeA, GameTime* timeB)
{
  GameSession* session = GetGameSession();
  if (session)
    session->ExchangeVotesTimed(allVoted, unanimous, timeA, timeB);
}

void UiResetVoteAll(int id, ObjectKey* key)
{
  GameSession* session = GetGameSession();
  if (session)
    session->ResetVoteAll(id, key);
}

void UiExchangeVotes(int* allConfirmed)
{
  GameSession* session = GetGameSession();
  if (session)
    session->ExchangeVotes(allConfirmed);
}

void UiRefreshAllPlayers()
{
  GameSession* session = GetGameSession();
  if (session)
    session->RefreshAllPlayers();
}

void UiComputeAllAdvisorChoices()
{
  GameSession* session = GetGameSession();
  if (session)
    session->ComputeAllAdvisorChoices();
}

int UiSyncAllPlayers(unsigned syncId)
{
  int result = 1;
  GameSession* session = GetGameSession();
  if (session)
    result = session->SyncAllPlayers(syncId);
  return result;
}

int UiResetAllPlayerFlags()
{
  int result = 1;
  GameSession* session = GetGameSession();
  if (session)
    result = session->ResetAllPlayerFlags();
  return result;
}

AvlNode* UiGetFirstMessage()
{
  GameSession* session = GetGameSession();
  if (session)
    return session->GetFirstMessage();
  return nullptr;
}

int UiHumanHasPending()
{
  int result = 0;
  GameSession* session = GetGameSession();
  if (session)
    result = session->HumanHasPending();
  return result;
}

int UiNextId()
{
  int result = 0;
  GameSession* session = GetGameSession();
  if (session)
    result = session->NextId();
  return result;
}

void UiSetHumanUiTarget(int a, int b)
{
  GameSession* session = GetGameSession();
  if (session)
    session->SetHumanUiTarget(a, b);
}

void UiHumanRefreshMenus(EventKindSet* commands, int requestUpdate)
{
  GameSession* session = GetGameSession();
  if (session)
  {
    session->HumanRefreshMenus(reinterpret_cast<intptr_t>(commands));
    if (requestUpdate)
      session->HumanRequestUpdate();
  }
}

void UiHumanRefreshOrderMenus(EventKindSet* commands)
{
  GameSession* session = GetGameSession();
  if (session)
    session->HumanRefreshOrderMenus(reinterpret_cast<intptr_t>(commands));
}

IdAvlTree* UiGetHumanOrderTree()
{
  GameSession* session = GetGameSession();
  IdAvlTree* result = nullptr;
  if (session)
    result = session->GetHumanOrderTree();
  return result;
}

void UiDeleteHumanMessage(int id)
{
  GameSession* session = GetGameSession();
  if (session)
    session->DeleteHumanMessage(id);
}

void UiSetHumanDispatchFlag(int on)
{
  GameSession* session = GetGameSession();
  if (session)
    session->SetHumanDispatchFlag(on);
}

void UiSetHumanUiFlag4000(int on)
{
  GameSession* session = GetGameSession();
  if (session)
    session->SetHumanUiFlag4000(on);
}

void UiSetHumanStateValue(int key, int value)
{
  GameSession* session = GetGameSession();
  if (session)
    session->SetHumanStateValue(key, value);
}

unsigned UiGetHumanStateValue(int key)
{
  unsigned result = 0;
  GameSession* session = GetGameSession();
  if (session)
    result = session->GetHumanStateValue(key);
  return result;
}

int UiGetHumanDefaultState()
{
  int result = 0;
  GameSession* session = GetGameSession();
  if (session)
    result = session->GetHumanDefaultState();
  return result;
}

void UiSetHumanDefaultState(int state)
{
  GameSession* session = GetGameSession();
  if (session)
    session->SetHumanDefaultState(state);
}

void UiQueueHumanPopup(intptr_t a, int b)
{
  GameSession* session = GetGameSession();
  if (session)
    session->QueueHumanPopup(a, b);
}

void UiHumanSetAdvisorMask(unsigned mask)
{
  GameSession* session = GetGameSession();
  if (session)
    session->HumanSetAdvisorMask(mask);
}

int UiBeginSync(unsigned requested)
{
  int result = 0;
  GameSession* session = GetGameSession();
  if (session)
    result = session->BeginSync(requested);
  return result;
}

void UiAckSync(int side)
{
  GameSession* session = GetGameSession();
  if (session)
    session->AckSync(side);
}

void UiFinishSync(PlayerMessage* message)
{
  GameSession* session = GetGameSession();
  if (session)
    session->FinishSync(message);
}

void UiSetSyncTargets(unsigned target)
{
  GameSession* session = GetGameSession();
  if (session)
    session->SetSyncTargets(target);
}

int UiHumanDispatchIdle()
{
  int result = 1;
  GameSession* session = GetGameSession();
  if (session)
    result = session->HumanDispatchIdle();
  return result;
}

int UiHaveSameSyncValue()
{
  int result = 1;
  GameSession* session = GetGameSession();
  if (session)
    result = session->HaveSameSyncValue();
  return result;
}

int UiHumanNextAdvice()
{
  GameSession* session = GetGameSession();
  if (session)
    return session->HumanNextAdvice();
  return 0;
}

void UiClearHumanAlerts(unsigned mask)
{
  GameSession* session = GetGameSession();
  if (session)
    session->ClearHumanAlerts(mask);
}

void UiHumanBeginHold()
{
  GameSession* session = GetGameSession();
  if (session)
    session->HumanBeginHold();
}

void UiHumanEndHold()
{
  GameSession* session = GetGameSession();
  if (session)
    session->HumanEndHold();
}

void UiSendNotificationToSide(int side, intptr_t argument)
{
  GameSession* session = GetGameSession();
  if (session)
    session->SendNotificationToSide(side, argument);
}

// ---- Game step driver ----------------------------------------------------------------------------------------

void UiAddManagerNotice(void* notice)
{
  if (notice)
    AddManagerNotice(static_cast<IdNode*>(notice));
}

unsigned UiRaisePauseTo(unsigned tick)
{
  if (GameStepDriver* driver = GetGameStepDriver())
    return driver->RaisePauseTo(tick);
  return tick;
}

int UiBeginPause()
{
  if (GameStepDriver* driver = GetGameStepDriver())
    return driver->BeginPause();
  return 0;
}

void UiClearHold()
{
  if (GameStepDriver* driver = GetGameStepDriver())
    driver->ClearHold();
}

int UiEndPause()
{
  int result = 0;
  if (GameStepDriver* driver = GetGameStepDriver())
    result = driver->EndPause();
  return result;
}

int UiIsPauseTick()
{
  int result = 1;
  if (GameStepDriver* driver = GetGameStepDriver())
    result = driver->IsPauseTick();
  return result;
}

int UiGetHold()
{
  int result = 1;
  if (GameStepDriver* driver = GetGameStepDriver())
    result = driver->m_hold;
  return result;
}

void UiSetSetupDone()
{
  if (GameStepDriver* driver = GetGameStepDriver())
    driver->m_setupDone = 1;
}

void UiBuildAvailableOrders(int player, ObjectKeyValueSet* selection, EventKindSet* commands)
{
  BuildAvailableOrdersViaManager(player, selection, commands);
}

// ---- Galaxy view ---------------------------------------------------------------------------------------------

namespace
{
constexpr UINT kOpenModalMessage = 0x468;
constexpr UINT kCommandMessage = WM_COMMAND;
constexpr uint32_t kSetDisplayModeCommand = 0x140;
constexpr uint32_t kRefinedMaterialsCounter = 0x14d;
constexpr uint32_t kRawMaterialsCounter = 0x14c;
constexpr uint32_t kMaintenanceCounter = 0x14e;
} // namespace

void UiOnManagerCharacterChanged(const ObjectKey* manager, const ObjectKey* character)
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->OnManagerCharacterChanged(manager, character);
}

void UiOnManagerStatusTextChanged(const ObjectKey* manager, std::string* text)
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->OnManagerStatusTextChanged(manager, text);
}

void UiOnManagerProductionChanged(const ObjectKey* manager, int unused)
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->OnManagerProductionChanged(manager, unused);
}

void UiOnManagerProgressChanged(const ObjectKey* manager, int position)
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->OnManagerProgressChanged(manager, position);
}

void UiBroadcastObjectChanged(const ObjectKey* key, GameObjNotifier* object)
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->BroadcastObjectChanged(key, object);
}

void UiBroadcastObjectMoved(const ObjectKey* key,
                            GameObjNotifier* object,
                            const ObjectKey* otherKey,
                            GameObjNotifier* otherObject)
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->BroadcastObjectMoved(key, object, otherKey, otherObject);
}

void UiBroadcastObjectUpdated(const ObjectKey* key, GameObjNotifier* object)
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->BroadcastObjectUpdated(key, object);
}

void UiSetRefinedMaterials(int value, int unused)
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->SetResourceCounter(kRefinedMaterialsCounter, value, unused);
}

void UiSetRawMaterials(int value, int unused)
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->SetResourceCounter(kRawMaterialsCounter, value, unused);
}

void UiSetMaintenance(int value, int unused)
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->SetResourceCounter(kMaintenanceCounter, value, unused);
}

void UiOnSystemChangedKind1(const ObjectKey* system, int a, int b)
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->OnSystemChanged(system, 1, a, b);
}

void UiOnSystemChangedKind2(const ObjectKey* system, int a, int b)
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->OnSystemChanged(system, 2, a, b);
}

void UiBeginTargeting(void* command)
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->BeginTargeting(command);
}

void UiBeginRename(TextMessage* request)
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->BeginRename(request);
}

void UiSetDateAndSpeed(int day, int speed)
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->SetDateAndSpeed(day, speed, 0);
}

void UiOpenManufactureWindow(ManufactureOrderBase* order, uint32_t unused)
{
  ManufactureOrderWindow::Open(order, unused);
}

void UiPlayDroidScript(uint32_t resourceId, uint32_t x, uint32_t y, int alternateSet)
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->PlayDroidScript(resourceId, x, y, alternateSet);
}

void UiPlaySecondDroidScript(uint32_t resourceId, uint32_t x, uint32_t y)
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->PlaySecondDroidScript(resourceId, x, y);
}

void UiOpenEncyclopedia(const ObjectKey* object, const NullableKey* type)
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->OpenEncyclopedia(object, type);
}

int UiIsGalaxyViewShown()
{
  int result = 0;
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    result = galaxy->IsShownAndIdle();
  return result;
}

void UiRefreshChildWindows()
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->RefreshChildWindows();
}

void UiOpenType6Window(LPARAM argument)
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->OpenType6Window(argument);
}

void UiOpenType8Window(LPARAM argument)
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->OpenType8Window(argument);
}

void UiForwardToType6Window(int a, int b)
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->ForwardToType6Window(a, b);
}

void UiPostOpenType0dWindow(uint32_t x, int y)
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    PostMessageA(galaxy->m_hwnd, kOpenModalMessage, 0xd, (y << 16) | (x & 0xffff));
}

void UiOpenOrderConfirm(LPARAM argument)
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->OpenOrderConfirm(argument);
}

void UiOpenType1cWindow(LPARAM argument)
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->OpenType1cWindow(argument);
}

void UiOpenObjectStatus(const ObjectKey* key)
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->OpenObjectStatus(key);
}

void UiMarkGalaxyViewUnknownAc()
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->m_unknownAc = 1;
}

void UiSetAlternateDisplayMode(uint32_t flags)
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->SetAlternateDisplayMode(flags);
}

void UiSetPendingMovie(LPARAM movie)
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->SetPendingMovie(movie);
}

void UiRefreshMessageIndex()
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->RefreshMessageIndex();
}

void UiSetDisplayModeTitled(int mode, LPARAM argument, const ResourceStringId* titleStringId)
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->m_titleStringId = *titleStringId;
  UiSetDisplayMode(mode, argument);
}

void UiSetDisplayMode(int mode, LPARAM argument)
{
  GalaxyWindow* galaxy = GetGalaxyWindow();
  if (galaxy)
    SendMessageA(galaxy->m_hwnd, kCommandMessage, (mode << 16) | kSetDisplayModeCommand, argument);
}

void UiClearButtonHighlights(uint32_t mask)
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->ClearButtonHighlights(mask);
}

void UiSetButtonHighlights(uint32_t mask)
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->SetButtonHighlights(mask);
}

void UiResumePendingRequest()
{
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->ResumePendingRequest();
}

// ---- Input hooks ---------------------------------------------------------------------------------------------

// 0x43a200: flags the request that Escape (or a click outside the game's windows) was pressed.

extern HHOOK g_uiKeyboardHook; // 0x6b14ac
extern HHOOK g_uiMouseHook; // 0x6b14b0
extern int g_uiGameInputStarted; // 0x6b14b4
extern int g_uiPausePending; // 0x6b14b8
extern int g_uiInputActive; // 0x6b14bc

LRESULT CALLBACK UiKeyboardHook(int code, WPARAM wParam, LPARAM lParam)
{
  const int notStarted = g_uiGameInputStarted == 0;
  if (code >= 0 && g_uiGameInputStarted == 0 && wParam == VK_ESCAPE && (lParam & 0x80000000))
    AdvisorRequestIdle();
  CallNextHookEx(g_uiKeyboardHook, code, wParam, lParam);
  return notStarted;
}

LRESULT CALLBACK UiMouseHook(int code, WPARAM wParam, LPARAM lParam)
{
  bool handled = false;
  if (code == 0)
  {
    switch (wParam)
    {
      case WM_LBUTTONDOWN:
      case WM_RBUTTONDOWN:
        AdvisorRequestIdle();
        handled = true;
        break;
      case WM_LBUTTONUP:
      case WM_RBUTTONUP:
        handled = true;
        break;
      default:
        handled = false;
    }
  }
  if (!handled && CallNextHookEx(g_uiMouseHook, code, wParam, lParam) == 0)
    return 0;
  return 1;
}

void UiInstallInputHooks()
{
  g_uiKeyboardHook = SetWindowsHookExA(WH_KEYBOARD, UiKeyboardHook, nullptr, 0);
  g_uiMouseHook = SetWindowsHookExA(WH_MOUSE, UiMouseHook, nullptr, 0);
  g_uiGameInputStarted = 0;
  g_uiInputActive = 1;
  if (GalaxyWindow* galaxy = GetGalaxyWindow())
    galaxy->m_optionsRequested = 0;
}

void UiBeginGameInput()
{
  GalaxyWindow* galaxy = GetGalaxyWindow();
  if (galaxy && g_uiGameInputStarted == 0)
  {
    galaxy->StopDroidScripts();
    g_uiGameInputStarted = 1;
  }
  g_uiInputActive = 0;
  if (g_uiPausePending != 0)
  {
    g_uiPausePending = 0;
    AppPromptPause();
  }
}

void UiRemoveInputHooks()
{
  if (g_uiKeyboardHook)
  {
    UnhookWindowsHookEx(g_uiKeyboardHook);
    g_uiKeyboardHook = nullptr;
  }
  if (g_uiMouseHook)
  {
    UnhookWindowsHookEx(g_uiMouseHook);
    g_uiMouseHook = nullptr;
  }
  g_uiInputActive = 0;
  if (g_uiPausePending != 0)
  {
    g_uiPausePending = 0;
    AppPromptPause();
  }
}

int UiGetInputActive()
{
  return g_uiInputActive;
}

void UiSetPausePending(int pending)
{
  g_uiPausePending = pending;
}

HHOOK g_uiKeyboardHook{};
HHOOK g_uiMouseHook{};
ManufactureOrderWindow* g_manufactureOrderWindow{};
NullableKey g_lastFacilityType{};
NullableKey g_lastShipType{};
NullableKey g_lastTroopType{};
int g_uiGameInputStarted{};
int g_uiInputActive{};
int g_uiPausePending{};
