#include <new>

#include "app/battle_flow.h"
#include "app/game_session.h"
#include "app/rebapp_globals.h"
#include "core/checked_new.h"
#include "core/file_util.h"
#include "core/iostream_lib.h"
#include "core/vptr_store.h"
#include "events/game_clock_hooks.h"
#include "events/game_manager.h"
#include "platform/win32.h"
#include "tactical/tactical_game.h"

// The battle-flow object's lifetime, its save and restore, and the pre-battle autosave. A 3D battle destroys the
// strategic game while it runs, so the strategic game is written to a file when the battle starts (BattleFlow::Step
// state 7) and read back from it afterwards; a save made during the battle carries that file inside it.

extern float g_flowScaleC;         // 0x6585d0
extern float g_flowScaleCSquared;  // 0x6b1200
extern int g_flowRange2Start;      // 0x6585c0
extern int g_flowRange2End;        // 0x6585c8
extern int g_flowRange2Span;       // 0x6b1204

namespace {

constexpr int kWaitCursorId = 0x7f02;
constexpr int kResourceSaveDirectory = 0x610;
constexpr int kResourcePreBattleSaveName = 0x613;
constexpr char kBackslash[] = "\\";
constexpr int kMinGalaxySize = 1;
constexpr int kMaxGalaxySize = 3;
constexpr int kRangeSlack = 5;

HCURSOR showWaitCursor() { return SetCursor(LoadCursorA(nullptr, MAKEINTRESOURCEA(kWaitCursorId))); }

void appendResource(String* path, int resourceId) {
    String part(LoadRcdataString(resourceId));
    *path += part;
}

void deleteStepDriver() {
    if (g_gameStepDriver) {
        delete g_gameStepDriver;
        g_gameStepDriver = nullptr;
    }
}

void deleteBattleFlow() {
    if (g_battleFlow) {
        delete g_battleFlow;
        g_battleFlow = nullptr;
    }
}

}  // namespace

// ---- static initializers ----------------------------------------------------------------------------------------

void InitFlowRange2Span() { g_flowRange2Span = g_flowRange2End - g_flowRange2Start - kRangeSlack; }

void InitFlowScaleC() { g_flowScaleCSquared = g_flowScaleC * g_flowScaleC; }

// ---- hook callbacks ---------------------------------------------------------------------------------------------

void RouteNoticeToSession(int notice) {
    GameSession* session = GetGameSession();
    if (session) session->RouteNotice(reinterpret_cast<PlayerNotice*>(static_cast<intptr_t>(notice)));
}

const char* GetStepDriverDataDirectory() { return g_gameStepDriver ? g_gameStepDriver->m_dataLocation : nullptr; }

// ---- lifetime ---------------------------------------------------------------------------------------------------

BattleFlow::BattleFlow() : m_ok(1), m_state(1) {
    m_voteTimeA.m_value = 0;
    m_voteTimeB.m_value = 0;
}

BattleFlow::~BattleFlow() {
    m_state = 0;
    m_ok = 1;
    deleteStepDriver();
    DestroyTacticalGame();
    REB_KEEP_VPTR_STORE();
}

BattleFlow* GetBattleFlow() { return g_battleFlow; }

void CreateBattleFlow() {
    DestroyTacticalGame();
    deleteBattleFlow();
    BattleFlow* flow = NewOrNull<BattleFlow>();
    if (flow) {
        g_gameHook2AE0 = RouteNoticeToSession;
        g_gameHook2AE8 = GetStepDriverDataDirectory;
        InstallGameManagerHooks();
    }
    g_battleFlow = flow;
    if (flow && flow->m_ok == 0) {
        DestroyTacticalGame();
        deleteBattleFlow();
    }
}

void DestroyBattleFlow() {
    DestroyTacticalGame();
    deleteBattleFlow();
}

int BattleFlow::StartGame(GameSettings* settings, int a, int b) {
    m_ok = 1;
    m_state = 0;
    deleteStepDriver();
    DestroyTacticalGame();
    int result = 0;
    if (settings->m_field04 >= kMinGalaxySize && settings->m_field04 <= kMaxGalaxySize) {
        result = CreateGameStepDriverFor(settings, a, b);
        if (result != 0) m_state = 1;
    }
    return result;
}

int BattleFlow::GetAutosaveFlag() {
    int flag = 0;
    if (m_state == 2 && g_gameStepDriver) flag = g_gameStepDriver->m_unk1c;
    return flag;
}

void BattleFlow::SetAutosaveFlag(int value) {
    if (m_state == 2 && g_gameStepDriver) g_gameStepDriver->m_unk1c = value;
}

// ---- save and restore -------------------------------------------------------------------------------------------

void BattleFlow::Restore(istream* in) {
    m_ok = 1;
    m_state = 0;
    deleteStepDriver();
    DestroyTacticalGame();
    CheckPositionMarker(in);
    ReadValue32(in, &m_ok);
    ReadValue32(in, &m_state);
    ReadGameTime(&m_voteTimeA, in);
    ReadGameTime(&m_voteTimeB, in);
    int ok = m_ok;
    if (ok != 0) {
        if (m_state == 2) {
            const int created = CreateGameStepDriver();
            ok = 0;
            if (created != 0) {
                CheckPositionMarker(in);
                if (g_gameStepDriver) g_gameStepDriver->Load(in);
                CheckPositionMarker(in);
                AppLoadStrategicState(in);
                CheckPositionMarker(in);
                m_ok = 1;
                return;
            }
        } else if (m_state == 0x14) {
            TacticalGame* game = GetTacticalGame();
            ok = 0;
            if (game->m_unk694 != 0) {
                game = GetTacticalGame();
                ok = 0;
                if (game->m_unk694 != 0) {
                    ok = g_gameManager->NoteDispatch(in);
                    if (ok != 0) {
                        ok = AppLoadTacticalState(in);
                        if (ok != 0) {
                            ok = ExtractPreBattleSave(in);
                            if (ok != 0) ok = game->ResetBattleView();
                        }
                    }
                }
            }
        }
    }
    m_ok = ok;
}

int BattleFlow::Save(ostream* out) {
    int ok = 1;
    WritePositionMarker(out);
    WriteValue32(out, &m_ok);
    WriteValue32(out, &m_state);
    StreamWrite32(out, &m_voteTimeA);
    StreamWrite32(out, &m_voteTimeB);
    if (m_state == 2) {
        WritePositionMarker(out);
        if (g_gameStepDriver) g_gameStepDriver->Save(out);
        WritePositionMarker(out);
        AppSaveStrategicState(out);
        WritePositionMarker(out);
        ok = 1;
        m_ok = ok;
        return ok;
    }
    if (m_state == 0x14) {
        ok = 0;
        if (GetTacticalGame()->m_unk694 != 0) {
            ok = g_gameManager->NoteDispatch(out);
            if (ok != 0) {
                ok = AppSaveTacticalState(out);
                if (ok != 0) ok = AppendPreBattleSave(out);
            }
        }
    }
    m_ok = ok;
    return ok;
}

int BattleFlow::SaveStrategic(ostream* out) {
    WritePositionMarker(out);
    if (g_gameStepDriver) g_gameStepDriver->Save(out);
    WritePositionMarker(out);
    AppSaveStrategicState(out);
    WritePositionMarker(out);
    return 1;
}

// ---- the pre-battle autosave ------------------------------------------------------------------------------------

int BattleFlow::LoadPreBattleSave() {
    String path;
    BuildInstalledPrefix(&path);
    appendResource(&path, kResourceSaveDirectory);
    appendResource(&path, kResourcePreBattleSaveName);
    int loaded = 0;
    ScopedIfstream file;
    OpenInputFile(file.get(), path.m_text);
    if (GetFileError() == 0) {
        HCURSOR previousCursor = showWaitCursor();
        istream* in = reinterpret_cast<istream*>(file.get());
        CheckPositionMarker(in);
        if (g_gameStepDriver) g_gameStepDriver->Load(in);
        CheckPositionMarker(in);
        AppLoadStrategicState(in);
        CheckPositionMarker(in);
        loaded = 1;
        SetCursor(previousCursor);
    }
    CloseFileStream(file.get());
    return loaded;
}

int BattleFlow::AppendPreBattleSave(ostream* out) {
    String path;
    BuildInstalledPrefix(&path);
    appendResource(&path, kResourceSaveDirectory);
    appendResource(&path, kResourcePreBattleSaveName);
    int appended = 0;
    ScopedIfstream file;
    OpenInputFile(file.get(), path.m_text);
    if (GetFileError() == 0) {
        HCURSOR previousCursor = showWaitCursor();
        ostream_operator_ls_streambuf(out, iostream_access::IosOf(file.get())->m_bp);
        SetCursor(previousCursor);
        appended = 1;
    }
    CloseFileStream(file.get());
    return appended;
}

int BattleFlow::ExtractPreBattleSave(istream* in) {
    String path;
    BuildInstalledPrefix(&path);
    appendResource(&path, kResourceSaveDirectory);
    appendResource(&path, kResourcePreBattleSaveName);
    int extracted = 0;
    ScopedOfstream file;
    OpenOutputFile(file.get(), path.m_text);
    if (GetFileError() == 0) {
        HCURSOR previousCursor = showWaitCursor();
        istream_operator_rs_streambuf(in, iostream_access::IosOf(file.get())->m_bp);
        SetCursor(previousCursor);
        extracted = 1;
    }
    CloseFileStream(file.get());
    return extracted;
}

void BattleFlow::DeletePreBattleSave() {
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
    appendResource(&path, kResourceSaveDirectory);
    appendResource(&path, kResourcePreBattleSaveName);
    DeleteGameFile(path.m_text);
}

// ---- file names -------------------------------------------------------------------------------------------------

void BattleFlow::BuildInstalledPrefix(String* out) {
    out->Clear();
    char* installed = GetInstalledLocation();
    if (installed) {
        String separator(kBackslash);
        String installedText(installed);
        *out += installedText;
        *out += separator;
        operator delete(installed);
    }
}

void BattleFlow::BuildSaveDirectory(String* out) {
    BuildInstalledPrefix(out);
    appendResource(out, kResourceSaveDirectory);
}

void BattleFlow::BuildPreBattleSavePath(String* out) {
    BuildInstalledPrefix(out);
    appendResource(out, kResourceSaveDirectory);
    appendResource(out, kResourcePreBattleSaveName);
}
