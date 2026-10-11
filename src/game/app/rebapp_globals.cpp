#include "app/rebapp_globals.h"

#include <new>

#include "app/app_dialogs.h"
#include "app/app_externals.h"
#include "app/app_save_strings.h"
#include "app/battle_flow.h"
#include "app/rebellion_application.h"
#include "audio/music.h"
#include "port/native/audio_context.h"
#include "core/iostream_lib.h"
#include "core/registry.h"
#include "ui/cool_app.h"

namespace {

constexpr char kLucasArtsKey[] = "SOFTWARE\\LucasArts Entertainment Company LLC";
constexpr char kBackslash[] = "\\";
constexpr char kGameName[] = "Star Wars Rebellion";

RebellionApplication* App() { return static_cast<RebellionApplication*>(g_app); }

// Builds "SOFTWARE\LucasArts Entertainment Company LLC\Star Wars Rebellion" in a strstream and reads `valueName`
// from that key.
char* ReadGameRegistryValue(const char* valueName) {
    ScopedStrstream key;
    ostream* out = iostream_access::AsOstream(key.get());
    ostream_operator_ls_char(
        ostream_operator_ls_cstr(ostream_operator_ls_cstr(ostream_operator_ls_cstr(out, kLucasArtsKey), kBackslash),
                                 kGameName),
        0);
    streambuf* buffer = iostream_access::BufferOf(key.get());
    char* value = reinterpret_cast<char*>(ReadRegistryValue(strstreambuf_str(buffer), valueName));
    strstreambuf_freeze(iostream_access::BufferOf(key.get()), 0);
    ios* state = iostream_access::IosOf(key.get());
    iostream_access::Lock(state);
    state->m_state = 0;
    iostream_access::Unlock(state);
    return value;
}

}  // namespace

char* GetDllLocation() { return ReadGameRegistryValue(g_dllLocationName); }

char* GetDataLocation() { return ReadGameRegistryValue(g_dataLocationName); }

char* GetTooBigLocation() { return ReadGameRegistryValue(g_tooBigLocationName); }

char* GetInstalledLocation() { return ReadGameRegistryValue(g_installedLocationName); }

StringMap GetNetworkSettings() {
    if (g_app) return App()->CopyNetworkSettings();
    return {};
}

void AppForwardToStrategicWindow() {
    if (g_app) App()->ForwardToStrategicWindow();
}

void AppSetState(int state) {
    if (g_app) App()->SetState(state);
}

int AppGetSide() {
    int side = 0;
    if (g_app) side = App()->m_side;
    return side;
}

int AppGetGameType() {
    if (g_app) return App()->m_gameType;
    return 0;
}

void AppSetGameParams(int side, int gameType, int param8, int param10, int connectionKind, int networkMode) {
    if (g_app) App()->SetGameParams(side, gameType, param8, param10, connectionKind, networkMode);
}

int AppHasWaitedLongEnough() {
    int waited = 0;
    if (g_app) waited = App()->HasWaitedLongEnough();
    return waited;
}

void AppResetWaitTimer() {
    if (g_app) {
        App()->m_waitElapsed = 0;
        App()->m_waitTimer.Reset();
    }
}

void AppShowQuestionDialog(int message, int textA, int textB, int x, int y) {
    if (g_app) ShowAppDialog(message, g_app, textA, textB, x, y);
}

void AppShowNoticeDialog(int message, int a, int textA, int textB, int x, int y) {
    if (g_app) ShowAppNoticeDialog(message, g_app, a, textA, textB, x, y);
}

void AppPostDialogResult(int accepted, CoolWindow* app) {
    if (app || (app = g_app)) {
        PostMessageA(app->m_hwnd, kMsgActivated, 0, 0);
        PostMessageA(app->m_hwnd, kMsgChildDone, 0xd, 0);
        PostMessageA(app->m_hwnd, kMsgDialogEnded - (accepted != 0), g_appDialogMessage, 0);
    }
}

void AppShowAckDialog(int a, int textA, int x, int y) {
    if (g_app) ShowAppWaitDialog(g_app, a, textA, x, y);
}

void AppEnableSpeedAfterDialog() {
    if (g_app) App()->OnDialogEndedEnableSpeed();
}

int AppSaveStrategicState(void* stream) {
    int saved = 0;
    if (g_app) saved = App()->SaveStrategicState(stream);
    return saved;
}

int AppLoadStrategicState(void* stream) {
    int loaded = 0;
    if (g_app) loaded = App()->LoadStrategicState(stream);
    return loaded;
}

int AppSaveTacticalState(void* stream) {
    int saved = 0;
    if (g_app) saved = App()->SaveTacticalState(stream);
    return saved;
}

int AppLoadTacticalState(void* stream) {
    int loaded = 0;
    if (g_app) loaded = App()->LoadTacticalState(stream);
    return loaded;
}

void AppOpenOptions() {
    if (g_app) App()->SetState(kStateOptionsMenu);
}

void AppProcessSaveLoad() {
    if (g_app) App()->ProcessPendingSaveLoad();
}

int AppIsSaveLoadPending() {
    int pending = 0;
    if (g_app) pending = App()->IsSessionActive();
    return pending;
}

void AppRequestSave(int slot, String name) {
    if (g_app) {
        App()->m_pendingSaveSlot = slot;
        App()->m_pendingSaveName.Assign(name);
    }
}

void AppRequestLoad(int slot) {
    if (g_app) App()->m_pendingLoadSlot = slot;
}

void AppSendSaveCommand(int a, int b, String name, int d) {
    if (g_app) App()->SendSaveCommand(a, b, name, d);
}

void AppSetAutosaveTick(int tick) {
    if (g_app) App()->m_autosaveTick = tick;
}

SavedGameInfo AppReadSaveHeader(int slot, int isNetworkGame) {
    SavedGameInfo info;
    if (g_app) {
        info = App()->ReadSaveHeader(slot, isNetworkGame);
    }
    return SavedGameInfo(info);
}

int AppGetSavedGameType(int slot, int isNetworkGame) {
    if (g_app) return App()->GetSavedGameType(slot, isNetworkGame);
    return isNetworkGame;
}

void AppPromptPause() {
    if (g_app) App()->PromptForPause();
}

void AppShowResumeNotice() {
    if (g_app) App()->ShowResumeNotice();
}

void AppPlayClickSound() {
    if (g_app) App()->PlayClickSound();
}

int AppGetUnknown138() {
    int value = 0;
    if (g_app) value = App()->m_unknown138;
    return value;
}

void AppSetUnknown138(int value) {
    if (g_app) App()->m_unknown138 = value;
}

void AppPlayMovie(int movieId, const String* caption, void(* callback)(uint32_t, uint32_t, int)) {
    if (g_app) App()->PlayMovie(movieId, caption, callback);
}

int AppShowHelp() {
    int shown = 0;
    if (g_app) shown = App()->ShowHelp();
    return shown;
}

uint32_t AppGetDisplayOptions() {
    uint32_t options = 0x100;
    if (g_app) options = App()->m_displayOptions;
    return options;
}

uint32_t AppGetDisplayOptions2() {
    uint32_t options = 0x13110000;
    if (g_app) options = App()->m_displayOptions2;
    return options;
}

void AppSetDisplayOptions(uint32_t options) {
    if (g_app) App()->SetDisplayOptions(options);
}

void AppSetDisplayOptions2(uint32_t options) {
    if (g_app) App()->m_displayOptions2 = options;
}

void AppSetVolume(int volume) {
    if (g_app) App()->SetVolume(volume);
}

int AppGetVolume() {
    int volume = 0;
    if (g_app) volume = App()->m_volume;
    return volume;
}

void AppSetMusicVolume(int volume) { SetMusicVolume(volume); }

void AppSetMusicSwitch(int enabled) { SetMusicSwitch(enabled); }

int AppGetMusicVolume() {
    const reb::native::AudioContext* audio = reb::native::CurrentAudioContext();
    return audio ? audio->musicVolume : 0;
}

void AppDestroyTacticalWindow() {
    if (g_app) App()->DestroyTacticalWindow();
}

uint32_t AppGetCommandLineFlags() {
    uint32_t flags = 0;
    if (g_app) flags = App()->m_commandLineFlags;
    return flags;
}

int AppGetNetworkMode() {
    int mode = 0;
    if (g_app) mode = App()->m_networkMode;
    return mode;
}

void AppRestoreGame(void* stream) {
    BattleFlow* battleFlow = GetBattleFlow();
    if (battleFlow) battleFlow->Restore(static_cast<istream*>(stream));
}

void AppStartGame(void* setup, int gameType, int side) {
    BattleFlow* battleFlow = GetBattleFlow();
    if (battleFlow) battleFlow->StartGame(static_cast<GameSettings*>(setup), gameType, side);
}

void AppSaveGame(void* stream) {
    BattleFlow* battleFlow = GetBattleFlow();
    if (battleFlow) battleFlow->Save(static_cast<ostream*>(stream));
}

void AppStepGame(int* stepped) {
    BattleFlow* battleFlow = GetBattleFlow();
    *stepped = 1;
    if (battleFlow) battleFlow->Step(stepped);
}

void AppReleaseBattleFlow() {
    BattleFlow* battleFlow = GetBattleFlow();
    if (battleFlow) battleFlow->DeletePreBattleSave();
}

int AppGetBattleFlowState() {
    int state = 0;
    BattleFlow* battleFlow = GetBattleFlow();
    if (battleFlow) state = battleFlow->m_state;
    return state;
}

int AppIsGameRunning() {
    int running = 0;
    BattleFlow* battleFlow = GetBattleFlow();
    if (battleFlow) running = battleFlow->GetAutosaveFlag();
    return running;
}

void AppSetAutosaveFlag(int value) {
    BattleFlow* battleFlow = GetBattleFlow();
    if (battleFlow) battleFlow->SetAutosaveFlag(value);
}
