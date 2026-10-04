#include "events/game_step_driver.h"

#include "app/game_session.h"
#include "app/rebapp_globals.h"
#include "core/vptr_store.h"
#include "events/game_clock_hooks.h"
#include "events/game_manager.h"
#include "gameobj/factory_registries.h"
#include "orders/order_base.h"

// Forwarders of the game session (0x41cc00-0x41d3a0 call into 0x435a40's object) and UI glue, declared by address.
void FUN_0041cca0(int a, int b);
int FUN_0041cce0(int* status);
int FUN_0041cd10(istream* in);
int FUN_0041cd50(ostream* out);
int FUN_0041cf20();
int FUN_0041cf40();
void FUN_0041cf60();
void FUN_0041d030();
void FUN_0041d050();
int FUN_0041d070(int tick);
int FUN_0041d0a0();
int FUN_0041d0e0();
void FUN_0041d350(int tick);
int FUN_0041d370();
int FUN_0041d390();
void FUN_0041db60();
int FUN_0040f340();

extern int g_strategicFactoriesRegistered;  // 0x6b14c0

namespace {

enum Phase {
    kPhaseAdvanceTick = 0,
    kPhaseSyncA = 1,
    kPhasePauseCheck = 2,
    kPhaseSyncB = 3,
    kPhaseIdle = 4,
    kPhaseScheduler = 5,
    kPhaseEndOfStep = 6,
    kPhaseSyncC = 7,
    kPhaseSyncD = 8,
    kPhaseSyncE = 9,
    kPhaseSyncF = 10,
    kPhaseBattleCheck = 11,
    kPhaseSnapshot = 12,
    kPhaseFinish = 13,
    kSessionTypeSinglePlayer = 1,
};

enum SetupStage { kSetupGame = 1, kSetupSession = 2, kSetupWaitForUser = 3 };

}  // namespace

GameStepDriver::GameStepDriver() { InitFields(); }

GameStepDriver::GameStepDriver(int a, int b) {
    InitFields();
    FUN_0041cca0(a, b);
}

GameStepDriver::~GameStepDriver() {
    REB_KEEP_VPTR_STORE();
    ReleaseResources();
}

int GameStepDriver::InitFields() {
    m_ok = 1;
    m_hold = 0;
    m_phase = 0;
    m_setupStage = 1;
    m_pauseTick = 0;
    m_unk18 = 0;
    m_unk1c = 0;
    m_setupDone = 0;
    m_dataLocation = GetDataLocation();
    return m_ok;
}

int GameStepDriver::ReleaseResources() {
    if (m_dataLocation) {
        operator delete(m_dataLocation);
        m_dataLocation = nullptr;
    }
    DestroyGameSession();
    DestroyGameManager();
    return 1;
}

int GameStepDriver::Load(istream* in) {
    RegisterStrategicFactoriesOnce();
    InstallGameManagerHooks();
    int ok = ReleaseResources();
    if (ok) ok = InitFields();
    if (ok) ok = CheckPositionMarker(in);
    if (ok) {
        int* fields = &m_ok;
        for (int i = 0; i < 7; ++i) ReadValue32(in, fields + i);
        ok = CheckPositionMarker(in);
        if (ok) {
            CreateGameManager();
            ok = GameManagerExists();
            if (ok) ok = GetGameManager()->Read(in);
            if (ok) ok = CheckPositionMarker(in);
            if (ok) ok = FUN_0041cd10(in);
            if (ok) ok = CheckPositionMarker(in);
        }
    }
    return ok;
}

int GameStepDriver::Save(ostream* out) {
    WritePositionMarker(out);
    const int* fields = &m_ok;
    for (int i = 0; i < 7; ++i) WriteValue32(out, fields + i);
    WritePositionMarker(out);
    int ok = GameManagerExists();
    if (ok) {
        ok = GetGameManager()->Write(out);
        if (ok) {
            WritePositionMarker(out);
            ok = FUN_0041cd50(out);
            if (ok) WritePositionMarker(out);
        }
    }
    return ok;
}

void GameStepDriver::BeginNewGame(GameSettings* settings) {
    RegisterStrategicFactoriesOnce();
    m_hold = 1;
    CreateGameManager();
    if (GameManagerExists()) GetGameManager()->BeginNewGame(settings);
}

int GameStepDriver::StepSetup(int* status) {
    *status = 1;
    switch (m_setupStage) {
        case kSetupGame:
            if (GetGameManager()->StepSetup(status)) m_setupStage = kSetupSession;
            break;
        case kSetupSession:
            if (FUN_0041cce0(status)) {
                m_setupStage = kSetupWaitForUser;
                return 0;
            }
            break;
        case kSetupWaitForUser:
            if (m_setupDone) {
                m_setupStage = 0;
                return 1;
            }
            break;
    }
    return 0;
}

int GameStepDriver::Step(uint32_t* done) {
    int battlePending = 0;
    switch (m_phase) {
        case kPhaseAdvanceTick:
            if (AdvanceClock()) ++m_phase;
            break;
        case kPhaseSyncA:
            if (PausePhase()) ++m_phase;
            break;
        case kPhasePauseCheck:
            if (!IsPauseTick()) ++m_phase;
            break;
        case kPhaseSyncB:
            if (FUN_0041cf40()) ++m_phase;
            break;
        case kPhaseIdle:
            if (IdlePhase()) ++m_phase;
            break;
        case kPhaseScheduler:
            if (SchedulerPhase()) ++m_phase;
            break;
        case kPhaseEndOfStep:
            if (EndOfStepPhase()) ++m_phase;
            break;
        case kPhaseSyncC:
            if (FUN_0041d0a0()) ++m_phase;
            break;
        case kPhaseSyncD:
            if (FUN_0041cf20()) ++m_phase;
            break;
        case kPhaseSyncE:
            if (!FUN_0041d0e0()) ++m_phase;
            break;
        case kPhaseSyncF:
            if (FUN_0041d370()) ++m_phase;
            break;
        case kPhaseBattleCheck:
            if (!FUN_0041d390()) {
                if (AppGetGameType() != kSessionTypeSinglePlayer) AppShowResumeNotice();
            } else if (AppGetGameType() != kSessionTypeSinglePlayer) {
                AppPlayClickSound();
                ++m_phase;
            } else {
                ++m_phase;
            }
            break;
        case kPhaseSnapshot:
            if (SnapshotPhase()) ++m_phase;
            break;
        case kPhaseFinish:
            if (FinishPhase(&battlePending)) m_phase = 0;
            break;
    }
    FUN_0041cf60();
    *done = m_phase == 2;
    return battlePending;
}

int GameManagerExists() { return GetGameManager() != nullptr; }

int GetGameTickCount() {
    int tick = 0;
    if (GameManagerExists()) tick = GetGameManager()->m_tickCount;
    return tick;
}

int GameStepDriver::GetRootKey(ObjectKey* out) {
    if (!GameManagerExists()) return 0;
    return GetGameManager()->GetRootKey(out);
}

int GameStepDriver::QueryBattle(int side, uint32_t* a, uint32_t* b, int* c, uint32_t* d) {
    if (!GameManagerExists()) return 0;
    return GetGameManager()->QueryRootBattle(side, a, b, c, d);
}

int StartPendingGameBattle() {
    if (!GameManagerExists()) return 0;
    return GetGameManager()->StartPendingBattle();
}

int RunSchedulerUntilIdle() {
    if (!GameManagerExists()) return 0;
    return GetGameManager()->RunUntilIdle();
}

int RunGameFollowUps() {
    if (!GameManagerExists()) return 0;
    return GetGameManager()->RunFollowUps();
}

uint32_t GameStepDriver::RaisePauseTo(uint32_t tick) {
    if (static_cast<uint32_t>(m_pauseTick) < tick) {
        m_pauseTick = tick;
        AppPromptPause();
    }
    return m_pauseTick;
}

int GameStepDriver::BeginPause() {
    if (m_pauseTick == 0) {
        m_pauseTick = GetGameTickCount();
        FUN_0041d350(m_pauseTick);
    }
    return m_pauseTick;
}

int GameStepDriver::EndPause() {
    const int atPauseTick = m_pauseTick == GetGameTickCount();
    if (atPauseTick) {
        m_pauseTick = 0;
        AppPlayClickSound();
    }
    return atPauseTick;
}

int GameStepDriver::IsPauseTick() { return GetGameTickCount() == m_pauseTick; }

void GameStepDriver::ClearHold() {
    m_hold = 0;
    AppPlayClickSound();
}

int GameStepDriver::AdvanceClock() {
    const int waited = AppHasWaitedLongEnough();
    int battlePending = 0;
    if (GameManagerExists()) battlePending = GetGameManager()->HasPendingBattle();
    if (waited) {
        if (battlePending) {
            FUN_0041db60();
            return 0;
        }
        if (m_hold == 0) {
            if (AppHasWaitedLongEnough()) AppResetWaitTimer();
            if (GameManagerExists()) GetGameManager()->AdvanceTick();
            return 1;
        }
    }
    if (battlePending == 0) return 0;
    FUN_0041db60();
    return 0;
}

int GameStepDriver::PausePhase() { return FUN_0041d070(GetGameTickCount()); }

int IdlePhase() {
    int result = 1;
    if (GameManagerExists()) result = FUN_0040f340();
    return result;
}

int SchedulerPhase() {
    int result = 1;
    if (GameManagerExists()) result = GetGameManager()->RunScheduler();
    return result;
}

int EndOfStepPhase() {
    int result = 1;
    if (GameManagerExists()) result = GetGameManager()->EndOfStep();
    return result;
}

int SnapshotPhase() {
    int result = 1;
    if (GameManagerExists()) result = GetGameManager()->CaptureSnapshot();
    return result;
}

int GameStepDriver::FinishPhase(int* battlePending) {
    FUN_0041d030();
    if (GameManagerExists()) {
        if (GetGameManager()->m_dayEnded != 0) FUN_0041d050();
        *battlePending = GetGameManager()->GetRootField70();
        if (*battlePending == 0) FUN_0041db60();
    }
    return 1;
}

void RegisterStrategicFactoriesOnce() {
    if (g_strategicFactoriesRegistered == 0) {
        RegisterAllFactoriesOnce();
        RegisterKeyedFactories();
        RegisterMultiplayerOrderFactories();
        g_strategicFactoriesRegistered = 1;
    }
}

void* FUN_0043a320();
void* FUN_0043a420();
void* FUN_0043a510();
void* FUN_0043a5f0();
void* FUN_0043a6e0();
void* FUN_0043a7e0();
void* FUN_0043a8d0();
void* FUN_0043a9d0();
void* FUN_0043aac0();
void* FUN_0043abb0();
void* FUN_0043aca0();
void* FUN_0043ad90();
void* FUN_0043ae80();
void* FUN_0043af70();
void* FUN_0043b060();
void* FUN_0043b150();
void* FUN_0043b240();
void* FUN_0043b310();
void* FUN_0043b3e0();
void* FUN_0043b4c0();
void* FUN_0043b5b0();
void* FUN_0043b690();
void* FUN_0043b780();
void* FUN_0043b870();
void* FUN_0043b960();
void* FUN_0043ba50();
void* FUN_0043bb40();
void* FUN_0043bc30();
void* FUN_0043bd20();
void* FUN_0043be70();
void* FUN_0043bf60();
void* FUN_0043c050();
void* FUN_0043c170();
void* FUN_0043c270();
void* FUN_0043c340();
void* FUN_0043c410();
void* FUN_0043c4e0();
void* FUN_0043c5b0();
void* FUN_0043c720();
void* FUN_0043c900();
void* FUN_0043c9f0();
void* FUN_0043cae0();
void* FUN_0043cbd0();
void* FUN_0043cce0();
void* FUN_0043cda0();
void* FUN_0043ce60();
void* FUN_0043cf50();
void* FUN_0043d030();
void* FUN_0043d110();
void* FUN_0043d290();
void* FUN_0043d380();
void* FUN_0043d470();
void* FUN_0043d570();
void* FUN_0043d660();
void* FUN_0043d730();
void* FUN_0043d800();
void* FUN_0043d8d0();
void* FUN_0043d9c0();
void* FUN_0043da90();
void* FUN_0043dc30();
void* FUN_0043df80();
void* FUN_0043e070();
void* FUN_0043e140();
void* FUN_0043e200();
void* FUN_0043e2c0();
void* FUN_0043e380();
void* FUN_0043e440();
void* FUN_0043e540();
void* FUN_0043e620();
void* FUN_0043e710();
void* FUN_0043e800();
void* FUN_0043e8f0();
void* FUN_0043ea00();
void* FUN_0043ead0();
void* FUN_0043eca0();
void* FUN_0043ed60();
void* FUN_0043ee90();
void* FUN_0043f030();
void* FUN_0043f180();
void* FUN_0043f270();
void* FUN_0043f360();
void* FUN_0043f4e0();
void* FUN_0043f5d0();
void* FUN_0043f6c0();
void* FUN_0043f7b0();
void* FUN_0043f8a0();
void* FUN_0043f990();
void* FUN_0043fa80();
void* FUN_0043fb70();
void* FUN_0043fca0();
void* FUN_0043fd90();
void* FUN_0043fe80();
void* FUN_0043ff70();
void* FUN_00440060();
void* FUN_00440150();
void* FUN_00440240();
void* FUN_00440350();

void RegisterKeyedFactories() {
    RegisterKeyedFactory(1, FUN_00440350);
    RegisterKeyedFactory(0x1c0, FUN_0043fca0);
    RegisterKeyedFactory(0x1c1, FUN_0043fd90);
    RegisterKeyedFactory(0x1c2, FUN_0043fe80);
    RegisterKeyedFactory(0x1c3, FUN_0043ff70);
    RegisterKeyedFactory(0x1c4, FUN_00440060);
    RegisterKeyedFactory(0x1c5, FUN_00440150);
    RegisterKeyedFactory(0x1c6, FUN_00440240);
    RegisterKeyedFactory(0x1e2, FUN_0043df80);
    RegisterKeyedFactory(0x360, FUN_0043d660);
    RegisterKeyedFactory(0x1e1, FUN_0043d8d0);
    RegisterKeyedFactory(0x1e5, FUN_0043d9c0);
    RegisterKeyedFactory(0x1e4, FUN_0043da90);
    RegisterKeyedFactory(0x362, FUN_0043d800);
    RegisterKeyedFactory(0x1e3, FUN_0043e070);
    RegisterKeyedFactory(0x1e0, FUN_0043dc30);
    RegisterKeyedFactory(0x361, FUN_0043d730);
    RegisterKeyedFactory(0x230, FUN_0043e380);
    RegisterKeyedFactory(0x231, FUN_0043e440);
    RegisterKeyedFactory(2, FUN_0043fb70);
    RegisterKeyedFactory(0x1a0, FUN_0043fa80);
    RegisterKeyedFactory(0x180, FUN_0043f7b0);
    RegisterKeyedFactory(0x181, FUN_0043f8a0);
    RegisterKeyedFactory(0x182, FUN_0043f990);
    RegisterKeyedFactory(0x183, FUN_0043f6c0);
    RegisterKeyedFactory(0x103, FUN_0043e710);
    RegisterKeyedFactory(0x100, FUN_0043e8f0);
    RegisterKeyedFactory(0x102, FUN_0043ea00);
    RegisterKeyedFactory(0x106, FUN_0043f5d0);
    RegisterKeyedFactory(0x107, FUN_0043e800);
    RegisterKeyedFactory(0x302, FUN_0043ead0);
    RegisterKeyedFactory(0x303, FUN_0043eca0);
    RegisterKeyedFactory(0x304, FUN_0043ed60);
    RegisterKeyedFactory(0x305, FUN_0043ee90);
    RegisterKeyedFactory(0x306, FUN_0043f030);
    RegisterKeyedFactory(0x104, FUN_0043f270);
    RegisterKeyedFactory(0x105, FUN_0043f180);
    RegisterKeyedFactory(0x500, FUN_0043f360);
    RegisterKeyedFactory(0x300, FUN_0043e540);
    RegisterKeyedFactory(0x301, FUN_0043e620);
    RegisterKeyedFactory(0x101, FUN_0043f4e0);
    RegisterKeyedFactory(0x212, FUN_0043e2c0);
    RegisterKeyedFactory(0x210, FUN_0043e140);
    RegisterKeyedFactory(0x211, FUN_0043e200);
    RegisterKeyedFactory(0x161, FUN_0043ce60);
    RegisterKeyedFactory(0x321, FUN_0043cf50);
    RegisterKeyedFactory(0x520, FUN_0043d110);
    RegisterKeyedFactory(0x320, FUN_0043d030);
    RegisterKeyedFactory(0x160, FUN_0043d290);
    RegisterKeyedFactory(0x163, FUN_0043d570);
    RegisterKeyedFactory(0x162, FUN_0043d380);
    RegisterKeyedFactory(0x400, FUN_0043d470);
    RegisterKeyedFactory(0x1f1, FUN_0043cae0);
    RegisterKeyedFactory(0x1f0, FUN_0043c9f0);
    RegisterKeyedFactory(0x370, FUN_0043cbd0);
    RegisterKeyedFactory(0x221, FUN_0043cce0);
    RegisterKeyedFactory(0x220, FUN_0043cda0);
    RegisterKeyedFactory(0x200, FUN_0043c720);
    RegisterKeyedFactory(0x380, FUN_0043c900);
    RegisterKeyedFactory(0x12b, FUN_0043c410);
    RegisterKeyedFactory(0x127, FUN_0043c170);
    RegisterKeyedFactory(0x120, FUN_0043be70);
    RegisterKeyedFactory(0x600, FUN_0043bd20);
    RegisterKeyedFactory(0x123, FUN_0043ba50);
    RegisterKeyedFactory(0x121, FUN_0043b960);
    RegisterKeyedFactory(0x12c, FUN_0043c4e0);
    RegisterKeyedFactory(0x122, FUN_0043bb40);
    RegisterKeyedFactory(0x124, FUN_0043bc30);
    RegisterKeyedFactory(0x129, FUN_0043c270);
    RegisterKeyedFactory(0x125, FUN_0043bf60);
    RegisterKeyedFactory(0x12a, FUN_0043c340);
    RegisterKeyedFactory(0x126, FUN_0043c050);
    RegisterKeyedFactory(0x128, FUN_0043c5b0);
    RegisterKeyedFactory(0x151, FUN_0043b4c0);
    RegisterKeyedFactory(0x140, FUN_0043aac0);
    RegisterKeyedFactory(0x141, FUN_0043a8d0);
    RegisterKeyedFactory(0x142, FUN_0043a7e0);
    RegisterKeyedFactory(0x143, FUN_0043af70);
    RegisterKeyedFactory(0x144, FUN_0043ae80);
    RegisterKeyedFactory(0x145, FUN_0043b060);
    RegisterKeyedFactory(0x146, FUN_0043ad90);
    RegisterKeyedFactory(0x147, FUN_0043b240);
    RegisterKeyedFactory(0x156, FUN_0043b3e0);
    RegisterKeyedFactory(0x148, FUN_0043b310);
    RegisterKeyedFactory(0x149, FUN_0043aca0);
    RegisterKeyedFactory(0x14a, FUN_0043a9d0);
    RegisterKeyedFactory(0x14b, FUN_0043b150);
    RegisterKeyedFactory(0x152, FUN_0043b5b0);
    RegisterKeyedFactory(0x153, FUN_0043b690);
    RegisterKeyedFactory(0x154, FUN_0043b780);
    RegisterKeyedFactory(0x155, FUN_0043b870);
    RegisterKeyedFactory(0x14c, FUN_0043abb0);
    RegisterKeyedFactory(0x14d, FUN_0043a5f0);
    RegisterKeyedFactory(0x14e, FUN_0043a6e0);
    RegisterKeyedFactory(0x14f, FUN_0043a420);
    RegisterKeyedFactory(0x150, FUN_0043a510);
    RegisterKeyedFactory(0x340, FUN_0043a320);
}

void* FUN_00420c20();
void* FUN_00420de0();
void* FUN_00420f60();
void* FUN_00421190();
void* FUN_00440410();
void* FUN_00440540();
void* FUN_00440720();
void* FUN_00440880();
void* FUN_00440a30();
void* FUN_00440cb0();

void RegisterMultiplayerOrderFactories() {
    RegisterOrderFactory(0x2, FUN_00440cb0);
    RegisterOrderFactory(0x1, FUN_00440a30);
    RegisterOrderFactory(0x10, FUN_00420de0);
    RegisterOrderFactory(0x12, FUN_00421190);
    RegisterOrderFactory(0x11, FUN_00420f60);
    RegisterOrderFactory(0x13, FUN_00440880);
    RegisterOrderFactory(0x4, FUN_00440720);
    RegisterOrderFactory(0x15, FUN_00440540);
    RegisterOrderFactory(0x14, FUN_00420c20);
    RegisterOrderFactory(0x5, FUN_00440410);
}
