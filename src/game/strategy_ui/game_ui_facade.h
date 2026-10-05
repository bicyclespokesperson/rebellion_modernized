#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/stream_io.h"
#include "gameobj/object_key.h"
#include "platform/win32.h"

class CoolWindow;
struct GameTime;
class PlayerMessage;
class PlayerNotice;

// The game UI facade (0x41cca0-0x41dce0): the global entry points the rest of the game uses to reach the game
// session (0x435a40: GetGameSession) and the galaxy view (0x422ca0: GetGalaxyWindow). Almost every one of them is
// a null-checked forwarder; the return value without a session / view is noted where it is not 0.

// ---- Game session ------------------------------------------------------------------------------------------
void UiCreateGameSession(int opponentType, int humanSide);  // 0x41cca0
int UiAdvanceTurn(int* roundComplete);                       // 0x41cce0 1 without a session
int UiReadSession(istream* in);                              // 0x41cd10 creates an empty session first if needed
int UiWriteSession(ostream* out);                            // 0x41cd50
void UiSetSessionViewReady(int ready);                       // 0x41cd70
int UiSessionRandom(int limit);                              // 0x41cd80 the session's random generator
int UiGetHumanSide();                                        // 0x41cdb0
int UiIsOpponentAI();                                        // 0x41cdd0
void UiHumanCommand(int command, void* selection, void* key, int unknown);  // 0x41cdf0
void UiHumanReceiveMessage(PlayerMessage* message, int flag);  // 0x41ce20
void UiHumanAdvisorNotify(int value);                        // 0x41ce40
void UiHumanSetVote(int choice, int timed, PlayerMessage* reply);  // 0x41ce60
void UiHumanConfirmVote(PlayerMessage* reply);               // 0x41ce80
void UiRequestRedraw(int value);                             // 0x41cea0
void UiRouteMessage(PlayerMessage* message);                 // 0x41cee0
void UiDeliverQueuedMessage(int id);                         // 0x41cf00
int UiAdvancePhase();                                        // 0x41cf20
int UiProcessAllMessages();                                  // 0x41cf40 1 without a session
void UiUpdateSession();                                      // 0x41cf60
int UiAreAllSynced();                                        // 0x41cf80
void UiBeginVoteAll(int id, ObjectKey* key);                 // 0x41cfa0
void UiExchangeVotesTimed(int* allVoted, int* unanimous, GameTime* timeA, GameTime* timeB);  // 0x41cfc0
void UiResetVoteAll(int id, ObjectKey* key);                 // 0x41cff0
void UiExchangeVotes(int* allConfirmed);                     // 0x41d010
void UiRefreshAllPlayers();                                  // 0x41d030
void UiComputeAllAdvisorChoices();                           // 0x41d050
int UiSyncAllPlayers(unsigned syncId);                       // 0x41d070 1 without a session
int UiResetAllPlayerFlags();                                 // 0x41d0a0 1 without a session
AvlNode* UiGetFirstMessage();                                // 0x41d0c0
int UiHumanHasPending();                                     // 0x41d0e0
int UiNextId();                                              // 0x41d120
void UiSetHumanUiTarget(int a, int b);                       // 0x41d140
void UiHumanRefreshMenus(int value, int requestUpdate);      // 0x41d160
void UiHumanRefreshOrderMenus(int value);                    // 0x41d190
IdAvlTree* UiGetHumanOrderTree();                            // 0x41d1b0
void UiDeleteHumanMessage(int id);                           // 0x41d1d0
void UiSetHumanDispatchFlag(int on);                         // 0x41d1f0
void UiSetHumanUiFlag4000(int on);                           // 0x41d210
void UiSetHumanStateValue(int key, int value);               // 0x41d230
unsigned UiGetHumanStateValue(int key);                      // 0x41d250
int UiGetHumanDefaultState();                                // 0x41d270
void UiSetHumanDefaultState(int state);                      // 0x41d290
void UiQueueHumanPopup(int a, int b);                        // 0x41d2b0
void UiHumanSetAdvisorMask(unsigned mask);                   // 0x41d2d0
int UiBeginSync(unsigned requested);                         // 0x41d2f0
void UiAckSync(int side);                                    // 0x41d310
void UiFinishSync(PlayerMessage* message);                   // 0x41d330
void UiSetSyncTargets(unsigned target);                      // 0x41d350
int UiHumanDispatchIdle();                                   // 0x41d370 1 without a session
int UiHaveSameSyncValue();                                   // 0x41d390 1 without a session
int UiHumanNextAdvice();                                     // 0x41d3b0
void UiClearHumanAlerts(unsigned mask);                      // 0x41d3d0
void UiHumanBeginHold();                                     // 0x41d3f0
void UiHumanEndHold();                                       // 0x41d410
void UiSendNotificationToSide(int side, int argument);       // 0x41dc80

// ---- Game step driver (g_gameStepDriver, null outside a game) -------------------------------------------------
void UiAddManagerNotice(void* notice);                       // 0x41db80
unsigned UiRaisePauseTo(unsigned tick);                      // 0x41dba0 returns `tick` without a driver
int UiBeginPause();                                          // 0x41dbc0
void UiClearHold();                                          // 0x41dbe0
int UiEndPause();                                            // 0x41dbf0
int UiIsPauseTick();                                         // 0x41dc10 1 without a driver
int UiGetHold();                                             // 0x41dc30 1 without a driver
void UiSetSetupDone();                                       // 0x41dc50
class ObjectKeyValueSet;
class EventKindSet;
void UiBuildAvailableOrders(int player, ObjectKeyValueSet* selection, EventKindSet* commands);  // 0x41dcc0

// Loads the sprite DLL of `side` (ALSPRITE.DLL / EMSPRITE.DLL, slot 9) and, with `loadBriefing`, the briefing DLL
// (ALBRIEF.DLL / EMBRIEF.DLL, slot 0xd); unloads the previous ones when the side changed.
void UiLoadSideSpriteDlls(int side, int loadBriefing);       // 0x41daf0
void LoadSideSpriteDlls(int side, int loadBriefing);         // 0x42d650
void UnloadSpriteDll();                                      // 0x42d740
void UnloadBriefingDll();                                    // 0x42d760
