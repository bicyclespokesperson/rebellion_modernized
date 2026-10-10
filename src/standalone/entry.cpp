// Entry glue of the standalone rebellion.exe. libcmt's WinMainCRTStartup initialises the CRT, runs the C++ static
// initialisers (the game has none of its own: its globals are set up by the original's initializer table), then calls
// the game's WinMain. The table runs from a .CRT$XCU entry, i.e. after the CRT is up and before WinMain; its body is
// generated from the user's REBEXE.EXE at build time (tools/gen_static_init.py -> gen/static_init_table.cpp).
#include <windows.h>

void InstallCrashLog();

#if defined(REB_MINWIN_IMPL)
namespace minwin_wiring {
void Install();
}
#endif

void RunStaticInitializers();

extern "C" {
extern unsigned char reb_data_block[];
extern unsigned char reb_data_block_end[];
}

namespace {

// REB_DUMP_INIT_STATE=<file>: the game-owned data block as it is when the static initializers are done (what WinMain
// sees), preceded by the block's run-time address so a comparison can undo the pointer relocation
// (tools/compare_init_state.py).
void DumpInitState() {
    char path[MAX_PATH];
    if (!GetEnvironmentVariableA("REB_DUMP_INIT_STATE", path, sizeof path)) return;
    const HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    const DWORD base = reinterpret_cast<DWORD>(reb_data_block);
    DWORD written;
    WriteFile(file, &base, sizeof base, &written, nullptr);
    WriteFile(file, reb_data_block, static_cast<DWORD>(reb_data_block_end - reb_data_block), &written, nullptr);
    CloseHandle(file);
}

void __cdecl RunStaticInitializersEntry() {
    InstallCrashLog();
#if defined(REB_MINWIN_IMPL)
    minwin_wiring::Install();
#endif
    RunStaticInitializers();
    DumpInitState();
}

}  // namespace

#pragma section(".CRT$XCU", read)
extern "C" __declspec(allocate(".CRT$XCU")) void(__cdecl* const reb_static_init_entry)() = RunStaticInitializersEntry;
#pragma comment(linker, "/include:_reb_static_init_entry")
