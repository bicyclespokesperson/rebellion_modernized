#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "app/player_controller_decl.h"
#include "core/containers.h"
#include "core/stream_io.h"
#include "gameobj/object_key.h"
#include "platform/win32.h"

class CoolWindow;
struct GameTime;
class ObjectKeyValueSet;
class EventKindSet;
class GameObjNotifier;
class TextMessage;

class ManufactureOrderBase;
struct ResourceStringId;
class NullableKey;

// The game UI facade (0x41cca0-0x41dce0): the global entry points the rest of the game uses to reach the game
// session (0x435a40: GetGameSession) and the galaxy view (0x422ca0: GetGalaxyWindow). Almost every one of them is
// a null-checked forwarder; the return value without a session / view is noted where it is not 0.

// ---- Game session ------------------------------------------------------------------------------------------
void UiCreateGameSession(int opponentType, int humanSide); // 0x41cca0
int UiAdvanceTurn(int* roundComplete); // 0x41cce0 1 without a session
int UiReadSession(istream* in); // 0x41cd10 creates an empty session first if needed
int UiWriteSession(ostream* out); // 0x41cd50
void UiSetSessionViewReady(int ready); // 0x41cd70
int UiSessionRandom(int limit); // 0x41cd80 the session's random generator
int UiGetHumanSide(); // 0x41cdb0
int UiIsOpponentAI(); // 0x41cdd0
void UiHumanCommand(int command, void* selection, void* key, const void* argument); // 0x41cdf0
void UiHumanReceiveMessage(PlayerMessage* message, int flag); // 0x41ce20
void UiHumanAdvisorNotify(const ObjectKey* key); // 0x41ce40
void UiHumanSetVote(int choice, int timed, PlayerMessage* reply); // 0x41ce60
void UiHumanConfirmVote(PlayerMessage* reply); // 0x41ce80
void UiRequestRedraw(int value); // 0x41cea0
void UiRequestRedrawAlt(int value); // 0x41cec0
void UiRouteMessage(MessageBase* message, int senderSide); // 0x41cee0
void UiDeliverQueuedMessage(int id); // 0x41cf00
int UiAdvancePhase(); // 0x41cf20
int UiProcessAllMessages(); // 0x41cf40 1 without a session
void UiUpdateSession(); // 0x41cf60
int UiAreAllSynced(); // 0x41cf80
void UiBeginVoteAll(int id, ObjectKey* key); // 0x41cfa0
void UiExchangeVotesTimed(int* allVoted, int* unanimous, GameTime* timeA, GameTime* timeB); // 0x41cfc0
void UiResetVoteAll(int id, ObjectKey* key); // 0x41cff0
void UiExchangeVotes(int* allConfirmed); // 0x41d010
void UiRefreshAllPlayers(); // 0x41d030
void UiComputeAllAdvisorChoices(); // 0x41d050
int UiSyncAllPlayers(unsigned syncId); // 0x41d070 1 without a session
int UiResetAllPlayerFlags(); // 0x41d0a0 1 without a session
std::vector<uint32_t> UiQueuedMessageIds();
int UiHumanHasPending(); // 0x41d0e0
int UiNextId(); // 0x41d120
void UiSetHumanUiTarget(int a, int b); // 0x41d140
void UiHumanRefreshMenus(EventKindSet* commands, int requestUpdate); // 0x41d160 fills `commands`
void UiHumanRefreshOrderMenus(EventKindSet* commands); // 0x41d190
IdAvlTree* UiGetHumanOrderTree(); // 0x41d1b0
void UiDeleteHumanMessage(int id); // 0x41d1d0
void UiSetHumanDispatchFlag(int on); // 0x41d1f0
void UiSetHumanUiFlag4000(int on); // 0x41d210
void UiSetHumanStateValue(int key, int value); // 0x41d230
unsigned UiGetHumanStateValue(int key); // 0x41d250
int UiGetHumanDefaultState(); // 0x41d270
void UiSetHumanDefaultState(int state); // 0x41d290
void UiQueueHumanPopup(intptr_t a, int b); // 0x41d2b0
void UiHumanSetAdvisorMask(unsigned mask); // 0x41d2d0
int UiBeginSync(unsigned requested); // 0x41d2f0
void UiAckSync(int side); // 0x41d310
void UiFinishSync(PlayerMessage* message); // 0x41d330
void UiSetSyncTargets(unsigned target); // 0x41d350
int UiHumanDispatchIdle(); // 0x41d370 1 without a session
int UiHaveSameSyncValue(); // 0x41d390 1 without a session
int UiHumanNextAdvice(); // 0x41d3b0
void UiClearHumanAlerts(unsigned mask); // 0x41d3d0
void UiHumanBeginHold(); // 0x41d3f0
void UiHumanEndHold(); // 0x41d410
void UiSendNotificationToSide(int side, intptr_t argument); // 0x41dc80

// ---- Game step driver (null outside a game) ------------------------------------------------------------------
void UiAddManagerNotice(void* notice); // 0x41db80
unsigned UiRaisePauseTo(unsigned tick); // 0x41dba0 returns `tick` without a driver
int UiBeginPause(); // 0x41dbc0
void UiClearHold(); // 0x41dbe0
int UiEndPause(); // 0x41dbf0
int UiIsPauseTick(); // 0x41dc10 1 without a driver
int UiGetHold(); // 0x41dc30 1 without a driver
void UiSetSetupDone(); // 0x41dc50
void UiBuildAvailableOrders(int player, ObjectKeyValueSet* selection, EventKindSet* commands); // 0x41dcc0

// Loads the sprite DLL of `side` (ALSPRITE.DLL / EMSPRITE.DLL, slot 9) and, with `loadBriefing`, the briefing DLL
// (ALBRIEF.DLL / EMBRIEF.DLL, slot 0xd); unloads the previous ones when the side changed.
void UiLoadSideSpriteDlls(int side, int loadBriefing); // 0x41daf0
void LoadSideSpriteDlls(int side, int loadBriefing); // 0x42d650
void UnloadSpriteDll(); // 0x42d740
void UnloadBriefingDll(); // 0x42d760

// ---- Galaxy view (GetGalaxyWindow, null outside the strategic game) ---------------------------------------------
void UiOnManagerCharacterChanged(const ObjectKey* manager, const ObjectKey* character); // 0x41d430
void UiOnManagerStatusTextChanged(const ObjectKey* manager, std::string* text); // 0x41d450
void UiOnManagerProductionChanged(const ObjectKey* manager, int unused); // 0x41d470
void UiOnManagerProgressChanged(const ObjectKey* manager, int position); // 0x41d490
void UiBroadcastObjectChanged(const ObjectKey* key, GameObjNotifier* object); // 0x41d4b0
void UiBroadcastObjectMoved(const ObjectKey* key,
                            GameObjNotifier* object,
                            const ObjectKey* otherKey,
                            GameObjNotifier* otherObject); // 0x41d4d0
void UiBroadcastObjectUpdated(const ObjectKey* key, GameObjNotifier* object); // 0x41d500
void UiSetRefinedMaterials(int value, int unused); // 0x41d520
void UiSetRawMaterials(int value, int unused); // 0x41d540
void UiSetMaintenance(int value, int unused); // 0x41d560
void UiOnSystemChangedKind1(const ObjectKey* system, int a, int b); // 0x41d580
void UiOnSystemChangedKind2(const ObjectKey* system, int a, int b); // 0x41d5b0
void UiBeginTargeting(void* command); // 0x41d5e0
void UiBeginRename(TextMessage* request); // 0x41d600
void UiSetDateAndSpeed(int day, int speed); // 0x41d620
void UiOpenManufactureWindow(ManufactureOrderBase* order, uint32_t unused); // 0x41d640
void UiPlayDroidScript(uint32_t resourceId, uint32_t x, uint32_t y, int alternateSet); // 0x41d660
void UiPlaySecondDroidScript(uint32_t resourceId, uint32_t x, uint32_t y); // 0x41d690
void UiOpenEncyclopedia(const ObjectKey* object, const NullableKey* type); // 0x41d6b0
int UiIsGalaxyViewShown(); // 0x41d6d0
void UiRefreshChildWindows(); // 0x41d6f0
void UiOpenType6Window(LPARAM argument); // 0x41d710
void UiOpenType8Window(LPARAM argument); // 0x41d730
void UiForwardToType6Window(int a, int b); // 0x41d750
void UiPostOpenType0dWindow(uint32_t x, int y); // 0x41d770
void UiOpenOrderConfirm(LPARAM argument); // 0x41d7b0
void UiOpenType1cWindow(LPARAM argument); // 0x41d7d0
void UiOpenObjectStatus(const ObjectKey* key); // 0x41d7f0
void UiMarkGalaxyViewUnknownAc(); // 0x41d810
void UiSetAlternateDisplayMode(uint32_t flags); // 0x41d830
void UiSetPendingMovie(LPARAM movie); // 0x41d850
void UiRefreshMessageIndex(); // 0x41d870
// Switches the GID to `mode` (the title string of the switch is *titleStringId); safe when no galaxy view is open.
void UiSetDisplayModeTitled(int mode, LPARAM argument, const ResourceStringId* titleStringId); // 0x41d890
void UiSetDisplayMode(int mode, LPARAM argument); // 0x41d8c0
void UiClearButtonHighlights(uint32_t mask); // 0x41db20
void UiSetButtonHighlights(uint32_t mask); // 0x41db40
void UiResumePendingRequest(); // 0x41db60

// Input hooks of the strategic game (keyboard: Escape release; mouse: swallows clicks).
LRESULT CALLBACK UiKeyboardHook(int code, WPARAM wParam, LPARAM lParam); // 0x41d8f0
LRESULT CALLBACK UiMouseHook(int code, WPARAM wParam, LPARAM lParam); // 0x41d950
void UiInstallInputHooks(); // 0x41d9d0
void UiBeginGameInput(); // 0x41da30
void UiRemoveInputHooks(); // 0x41da80
int UiGetInputActive(); // 0x41dad0
void UiSetPausePending(int pending); // 0x41dae0
