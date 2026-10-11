#pragma once
// The game step driver (0x41dce0-0x41e570, VT 0x658a9c): owns the game session and manager, and sequences them through
// the phases below as the application's main state machine (0x40a050) sets up and advances the game frame by frame.
//
// Frame phases (m_phase), run one per Step call, a phase that reports done advances to the next:
//   0 advance the tick (or hold while a battle is pending)   1 sync, 2 pause check, 3 sync   4 nothing
//   5 run the scheduler until idle   6 end-of-step work   7-11 session synchronisation   12 snapshot   13 finish
#include <cstdint>
#include <memory>

#include "core/stream_io.h"
#include "gameobj/game_settings.h"
#include "gameobj/object_key.h"

class GameManager;
class GameSession;

class GameStepDriver {
public:
    GameStepDriver();                  // 0x41dce0
    virtual ~GameStepDriver();         // 0x41dd50 (scalar deleting 0x41dd00)

    void InitializeSession(int opponentType, int humanSide);
    GameManager* Manager() const noexcept;
    GameSession* Session() const noexcept;
    void CreateManager();
    void DestroyManager();
    void CreateSession(int opponentType, int humanSide);
    int CreateEmptySession();
    void DestroySession();

    int InitFields();         // 0x41e330, returns m_ok
    int ReleaseResources();   // 0x41e360, returns 1

    int Load(istream* in);    // 0x41dd60
    int Save(ostream* out);   // 0x41de70
    void BeginNewGame(GameSettings* settings);  // 0x41df40
    // Setup sequence of a new or loaded game; returns 1 once the game is ready to run.
    int StepSetup(int* status);  // 0x41df80
    // One frame; *done is set when the phase counter is at 2 (the scheduler phase). Returns the 'battle pending' flag.
    int Step(uint32_t* done);    // 0x41dff0

    uint32_t RaisePauseTo(uint32_t tick);  // 0x41e290
    int BeginPause();                       // 0x41e2b0
    int EndPause();                         // 0x41e2d0
    int IsPauseTick();                      // 0x41e300
    void ClearHold();                       // 0x41e320

    int AdvanceClock();      // 0x41e390, phase 0

    // Forwarders to the manager owned by this driver.
    int GetRootKey(ObjectKey* out);  // 0x41e1d0
    int QueryBattle(int side, uint32_t* a, uint32_t* b, int* c, uint32_t* d);  // 0x41e1f0
    int PausePhase();                // 0x41e410, phase 1
    int FinishPhase(int* battlePending);  // 0x41e500, phase 13

    int m_ok;           // +0x04, 1
    int m_phase;        // +0x08
    int m_setupStage;   // +0x0c, 1
    int m_pauseTick;    // +0x10, the tick a pause was requested at, 0 when none
    int m_hold;         // +0x14, set while a new game is being created
    int m_unk18;        // +0x18
    int m_unk1c;        // +0x1c
    int m_setupDone;    // +0x20
    char* m_dataLocation;  // +0x24, GetDataLocation(), freed with operator delete

private:
    int RunIdlePhase();
    int RunSchedulerPhase();
    int RunEndOfStepPhase();
    int CaptureSnapshotPhase();

    std::unique_ptr<GameManager> m_manager;
    std::unique_ptr<GameSession> m_session;
};

GameStepDriver* GetGameStepDriver();

// Registers every factory table of the game once (0x41e550).
void RegisterStrategicFactoriesOnce();
void RegisterKeyedFactories();    // 0x41e580
void RegisterMultiplayerOrderFactories();  // 0x41ec50

// Static helpers that forward to the game manager when there is one.
int GameManagerExists();                       // 0x41e1a0
int GetGameTickCount();                        // 0x41e1b0
int StartPendingGameBattle();                  // 0x41e230
int RunSchedulerUntilIdle();                   // 0x41e250
int RunGameFollowUps();                        // 0x41e270
int IdlePhase();                               // 0x41e430, phase 4
int SchedulerPhase();                          // 0x41e460, phase 5
int EndOfStepPhase();                          // 0x41e490, phase 6
int SnapshotPhase();                           // 0x41e4d0, phase 12
