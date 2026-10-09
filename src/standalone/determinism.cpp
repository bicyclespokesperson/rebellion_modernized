// Opt-in determinism shims of the standalone exe: the same switches and arithmetic as the hybrid's (src/hybrid/determinism.cpp,
// docs/phase2_plan.md "Determinism"), so a standalone run and a hybrid run can be compared byte for byte.
//
// The hybrid patches the game's import address table; here the game's import slots are *defined* by this file
// (`__imp__QueryPerformanceCounter@4` ...), ahead of the import libraries in the link, so the wrappers below are what the game
// (and the UCRT) call. Without REB_DETERMINISTIC / REB_NO_TIMERS they forward to the real APIs.
//   REB_DETERMINISTIC=1  virtual clock: every timeGetTime/GetTickCount call advances it by REB_CLOCK_STEP_US (default 1000),
//                        QueryPerformanceCounter by REB_QPC_STEP_NS (default 1000) nanoseconds, the local/system time and
//                        time() are fixed.
//   REB_QPC_FAST_UNTIL_TICK=<n>, REB_QPC_FAST_STEP_NS=<m>: as in the hybrid, the counter steps m ns (default 1,000,000) per call
//                        while the game manager's tick counter is below n, then REB_QPC_STEP_NS.
//   REB_QPC_HOLD_UNTIL_KEY=<vk>: the counter stands still after that tick until the virtual key (decimal) is seen down.
//   REB_CLOCK_FROM_QPC=1: timeGetTime/GetTickCount read the virtual QueryPerformanceCounter instead of counting calls.
//   REB_QPC_START_KEY=<vk>: the counter stands still from the moment a game exists until the key is seen down (same key as the
//                        hold key: released and pressed again).
//   REB_NO_TIMERS=1      timeSetEvent returns a fake id and never calls back.
#include <windows.h>
#include <mmsystem.h>
#include <stdint.h>
#include <time.h>

#include "events/game_manager.h"

namespace {

constexpr int64_t kFrequency = 1000000;
constexpr uint32_t kTickBaseMs = 100000;
constexpr int64_t kFixedTime = 946728000;  // 2000-01-01 12:00:00 UTC

bool g_deterministic = false;
bool g_noTimers = false;
volatile LONG g_clockCalls = 0;
volatile LONG g_counterCalls = 0;
uint32_t g_stepUs = 1000;
bool g_clockFromCounter = false;
uint32_t g_counterStepNs = 1000;
uint32_t g_fastStepNs = 1000000;
uint32_t g_fastUntilTick = 0;
uint32_t g_startKey = 0;
bool g_startSeen = false;
uint32_t g_releaseKey = 0;
bool g_releaseSeen = false;
bool g_releaseArmed = true;
int64_t g_counterNs = 0;
volatile LONG g_counterLock = 0;
volatile LONG g_ready = 0;

uint32_t EnvNumber(const char* name, bool* present = nullptr) {
    char buffer[32];
    const DWORD n = GetEnvironmentVariableA(name, buffer, sizeof buffer);
    if (present) *present = n != 0 && n < sizeof buffer;
    if (n == 0 || n >= sizeof buffer) return 0;
    uint32_t value = 0;
    for (const char* c = buffer; *c >= '0' && *c <= '9'; ++c) value = value * 10 + static_cast<uint32_t>(*c - '0');
    return value;
}

void EnsureReady() {
    if (g_ready) return;
    g_deterministic = EnvNumber("REB_DETERMINISTIC") != 0;
    g_noTimers = EnvNumber("REB_NO_TIMERS") != 0;
    if (const uint32_t step = EnvNumber("REB_CLOCK_STEP_US")) g_stepUs = step;
    bool present = false;
    const uint32_t counterStep = EnvNumber("REB_QPC_STEP_NS", &present);
    if (present) g_counterStepNs = counterStep;
    if (const uint32_t step = EnvNumber("REB_QPC_FAST_STEP_NS")) g_fastStepNs = step;
    g_fastUntilTick = EnvNumber("REB_QPC_FAST_UNTIL_TICK");
    g_startKey = EnvNumber("REB_QPC_START_KEY");
    g_releaseKey = EnvNumber("REB_QPC_HOLD_UNTIL_KEY");
    g_releaseArmed = g_releaseKey != g_startKey;
    g_clockFromCounter = EnvNumber("REB_CLOCK_FROM_QPC") != 0;
    g_ready = 1;
}

template <typename Function>
Function RealFunction(const char* module, const char* name) {
    HMODULE handle = GetModuleHandleA(module);
    if (!handle) handle = LoadLibraryA(module);
    return reinterpret_cast<Function>(GetProcAddress(handle, name));
}

uint64_t AddCounterNs(uint32_t stepNs);

uint64_t AdvanceClockUs() {
    if (g_clockFromCounter) return AddCounterNs(0) / 1000;
    return static_cast<uint64_t>(static_cast<uint32_t>(InterlockedIncrement(&g_clockCalls))) * g_stepUs;
}

uint32_t CurrentGameTick() {
    return g_gameManager ? static_cast<uint32_t>(g_gameManager->m_tickCount) : 0;
}

bool KeyDown(uint32_t vk) { return (GetAsyncKeyState(static_cast<int>(vk)) & 0x8000) != 0; }

bool StartGateOpen(uint32_t tick) {
    if (g_startKey == 0 || g_startSeen) return true;
    if (tick == 0 || !KeyDown(g_startKey)) return tick == 0;
    g_startSeen = true;
    return true;
}

bool ReleaseGateOpen() {
    if (g_releaseKey == 0 || g_releaseSeen) return true;
    const bool down = KeyDown(g_releaseKey);
    if (!g_releaseArmed) {
        g_releaseArmed = !down;
        return false;
    }
    if (!down) return false;
    g_releaseSeen = true;
    return true;
}

uint32_t CurrentCounterStepNs() {
    if (g_fastUntilTick == 0) return g_counterStepNs;
    const uint32_t tick = CurrentGameTick();
    if (!StartGateOpen(tick)) return 0;
    if (tick == 0) return g_counterStepNs;
    if (tick < g_fastUntilTick) return g_fastStepNs;
    return ReleaseGateOpen() ? g_counterStepNs : 0;
}

uint64_t AddCounterNs(uint32_t step_ns) {
    while (InterlockedExchange(&g_counterLock, 1)) {
    }
    g_counterNs += step_ns;
    const uint64_t now = static_cast<uint64_t>(g_counterNs);
    InterlockedExchange(&g_counterLock, 0);
    return now;
}

uint64_t AdvanceCounterUs() {
    InterlockedIncrement(&g_counterCalls);
    return AddCounterNs(CurrentCounterStepNs()) / 1000;
}

BOOL WINAPI RebQueryPerformanceCounter(LARGE_INTEGER* out) {
    EnsureReady();
    if (g_deterministic) {
        if (out) out->QuadPart = static_cast<LONGLONG>(AdvanceCounterUs());
        return TRUE;
    }
    static auto real = RealFunction<BOOL(WINAPI*)(LARGE_INTEGER*)>("kernel32.dll", "QueryPerformanceCounter");
    return real(out);
}

BOOL WINAPI RebQueryPerformanceFrequency(LARGE_INTEGER* out) {
    EnsureReady();
    if (g_deterministic) {
        if (out) out->QuadPart = kFrequency;
        return TRUE;
    }
    static auto real = RealFunction<BOOL(WINAPI*)(LARGE_INTEGER*)>("kernel32.dll", "QueryPerformanceFrequency");
    return real(out);
}

DWORD WINAPI RebGetTickCount() {
    EnsureReady();
    if (g_deterministic) return kTickBaseMs + static_cast<DWORD>(AdvanceClockUs() / 1000);
    static auto real = RealFunction<DWORD(WINAPI*)()>("kernel32.dll", "GetTickCount");
    return real();
}

DWORD WINAPI RebTimeGetTime() {
    EnsureReady();
    if (g_deterministic) return kTickBaseMs + static_cast<DWORD>(AdvanceClockUs() / 1000);
    static auto real = RealFunction<DWORD(WINAPI*)()>("winmm.dll", "timeGetTime");
    return real();
}

void WINAPI RebGetLocalTime(SYSTEMTIME* out) {
    EnsureReady();
    if (g_deterministic) {
        if (out) *out = SYSTEMTIME{2000, 1, 6, 1, 12, 0, 0, 0};
        return;
    }
    static auto real = RealFunction<void(WINAPI*)(SYSTEMTIME*)>("kernel32.dll", "GetLocalTime");
    real(out);
}

void WINAPI RebGetSystemTime(SYSTEMTIME* out) {
    EnsureReady();
    if (g_deterministic) {
        if (out) *out = SYSTEMTIME{2000, 1, 6, 1, 12, 0, 0, 0};
        return;
    }
    static auto real = RealFunction<void(WINAPI*)(SYSTEMTIME*)>("kernel32.dll", "GetSystemTime");
    real(out);
}

UINT WINAPI RebTimeSetEvent(UINT delay, UINT resolution, LPTIMECALLBACK callback, DWORD_PTR user, UINT flags) {
    EnsureReady();
    if (g_noTimers) return 1;
    static auto real = RealFunction<UINT(WINAPI*)(UINT, UINT, LPTIMECALLBACK, DWORD_PTR, UINT)>("winmm.dll", "timeSetEvent");
    return real(delay, resolution, callback, user, flags);
}

}  // namespace

#define REB_DEFINE_IMPORT(symbol, function) \
    extern "C" void* const reb_import_##function __asm__(symbol); \
    extern "C" void* const reb_import_##function = reinterpret_cast<void*>(&function)

REB_DEFINE_IMPORT("__imp__QueryPerformanceCounter@4", RebQueryPerformanceCounter);
REB_DEFINE_IMPORT("__imp__QueryPerformanceFrequency@4", RebQueryPerformanceFrequency);
REB_DEFINE_IMPORT("__imp__GetTickCount@0", RebGetTickCount);
REB_DEFINE_IMPORT("__imp__timeGetTime@0", RebTimeGetTime);
REB_DEFINE_IMPORT("__imp__GetLocalTime@4", RebGetLocalTime);
REB_DEFINE_IMPORT("__imp__GetSystemTime@4", RebGetSystemTime);
REB_DEFINE_IMPORT("__imp__timeSetEvent@20", RebTimeSetEvent);

// The game's single time() call (a seed): the UCRT's time() is inline over these.
extern "C" __time64_t __cdecl _time64(__time64_t* out) {
    EnsureReady();
    __time64_t value = kFixedTime;
    if (!g_deterministic) {
        FILETIME fileTime;
        GetSystemTimeAsFileTime(&fileTime);
        const uint64_t ticks = (static_cast<uint64_t>(fileTime.dwHighDateTime) << 32) | fileTime.dwLowDateTime;
        value = static_cast<__time64_t>(ticks / 10000000ull - 11644473600ull);
    }
    if (out) *out = value;
    return value;
}

extern "C" __time32_t __cdecl _time32(__time32_t* out) {
    const __time32_t value = static_cast<__time32_t>(_time64(nullptr));
    if (out) *out = value;
    return value;
}
