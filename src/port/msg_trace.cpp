#include "port/msg_trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace msgtrace {
namespace {

struct MessageName {
    unsigned id;
    const char* name;
};

const MessageName kNames[] = {
    {0x01, "WM_CREATE"}, {0x02, "WM_DESTROY"}, {0x03, "WM_MOVE"}, {0x05, "WM_SIZE"}, {0x06, "WM_ACTIVATE"},
    {0x07, "WM_SETFOCUS"}, {0x08, "WM_KILLFOCUS"}, {0x0a, "WM_ENABLE"}, {0x0b, "WM_SETREDRAW"}, {0x0c, "WM_SETTEXT"},
    {0x0d, "WM_GETTEXT"}, {0x0e, "WM_GETTEXTLENGTH"}, {0x0f, "WM_PAINT"}, {0x10, "WM_CLOSE"}, {0x11, "WM_QUERYENDSESSION"},
    {0x12, "WM_QUIT"}, {0x13, "WM_QUERYOPEN"}, {0x14, "WM_ERASEBKGND"}, {0x18, "WM_SHOWWINDOW"}, {0x1c, "WM_ACTIVATEAPP"},
    {0x20, "WM_SETCURSOR"}, {0x21, "WM_MOUSEACTIVATE"}, {0x24, "WM_GETMINMAXINFO"}, {0x2b, "WM_DRAWITEM"},
    {0x30, "WM_SETFONT"}, {0x46, "WM_WINDOWPOSCHANGING"}, {0x47, "WM_WINDOWPOSCHANGED"}, {0x81, "WM_NCCREATE"},
    {0x82, "WM_NCDESTROY"}, {0x83, "WM_NCCALCSIZE"}, {0x84, "WM_NCHITTEST"}, {0x85, "WM_NCPAINT"}, {0x86, "WM_NCACTIVATE"},
    {0xa0, "WM_NCMOUSEMOVE"}, {0xa1, "WM_NCLBUTTONDOWN"}, {0xa3, "WM_NCLBUTTONDBLCLK"}, {0x100, "WM_KEYDOWN"},
    {0x101, "WM_KEYUP"}, {0x102, "WM_CHAR"}, {0x104, "WM_SYSKEYDOWN"}, {0x105, "WM_SYSKEYUP"}, {0x111, "WM_COMMAND"},
    {0x112, "WM_SYSCOMMAND"}, {0x113, "WM_TIMER"}, {0x200, "WM_MOUSEMOVE"}, {0x201, "WM_LBUTTONDOWN"},
    {0x202, "WM_LBUTTONUP"}, {0x203, "WM_LBUTTONDBLCLK"}, {0x204, "WM_RBUTTONDOWN"}, {0x205, "WM_RBUTTONUP"},
    {0x215, "WM_CAPTURECHANGED"}, {0x30f, "WM_QUERYNEWPALETTE"}, {0x311, "WM_PALETTECHANGED"},
};

constexpr int kMaxWindows = 4096;
uintptr_t g_windows[kMaxWindows];
int g_windowCount = 0;
FILE* g_file = nullptr;
int g_state = 0;  // 0 unknown, 1 enabled, -1 disabled

int Serial(uintptr_t handle) {
    if (!handle) return 0;
    for (int i = 0; i < g_windowCount; ++i)
        if (g_windows[i] == handle) return i + 1;
    return 0;
}

int SerialOrNew(uintptr_t handle) {
    if (const int known = Serial(handle)) return known;
    if (g_windowCount == kMaxWindows) return 0;
    g_windows[g_windowCount] = handle;
    return ++g_windowCount;
}

void Forget(uintptr_t handle) {
    for (int i = 0; i < g_windowCount; ++i)
        if (g_windows[i] == handle) g_windows[i] = 0;  // the serial stays reserved: handles can be reused, serials are not
}

void Init() {
    const char* path = getenv("REB_MSG_TRACE");
    if (!path || !*path) { g_state = -1; return; }
    g_file = fopen(path, "w");
    g_state = g_file ? 1 : -1;
}

void FormatValue(char* out, size_t size, uintptr_t value, bool pointerLike) {
    if (value != 0) {
        if (const int serial = Serial(value)) { snprintf(out, size, "H%d", serial); return; }
        if (pointerLike && value > 0xffff) { snprintf(out, size, "ptr"); return; }
    }
    snprintf(out, size, "%x", static_cast<unsigned>(value));
}

bool CarriesPointer(unsigned message, bool lParam) {
    switch (message) {
        case 0x01: case 0x81: case 0x0c: case 0x0d: case 0x24: case 0x46: case 0x47: case 0x83: case 0x2b: case 0x7c: case 0x7d: return lParam;
        default: return message >= 0x400 && message < 0xc000;
    }
}

}  // namespace

bool Enabled() {
    if (g_state == 0) Init();
    return g_state > 0;
}

void Log(const char* windowClass, uintptr_t window, unsigned message, uintptr_t wParam, intptr_t lParam, int depth) {
    if (!Enabled()) return;
    const int serial = SerialOrNew(window);
    char name[24];
    name[0] = 0;
    for (const MessageName& n : kNames)
        if (n.id == message) { snprintf(name, sizeof name, "%s", n.name); break; }
    if (!name[0]) snprintf(name, sizeof name, "0x%x", message);
    char w[24], l[24];
    FormatValue(w, sizeof w, wParam, CarriesPointer(message, false));
    FormatValue(l, sizeof l, static_cast<uintptr_t>(lParam), CarriesPointer(message, true));
    char cls[64];
    snprintf(cls, sizeof cls, "%s", windowClass);
    for (char* c = cls; *c; ++c)
        if (*c == ' ') *c = '_';
    fprintf(g_file, "%d %s #%d %s %s %s\n", depth, cls, serial, name, w, l);
    fflush(g_file);
    if (message == 0x82) Forget(window);
}

void Note(const char* text) {
    if (!Enabled()) return;
    fprintf(g_file, "# %s\n", text);
    fflush(g_file);
}

}  // namespace msgtrace
