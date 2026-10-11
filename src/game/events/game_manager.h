#pragma once
// The game manager (0x51cd00-0x51ebc0, VT 0x65ecf4, global 0x6b2fcc): the one object that owns the game clock, the
// three scheduler queues, the notification tree, and the game root. Everything that happens on a schedule in the
// game is an item in one of its queues; the frame step (game_step_driver.h) calls AdvanceTick once per game tick and
// RunScheduler until nothing is due.
#include <cstdint>

#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "events/event_queue.h"
#include "events/events_decl.h"
#include "gameobj/game_root.h"
#include "gameobj/game_settings.h"
#include "gameobj/object_key.h"
#include "gameobj/status_code.h"
#include "orders/order_list.h"

class Crc32;
class TimedEvent;

// 0x1c bytes (ctor 0x5397d0, assign 0x539880, read 0x5398c0, write 0x539910): three ints and two checksums. The game
// manager keeps three of them: the snapshot taken at the end of the last tick and the two sides' latest ones.
struct GameStateSnapshot {
    int m_tick;       // +0x00
    int m_stamp;      // +0x04
    int m_unk08;      // +0x08
    Crc32 m_stateCrc;   // +0x0c
    Crc32 m_tableCrc;   // +0x14

    GameStateSnapshot() : m_tick(0), m_stamp(0), m_unk08(0) {}

    Crc32* CopyStateCrc(Crc32* out);  // 0x51e1e0
    Crc32* CopyTableCrc(Crc32* out);  // 0x51e200
};


class GameManager {
public:
    GameManager();           // 0x51cd00
    virtual ~GameManager();  // 0x51cd90 (scalar deleting 0x51cd70)

    // Creates the notification tree, the three queues, the two posted-event queues and the tick table. Returns m_ok.
    int InitState();       // 0x51e320
    // Deletes everything InitState and BeginNewGame created. Returns 1.
    int ReleaseMembers();  // 0x51e5b0

    int Read(istream* in);        // 0x51cec0
    int Write(ostream* out);      // 0x51d2c0
    int Checksum(Crc32* crc);     // 0x51d5e0

    // Starts a new game: a fresh game root with the given settings.
    void BeginNewGame(GameSettings* settings);  // 0x51d6c0
    // Copies the root's settings (defaults without a root) into *out. Returns 1.
    int GetSettings(GameSettings* out);          // 0x51d780
    // One step of the setup stages; sets *status to 1 and marks setup done when the stages finish.
    int StepSetup(int* status);                 // 0x51d800
    // which 1 or 2: replaces the snapshot of that side. Returns 0 for any other value.
    int SetSnapshot(GameStateSnapshot* snapshot, int which);  // 0x51d830
    // Hands a received notification to the tree.
    void AddNotice(IdNode* notice);               // 0x51d880

    void ScheduleTimer(TimedEvent* timer, int delay);  // 0x51d8a0, queue +0xa8; due day = today + delay
    void ScheduleCommand(EventBase* event);            // 0x51d8d0, queue +0xac
    void ScheduleEvent(EventBase* event);              // 0x51d8f0, queue +0xb0
    // Routes a posted event by kind (1-4) to the handlers below. Returns 0 for any other kind.
    int DispatchPostedEvent(PostedEvent* event);       // 0x51d910

    // Builds the set of order kinds that every object of `team` can be given: each object's own kinds, intersected
    // (the first one's are copied). `side` is stored in the trial orders. *orders receives the kinds; returns whether
    // the walk completed.
    int BuildAvailableOrders(int side, ObjectKeyValueSet* team, EventKindSet* orders);  // 0x51d990

    int GetRootField70();                               // 0x51dcb0
    int GetRootKey(ObjectKey* out);                    // 0x51dcc0
    int QueryRootBattle(int side, uint32_t* a, uint32_t* b, int* c, uint32_t* d);  // 0x51dce0
    int StartPendingBattle();                           // 0x51ddb0
    int RunFollowUps();                                 // 0x51ddf0
    int HasPendingBattle();                             // 0x51de80
    int AdvanceTick();                                  // 0x51dea0
    int RunScheduler();                                 // 0x51df30
    int EndOfStep();                                    // 0x51e050
    int CaptureSnapshot();                              // 0x51e220
    int RunUntilIdle();                                 // 0x51eb90

    int ExecuteNotice(SidedMessage* notice);            // 0x51dd20
    int NoteDispatch(void* item);                       // 0x51ebb0, always 1

    int HandleKind1(PostedEvent* event);                // 0x51e670
    int HandleKind2(PostedEvent* event);                // 0x51e6a0
    int HandleKind3(PostedEvent* event);                // 0x51e770
    int HandleKind4(PostedEvent* event);                // 0x51e840
    int ResolveTickValue(SeedKey* key, int* out);       // 0x51e910
    int PostTickValue(int a, int b, int c);             // 0x51e9a0
    int PostToFirstQueue(PostedEvent* event);           // 0x51eaa0
    int FlushFirstQueue();                              // 0x51eaf0
    int FlushSecondQueue();                             // 0x51eb40

    int m_ok;                          // +0x04, 0 when InitState could not build everything
    int m_unk08;                       // +0x08, 1
    int m_tickCount;                   // +0x0c, 1
    int m_day;                         // +0x10, 1
    int m_serialA;                     // +0x14, counter behind hook 0x6b2af4
    int m_serialB;                     // +0x18, counter behind hook 0x6b2afc
    uint32_t m_ticksPerDay;            // +0x1c, 1
    uint32_t m_tickOfDay;              // +0x20
    int m_dayEnded;                    // +0x24
    int m_lastBattleTick;              // +0x28
    int m_setupDone;                   // +0x2c
    int m_stateDiffers;             // +0x30
    int m_unk34;                       // +0x34
    int m_unk38;                       // +0x38
    int m_unk3c;                       // +0x3c
    int m_unk40;                       // +0x40
    int m_unk44;                       // +0x44
    int m_firstSnapshotTick;           // +0x48
    int m_haveSideSnapshots;           // +0x4c
    GameStateSnapshot m_snapshot[3];   // +0x50
    MessageTree* m_notices;            // +0xa4
    TimerQueue* m_timers;              // +0xa8
    CommandQueue* m_commands;          // +0xac
    TypeQueue* m_events;               // +0xb0
    PostedEventQueue* m_firstQueue;    // +0xb4
    PostedEventQueue* m_secondQueue;   // +0xb8
    TickTable* m_tickTable;            // +0xbc
    GameRoot* m_root;                  // +0xc0
};

extern GameManager* g_gameManager;  // 0x6b2fcc, null until CreateGameManager

GameManager* GetGameManager();  // 0x51ce00
void CreateGameManager();       // 0x51ce10, replaces the global; destroyed again if construction failed
void DestroyGameManager();      // 0x51cea0

// Registers the factories of all three tables once (0x51ebc0).
void RegisterAllFactoriesOnce();
