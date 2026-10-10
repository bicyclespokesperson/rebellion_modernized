// REB_MSG_TRACE_SHIM (CMake option, needs REB_MINWIN): the game's RegisterClassA calls land here (minwin/user32.h renames them), and
// every window procedure is wrapped so that the real user32's message sequence is logged by src/port/msg_trace.cpp in the same
// format as minwin_impl's. Default builds do not contain this file.
#include <windows.h>

#include "port/msg_trace.h"

namespace {

struct ClassProc {
    char name[64];
    WNDPROC proc;
};

constexpr int kMaxClasses = 64;
ClassProc g_classes[kMaxClasses];
int g_classCount = 0;
int g_depth = 0;

ClassProc* FindClass(const char* className) {
    for (int i = 0; i < g_classCount; ++i)
        if (!lstrcmpA(g_classes[i].name, className)) return &g_classes[i];
    return nullptr;
}

WNDPROC OriginalProc(const char* className) {
    const ClassProc* entry = FindClass(className);
    return entry ? entry->proc : nullptr;
}

LRESULT CALLBACK TracedProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    char className[64] = "?";
    GetClassNameA(hwnd, className, sizeof className);
    msgtrace::Log(className, reinterpret_cast<uintptr_t>(hwnd), message, wParam, lParam, g_depth);
    WNDPROC original = OriginalProc(className);
    if (!original) return DefWindowProcA(hwnd, message, wParam, lParam);
    ++g_depth;
    const LRESULT result = original(hwnd, message, wParam, lParam);
    --g_depth;
    return result;
}

}  // namespace

extern "C" ATOM WINAPI RebTracedRegisterClassA(const WNDCLASSA* cls) {
    WNDCLASSA wrapped = *cls;
    if (msgtrace::Enabled() && cls->lpszClassName && reinterpret_cast<uintptr_t>(cls->lpszClassName) >= 0x10000) {
        // A class that is unregistered and registered again (the game does this for its buttons) must be wrapped again: its
        // windows would otherwise vanish from the trace.
        ClassProc* entry = FindClass(cls->lpszClassName);
        if (!entry && g_classCount < kMaxClasses) {
            entry = &g_classes[g_classCount++];
            lstrcpynA(entry->name, cls->lpszClassName, sizeof entry->name);
        }
        if (entry) {
            entry->proc = cls->lpfnWndProc;
            wrapped.lpfnWndProc = TracedProc;
        }
    }
    return RegisterClassA(&wrapped);
}
