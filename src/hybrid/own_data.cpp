// REB_OWN_DATA: the game's globals live in this DLL (the generated data image, tools/gen_data_image.py). Original
// code that has not been reconstructed still addresses the EXE's copy, so at load every such reference is shifted
// to the DLL's block (`reb_code_refs` lists the dwords to adjust). Must run before anything touches game data.
#include <windows.h>

#include <cstdint>

#include "hybrid/runtime.h"

#ifdef REB_OWN_DATA

extern "C" {
extern const uint32_t reb_code_refs[];
extern char reb_data_block[];
extern char reb_data_block_original[];  // absolute symbol: its "address" is the block's original address
}

namespace reb {

namespace {

struct Region {
    uint32_t begin;
    uint32_t size;
};

constexpr Region kPatchedRegions[] = {{0x00401000, 0x00257000}, {0x00658000, 0x0004E000}, {0x006A6000, 0x0001A000}};

// Segments of the block (tools/own_data.py) whose EXE copy no code should touch any more; only pages that lie
// entirely inside a segment can be guarded, because the CRT's data shares the edge pages.
constexpr Region kOwnSegments[] = {{0x006A7C10, 0x006AC970 - 0x006A7C10}, {0x006B059C, 0x006BE708 - 0x006B059C}};
constexpr uint32_t kPage = 0x1000;

uint32_t g_guarded_begin[8], g_guarded_end[8];
int g_guarded_count;

LONG CALLBACK tripwire(EXCEPTION_POINTERS* info) {
    EXCEPTION_RECORD* record = info->ExceptionRecord;
    if (record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || record->NumberParameters < 2) return EXCEPTION_CONTINUE_SEARCH;
    auto address = static_cast<uint32_t>(record->ExceptionInformation[1]);
    for (int i = 0; i < g_guarded_count; ++i) {
        if (address < g_guarded_begin[i] || address >= g_guarded_end[i]) continue;
        uint32_t page = address & ~(kPage - 1);
        DWORD old;
        VirtualProtect(reinterpret_cast<void*>(page), kPage, PAGE_READWRITE, &old);
        log("tripwire: %s of the EXE's copy of game data %08x from %p", record->ExceptionInformation[0] ? "write" : "read",
            address, record->ExceptionAddress);
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

void arm_tripwire() {
    AddVectoredExceptionHandler(1, tripwire);
    for (const Region& s : kOwnSegments) {
        uint32_t first = (s.begin + kPage - 1) & ~(kPage - 1);
        uint32_t last = (s.begin + s.size) & ~(kPage - 1);
        if (first >= last || g_guarded_count == 8) continue;
        DWORD old;
        VirtualProtect(reinterpret_cast<void*>(first), last - first, PAGE_NOACCESS, &old);
        g_guarded_begin[g_guarded_count] = first;
        g_guarded_end[g_guarded_count++] = last;
        log("tripwire armed on %08x-%08x", first, last);
    }
}

bool env_set(const char* name) {
    char value[8];
    return GetEnvironmentVariableA(name, value, sizeof value) > 0 && value[0] != '0';
}

}  // namespace

void install_own_data() {
    const uint32_t delta = reinterpret_cast<uint32_t>(reb_data_block) - reinterpret_cast<uint32_t>(reb_data_block_original);
    DWORD old[3];
    for (int i = 0; i < 3; ++i)
        VirtualProtect(reinterpret_cast<void*>(kPatchedRegions[i].begin), kPatchedRegions[i].size, PAGE_EXECUTE_READWRITE, &old[i]);
    int patched = 0;
    // REB_OWN_DATA_NO_PATCH=1 leaves the original code on the EXE's copy: a negative control for the tripwire.
    const bool skip = env_set("REB_OWN_DATA_NO_PATCH");
    for (const uint32_t* site = reb_code_refs; *site && !skip; ++site, ++patched) *reinterpret_cast<uint32_t*>(*site) += delta;
    for (int i = 0; i < 3; ++i) {
        DWORD ignored;
        VirtualProtect(reinterpret_cast<void*>(kPatchedRegions[i].begin), kPatchedRegions[i].size, old[i], &ignored);
    }
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(0x00401000), 0x00257000);
    log("own data: block at %p (original %08x), %d references shifted", reb_data_block,
        reinterpret_cast<uint32_t>(reb_data_block_original), patched);
    if (env_set("REB_DATA_TRIPWIRE")) arm_tripwire();
}

}  // namespace reb

#endif
