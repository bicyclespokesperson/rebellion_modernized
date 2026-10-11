// The host initializes the C runtime. Program startup, assets and platform services have explicit native lifetimes.
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <exception>
#include <iostream>

#include <string>

#include "app/rebellion_application.h"
#include "platform/win32.h"
#include "port/native/first_run.h"
#include "port/native/game_initialization.h"
#include "port/native/native_clock.h"
#include "port/native/startup_assets.h"

namespace minwin_wiring {
void Install();
}

extern "C" {
// WINMM clock and timers: the window manager's implementations (src/port/minwin_impl/user/mmtimer.cpp).
DWORD MinwinTimeGetTime(void);
UINT MinwinTimeSetEvent(UINT delay, UINT resolution, LPTIMECALLBACK callback, DWORD_PTR user, UINT flags);

DWORD timeGetTime(void) {
    if (native_clock::Deterministic()) return native_clock::VirtualTickMs();
    return MinwinTimeGetTime();
}

UINT timeSetEvent(UINT delay, UINT resolution, LPTIMECALLBACK callback, DWORD_PTR user, UINT flags) {
    if (native_clock::NoTimers()) return 1;
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
    try {
        reb::native::StartupAssets startupAssets(reb::native::StartupAssetPath());
        if (!reb::native::CheckInstallation()) return 1;
        minwin_wiring::Install();
        reb::native::GameInitialization initialization;
        initialization.Start();
        std::string commandLine = CommandLineFrom(argc, argv);
        HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetModuleHandleA(nullptr));
        int result = WinMain(instance, nullptr, commandLine.data(), 10 /* SW_SHOWDEFAULT */);
        initialization.Shutdown();
        return result;
    } catch (const std::exception& error) {
        std::cerr << "Native startup failed: " << error.what() << '\n';
    } catch (...) {
        std::cerr << "Native startup failed with an unknown exception\n";
    }
    return 1;
}
