#include <new>

#include "app/app_control_cmds.h"
#include "app/app_externals.h"
#include "app/app_save_strings.h"
#include "app/rebapp_globals.h"
#include "app/rebellion_application.h"
#include "audio/music.h"
#include "audio/sound_manager.h"
#include "core/file_util.h"
#include "core/iostream_lib.h"
#include "core/registry.h"
#include "events/game_clock_hooks.h"
#include "strategy_ui/game_options_window.h"
#include "strategy_ui/game_ui_facade.h"
#include "tactical/comm2.h"
#include "ui/cool_app.h"

// Save and load of games, the small state hooks the other screens call, and a few settings. The save file starts
// with a SavedGameInfo header followed by the battle-flow object's state.

namespace {

constexpr char kBackslash[] = "\\";
constexpr char kHelpFile[] = "swhelp.hlp";
constexpr int kAutosaveSlot = 6;
constexpr int kResourceSaveDirectory = 0x610;
constexpr int kResourceSaveFileLocal = 0x611;
constexpr int kResourceSaveFileNetwork = 0x612;
constexpr int kResourceDllSlot = 2;
constexpr int kNoticePausedMessage = 0x46c;
constexpr int kSpeedAckText = 0x1206;

HCURSOR ShowWaitCursor() { return SetCursor(LoadCursorA(nullptr, MAKEINTRESOURCEA(0x7f02))); }

void ClearStreamState(void* stream) {
    ios* state = iostream_access::IosOf(stream);
    iostream_access::Lock(state);
    state->m_state = 0;
    iostream_access::Unlock(state);
}

void RetireActiveWindow(RebellionApplication* app) {
    CoolWindow* retired = app->m_activeWindow;
    if (!retired) return;
    retired->Hide();
    if (app->m_strategicWindow == retired) app->m_strategicWindow = nullptr;
    if (app->m_tacticalWindow == retired) app->m_tacticalWindow = nullptr;
    PostMessageA(app->m_hwnd, kMsgDestroyWindow, 0, reinterpret_cast<LPARAM>(retired));
    app->m_activeWindow = nullptr;
}

void DestroyStrategicWindowObject(RebellionApplication* app) {
    CoolWindow* window = app->m_strategicWindow;
    if (!window) return;
    if (app->m_activeWindow == window) app->m_activeWindow = nullptr;
    BOOL destroyed = DestroyWindow(window->m_hwnd);
    if (destroyed && app->m_strategicWindow) delete app->m_strategicWindow;
    app->m_strategicWindow = nullptr;
}

void DestroyTacticalWindowObject(RebellionApplication* app) {
    CoolWindow* window = app->m_tacticalWindow;
    if (!window) return;
    if (app->m_activeWindow == window) app->m_activeWindow = nullptr;
    DestroyWindow(window->m_hwnd);
    if (app->m_tacticalWindow) delete app->m_tacticalWindow;
    app->m_tacticalWindow = nullptr;
}

void DestroyAllScreensForLoad(RebellionApplication* app) {
    RetireActiveWindow(app);
    DestroyStrategicWindowObject(app);
    DestroyTacticalWindowObject(app);
}

}  // namespace

// ---- Window state saved with the game ------------------------------------------------------------------------

int RebellionApplication::SaveStrategicState(void* stream) {
    StrategicWindow* window = static_cast<StrategicWindow*>(m_strategicWindow);
    if (window) window->Virtual0F(stream);
    return window != nullptr;
}

int RebellionApplication::LoadStrategicState(void* stream) {
    DestroyAllScreensForLoad(this);
    m_connected = 1;
    m_pauseRequested = 0;
    m_peersReady = 0;
    SetState(kStateStrategic);
    StrategicWindow* window = static_cast<StrategicWindow*>(m_strategicWindow);
    if (window) {
        window->Virtual10(stream);
        UiAckSync(m_side);
    }
    return window != nullptr;
}

int RebellionApplication::SaveTacticalState(void* stream) {
    TacticalWindow* window = static_cast<TacticalWindow*>(m_tacticalWindow);
    if (window) FUN_0059cb60(window, stream);
    return window != nullptr;
}

int RebellionApplication::LoadTacticalState(void* stream) {
    DestroyAllScreensForLoad(this);
    SetState(kStateTactical);
    TacticalWindow* window = static_cast<TacticalWindow*>(m_tacticalWindow);
    if (window) FUN_0059cb80(window, stream);
    return window != nullptr;
}

// ---- Save file names and headers --------------------------------------------------------------------------------

void SavedGameInfo::SetName(String name) { m_name.Assign(name); }

String RebellionApplication::MakeSaveFileName(int slot, int isNetworkGame) {
    String name;
    ScopedIfstream unusedFile;
    ScopedStrstream text;
    if (slot != 0) {
        ostream* out = iostream_access::AsOstream(text.get());
        char* installed = GetInstalledLocation();
        if (installed) {
            ostream_operator_ls_cstr(ostream_operator_ls_cstr(out, installed), kBackslash);
            operator delete(installed);
        }
        ostream_operator_ls_cstr(out, LoadRcdataString(kResourceSaveDirectory));
        bool networkFile = isNetworkGame || (m_networkMode == 7 && m_gameType == 2) || g_isNetworkGame;
        unsigned fileResource = networkFile ? kResourceSaveFileNetwork : kResourceSaveFileLocal;
        ostream* afterFile = ostream_operator_ls_cstr(out, LoadRcdataString(fileResource));
        ostream_operator_ls_char(ostream_operator_ls_long(afterFile, slot), 0);
        name = strstreambuf_str(iostream_access::BufferOf(text.get()));
        strstreambuf_freeze(iostream_access::BufferOf(text.get()), 0);
        ClearStreamState(text.get());
    }
    return String(name);
}

SavedGameInfo RebellionApplication::ReadSaveHeader(int slot, int isNetworkGame) {
    SavedGameInfo info;
    String fileName = MakeSaveFileName(slot, isNetworkGame);
    ScopedIfstream file;
    OpenInputFile(file.get(), fileName.m_text);
    if (GetFileError() == 0) info.Read(reinterpret_cast<istream*>(file.get()));
    CloseFileStream(file.get());
    return SavedGameInfo(info);
}

String RebellionApplication::GetSaveName(int slot, int isNetworkGame) {
    String name;
    SavedGameInfo info = ReadSaveHeader(slot, isNetworkGame);
    name.Assign(info.GetName());
    return String(name);
}

int RebellionApplication::GetSavedGameType(int slot, int isNetworkGame) {
    SavedGameInfo info = ReadSaveHeader(slot, isNetworkGame);
    return info.m_gameType;
}

// ---- Loading and saving ------------------------------------------------------------------------------------------

int RebellionApplication::LoadPendingGame() {
    int loaded = 0;
    UiSetPausePending(0);
    int isNetworkGame = m_networkMode == 7 && m_gameType == 2;
    String fileName = MakeSaveFileName(m_pendingLoadSlot, isNetworkGame);
    m_pendingLoadSlot = 0;
    ScopedIfstream file;
    OpenInputFile(file.get(), fileName.m_text);
    if (GetFileError() == 0) {
        HCURSOR previousCursor = ShowWaitCursor();
        SavedGameInfo info;
        info.Read(reinterpret_cast<istream*>(file.get()));
        m_unknown130 = info.m_unknown10;
        m_side = info.m_side;
        m_unknown134 = info.m_unknown14;
        m_gameType = info.m_gameType;
        m_autosaveTick = 0;
        if (CoolGetResourceModule(kResourceDllSlot) == nullptr) {
            char* dllLocation = GetDllLocation();
            CoolRegisterResourceDll(dllLocation, g_textstraDllName, kResourceDllSlot);
            if (dllLocation) operator delete(dllLocation);
        }
        FUN_00401b50();
        AppRestoreGame(file.get());
        loaded = 1;
        SetCursor(previousCursor);
    }
    CloseFileStream(file.get());
    return loaded;
}

int RebellionApplication::SavePendingGame() {
    int saved = 0;
    int slot = m_pendingSaveSlot;
    int isNetworkGame = m_networkMode == 7 && m_gameType == 2;
    String fileName = MakeSaveFileName(slot, isNetworkGame);
    String name(m_pendingSaveName);
    m_pendingSaveName.Assign(String());
    if (!name.m_text) {
        isNetworkGame = m_networkMode == 7 && m_gameType == 2;
        name.Assign(GetSaveName(slot, isNetworkGame));
        if (!name.m_text) name.AppendSpaces(1);
    }
    m_pendingSaveSlot = 0;
    ScopedOfstream file;
    OpenOutputFile(file.get(), fileName.m_text);
    if (GetFileError() == 0) {
        HCURSOR previousCursor = ShowWaitCursor();
        SavedGameInfo info;
        info.m_gameType = m_gameType;
        info.m_unknown10 = 1;
        info.m_unknown14 = 0;
        info.m_slot = slot;
        info.SetName(name);
        info.m_side = m_side;
        info.m_unknown24 = m_autosaveTick;
        info.Write(reinterpret_cast<ostream*>(file.get()));
        AppSaveGame(file.get());
        saved = 1;
        SetCursor(previousCursor);
    }
    CloseFileStream(file.get());
    if ((m_state == kStateOptionsMenu || m_state == kStateOptionsInGame) && m_activeWindow)
        static_cast<GameOptionsWindow*>(m_activeWindow)->GameOptionsWindow::OnSaveResult(saved, slot);
    return saved;
}

void RebellionApplication::ProcessPendingSaveLoad() {
    if (!(m_commandLineFlags & kAppNoAutosave) && m_pendingSaveSlot == 0 && AppIsGameRunning()) {
        AppSetAutosaveFlag(0);
        String autosaveName(g_autosaveName);
        m_pendingSaveSlot = kAutosaveSlot;
        m_pendingSaveName.Assign(autosaveName);
        m_autosaveTick = GameHookTickCount();
    }
    if (m_pendingLoadSlot != 0) LoadPendingGame();
    if (m_pendingSaveSlot != 0) SavePendingGame();
}

int RebellionApplication::IsSessionActive() {
    int active = 0;
    if (m_pendingLoadSlot != 0 || m_pendingSaveSlot != 0 || AppIsGameRunning()) active = 1;
    return active;
}

void RebellionApplication::SendSaveCommand(int a, int b, String name, int d) {
    void* commandMemory = operator new(0x38);
    AppCommand* command = commandMemory ? reinterpret_cast<AppCommand*>(new (commandMemory) MpPlayerInfoMsg()) : nullptr;
    if (!command) return;
    command->m_unknown24 = b;
    command->m_unknown20 = a;
    command->SetName(name);
    command->m_unknown34 = d;
    void* executorMemory = operator new(0x1c);
    CommandExecutor* executor = executorMemory ? static_cast<CommandExecutor*>(new (executorMemory) MessageCodec()) : nullptr;
    if (!executor) return;
    static_cast<MessageCodec*>(executor)->MessageCodec::Serialize(reinterpret_cast<MessageBase*>(command));
    if (executor->m_valid) {
        CommSendMessage(executor->m_buffer, executor->m_size, 4, 1);
        CommFlushAll();
    }
    delete executor;
    delete command;
}

void AppCommand::SetName(String name) { m_name.Assign(name); }

// ---- Pause, speed and notices ----------------------------------------------------------------------------------

int RebellionApplication::HasWaitedLongEnough() {
    if (m_waitElapsed == 0 && m_waitTimer.ElapsedSeconds() > 0.25f) m_waitElapsed = 1;
    return m_waitElapsed;
}

void RebellionApplication::PromptForPause() {
    if (m_state < kStateOptionsMenu || m_state > kStateOptionsInGame) {
        if (UiGetInputActive() == 0) AppShowNoticeDialog(kNoticePausedMessage, 0, 0x1203, 0x190d, 500, 0x50);
        UiSetPausePending(1);
    }
}

void RebellionApplication::PlayClickSound() { AppConfirmWaitDialog(); }

void RebellionApplication::OnDialogEndedEnableSpeed() {
    if (m_gameType == 2) {
        if (m_state != kStateStrategic) return;
        UiAckSync(UiGetHumanSide());
        if (static_cast<StrategicWindow*>(m_strategicWindow)->m_unknownAc != 0) return;
        if ((UiAreAllSynced() == 0 && UiIsPauseTick() != 0) || UiGetHold() != 0)
            AppShowAckDialog(0, kSpeedAckText, 500, 0x50);
    } else if (m_gameType == 1) {
        UiAckSync(UiGetHumanSide());
    }
}

void RebellionApplication::ShowResumeNotice() {
    if (m_gameType == 2) AppShowAckDialog(0, kSpeedAckText, 500, 0x50);
}

// ---- Help, settings and display ------------------------------------------------------------------------------

BOOL RebellionApplication::ShowHelp() {
    ScopedStrstream path;
    ostream* out = iostream_access::AsOstream(path.get());
    char* dllLocation = GetDllLocation();
    // BUG(original): the heap copy of the location is never freed.
    if (dllLocation) ostream_operator_ls_cstr(ostream_operator_ls_cstr(out, dllLocation), kBackslash);
    ostream_operator_ls_char(ostream_operator_ls_cstr(out, kHelpFile), 0);
    return WinHelpA(m_hwnd, strstreambuf_str(iostream_access::BufferOf(path.get())), HELP_FINDER, 0);
}

void RebellionApplication::SetVolume(int volume) {
    m_volume = volume;
    WriteRegistryDword(g_localMachineRootName, g_registryKeyPath, g_volumeName, -volume);
    SoundManager::SetDefaultVolume(m_volume);
}

void RebellionApplication::SetDisplayOptions(uint32_t options) {
    m_displayOptions = options;
    WriteRegistryDword(g_localMachineRootName, g_registryKeyPath, g_displayOptsName, options);
}

void RebellionApplication::SetIdentityPaletteForDisplay() {
    HDC dc = GetDC(m_hwnd);
    if (!dc) return;
    if (GetDeviceCaps(dc, RASTERCAPS) & RC_PALETTE) {
        LOGPALETTE header;
        header.palVersion = 0x300;
        header.palNumEntries = 1;
        HPALETTE palette = CreatePalette(&header);
        int reservedColors = GetDeviceCaps(dc, NUMCOLORS);
        if (GetDeviceCaps(dc, SIZEPALETTE) == 256) {
            PALETTEENTRY entries[256];
            SetSystemPaletteUse(dc, SYSPAL_NOSTATIC);
            SetSystemPaletteUse(dc, SYSPAL_STATIC);
            GetSystemPaletteEntries(dc, 0, 256, entries);
            int half = reservedColors / 2;
            for (int i = 0; i < half; ++i) entries[i].peFlags = 0;
            int next = half;
            if (half < 256 - half) {
                for (int i = half; i < 256 - half; ++i) entries[i] = PALETTEENTRY{0, 0, 0, 0};
                next = 256 - half;
            }
            for (int i = next; i < 256; ++i) entries[i].peFlags = 0;
            if (palette) {
                if (ResizePalette(palette, 256)) {
                    SetPaletteEntries(palette, 0, 256, entries);
                    HPALETTE previous = SelectPalette(dc, palette, 0);
                    RealizePalette(dc);
                    SelectPalette(dc, previous, 0);
                }
                DeleteObject(palette);
            }
        }
    }
    ReleaseDC(m_hwnd, dc);
}

StringMap* RebellionApplication::CopyNetworkSettings(StringMap* out) {
    new (out) StringMap(m_networkSettings);
    return out;
}

// ---- Static initialisers (constants no other function reads) ----------------------------------------------------

extern uint32_t g_unknown6b145c;
extern uint32_t g_unknown6b1464;

void StaticInitUnknown6b145c() { g_unknown6b145c = 0x41100000; }

void StaticInitUnknown6b1464() { g_unknown6b1464 = 0x15f; }
