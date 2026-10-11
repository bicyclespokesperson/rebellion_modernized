#include <new>
#include <utility>

#include "app/battle_flow.h"
#include "app/game_session.h"
#include "app/rebapp_globals.h"
#include "app/rebellion_application.h"
#include "core/checked_new.h"
#include "core/file_util.h"
#include "core/iostream_lib.h"
#include "events/game_clock_hooks.h"
#include "events/game_manager.h"
#include "platform/win32.h"
#include "port/native/game_context.h"
#include "tactical/tactical_game.h"
#include "ui/cool_app.h"

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

void appendResource(std::string* path, int resourceId) {
    std::string part = StringFromText(LoadRcdataString(resourceId));
    *path += part;
}

reb::native::GameContext* currentGameContext() {
    auto* application = static_cast<RebellionApplication*>(g_app);
    return application ? &application->m_context : nullptr;
}

}  // namespace

// ---- static initializers ----------------------------------------------------------------------------------------

void InitFlowRange2Span() { g_flowRange2Span = g_flowRange2End - g_flowRange2Start - kRangeSlack; }

void InitFlowScaleC() { g_flowScaleCSquared = g_flowScaleC * g_flowScaleC; }

// ---- hook callbacks ---------------------------------------------------------------------------------------------

void RouteNoticeToSession(HookArg notice) {
    GameSession* session = GetGameSession();
    if (session) session->RouteNotice(reinterpret_cast<NotifPayload*>(notice));
}

const char* GetStepDriverDataDirectory() {
    GameStepDriver* driver = GetGameStepDriver();
    return driver ? driver->m_dataLocation : nullptr;
}

// ---- lifetime ---------------------------------------------------------------------------------------------------

BattleFlow::BattleFlow() : m_ok(1), m_state(1) {
    m_voteTimeA.m_value = 0;
    m_voteTimeB.m_value = 0;
}

BattleFlow::~BattleFlow() {
    m_state = 0;
    m_ok = 1;
    DestroyStepDriver();
    DestroyOwnedTacticalGame();

}

BattleFlow* GetBattleFlow() {
    reb::native::GameContext* context = currentGameContext();
    return context ? context->GetBattleFlow() : nullptr;
}

GameStepDriver* GetGameStepDriver() {
    BattleFlow* flow = GetBattleFlow();
    return flow ? flow->StepDriver() : nullptr;
}

void CreateBattleFlow() {
    DestroyTacticalGame();
    reb::native::GameContext* context = currentGameContext();
    if (!context) return;
    context->SetBattleFlow(nullptr);
    std::unique_ptr<BattleFlow> flow(NewOrNull<BattleFlow>());
    if (flow) {
        g_gameHook2AE0 = RouteNoticeToSession;
        g_gameHook2AE8 = GetStepDriverDataDirectory;
        InstallGameManagerHooks();
    }
    if (flow && flow->m_ok == 0) {
        flow->DestroyOwnedTacticalGame();
        flow.reset();
    }
    context->SetBattleFlow(std::move(flow));
}

void DestroyBattleFlow() {
    if (reb::native::GameContext* context = currentGameContext()) {
        if (BattleFlow* flow = context->GetBattleFlow()) flow->DestroyOwnedTacticalGame();
        context->SetBattleFlow(nullptr);
    } else {
        DestroyTacticalGame();
    }
}

int BattleFlow::StartGame(GameSettings* settings, int a, int b) {
    m_ok = 1;
    m_state = 0;
    DestroyStepDriver();
    DestroyOwnedTacticalGame();
    int result = 0;
    if (settings->m_field04 >= kMinGalaxySize && settings->m_field04 <= kMaxGalaxySize) {
        result = CreateStepDriverFor(settings, a, b);
        if (result != 0) m_state = 1;
    }
    return result;
}

int BattleFlow::GetAutosaveFlag() {
    int flag = 0;
    if (m_state == 2 && m_stepDriver) flag = m_stepDriver->m_unk1c;
    return flag;
}

void BattleFlow::SetAutosaveFlag(int value) {
    if (m_state == 2 && m_stepDriver) m_stepDriver->m_unk1c = value;
}

// ---- save and restore -------------------------------------------------------------------------------------------

void BattleFlow::Restore(istream* in) {
    m_ok = 1;
    m_state = 0;
    DestroyStepDriver();
    DestroyOwnedTacticalGame();
    CheckPositionMarker(in);
    ReadValue32(in, &m_ok);
    ReadValue32(in, &m_state);
    ReadGameTime(&m_voteTimeA, in);
    ReadGameTime(&m_voteTimeB, in);
    int ok = m_ok;
    if (ok != 0) {
        if (m_state == 2) {
            const int created = CreateStepDriver();
            ok = 0;
            if (created != 0) {
                CheckPositionMarker(in);
                if (m_stepDriver.get()) m_stepDriver.get()->Load(in);
                CheckPositionMarker(in);
                AppLoadStrategicState(in);
                CheckPositionMarker(in);
                m_ok = 1;
                return;
            }
        } else if (m_state == 0x14) {
            TacticalGame* game = GetOrCreateTacticalGame();
            ok = 0;
            if (game->m_unk694 != 0) {
                game = GetOrCreateTacticalGame();
                ok = 0;
                if (game->m_unk694 != 0) {
                    ok = m_stepDriver && m_stepDriver->Manager() ? m_stepDriver->Manager()->NoteDispatch(in) : 0;
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
        if (m_stepDriver.get()) m_stepDriver.get()->Save(out);
        WritePositionMarker(out);
        AppSaveStrategicState(out);
        WritePositionMarker(out);
        ok = 1;
        m_ok = ok;
        return ok;
    }
    if (m_state == 0x14) {
        ok = 0;
        if (GetOrCreateTacticalGame()->m_unk694 != 0) {
            ok = m_stepDriver && m_stepDriver->Manager() ? m_stepDriver->Manager()->NoteDispatch(out) : 0;
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
    if (m_stepDriver.get()) m_stepDriver.get()->Save(out);
    WritePositionMarker(out);
    AppSaveStrategicState(out);
    WritePositionMarker(out);
    return 1;
}

// ---- the pre-battle autosave ------------------------------------------------------------------------------------

int BattleFlow::LoadPreBattleSave() {
    std::string path;
    BuildInstalledPrefix(&path);
    appendResource(&path, kResourceSaveDirectory);
    appendResource(&path, kResourcePreBattleSaveName);
    int loaded = 0;
    ScopedIfstream file;
    OpenInputFile(file.get(), path.c_str());
    if (GetFileError() == 0) {
        HCURSOR previousCursor = showWaitCursor();
        istream* in = file.get()->In();
        CheckPositionMarker(in);
        if (m_stepDriver.get()) m_stepDriver.get()->Load(in);
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
    std::string path;
    BuildInstalledPrefix(&path);
    appendResource(&path, kResourceSaveDirectory);
    appendResource(&path, kResourcePreBattleSaveName);
    int appended = 0;
    ScopedIfstream file;
    OpenInputFile(file.get(), path.c_str());
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
    std::string path;
    BuildInstalledPrefix(&path);
    appendResource(&path, kResourceSaveDirectory);
    appendResource(&path, kResourcePreBattleSaveName);
    int extracted = 0;
    ScopedOfstream file;
    OpenOutputFile(file.get(), path.c_str());
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
    std::string path;
    path.clear();
    char* installed = GetInstalledLocation();
    if (installed) {
        std::string separator(kBackslash);
        std::string installedText = StringFromText(installed);
        path += installedText;
        path += separator;
        operator delete(installed);
    }
    appendResource(&path, kResourceSaveDirectory);
    appendResource(&path, kResourcePreBattleSaveName);
    DeleteGameFile(path.c_str());
}

// ---- file names -------------------------------------------------------------------------------------------------

void BattleFlow::BuildInstalledPrefix(std::string* out) {
    out->clear();
    char* installed = GetInstalledLocation();
    if (installed) {
        std::string separator(kBackslash);
        std::string installedText = StringFromText(installed);
        *out += installedText;
        *out += separator;
        operator delete(installed);
    }
}

void BattleFlow::BuildSaveDirectory(std::string* out) {
    BuildInstalledPrefix(out);
    appendResource(out, kResourceSaveDirectory);
}

void BattleFlow::BuildPreBattleSavePath(std::string* out) {
    BuildInstalledPrefix(out);
    appendResource(out, kResourceSaveDirectory);
    appendResource(out, kResourcePreBattleSaveName);
}

RandomGenerator g_sessionRandom{};
float g_flowScaleC = 3.0f;
float g_flowScaleCSquared{};
int g_flowRange2End = 500;
int g_flowRange2Span{};
int g_flowRange2Start = 144;
