// Headless simulation harness: loads a saved game the way RebellionApplication::LoadPendingGame does (minus the strategic
// window state, which belongs to the UI), then steps the battle flow (AppStepGame, the same call as the main loop) with
// the 0.25 s wait timer forced on, so no wall clock, message pump or input is involved, and saves the game.
// A "tick" here is one advance of the game manager's tick counter plus the rest of that tick's frame phases.
#include <windows.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/battle_flow.h"
#include "app/rebapp_globals.h"
#include "app/rebellion_application.h"
#include "core/file_util.h"
#include "core/iostream_lib.h"
#include "events/game_clock_hooks.h"
#include "events/game_step_driver.h"

#include "port/sim_harness.h"

namespace {

struct SimOptions {
    char load[MAX_PATH] = "";
    char save[MAX_PATH] = "";
    char log[MAX_PATH] = "";
    uint32_t ticks = 0;
    uint32_t every = 0;
};

HANDLE g_log = INVALID_HANDLE_VALUE;

void Log(const char* format, ...) {
    char line[512];
    va_list args;
    va_start(args, format);
    const int n = vsnprintf(line, sizeof line - 1, format, args);
    va_end(args);
    if (g_log == INVALID_HANDLE_VALUE || n <= 0) return;
    line[n] = '\n';
    DWORD written;
    WriteFile(g_log, line, static_cast<DWORD>(n) + 1, &written, nullptr);
}

void CopyField(char* destination, size_t capacity, const char* value, size_t length) {
    if (length >= capacity) length = capacity - 1;
    memcpy(destination, value, length);
    destination[length] = 0;
}

bool KeyIs(const char* text, size_t length, const char* key) { return strlen(key) == length && !strncmp(text, key, length); }

bool ParseOptions(const char* text, SimOptions* options) {
    while (*text) {
        const char* end = strchr(text, ',');
        const size_t length = end ? static_cast<size_t>(end - text) : strlen(text);
        const char* colon = static_cast<const char*>(memchr(text, ':', length));
        if (!colon) return false;
        const size_t keyLength = static_cast<size_t>(colon - text);
        const char* value = colon + 1;
        const size_t valueLength = length - keyLength - 1;
        if (KeyIs(text, keyLength, "load")) CopyField(options->load, sizeof options->load, value, valueLength);
        else if (KeyIs(text, keyLength, "save")) CopyField(options->save, sizeof options->save, value, valueLength);
        else if (KeyIs(text, keyLength, "log")) CopyField(options->log, sizeof options->log, value, valueLength);
        else if (KeyIs(text, keyLength, "ticks")) options->ticks = static_cast<uint32_t>(strtoul(value, nullptr, 10));
        else if (KeyIs(text, keyLength, "every")) options->every = static_cast<uint32_t>(strtoul(value, nullptr, 10));
        else return false;
        text = end ? end + 1 : text + length;
    }
    return options->load[0] && options->save[0];
}

bool ReadOptions(SimOptions* options) {
    char buffer[1024];
    const DWORD n = GetEnvironmentVariableA("REB_SIM", buffer, sizeof buffer);
    return n != 0 && n < sizeof buffer && ParseOptions(buffer, options);
}

[[noreturn]] void Finish(int code, const char* message) {
    Log("%s", message);
    if (g_log != INVALID_HANDLE_VALUE) CloseHandle(g_log);
    ExitProcess(static_cast<UINT>(code));
}

// Mirrors RebellionApplication::LoadPendingGame and BattleFlow::Restore for a running strategic game; the strategic
// window state that follows the game in the file is left unread (no window exists).
bool LoadGame(RebellionApplication* app, const char* path, SavedGameInfo* info) {
    ScopedIfstream file;
    OpenInputFile(file.get(), path);
    if (GetFileError() != 0) return false;
    istream* in = reinterpret_cast<istream*>(file.get());
    info->Read(in);
    app->m_unknown130 = info->m_unknown10;
    app->m_side = info->m_side;
    app->m_unknown134 = info->m_unknown14;
    app->m_gameType = info->m_gameType;
    app->m_autosaveTick = 0;
    CreateBattleFlow();
    BattleFlow* flow = GetBattleFlow();
    if (!flow) return false;
    flow->m_ok = 1;
    flow->m_state = 0;
    CheckPositionMarker(in);
    ReadValue32(in, &flow->m_ok);
    ReadValue32(in, &flow->m_state);
    ReadGameTime(&flow->m_voteTimeA, in);
    ReadGameTime(&flow->m_voteTimeB, in);
    if (flow->m_ok == 0 || flow->m_state != 2) return false;
    if (CreateGameStepDriver() == 0) return false;
    CheckPositionMarker(in);
    const int loaded = g_gameStepDriver->Load(in);
    flow->m_ok = 1;
    CloseFileStream(file.get());
    return loaded != 0;
}

bool SaveGame(RebellionApplication* app, const SavedGameInfo& source, const char* path) {
    ScopedOfstream file;
    OpenOutputFile(file.get(), path);
    if (GetFileError() != 0) return false;
    SavedGameInfo info;
    info.m_gameType = app->m_gameType;
    info.m_unknown10 = 1;
    info.m_unknown14 = 0;
    info.m_slot = 1;
    info.SetName(source.GetName());
    info.m_side = app->m_side;
    info.m_unknown24 = app->m_autosaveTick;
    info.Write(reinterpret_cast<ostream*>(file.get()));
    AppSaveGame(file.get());
    CloseFileStream(file.get());
    return true;
}

struct StepStats {
    uint64_t frames = 0;
    uint32_t maxFramesPerTick = 0;
};

bool FrameSettled(const BattleFlow* flow) {
    return flow->m_state == kFlowStrategic && g_gameStepDriver && g_gameStepDriver->m_phase == 0;
}

// Runs frames until the game's tick counter reaches `target` and the frame phases of that tick are done.
bool RunToTick(RebellionApplication* app, uint32_t target, StepStats* stats) {
    constexpr uint32_t kFrameLimit = 2000000;
    uint32_t frames = 0;
    for (;;) {
        BattleFlow* flow = GetBattleFlow();
        if (!flow || !g_gameStepDriver) return false;
        const uint32_t tick = static_cast<uint32_t>(GetGameTickCount());
        if (tick >= target && FrameSettled(flow)) break;
        if (++frames > kFrameLimit) {
            Log("stall: tick %u state %d phase %d hold %d pause %d", tick, flow->m_state, g_gameStepDriver->m_phase,
                g_gameStepDriver->m_hold, g_gameStepDriver->m_pauseTick);
            return false;
        }
        app->m_waitElapsed = 1;
        int stepped = 1;
        AppStepGame(&stepped);
        ++stats->frames;
    }
    if (frames > stats->maxFramesPerTick) stats->maxFramesPerTick = frames;
    return true;
}

}  // namespace

bool RebSimRequested() {
    char buffer[8];
    return GetEnvironmentVariableA("REB_SIM", buffer, sizeof buffer) != 0;
}

int RebSimRun(RebellionApplication* app) {
    SimOptions options;
    const bool valid = ReadOptions(&options);
    if (valid) {
        char path[MAX_PATH];
        snprintf(path, sizeof path, "%s.log", options.save);
        g_log = CreateFileA(options.log[0] ? options.log : path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
    }
    if (!valid) Finish(2, "REB_SIM: expected load:<file>,ticks:<N>,save:<path>[,every:<k>][,log:<path>]");

    SavedGameInfo header;
    if (!LoadGame(app, options.load, &header)) Finish(3, "REB_SIM: load failed");
    const uint32_t startTick = static_cast<uint32_t>(GetGameTickCount());
    Log("loaded %s: tick %u side %d type %d pause %d", options.load, startTick, app->m_side, app->m_gameType,
        g_gameStepDriver->m_pauseTick);
    g_gameStepDriver->m_pauseTick = 0;  // a loaded game starts paused; the player's un-pause click is not simulated

    StepStats stats;
    const uint32_t endTick = startTick + options.ticks;
    uint32_t next = options.every ? startTick + options.every : endTick;
    for (;;) {
        if (next > endTick) next = endTick;
        if (!RunToTick(app, next, &stats)) Finish(4, "REB_SIM: step stalled");
        const uint32_t tick = static_cast<uint32_t>(GetGameTickCount());
        char path[MAX_PATH];
        if (tick >= endTick) snprintf(path, sizeof path, "%s", options.save);
        else snprintf(path, sizeof path, "%s.t%u", options.save, tick);
        if (!SaveGame(app, header, path)) Finish(5, "REB_SIM: save failed");
        Log("saved tick %u frames %llu -> %s", tick, static_cast<unsigned long long>(stats.frames), path);
        if (tick >= endTick) break;
        next = tick + options.every;
    }
    Log("done: %llu frames, max %u frames per tick", static_cast<unsigned long long>(stats.frames), stats.maxFramesPerTick);
    Finish(0, "ok");
}
