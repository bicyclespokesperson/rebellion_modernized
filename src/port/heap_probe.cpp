// Allocator of the standalone's operator new/delete with an opt-in heap-address probe (docs/phase3_plan.md, P0a): the game must
// not depend on where the heap puts its objects (pointer-ordered containers, hashes of pointers, uninitialised memory).
// Without REB_HEAP_PROBE the allocator is malloc + zero fill (the standalone's usual behaviour, REB_ZERO_NEW).
//   REB_HEAP_PROBE=<bytes>       shift: before the first allocation, take <bytes> from the heap in small leaked pieces, so every
//                                later address moves by about that much (relative order unchanged)
//   REB_HEAP_PROBE_MODE=jitter   additionally leak 8..512 bytes before every 5th allocation (spacing and free-list reuse change,
//                                order mostly does not)
//   REB_HEAP_PROBE_MODE=reverse  serve operator new from a private arena that grows downwards, with LIFO free lists per size
//                                class: addresses of later allocations are lower than earlier ones, i.e. pointer order inverts
#include <windows.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void* RebAlloc(size_t size);
void RebFree(void* memory);
int RebHeapProbeMode();

namespace {

enum class Mode { kOff, kShift, kJitter, kReverse };

constexpr size_t kHeader = 16;
constexpr size_t kClassStep = 16;
constexpr size_t kSmallLimit = 1024;
constexpr size_t kSmallClasses = kSmallLimit / kClassStep;
constexpr size_t kLargeClasses = 14;  // 2 KiB .. 16 MiB
constexpr size_t kArenaBytesMax = 640u << 20;
constexpr size_t kArenaBytesMin = 32u << 20;
constexpr size_t kCommitStep = 1u << 20;

struct FreeBlock {
    FreeBlock* next;
};

struct Probe {
    Mode mode = Mode::kOff;
    bool ready = false;
    CRITICAL_SECTION lock = {};
    uint32_t count = 0;
    uint8_t* arenaBase = nullptr;
    size_t arenaBytes = 0;
    uint8_t* arenaTop = nullptr;
    uint8_t* committedLow = nullptr;
    FreeBlock* freeLists[kSmallClasses + kLargeClasses] = {};
};

Probe g_probe __attribute__((require_constant_initialization));
volatile LONG g_state = 0;  // 0 = not initialised, 1 = initialising, 2 = ready

uint32_t EnvNumber(const char* name) {
    char buffer[32];
    const DWORD n = GetEnvironmentVariableA(name, buffer, sizeof buffer);
    if (n == 0 || n >= sizeof buffer) return 0;
    uint32_t value = 0;
    for (const char* c = buffer; *c >= '0' && *c <= '9'; ++c) value = value * 10 + static_cast<uint32_t>(*c - '0');
    return value;
}

void LeakPad(size_t bytes) {
    while (bytes) {
        const size_t piece = bytes < 1000 ? bytes : 1000;
        malloc(piece);
        bytes -= piece;
    }
}

void Initialise() {
    if (InterlockedCompareExchange(&g_state, 1, 0) != 0) {
        while (g_state != 2) Sleep(0);
        return;
    }
    InitializeCriticalSection(&g_probe.lock);
    const uint32_t bytes = EnvNumber("REB_HEAP_PROBE");
    char mode[16] = "";
    GetEnvironmentVariableA("REB_HEAP_PROBE_MODE", mode, sizeof mode);
    if (!strcmp(mode, "reverse")) g_probe.mode = Mode::kReverse;
    else if (!strcmp(mode, "jitter")) g_probe.mode = Mode::kJitter;
    else if (bytes) g_probe.mode = Mode::kShift;
    if (g_probe.mode == Mode::kReverse) {
        for (size_t bytes = kArenaBytesMax; !g_probe.arenaBase && bytes >= kArenaBytesMin; bytes /= 2) {
            g_probe.arenaBase = static_cast<uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE, PAGE_NOACCESS));
            g_probe.arenaBytes = bytes;
        }
        if (g_probe.arenaBase) {
            g_probe.arenaTop = g_probe.arenaBase + g_probe.arenaBytes;
            g_probe.committedLow = g_probe.arenaTop;
        } else {
            g_probe.mode = Mode::kOff;
        }
    }
    if (bytes && g_probe.mode != Mode::kReverse) LeakPad(bytes);
    InterlockedExchange(&g_state, 2);
}

size_t ClassIndex(size_t size, size_t* blockSize) {
    if (size <= kSmallLimit) {
        const size_t index = size ? (size + kClassStep - 1) / kClassStep - 1 : 0;
        *blockSize = (index + 1) * kClassStep;
        return index;
    }
    size_t block = 2048;
    size_t index = kSmallClasses;
    while (block < size && index + 1 < kSmallClasses + kLargeClasses) {
        block <<= 1;
        ++index;
    }
    *blockSize = block < size ? 0 : block;
    return index;
}

void* ArenaAlloc(size_t size) {
    size_t blockSize;
    const size_t index = ClassIndex(size, &blockSize);
    if (blockSize == 0) return nullptr;
    EnterCriticalSection(&g_probe.lock);
    void* result = nullptr;
    if (FreeBlock* free = g_probe.freeLists[index]) {
        g_probe.freeLists[index] = free->next;
        result = free;
    } else {
        const size_t total = blockSize + kHeader;
        uint8_t* block = g_probe.arenaTop - total;
        if (block >= g_probe.arenaBase) {
            while (block < g_probe.committedLow) {
                uint8_t* next = g_probe.committedLow - kCommitStep;
                if (next < g_probe.arenaBase || !VirtualAlloc(next, kCommitStep, MEM_COMMIT, PAGE_READWRITE)) {
                    LeaveCriticalSection(&g_probe.lock);
                    return nullptr;
                }
                g_probe.committedLow = next;
            }
            g_probe.arenaTop = block;
            *reinterpret_cast<size_t*>(block) = index;
            result = block + kHeader;
        }
    }
    LeaveCriticalSection(&g_probe.lock);
    if (result) memset(result, 0, blockSize);
    return result;
}

bool InArena(void* memory) {
    return g_probe.arenaBase && static_cast<uint8_t*>(memory) >= g_probe.arenaBase &&
           static_cast<uint8_t*>(memory) < g_probe.arenaBase + g_probe.arenaBytes;
}

void ArenaFree(void* memory) {
    const size_t index = *reinterpret_cast<size_t*>(static_cast<uint8_t*>(memory) - kHeader);
    EnterCriticalSection(&g_probe.lock);
    FreeBlock* block = static_cast<FreeBlock*>(memory);
    block->next = g_probe.freeLists[index];
    g_probe.freeLists[index] = block;
    LeaveCriticalSection(&g_probe.lock);
}

void JitterBeforeAllocation() {
    const uint32_t n = static_cast<uint32_t>(InterlockedIncrement(reinterpret_cast<volatile LONG*>(&g_probe.count)));
    if (n % 5 == 0) malloc(((n * 2654435761u) >> 20 & 0x1ff) + 8);
}

}  // namespace

void* RebAlloc(size_t size) {
    if (g_state != 2) Initialise();
    if (g_probe.mode == Mode::kReverse) {
        if (void* memory = ArenaAlloc(size)) return memory;
    } else if (g_probe.mode == Mode::kJitter) {
        JitterBeforeAllocation();
    }
    void* memory = malloc(size);
    if (memory) memset(memory, 0, size);
    return memory;
}

void RebFree(void* memory) {
    if (!memory) return;
    if (g_state == 2 && InArena(memory)) ArenaFree(memory);
    else free(memory);
}

int RebHeapProbeMode() { return g_state == 2 ? static_cast<int>(g_probe.mode) : -1; }
