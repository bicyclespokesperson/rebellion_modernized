// Entry glue of the standalone rebellion.exe. libcmt's WinMainCRTStartup initialises the CRT, runs the C++ static
// initialisers (the game has none of its own: its globals are set up by the original's initializer table), then calls
// the game's WinMain. The table runs from a .CRT$XCU entry, i.e. after the CRT is up and before WinMain; the data image
// defines the table (entries without a reconstruction are 0).
#include <windows.h>

void InstallCrashLog();

extern "C" {
typedef void(__cdecl* StaticInitializer)();
extern StaticInitializer reb_init_table[];
extern const char reb_init_table_count[];  // absolute symbol: the value is the count
}

namespace {

void __cdecl RunStaticInitializers() {
    InstallCrashLog();
    const size_t count = reinterpret_cast<size_t>(reb_init_table_count);
    for (size_t i = 0; i < count; ++i)
        if (reb_init_table[i]) reb_init_table[i]();
}

}  // namespace

#pragma section(".CRT$XCU", read)
extern "C" __declspec(allocate(".CRT$XCU")) void(__cdecl* const reb_static_init_entry)() = RunStaticInitializers;
#pragma comment(linker, "/include:_reb_static_init_entry")
