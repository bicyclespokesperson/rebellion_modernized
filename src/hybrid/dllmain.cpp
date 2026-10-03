// REBEXE.EXE statically imports SMACKW32.DLL. This proxy forwards every Smacker export to the original
// (renamed smackorig.dll, see smackw32.def) and, on load, patches reimplemented functions into the game.
#include <windows.h>

#include "hybrid/runtime.h"

extern "C" int _fltused = 0;

namespace reb {

static HANDLE g_log = INVALID_HANDLE_VALUE;

void log(const char* fmt, ...) {
    if (g_log == INVALID_HANDLE_VALUE) {
        char path[MAX_PATH] = "rebhybrid.log";
        GetEnvironmentVariableA("REB_LOG", path, sizeof path);
        g_log = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (g_log == INVALID_HANDLE_VALUE) return;
    }
    char line[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = wvsprintfA(line, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    line[n++] = '\n';
    DWORD written;
    WriteFile(g_log, line, static_cast<DWORD>(n), &written, nullptr);
}

}  // namespace reb

extern "C" BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        reb::log("rebhybrid loaded");
        reb::install_captures();
        reb::install_hooks();
    }
    return TRUE;
}
