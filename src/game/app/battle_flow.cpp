#include "app/battle_flow.h"

#include <new>

#include "app/rebapp_globals.h"
#include "core/checked_new.h"
#include "core/file_util.h"
#include "core/iostream_lib.h"
#include "events/game_clock_hooks.h"
#include "multiplayer/multiplayer_window.h"
#include "platform/win32.h"
#include "strategy_ui/game_ui_facade.h"
#include "tactical/tactical_facade.h"
#include "tactical/tactical_game.h"

extern float g_tacticalFrameMilliseconds;  // 0x658640
extern float g_flowScaleA;                 // 0x65863c
extern float g_flowScaleASquared;          // 0x6b1210
extern float g_flowScaleB;                 // 0x658648
extern float g_flowScaleBSquared;          // 0x6b121c
extern int g_flowRangeSpan;                // 0x6b1240

namespace {

constexpr int kWaitCursorId = 0x7f02;
constexpr int kLogicalResourceSaveDirectory = 0x610;
constexpr char kBackslash[] = "\\";
constexpr int kRangeSlack = 5;

HCURSOR showWaitCursor() { return SetCursor(LoadCursorA(nullptr, MAKEINTRESOURCEA(kWaitCursorId))); }

// Builds a path from the installed location and a resource string; the result is not used (see CreateGameStepDriver).
void buildInstalledPath() {
    String path;
    path.Clear();
    char* installed = GetInstalledLocation();
    if (installed) {
        String separator(kBackslash);
        String installedText(installed);
        path += installedText;
        path += separator;
        operator delete(installed);
    }
    String resource(LoadRcdataString(kLogicalResourceSaveDirectory));
    path += resource;
}

}  // namespace

// ---- state names ----------------------------------------------------------------------------------------------------

int GetBattleStateName(int state, String* out) {
    const char* name = nullptr;
    switch (state) {
        case 0: name = "NONE"; break;
        case 1: name = "STRAT_INIT"; break;
        case 2: name = "STRAT"; break;
        case 3: name = "STRAT_ALL_BATTLE_START"; break;
        case 4: name = "STRAT_BATTLE_START"; break;
        case 5: name = "STRAT_BATTLE_CHOOSE_TYPE"; break;
        case 6: name = "STRAT_BATTLE_WAIT_FOR_TYPE_CHOICE"; break;
        case 7: name = "STRAT_BATTLE_CONVERT_TO_TACT"; break;
        case 8: name = "STRAT_BATTLE_CONVERT_TO_TACT_ABSTRACT"; break;
        case 9: name = "STRAT_BATTLE_FINISH"; break;
        case 10: name = "STRAT_BATTLE_CHOOSE_DONE"; break;
        case 11: name = "STRAT_BATTLE_WAIT_FOR_DONE_CHOICE"; break;
        case 12: name = "STRAT_ALL_BATTLE_FINISH"; break;
        case 0x14: name = "TACT"; break;
        case 0x15: name = "TACT_ABSTRACT"; break;
        case 0x16: name = "TACT_CONVERT_TO_STRAT"; break;
        case 0x17: name = "TACT_ABSTRACT_CONVERT_TO_STRAT"; break;
        default: break;
    }
    if (!name) {
        out->Assign(String("Invalid!"));
        return 0;
    }
    out->Assign(String(name));
    return 1;
}

// ---- game step driver lifetime ---------------------------------------------------------------------------------------------

GameStepDriver* BattleFlow::StepDriver() const noexcept { return m_stepDriver.get(); }

TacticalGame* BattleFlow::TacticalGameInstance() const noexcept { return m_tacticalGame.get(); }

TacticalGame* BattleFlow::GetOrCreateTacticalGame() {
    if (!m_tacticalGame) m_tacticalGame.reset(NewOrNull<TacticalGame>());
    return m_tacticalGame.get();
}

void BattleFlow::DestroyOwnedTacticalGame() {
    if (!m_tacticalGame) return;
    m_tacticalGame->ShutDownBattle();
    m_tacticalGame.reset();
}

int BattleFlow::CreateStepDriver() {
    m_stepDriver.reset(NewOrNull<GameStepDriver>());
    int ok = 0;
    if (m_stepDriver && m_stepDriver->m_ok != 0) {
        ok = m_stepDriver->m_ok;
        buildInstalledPath();
    }
    return ok;
}

int BattleFlow::CreateStepDriverFor(GameSettings* settings, int a, int b) {
    m_stepDriver.reset(NewOrNull<GameStepDriver>());
    if (m_stepDriver) m_stepDriver->InitializeSession(a, b);
    int ok = 0;
    if (m_stepDriver && m_stepDriver->m_ok != 0) {
        ok = m_stepDriver->m_ok;
        buildInstalledPath();
        m_stepDriver->BeginNewGame(settings);
    }
    return ok;
}

int BattleFlow::DestroyStepDriver() {
    m_stepDriver.reset();
    return 1;
}

int CreateGameStepDriver() {
    BattleFlow* flow = GetBattleFlow();
    return flow ? flow->CreateStepDriver() : 0;
}

int CreateGameStepDriverFor(GameSettings* settings, int a, int b) {
    BattleFlow* flow = GetBattleFlow();
    return flow ? flow->CreateStepDriverFor(settings, a, b) : 0;
}

int DestroyGameStepDriver() {
    BattleFlow* flow = GetBattleFlow();
    return flow ? flow->DestroyStepDriver() : 1;
}

void* AllocateChecked(unsigned size) { return ::operator new(size); }

void InitFlowScaleA() { g_flowScaleASquared = g_flowScaleA * g_flowScaleA; }

void InitFlowScaleB() { g_flowScaleBSquared = g_flowScaleB * g_flowScaleB; }

void InitFlowRangeSpan() { g_flowRangeSpan = g_rectSessionList.right - g_rectSessionList.left - kRangeSlack; }

// ---- the state machine ----------------------------------------------------------------------------------------------------

void BattleFlow::Step(int* stepped) {
    *stepped = 0;
    const bool strategicState = m_state >= 1 && m_state <= 0xc;
    const bool tacticalState = m_state >= 0x14 && m_state <= 0x17;
    int runnable = strategicState || tacticalState;
    if (!runnable) return;
    if (strategicState) runnable = m_stepDriver.get() != nullptr;
    if (!runnable) return;

    ObjectKey rootKey;
    const int previousState = m_state;
    int allVoted = 1;
    int allConfirmed = 1;
    int unanimous = 1;
    int status = runnable;

    switch (previousState) {
        case kFlowInit:
            if (m_stepDriver.get()->StepSetup(&status)) {
                AppForwardToStrategicWindow();
                m_state = kFlowStrategic;
            }
            break;
        case kFlowStrategic:
            if (m_stepDriver.get()->Step(reinterpret_cast<uint32_t*>(stepped))) m_state = kFlowAllBattleStart;
            break;
        case kFlowAllBattleStart:
            m_stepDriver.get()->m_hold = 1;
            m_state = kFlowBattleStart;
            break;
        case kFlowBattleStart:
            status = m_stepDriver->Manager() ? m_stepDriver->Manager()->StartPendingBattle() : 0;
            if (status != 0) {
                status = m_stepDriver.get()->GetRootKey(&rootKey);
                if (status != 0) m_state = rootKey.IsSet() ? kFlowBattleChooseType : kFlowAllBattleFinish;
            }
            break;
        case kFlowBattleChooseType:
            status = m_stepDriver.get()->GetRootKey(&rootKey);
            if (status != 0) UiBeginVoteAll(GameHookTickCount(), &rootKey);
            m_state = kFlowBattleWaitForType;
            break;
        case kFlowBattleWaitForType:
            m_stepDriver.get()->Step(reinterpret_cast<uint32_t*>(stepped));
            UiExchangeVotesTimed(&allVoted, &unanimous, &m_voteTimeA, &m_voteTimeB);
#ifdef REB_NO_TACTICAL_3D
            unanimous = 1;  // no 3D engine: a "tactical" vote (2) is resolved like auto-resolve (1)
#endif
            if (allVoted != 0) m_state = (unanimous != 0) + kFlowConvertToTactical;
            break;
        case kFlowConvertToTactical: {
            if (m_stepDriver.get()) {
                BattleSetup setup;
                setup.m_driver = m_stepDriver.get();
                setup.m_abstract = 0;
                setup.m_timeA = m_voteTimeA;
                setup.m_timeB = m_voteTimeB;
                setup.Convert();
            }
            {
                String saveName;
                BuildPreBattleSavePath(&saveName);
                ScopedOfstream file;
                OpenOutputFile(file.get(), saveName.m_text);
                if (GetFileError() == 0) {
                    HCURSOR previousCursor = showWaitCursor();
                    SaveStrategic(file.get()->Out());
                    SetCursor(previousCursor);
                }
                CloseFileStream(file.get());
            }
            DestroyStepDriver();
            TacticalStart();
            m_state = kFlowTactical;
            break;
        }
        case kFlowConvertToAbstract:
            if (m_stepDriver.get()) {
                BattleSetup setup;
                setup.m_driver = m_stepDriver.get();
                setup.m_abstract = 1;
                setup.m_timeA = m_voteTimeA;
                setup.m_timeB = m_voteTimeB;
                setup.Convert();
            }
            TacticalStart();
            m_state = kFlowTacticalAbstract;
            break;
        case kFlowBattleFinish:
            status = m_stepDriver->Manager() ? m_stepDriver->Manager()->RunFollowUps() : 0;
            m_state = kFlowBattleChooseDone;
            break;
        case kFlowBattleChooseDone:
            status = m_stepDriver.get()->GetRootKey(&rootKey);
            if (status != 0) UiResetVoteAll(GameHookTickCount(), &rootKey);
            m_state = kFlowBattleWaitForDone;
            break;
        case kFlowBattleWaitForDone:
            m_stepDriver.get()->Step(reinterpret_cast<uint32_t*>(stepped));
            UiExchangeVotes(&allConfirmed);
            if (allConfirmed != 0) m_state = kFlowBattleStart;
            break;
        case kFlowAllBattleFinish:
            m_stepDriver.get()->ClearHold();
            UiResumePendingRequest();
            m_state = kFlowStrategic;
            break;
        case kFlowTactical:
            if (GetOrCreateTacticalGame()->Update(g_tacticalFrameMilliseconds)) m_state = kFlowTacticalToStrategic;
            break;
        case kFlowTacticalAbstract:
            if (GetOrCreateTacticalGame()->Update(g_tacticalFrameMilliseconds)) m_state = kFlowAbstractToStrategic;
            break;
        case kFlowTacticalToStrategic: {
            int inconclusiveLive = 0;
            DestroyStepDriver();
            m_stepDriver.reset(NewOrNull<GameStepDriver>());
            if (m_stepDriver && m_stepDriver->m_ok != 0) {
                String path;
                BuildSaveDirectory(&path);
            }
            LoadPreBattleSave();
            {
                String saveName;
                BuildPreBattleSavePath(&saveName);
                DeleteGameFile(saveName.m_text);
            }
            if (m_stepDriver.get()) {
                BattleResults results;
                results.m_driver = m_stepDriver.get();
                results.m_abstract = 0;
                results.Apply(&inconclusiveLive);
            }
            DestroyOwnedTacticalGame();
            if (m_stepDriver && m_stepDriver->Manager()) m_stepDriver->Manager()->RunUntilIdle();
            m_state = 9 - (inconclusiveLive != 0);
            break;
        }
        case kFlowAbstractToStrategic: {
            int inconclusive = 0;
            if (m_stepDriver.get()) {
                BattleResults results;
                results.m_driver = m_stepDriver.get();
                results.m_abstract = 1;
                results.Apply(&inconclusive);
            }
            DestroyOwnedTacticalGame();
            if (m_stepDriver && m_stepDriver->Manager()) m_stepDriver->Manager()->RunUntilIdle();
            m_state = kFlowBattleFinish;
            break;
        }
    }
    if (m_state != previousState) {
        String stateName;
        GetBattleStateName(m_state, &stateName);
    }
}

float g_flowScaleA = 3.0f;
float g_flowScaleASquared{};
float g_flowScaleB = 3.0f;
float g_flowScaleBSquared{};
float g_tacticalFrameMilliseconds = 50.0f;
int g_flowRangeSpan{};
