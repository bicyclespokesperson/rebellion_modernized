// Test-case capture for the differential tester (tools/difftest).
//
// REB_CAPTURE="<hex addr>[:count[:skip]],..." arms an INT3 breakpoint at each listed original function.
// On a hit (after `skip` hits), the full CPU context and a snapshot of all committed readable memory are
// written to REB_CAPTURE_DIR (default "captures"), then execution continues in the original code via a
// single-step re-arm. Memory pages are deduplicated per session in pages.pack.
//
// Layout (all little-endian):
//   <dir>/pages.pack       concatenated unique 4 KiB pages
//   <dir>/<addr>_<n>.cap   header + region list (see CaptureHeader / PageRef)
#include <windows.h>

#include <cstddef>
#include <cstdint>

#include "hybrid/runtime.h"

namespace reb {

namespace {

constexpr uint32_t kPage = 4096;
constexpr int kMaxTargets = 64;
constexpr uint32_t kCaptureMagic = 0x50414352;  // "RCAP"
constexpr uint32_t kCaptureVersion = 1;

struct Target {
    uint32_t address;
    uint8_t original_byte;
    int remaining;
    int skip;
    int taken;
};

#pragma pack(push, 1)
struct CaptureHeader {
    uint32_t magic, version;
    uint32_t function;
    uint32_t thread_id;
    uint32_t teb;
    uint32_t eax, ebx, ecx, edx, esi, edi, ebp, esp, eip, eflags;
    uint32_t fpu_control, fpu_status, fpu_tag;
    uint8_t fpu_registers[80];
    uint32_t page_count;
};
struct PageRef {
    uint32_t address;
    uint32_t protect;
    uint64_t pack_offset;
};
#pragma pack(pop)

Target g_targets[kMaxTargets];
int g_target_count = 0;
char g_dir[MAX_PATH] = "captures";
HANDLE g_pack = INVALID_HANDLE_VALUE;
uint64_t g_pack_size = 0;
CRITICAL_SECTION g_lock;
// Thread that must re-arm a breakpoint on its next single-step, per target index.
DWORD g_rearm_thread[kMaxTargets];

// Open-addressing table: page hash -> pack offset.
struct HashSlot {
    uint64_t hash;
    uint64_t offset;
};
HashSlot* g_slots = nullptr;
uint32_t g_slot_mask = 0;
uint32_t g_slot_used = 0;

uint64_t fnv1a(const uint8_t* p, uint32_t n) {
    uint64_t h = 1469598103934665603ull;
    for (uint32_t i = 0; i < n; ++i) h = (h ^ p[i]) * 1099511628211ull;
    return h | 1;  // 0 marks an empty slot
}

void grow_table() {
    uint32_t new_size = g_slot_mask ? (g_slot_mask + 1) * 2 : (1u << 16);
    auto* slots = static_cast<HashSlot*>(VirtualAlloc(nullptr, new_size * sizeof(HashSlot), MEM_COMMIT, PAGE_READWRITE));
    for (uint32_t i = 0; g_slots && i <= g_slot_mask; ++i) {
        if (!g_slots[i].hash) continue;
        uint32_t j = static_cast<uint32_t>(g_slots[i].hash) & (new_size - 1);
        while (slots[j].hash) j = (j + 1) & (new_size - 1);
        slots[j] = g_slots[i];
    }
    if (g_slots) VirtualFree(g_slots, 0, MEM_RELEASE);
    g_slots = slots;
    g_slot_mask = new_size - 1;
}

uint64_t store_page(const uint8_t* page) {
    if (!g_slots || g_slot_used * 2 > g_slot_mask) grow_table();
    uint64_t h = fnv1a(page, kPage);
    uint32_t j = static_cast<uint32_t>(h) & g_slot_mask;
    while (g_slots[j].hash) {
        if (g_slots[j].hash == h) return g_slots[j].offset;
        j = (j + 1) & g_slot_mask;
    }
    DWORD written;
    WriteFile(g_pack, page, kPage, &written, nullptr);
    g_slots[j] = {h, g_pack_size};
    ++g_slot_used;
    g_pack_size += kPage;
    return g_slots[j].offset;
}

bool readable(DWORD protect) {
    if (protect & (PAGE_GUARD | PAGE_NOACCESS)) return false;
    return protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                      PAGE_EXECUTE_WRITECOPY);
}

uint32_t parse_hex(const char*& p) {
    uint32_t v = 0;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
    for (;; ++p) {
        char c = *p;
        if (c >= '0' && c <= '9') v = v * 16 + (c - '0');
        else if ((c | 0x20) >= 'a' && (c | 0x20) <= 'f') v = v * 16 + ((c | 0x20) - 'a' + 10);
        else return v;
    }
}

uint32_t parse_dec(const char*& p) {
    uint32_t v = 0;
    while (*p >= '0' && *p <= '9') v = v * 10 + (*p++ - '0');
    return v;
}

void write_capture(Target& t, const CONTEXT* ctx) {
    char path[MAX_PATH];
    wsprintfA(path, "%s\\%08x_%d.cap", g_dir, t.address, t.taken);
    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;

    CaptureHeader h = {};
    h.magic = kCaptureMagic;
    h.version = kCaptureVersion;
    h.function = t.address;
    h.thread_id = GetCurrentThreadId();
    h.teb = reinterpret_cast<uint32_t>(NtCurrentTeb());
    h.eax = ctx->Eax, h.ebx = ctx->Ebx, h.ecx = ctx->Ecx, h.edx = ctx->Edx;
    h.esi = ctx->Esi, h.edi = ctx->Edi, h.ebp = ctx->Ebp, h.esp = ctx->Esp;
    h.eip = t.address, h.eflags = ctx->EFlags;
    h.fpu_control = ctx->FloatSave.ControlWord;
    h.fpu_status = ctx->FloatSave.StatusWord;
    h.fpu_tag = ctx->FloatSave.TagWord;
    CopyMemory(h.fpu_registers, ctx->FloatSave.RegisterArea, sizeof h.fpu_registers);
    DWORD written;
    WriteFile(f, &h, sizeof h, &written, nullptr);

    uint32_t pages = 0;
    MEMORY_BASIC_INFORMATION mbi;
    for (uintptr_t a = 0x10000; a < 0x7fff0000 && VirtualQuery(reinterpret_cast<void*>(a), &mbi, sizeof mbi);
         a = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize) {
        if (mbi.State != MEM_COMMIT || !readable(mbi.Protect)) continue;
        auto base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        // Our own capture bookkeeping is not part of the program state.
        if (g_slots && base == reinterpret_cast<uintptr_t>(g_slots)) continue;
        for (uintptr_t p = base; p < base + mbi.RegionSize; p += kPage) {
            static uint8_t copy[kPage];  // static: no stack probe, and we hold g_lock
            CopyMemory(copy, reinterpret_cast<const void*>(p), kPage);
            if (p <= t.address && t.address < p + kPage) copy[t.address - p] = t.original_byte;
            PageRef ref = {static_cast<uint32_t>(p), mbi.Protect, store_page(copy)};
            WriteFile(f, &ref, sizeof ref, &written, nullptr);
            ++pages;
        }
    }
    SetFilePointer(f, offsetof(CaptureHeader, page_count), nullptr, FILE_BEGIN);
    WriteFile(f, &pages, sizeof pages, &written, nullptr);
    CloseHandle(f);
    FlushFileBuffers(g_pack);
    log("capture %08x #%d: %u pages, pack %u MiB", t.address, t.taken, pages, static_cast<uint32_t>(g_pack_size >> 20));
}

void set_byte(uint32_t address, uint8_t value) {
    auto* p = reinterpret_cast<uint8_t*>(address);
    DWORD old;
    VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &old);
    *p = value;
    VirtualProtect(p, 1, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 1);
}

LONG CALLBACK on_exception(EXCEPTION_POINTERS* info) {
    CONTEXT* ctx = info->ContextRecord;
    DWORD code = info->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_BREAKPOINT) {
        uint32_t at = reinterpret_cast<uint32_t>(info->ExceptionRecord->ExceptionAddress);
        for (int i = 0; i < g_target_count; ++i) {
            Target& t = g_targets[i];
            if (t.address != at) continue;
            EnterCriticalSection(&g_lock);
            if (t.skip > 0) {
                --t.skip;
            } else if (t.remaining > 0) {
                write_capture(t, ctx);
                ++t.taken;
                --t.remaining;
            }
            set_byte(t.address, t.original_byte);
            ctx->Eip = t.address;
            if (t.remaining > 0) {
                g_rearm_thread[i] = GetCurrentThreadId();
                ctx->EFlags |= 0x100;  // trap after the original first instruction, then re-arm
            }
            LeaveCriticalSection(&g_lock);
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    } else if (code == EXCEPTION_SINGLE_STEP) {
        bool handled = false;
        for (int i = 0; i < g_target_count; ++i) {
            if (g_rearm_thread[i] != GetCurrentThreadId()) continue;
            g_rearm_thread[i] = 0;
            set_byte(g_targets[i].address, 0xCC);
            handled = true;
        }
        if (handled) {
            ctx->EFlags &= ~0x100u;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

}  // namespace

bool is_capture_target(uint32_t address) {
    for (int i = 0; i < g_target_count; ++i)
        if (g_targets[i].address == address) return true;
    return false;
}

void install_captures() {
    char spec[1024];
    DWORD n = GetEnvironmentVariableA("REB_CAPTURE", spec, sizeof spec);
    if (n == 0 || n >= sizeof spec) return;
    GetEnvironmentVariableA("REB_CAPTURE_DIR", g_dir, sizeof g_dir);
    CreateDirectoryA(g_dir, nullptr);
    char pack_path[MAX_PATH];
    wsprintfA(pack_path, "%s\\pages.pack", g_dir);
    g_pack = CreateFileA(pack_path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_pack == INVALID_HANDLE_VALUE) {
        log("capture: cannot create %s", pack_path);
        return;
    }
    InitializeCriticalSection(&g_lock);
    for (const char* p = spec; *p && g_target_count < kMaxTargets;) {
        Target t = {};
        t.address = parse_hex(p);
        t.remaining = 3;
        if (*p == ':') t.remaining = static_cast<int>(parse_dec(++p));
        if (*p == ':') t.skip = static_cast<int>(parse_dec(++p));
        while (*p && *p != ',') ++p;
        if (*p == ',') ++p;
        t.original_byte = *reinterpret_cast<uint8_t*>(t.address);
        g_targets[g_target_count++] = t;
    }
    AddVectoredExceptionHandler(1, on_exception);
    for (int i = 0; i < g_target_count; ++i) {
        set_byte(g_targets[i].address, 0xCC);
        log("capture armed at %08x (count %d, skip %d)", g_targets[i].address, g_targets[i].remaining, g_targets[i].skip);
    }
}

}  // namespace reb
