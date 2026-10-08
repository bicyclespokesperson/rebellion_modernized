#include "app/battle_flow.h"

#include <new>

#include "app/rebapp_globals.h"
#include "core/file_util.h"
#include "core/iostream_lib.h"
#include "events/game_clock_hooks.h"
#include "platform/win32.h"
#include "strategy_ui/game_ui_facade.h"
#include "tactical/tactical_facade.h"
#include "tactical/tactical_game.h"

// The save/load members of the battle-flow object (0x409230-0x40a050; not reconstructed): file names for the pre-battle
// save, the stream writer and reader.
void __thiscall FUN_00409ec0(BattleFlow* flow, String* out);     // the pre-battle save's path
void __thiscall FUN_00409dd0(BattleFlow* flow, String* out);     // the saved-games directory
int __thiscall FUN_00409690(BattleFlow* flow, ofstream* file);   // writes the game
int __thiscall FUN_00409700(BattleFlow* flow);                   // restores the game from the pre-battle save

extern float g_tacticalFrameMilliseconds;  // 0x658640
extern float g_flowScaleA;                 // 0x65863c
extern float g_flowScaleASquared;          // 0x6b1210
extern float g_flowScaleB;                 // 0x658648
extern float g_flowScaleBSquared;          // 0x6b121c
extern int g_flowRangeEnd;                 // 0x658780
extern int g_flowRangeStart;               // 0x658778
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

int CreateGameStepDriver() {
    void* memory = AllocateChecked(0x28);
    g_gameStepDriver = memory ? new (memory) GameStepDriver() : nullptr;
    int ok = 0;
    if (g_gameStepDriver && g_gameStepDriver->m_ok != 0) {
        ok = g_gameStepDriver->m_ok;
        buildInstalledPath();
    }
    return ok;
}

int __stdcall CreateGameStepDriverFor(GameSettings* settings, int a, int b) {
    void* memory = AllocateChecked(0x28);
    g_gameStepDriver = memory ? new (memory) GameStepDriver(a, b) : nullptr;
    int ok = 0;
    if (g_gameStepDriver && g_gameStepDriver->m_ok != 0) {
        ok = g_gameStepDriver->m_ok;
        buildInstalledPath();
        g_gameStepDriver->BeginNewGame(settings);
    }
    return ok;
}

int DestroyGameStepDriver() {
    if (g_gameStepDriver) {
        delete g_gameStepDriver;
        g_gameStepDriver = nullptr;
    }
    return 1;
}

void* AllocateChecked(unsigned size) { return ::operator new(size); }

void InitFlowScaleA() { g_flowScaleASquared = g_flowScaleA * g_flowScaleA; }

void InitFlowScaleB() { g_flowScaleBSquared = g_flowScaleB * g_flowScaleB; }

void InitFlowRangeSpan() { g_flowRangeSpan = g_flowRangeEnd - g_flowRangeStart - kRangeSlack; }

// ---- the state machine ----------------------------------------------------------------------------------------------------

void BattleFlow::Step(int* stepped) {
    *stepped = 0;
    const bool strategicState = m_state >= 1 && m_state <= 0xc;
    const bool tacticalState = m_state >= 0x14 && m_state <= 0x17;
    int runnable = strategicState || tacticalState;
    if (!runnable) return;
    if (strategicState) runnable = g_gameStepDriver != nullptr;
    if (!runnable) return;

    ObjectKey rootKey;
    const int previousState = m_state;
    int allVoted = 1;
    int allConfirmed = 1;
    int unanimous = 1;
    int status = runnable;

    switch (previousState) {
        case kFlowInit:
            if (g_gameStepDriver->StepSetup(&status)) {
                AppForwardToStrategicWindow();
                m_state = kFlowStrategic;
            }
            break;
        case kFlowStrategic:
            if (g_gameStepDriver->Step(reinterpret_cast<uint32_t*>(stepped))) m_state = kFlowAllBattleStart;
            break;
        case kFlowAllBattleStart:
            g_gameStepDriver->m_hold = 1;
            m_state = kFlowBattleStart;
            break;
        case kFlowBattleStart:
            status = StartPendingGameBattle();
            if (status != 0) {
                status = g_gameStepDriver->GetRootKey(&rootKey);
                if (status != 0) m_state = rootKey.IsSet() ? kFlowBattleChooseType : kFlowAllBattleFinish;
            }
            break;
        case kFlowBattleChooseType:
            status = g_gameStepDriver->GetRootKey(&rootKey);
            if (status != 0) UiBeginVoteAll(GameHookTickCount(), &rootKey);
            m_state = kFlowBattleWaitForType;
            break;
        case kFlowBattleWaitForType:
            g_gameStepDriver->Step(reinterpret_cast<uint32_t*>(stepped));
            UiExchangeVotesTimed(&allVoted, &unanimous, &m_voteTimeA, &m_voteTimeB);
            if (allVoted != 0) m_state = (unanimous != 0) + kFlowConvertToTactical;
            break;
        case kFlowConvertToTactical: {
            if (g_gameStepDriver) {
                BattleSetup setup;
                setup.m_driver = g_gameStepDriver;
                setup.m_abstract = 0;
                setup.m_timeA = m_voteTimeA;
                setup.m_timeB = m_voteTimeB;
                setup.Convert();
            }
            {
                String saveName;
                FUN_00409ec0(this, &saveName);
                ScopedOfstream file;
                OpenOutputFile(file.get(), saveName.m_text);
                if (GetFileError() == 0) {
                    HCURSOR previousCursor = showWaitCursor();
                    FUN_00409690(this, file.get());
                    SetCursor(previousCursor);
                }
                CloseFileStream(file.get());
            }
            DestroyGameStepDriver();
            TacticalStart();
            m_state = kFlowTactical;
            break;
        }
        case kFlowConvertToAbstract:
            if (g_gameStepDriver) {
                BattleSetup setup;
                setup.m_driver = g_gameStepDriver;
                setup.m_abstract = 1;
                setup.m_timeA = m_voteTimeA;
                setup.m_timeB = m_voteTimeB;
                setup.Convert();
            }
            TacticalStart();
            m_state = kFlowTacticalAbstract;
            break;
        case kFlowBattleFinish:
            status = RunGameFollowUps();
            m_state = kFlowBattleChooseDone;
            break;
        case kFlowBattleChooseDone:
            status = g_gameStepDriver->GetRootKey(&rootKey);
            if (status != 0) UiResetVoteAll(GameHookTickCount(), &rootKey);
            m_state = kFlowBattleWaitForDone;
            break;
        case kFlowBattleWaitForDone:
            g_gameStepDriver->Step(reinterpret_cast<uint32_t*>(stepped));
            UiExchangeVotes(&allConfirmed);
            if (allConfirmed != 0) m_state = kFlowBattleStart;
            break;
        case kFlowAllBattleFinish:
            g_gameStepDriver->ClearHold();
            UiResumePendingRequest();
            m_state = kFlowStrategic;
            break;
        case kFlowTactical:
            if (GetTacticalGame()->Update(g_tacticalFrameMilliseconds)) m_state = kFlowTacticalToStrategic;
            break;
        case kFlowTacticalAbstract:
            if (GetTacticalGame()->Update(g_tacticalFrameMilliseconds)) m_state = kFlowAbstractToStrategic;
            break;
        case kFlowTacticalToStrategic: {
            int inconclusiveLive = 0;
            void* memory = AllocateChecked(0x28);
            g_gameStepDriver = memory ? new (memory) GameStepDriver() : nullptr;
            if (g_gameStepDriver && g_gameStepDriver->m_ok != 0) {
                String path;
                FUN_00409dd0(this, &path);
            }
            FUN_00409700(this);
            {
                String saveName;
                FUN_00409ec0(this, &saveName);
                DeleteGameFile(saveName.m_text);
            }
            if (g_gameStepDriver) {
                BattleResults results;
                results.m_driver = g_gameStepDriver;
                results.m_abstract = 0;
                results.Apply(&inconclusiveLive);
            }
            DestroyTacticalGame();
            if (g_gameStepDriver) RunSchedulerUntilIdle();
            m_state = 9 - (inconclusiveLive != 0);
            break;
        }
        case kFlowAbstractToStrategic: {
            int inconclusive = 0;
            if (g_gameStepDriver) {
                BattleResults results;
                results.m_driver = g_gameStepDriver;
                results.m_abstract = 1;
                results.Apply(&inconclusive);
            }
            DestroyTacticalGame();
            if (g_gameStepDriver) RunSchedulerUntilIdle();
            m_state = kFlowBattleFinish;
            break;
        }
    }
    if (m_state != previousState) {
        String stateName;
        GetBattleStateName(m_state, &stateName);
    }
}
