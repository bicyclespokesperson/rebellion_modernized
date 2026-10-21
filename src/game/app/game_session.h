#pragma once
class UiRecordTree;
class UiNotice;
#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include "app/player_controller_decl.h"
#include "core/entry_identity.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "gameobj/object_key.h"

// The game session (0x4359f0-0x436a40): owned by the running game-step driver and owns the two player controllers
// (one per side) and forwards the strategic game's turn, message and vote traffic to them. Most methods are thin
// forwarders to a virtual slot of both controllers, or to the human controller (null when nobody is human).
//
// The session is created by the current driver's CreateGameSession bridge (new game / load) and read/written in saves.

class GameSession
{
  public:
  GameSession();                                // 0x435b90
  GameSession(int opponentType, int humanSide); // 0x435bc0: humanSide 0 = two AIs, 1 or 2 = human side
  ~GameSession();

  int Read(istream* in);   // returns 0 if a controller kind could not be created
  int Write(ostream* out); // always 1

  // Creates the controller of `kind` (1 human, 2 remote, 3 AI) for `slot` without a side. Returns 0 for an
  // unknown kind or when a second human controller was requested (the controller is stored in the slot anyway).
  int CreateController(int kind, int slot);

  // Turn and message flow.
  // Advances the active side when it has finished its turn; returns 1 (and sets *outRoundComplete) when that
  // wraps back to side 0.
  int AdvanceTurn(int* outRoundComplete);
  int AdvancePhase();
  void Update();
  int AreAllSynced();
  int ProcessAllMessages();
  int SyncAllPlayers(unsigned syncId);
  int ResetAllPlayerFlags();
  void RefreshAllPlayers();
  void ComputeAllAdvisorChoices();
  int HaveSameSyncValue();
  int NextId();
  void RouteMessage(MessageBase* message, int senderSide);
  void RouteNotice(NotifPayload* notice);
  void DeliverQueuedMessage(int id);
  void SendNotificationToSide(int side, intptr_t argument);

  // Sync points between the two sides.
  int BeginSync(unsigned requested);
  void AckSync(int side);
  void FinishSync(UiNotice* message);
  void SetSyncTargets(unsigned target);

  // Battle-type vote: each side publishes its vote, the other side receives it.
  void BeginVoteAll(int id, ObjectKey* key);
  void ResetVoteAll(int id, ObjectKey* key);
  void ExchangeVotesTimed(int* outAllVoted, int* outUnanimous, GameTime* timeA, GameTime* timeB);
  void ExchangeVotes(int* outAllConfirmed);

  int GetHumanSide();
  int IsOpponentAI();
  bool SwitchHumanSide();
  // Changes an AI-controlled side while retaining its queued base state. Human and remote sides are unchanged.
  bool SetAiControllerKind(int side, int kind);
  // Selects controllers for a headless match after setup or load, preserving their queued base state.
  bool UseAiControllers(int sideOneKind, int sideTwoKind);
  bool UseLanControllers(int humanSide);

  // Forwarders to the human controller (no-ops, or 0, when there is none).
  void HumanCommand(int command, void* a, void* b, const void* c);
  void HumanReceiveMessage(PlayerMessage* message, int flag);
  void HumanAdvisorNotify(const ObjectKey* key);
  void HumanSetVote(int choice, int timed, UiNotice* reply);
  void HumanConfirmVote(UiNotice* reply);
  void SetHumanUiTarget(int a, int b);
  void HumanRequestUpdate();
  void HumanRefreshMenus(intptr_t value);
  void HumanRefreshOrderMenus(intptr_t value);
  int HumanHasPending();
  int HumanNextAdvice();
  void HumanSetAdvisorMask(unsigned mask);
  void HumanBeginHold();
  void HumanEndHold();
  void SetHumanUiFlag4000(int on);
  UiRecordTree* GetHumanOrderTree();
  void DeleteHumanMessage(int id);
  void SetHumanDispatchFlag(int on);
  void SetHumanStateValue(int key, int value);
  unsigned GetHumanStateValue(int key);
  int GetHumanDefaultState();
  void SetHumanDefaultState(int state);
  void QueueHumanPopup(intptr_t a, int b);
  int HumanDispatchIdle();
  void ClearHumanAlerts(unsigned mask);

  // Forward to static UI helpers of the human player; `this` is unused.
  void RequestRedraw(int value);
  void RequestRedrawAlt(int value);

  int m_viewReady;                    // +0x00 latched by Update once the strategy view exists
  int m_unk04;                        // +0x04 zero at construction; set when the active side wraps (AdvanceTurn)
  int m_nextId;                       // +0x08 handed out by NextId
  HumanPlayer* m_human;               // +0x0c the human controller, or null
  PlayerController* m_controllers[2]; // +0x10 by side (index 0 = side 1)
  int m_activeIndex;                  // +0x18 index of the side whose turn it is
  int m_opponentType;                 // +0x1c 1 = AI opponent, otherwise remote
  int m_valid;                        // +0x20 always 1 after construction
  PlayerMessageTree m_messages;       // +0x24
  std::unordered_map<int, int> m_messageSides;
};

extern RandomGenerator g_sessionRandom; // 0x6b14d8, session presentation randomness, saved and restored

GameSession* GetGameSession();
// Replaces the current driver's session; destroys it again if construction left it invalid.
void CreateGameSession(int opponentType, int humanSide);
// Replaces the current driver's session with an empty one (controllers are created by GameSession::Read); returns 0 on
// failure.
int CreateEmptyGameSession();
void DestroyGameSession();
