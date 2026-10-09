// Entry glue of the standalone rebellion.exe. libcmt's WinMainCRTStartup initialises the CRT and the compiler's own
// static initialisers, then calls _WinMain@16, which the linker maps to reb_winmain below.
#include <windows.h>

int __stdcall WinMain(HINSTANCE instance, HINSTANCE previousInstance, LPSTR commandLine, int showCommand);

extern "C" int __stdcall reb_winmain(HINSTANCE instance, HINSTANCE previousInstance, LPSTR commandLine, int showCommand) {
    return WinMain(instance, previousInstance, commandLine, showCommand);
}
