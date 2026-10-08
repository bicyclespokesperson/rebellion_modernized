#pragma once
#include <cstddef>
#include <cstdint>

#include "app/player_controller_decl.h"
#include "core/containers.h"
#include "core/layout.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "gameobj/object_key.h"

// The game session (0x4359f0-0x436a40): the singleton that owns the two player controllers of a running game
// (one per side) and forwards the strategic game's turn, message and vote traffic to them. Most methods are thin
// forwarders to a virtual slot of both controllers, or to the human controller (null when nobody is human).
//
// The session is created by CreateGameSession (new game / load) and read/written as part of the save file.

class GameSession {
public:
    GameSession();                                 // 0x435b90
    GameSession(int opponentType, int humanSide);  // 0x435bc0: humanSide 1 or 2, opponentType 1 = AI else remote
    ~GameSession();

    int Read(istream* in);    // returns 0 if a controller kind could not be created
    int Write(ostream* out);  // always 1

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
    void RouteMessage(PlayerMessage* message);
    void RouteNotice(PlayerNotice* notice);
    void DeliverQueuedMessage(int id);
    AvlNode* GetFirstMessage();
    void SendNotificationToSide(int side, int argument);

    // Sync points between the two sides.
    int BeginSync(unsigned requested);
    void AckSync(int side);
    void FinishSync(PlayerMessage* message);
    void SetSyncTargets(unsigned target);

    // Battle-type vote: each side publishes its vote, the other side receives it.
    void BeginVoteAll(int id, ObjectKey* key);
    void ResetVoteAll(int id, ObjectKey* key);
    void ExchangeVotesTimed(int* outAllVoted, int* outUnanimous, GameTime* timeA, GameTime* timeB);
    void ExchangeVotes(int* outAllConfirmed);

    int GetHumanSide();
    int IsOpponentAI();

    // Forwarders to the human controller (no-ops, or 0, when there is none).
    void HumanCommand(int command, void* a, void* b, const void* c);
    void HumanReceiveMessage(PlayerMessage* message, int flag);
    void HumanAdvisorNotify(const ObjectKey* key);
    void HumanSetVote(int choice, int timed, PlayerMessage* reply);
    void HumanConfirmVote(PlayerMessage* reply);
    void SetHumanUiTarget(int a, int b);
    void HumanRequestUpdate();
    void HumanRefreshMenus(int value);
    void HumanRefreshOrderMenus(int value);
    int HumanHasPending();
    int HumanNextAdvice();
    void HumanSetAdvisorMask(unsigned mask);
    void HumanBeginHold();
    void HumanEndHold();
    void SetHumanUiFlag4000(int on);
    IdAvlTree* GetHumanOrderTree();
    void DeleteHumanMessage(int id);
    void SetHumanDispatchFlag(int on);
    void SetHumanStateValue(int key, int value);
    unsigned GetHumanStateValue(int key);
    int GetHumanDefaultState();
    void SetHumanDefaultState(int state);
    void QueueHumanPopup(int a, int b);
    int HumanDispatchIdle();
    void ClearHumanAlerts(unsigned mask);

    // Forward to static UI helpers of the human player; `this` is unused.
    void RequestRedraw(int value);
    void RequestRedrawAlt(int value);

    int m_viewReady;                      // +0x00 latched by Update once the strategy view exists
    int m_unk04;                          // +0x04 set to 1 when the active side wraps (AdvanceTurn); not initialised
    int m_nextId;                         // +0x08 handed out by NextId
    HumanPlayer* m_human;                 // +0x0c the human controller, or null
    PlayerController* m_controllers[2];   // +0x10 by side (index 0 = side 1)
    int m_activeIndex;                    // +0x18 index of the side whose turn it is
    int m_opponentType;                   // +0x1c 1 = AI opponent, otherwise remote
    int m_valid;                          // +0x20 always 1 after construction
    PlayerMessageTree m_messages;         // +0x24
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(GameSession, 0x34);
REB_ASSERT_OFFSET(GameSession, m_viewReady, 0x00);
REB_ASSERT_OFFSET(GameSession, m_unk04, 0x04);
REB_ASSERT_OFFSET(GameSession, m_nextId, 0x08);
REB_ASSERT_OFFSET(GameSession, m_human, 0x0c);
REB_ASSERT_OFFSET(GameSession, m_controllers, 0x10);
REB_ASSERT_OFFSET(GameSession, m_activeIndex, 0x18);
REB_ASSERT_OFFSET(GameSession, m_opponentType, 0x1c);
REB_ASSERT_OFFSET(GameSession, m_valid, 0x20);
REB_ASSERT_OFFSET(GameSession, m_messages, 0x24);
#pragma clang diagnostic pop

extern GameSession* g_gameSession;        // 0x6b14e4
extern RandomGenerator g_sessionRandom;   // 0x6b14d8, saved with the session (its state is ignored when loading)
extern uint8_t g_sessionRandomDestroyedGuard;  // 0x6b14e0

GameSession* GetGameSession();
// Replaces the session with a new one; destroys it again if construction left it invalid.
void CreateGameSession(int opponentType, int humanSide);
// Replaces the session with an empty one (controllers are created by GameSession::Read); returns 0 on failure.
int CreateEmptyGameSession();
void DestroyGameSession();

// Static initializer of g_sessionRandom (0x4359f0 constructs it and registers the destructor with atexit).
void GameSessionStaticInit();
void ConstructSessionRandom();
void RegisterSessionRandomDestructor();
void DestroySessionRandom();
