// Process entry of the native executable: what WinMainCRTStartup does for the Windows build. The C runtime is initialised by
// the host; this runs the original's static initializers (generated from REBEXE.EXE: tools/gen_native_init.py), connects the
// platform components, and calls the game's WinMain with a synthetic instance handle and the command line.
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include "app/rebellion_application.h"
#include "platform/win32.h"

void RebRunStaticInitializers();

namespace minwin_wiring {
void Install();
}

extern "C" {
// WINMM clock and timers: the window manager's implementations (src/port/minwin_impl/user/mmtimer.cpp).
DWORD MinwinTimeGetTime(void);
UINT MinwinTimeSetEvent(UINT delay, UINT resolution, LPTIMECALLBACK callback, DWORD_PTR user, UINT flags);

DWORD timeGetTime(void) { return MinwinTimeGetTime(); }

UINT timeSetEvent(UINT delay, UINT resolution, LPTIMECALLBACK callback, DWORD_PTR user, UINT flags) {
    return MinwinTimeSetEvent(delay, resolution, callback, user, flags);
}
}

namespace {

std::string CommandLineFrom(int argc, char** argv) {
    std::string line;
    for (int i = 1; i < argc; ++i) {
        if (i > 1) line += ' ';
        line += argv[i];
    }
    return line;
}

}  // namespace

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IOLBF, 0);
    minwin_wiring::Install();
    RebRunStaticInitializers();
    std::string commandLine = CommandLineFrom(argc, argv);
    HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetModuleHandleA(nullptr));
    return WinMain(instance, nullptr, commandLine.data(), 10 /* SW_SHOWDEFAULT */);
}
