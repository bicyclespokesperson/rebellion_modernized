// Opt-in determinism shims for comparing runs (docs/phase2_plan.md, "Determinism").
//
// REB_DETERMINISTIC=1      the game's clocks become a virtual clock that advances by a fixed step on every call,
//                          and the CRT time() returns a fixed value. The game's own import table slots are
//                          patched, so only the game (not Wine's DLLs) sees the virtual time.
// REB_CLOCK_STEP_US=<n>    virtual microseconds per clock call (default 1000).
// REB_QPC_STEP_NS=<n>     nanoseconds per QueryPerformanceCounter call (default 1000). The game's simulation advances
//                          one tick whenever a QPC stopwatch passes 0.25 s, and the main loop calls it once per
//                          iteration, so the step sets how many loop iterations a tick takes (default: 250,000,
//                          ~3 s of wall time). Ticks then almost never fall near the (wall-clock scripted) input,
//                          which makes the state at a save independent of timing jitter; 0 freezes the simulation
//                          (and with it saving, which runs as a simulation command).
// REB_QPC_FAST_UNTIL_TICK=<n>  runs the simulation fast until the game's tick counter reaches n, then drops to REB_QPC_STEP_NS.
//                          The step is REB_QPC_FAST_STEP_NS (default 1,000,000: a tick every ~250 loop iterations) while the
//                          tick is below n. The switch happens on the first clock call that sees tick >= n, so a long run
//                          stops at a tick that does not depend on when scripted input arrives (reads the game manager's
//                          tick counter; never calls game code).
// REB_NO_TIMERS=1         timeSetEvent returns a fake id and never calls back (the game's CoolTimer threads).
// REB_CLOCK_TRACE=1       logs the first call from every distinct caller of the virtual clocks.
// REB_HEAP_PROBE=<bytes>   shifts heap addresses: reserves <bytes> of address space before the game's heap exists
//                          and, once the CRT heap exists, makes a small allocation of <bytes> % 4093 + 1 bytes
//                          from it. Test aid for finding address-dependent behaviour; independent of the clocks.
// REB_HEAP_PROBE_SBH=1     additionally allocates through the game's malloc (a deliberate exception to "the
//                          runtime never calls game code") so offsets inside the small-block heap move as well.
#include <windows.h>

#include <cstdint>

#include "hybrid/runtime.h"

namespace reb {

namespace {

constexpr uint32_t kIatQueryPerformanceCounter = 0x00658140;
constexpr uint32_t kIatQueryPerformanceFrequency = 0x00658144;
constexpr uint32_t kIatGetTickCount = 0x00658184;
constexpr uint32_t kIatGetLocalTime = 0x00658210;
constexpr uint32_t kIatGetSystemTime = 0x00658214;
constexpr uint32_t kIatTimeGetTime = 0x00658444;
constexpr uint32_t kIatTimeSetEvent = 0x00658448;
constexpr uint32_t kGameManagerGlobal = 0x006b2fcc;
constexpr uint32_t kGameManagerTickOffset = 0x0c;
constexpr uint32_t kGameTime = 0x0061b120;
constexpr uint32_t kGameMalloc = 0x00619170;
constexpr uint32_t kGameCrtHeap = 0x006bed14;

constexpr int64_t kFrequency = 1000000;
constexpr uint32_t kTickBaseMs = 100000;
constexpr int32_t kFixedTime = 946728000;  // 2000-01-01 12:00:00 UTC

volatile LONG g_clock_calls = 0;
volatile LONG g_counter_calls = 0;
uint32_t g_step_us = 1000;
uint32_t g_counter_step_ns = 1000;
uint32_t g_fast_step_ns = 1000000;
uint32_t g_fast_until_tick = 0;
int64_t g_counter_ns = 0;
volatile LONG g_counter_lock = 0;
volatile LONG g_fast_phase_logged = 0;
SIZE_T g_probe_bytes = 0;
bool g_probe_sbh = false;
volatile LONG g_probe_pending = 0;

bool g_trace_callers = false;
uintptr_t g_seen_callers[256];
LONG g_seen_count = 0;

void trace_caller(const char* api, void* return_address) {
    auto address = reinterpret_cast<uintptr_t>(return_address);
    for (LONG i = 0; i < g_seen_count; ++i)
        if (g_seen_callers[i] == address) return;
    if (g_seen_count >= 256) return;
    g_seen_callers[g_seen_count++] = address;
    log("clock caller: %s from %p (call %d)", api, return_address, static_cast<int>(g_clock_calls));
}

uint64_t advance_clock_us() { return static_cast<uint64_t>(static_cast<uint32_t>(InterlockedIncrement(&g_clock_calls))) * g_step_us; }

uint32_t current_game_tick() {
    auto* manager = *reinterpret_cast<const uint8_t* const*>(kGameManagerGlobal);
    return manager ? *reinterpret_cast<const uint32_t*>(manager + kGameManagerTickOffset) : 0;
}

uint32_t current_counter_step_ns() {
    if (g_fast_until_tick == 0) return g_counter_step_ns;
    const uint32_t tick = current_game_tick();
    if (tick < g_fast_until_tick) return g_fast_step_ns;
    if (InterlockedExchange(&g_fast_phase_logged, 1) == 0)
        log("determinism: fast phase over at game tick %u after %d counter calls", tick, static_cast<int>(g_counter_calls));
    return g_counter_step_ns;
}

uint64_t add_counter_ns(uint32_t step_ns) {
    while (InterlockedExchange(&g_counter_lock, 1)) {
    }
    g_counter_ns += step_ns;
    const uint64_t now = static_cast<uint64_t>(g_counter_ns);
    InterlockedExchange(&g_counter_lock, 0);
    return now;
}

uint64_t advance_counter_us() {
    InterlockedIncrement(&g_counter_calls);
    return add_counter_ns(current_counter_step_ns()) / 1000;
}

void apply_heap_probe_when_ready() {
    if (!g_probe_pending) return;
    HANDLE heap = *reinterpret_cast<HANDLE*>(kGameCrtHeap);
    if (!heap || InterlockedExchange(&g_probe_pending, 0) == 0) return;
    SIZE_T size = g_probe_bytes % 4093 + 1;
    void* p = HeapAlloc(heap, 0, size);
    void* q = g_probe_sbh ? reinterpret_cast<void*(__cdecl*)(SIZE_T)>(kGameMalloc)(size) : nullptr;
    log("heap probe: HeapAlloc(%u) = %p, game malloc = %p", static_cast<unsigned>(size), p, q);
}

BOOL WINAPI fake_query_performance_counter(LARGE_INTEGER* out) {
    if (g_trace_callers) trace_caller("QueryPerformanceCounter", __builtin_return_address(0));
    apply_heap_probe_when_ready();
    if (out) out->QuadPart = advance_counter_us();
    if (g_trace_callers && g_counter_calls % 100000 == 0)
        log("qpc: %d calls at real tick %u", static_cast<int>(g_counter_calls), static_cast<unsigned>(GetTickCount64()));
    return TRUE;
}

BOOL WINAPI fake_query_performance_frequency(LARGE_INTEGER* out) {
    if (out) out->QuadPart = kFrequency;
    return TRUE;
}

DWORD WINAPI fake_tick_count() {
    if (g_trace_callers) trace_caller("tick count", __builtin_return_address(0));
    apply_heap_probe_when_ready();
    return kTickBaseMs + static_cast<DWORD>(advance_clock_us() / 1000);
}

void WINAPI fake_system_time(SYSTEMTIME* out) {
    if (!out) return;
    *out = SYSTEMTIME{2000, 1, 6, 1, 12, 0, 0, 0};
}

int __cdecl fake_time(int* out) {
    if (out) *out = kFixedTime;
    return kFixedTime;
}

UINT WINAPI fake_time_set_event(UINT, UINT, void*, DWORD_PTR, UINT) { return 1; }

void patch_iat(uint32_t slot, const void* replacement) {
    auto* p = reinterpret_cast<const void**>(slot);
    DWORD old;
    if (!VirtualProtect(p, sizeof *p, PAGE_READWRITE, &old)) {
        log("determinism: VirtualProtect failed at IAT slot %08x", slot);
        return;
    }
    *p = replacement;
    VirtualProtect(p, sizeof *p, old, &old);
}

SIZE_T env_number(const char* name) {
    char buf[32];
    DWORD n = GetEnvironmentVariableA(name, buf, sizeof buf);
    if (n == 0 || n >= sizeof buf) return 0;
    SIZE_T v = 0;
    for (const char* c = buf; *c >= '0' && *c <= '9'; ++c) v = v * 10 + (*c - '0');
    return v;
}

bool env_is_set(const char* name) {
    char buf[2];
    return GetEnvironmentVariableA(name, buf, sizeof buf) != 0;
}

void install_clock_shims() {
    if (SIZE_T step = env_number("REB_CLOCK_STEP_US")) g_step_us = static_cast<uint32_t>(step);
    if (env_is_set("REB_QPC_STEP_NS")) g_counter_step_ns = static_cast<uint32_t>(env_number("REB_QPC_STEP_NS"));
    if (SIZE_T step = env_number("REB_QPC_FAST_STEP_NS")) g_fast_step_ns = static_cast<uint32_t>(step);
    g_fast_until_tick = static_cast<uint32_t>(env_number("REB_QPC_FAST_UNTIL_TICK"));
    g_trace_callers = env_number("REB_CLOCK_TRACE") != 0;
    patch_iat(kIatQueryPerformanceCounter, reinterpret_cast<void*>(fake_query_performance_counter));
    patch_iat(kIatQueryPerformanceFrequency, reinterpret_cast<void*>(fake_query_performance_frequency));
    patch_iat(kIatGetTickCount, reinterpret_cast<void*>(fake_tick_count));
    patch_iat(kIatTimeGetTime, reinterpret_cast<void*>(fake_tick_count));
    patch_iat(kIatGetLocalTime, reinterpret_cast<void*>(fake_system_time));
    patch_iat(kIatGetSystemTime, reinterpret_cast<void*>(fake_system_time));
    patch_jump(kGameTime, reinterpret_cast<void*>(fake_time));
    log("determinism: virtual clock, step %u us, QueryPerformanceCounter step %u ns", g_step_us, g_counter_step_ns);
}

void install_timer_suppression() {
    patch_iat(kIatTimeSetEvent, reinterpret_cast<void*>(fake_time_set_event));
    log("determinism: multimedia timers never fire");
}

void install_heap_probe() {
    g_probe_bytes = env_number("REB_HEAP_PROBE");
    if (g_probe_bytes == 0) return;
    g_probe_sbh = env_number("REB_HEAP_PROBE_SBH") != 0;
    g_probe_pending = 1;
    void* reserved = VirtualAlloc(nullptr, g_probe_bytes, MEM_RESERVE, PAGE_NOACCESS);
    log("heap probe: reserved %u bytes at %p", static_cast<unsigned>(g_probe_bytes), reserved);
}

}  // namespace

void install_determinism() {
    if (env_number("REB_DETERMINISTIC")) install_clock_shims();
    if (env_number("REB_NO_TIMERS")) install_timer_suppression();
    install_heap_probe();
}

}  // namespace reb
