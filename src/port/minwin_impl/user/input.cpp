// SDL input events -> Windows messages, keyboard and mouse state, the cursor.
#include <SDL3/SDL.h>

#define MINWIN_IMPLEMENTATION 1
#include <string.h>

#include "port/minwin_impl/user/input.h"
#include "port/minwin_impl/user/user_internal.h"

namespace minwin {
namespace {

uint8_t g_keyDown[256];
uint8_t g_keyToggle[256];
POINT g_cursorPos = {0, 0};
unsigned g_buttonMask = 0;  // MK_LBUTTON 1, MK_RBUTTON 2, MK_MBUTTON 0x10
bool g_altDown = false;
unsigned g_lastKeyIndex = 0;

struct KeyInfo {
    int vk;
    int scan;
    bool extended;
};

KeyInfo MapScancode(int sc) {
    static const char kRow1[] = "qwertyuiop";
    static const char kRow2[] = "asdfghjkl";
    static const char kRow3[] = "zxcvbnm";
    if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z) {
        const char c = static_cast<char>('a' + (sc - SDL_SCANCODE_A));
        int scan = 0;
        for (int i = 0; kRow1[i]; ++i) if (kRow1[i] == c) scan = 16 + i;
        for (int i = 0; kRow2[i]; ++i) if (kRow2[i] == c) scan = 30 + i;
        for (int i = 0; kRow3[i]; ++i) if (kRow3[i] == c) scan = 44 + i;
        return {'A' + (sc - SDL_SCANCODE_A), scan, false};
    }
    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9) return {'1' + (sc - SDL_SCANCODE_1), 2 + (sc - SDL_SCANCODE_1), false};
    if (sc == SDL_SCANCODE_0) return {'0', 11, false};
    if (sc >= SDL_SCANCODE_F1 && sc <= SDL_SCANCODE_F10) return {0x70 + (sc - SDL_SCANCODE_F1), 59 + (sc - SDL_SCANCODE_F1), false};
    if (sc == SDL_SCANCODE_F11) return {0x7a, 87, false};
    if (sc == SDL_SCANCODE_F12) return {0x7b, 88, false};
    if (sc >= SDL_SCANCODE_KP_1 && sc <= SDL_SCANCODE_KP_9) {
        static const int scans[] = {79, 80, 81, 75, 76, 77, 71, 72, 73};
        return {0x61 + (sc - SDL_SCANCODE_KP_1), scans[sc - SDL_SCANCODE_KP_1], false};
    }
    switch (sc) {
        case SDL_SCANCODE_RETURN: return {0x0d, 28, false};
        case SDL_SCANCODE_ESCAPE: return {0x1b, 1, false};
        case SDL_SCANCODE_BACKSPACE: return {0x08, 14, false};
        case SDL_SCANCODE_TAB: return {0x09, 15, false};
        case SDL_SCANCODE_SPACE: return {0x20, 57, false};
        case SDL_SCANCODE_MINUS: return {0xbd, 12, false};
        case SDL_SCANCODE_EQUALS: return {0xbb, 13, false};
        case SDL_SCANCODE_LEFTBRACKET: return {0xdb, 26, false};
        case SDL_SCANCODE_RIGHTBRACKET: return {0xdd, 27, false};
        case SDL_SCANCODE_BACKSLASH: return {0xdc, 43, false};
        case SDL_SCANCODE_SEMICOLON: return {0xba, 39, false};
        case SDL_SCANCODE_APOSTROPHE: return {0xde, 40, false};
        case SDL_SCANCODE_GRAVE: return {0xc0, 41, false};
        case SDL_SCANCODE_COMMA: return {0xbc, 51, false};
        case SDL_SCANCODE_PERIOD: return {0xbe, 52, false};
        case SDL_SCANCODE_SLASH: return {0xbf, 53, false};
        case SDL_SCANCODE_CAPSLOCK: return {0x14, 58, false};
        case SDL_SCANCODE_SCROLLLOCK: return {0x91, 70, false};
        case SDL_SCANCODE_PAUSE: return {0x13, 69, false};
        case SDL_SCANCODE_PRINTSCREEN: return {0x2c, 55, true};
        case SDL_SCANCODE_INSERT: return {0x2d, 0x52, true};
        case SDL_SCANCODE_HOME: return {0x24, 0x47, true};
        case SDL_SCANCODE_PAGEUP: return {0x21, 0x49, true};
        case SDL_SCANCODE_DELETE: return {0x2e, 0x53, true};
        case SDL_SCANCODE_END: return {0x23, 0x4f, true};
        case SDL_SCANCODE_PAGEDOWN: return {0x22, 0x51, true};
        case SDL_SCANCODE_RIGHT: return {0x27, 0x4d, true};
        case SDL_SCANCODE_LEFT: return {0x25, 0x4b, true};
        case SDL_SCANCODE_DOWN: return {0x28, 0x50, true};
        case SDL_SCANCODE_UP: return {0x26, 0x48, true};
        case SDL_SCANCODE_NUMLOCKCLEAR: return {0x90, 69, true};
        case SDL_SCANCODE_KP_DIVIDE: return {0x6f, 0x35, true};
        case SDL_SCANCODE_KP_MULTIPLY: return {0x6a, 55, false};
        case SDL_SCANCODE_KP_MINUS: return {0x6d, 74, false};
        case SDL_SCANCODE_KP_PLUS: return {0x6b, 78, false};
        case SDL_SCANCODE_KP_ENTER: return {0x0d, 0x1c, true};
        case SDL_SCANCODE_KP_0: return {0x60, 82, false};
        case SDL_SCANCODE_KP_PERIOD: return {0x6e, 83, false};
        case SDL_SCANCODE_LSHIFT: return {0x10, 42, false};
        case SDL_SCANCODE_RSHIFT: return {0x10, 54, false};
        case SDL_SCANCODE_LCTRL: return {0x11, 29, false};
        case SDL_SCANCODE_RCTRL: return {0x11, 0x1d, true};
        case SDL_SCANCODE_LALT: return {0x12, 56, false};
        case SDL_SCANCODE_RALT: return {0x12, 0x38, true};
        default: return {0, 0, false};
    }
}

int SideSpecificVk(int sc) {
    switch (sc) {
        case SDL_SCANCODE_LSHIFT: return 0xa0;
        case SDL_SCANCODE_RSHIFT: return 0xa1;
        case SDL_SCANCODE_LCTRL: return 0xa2;
        case SDL_SCANCODE_RCTRL: return 0xa3;
        case SDL_SCANCODE_LALT: return 0xa4;
        case SDL_SCANCODE_RALT: return 0xa5;
        default: return 0;
    }
}

unsigned ModifierFlags() {
    unsigned mk = g_buttonMask;
    if (g_keyDown[VK_SHIFT]) mk |= MK_SHIFT;
    if (g_keyDown[VK_CONTROL]) mk |= MK_CONTROL;
    return mk;
}

void RefreshGenericModifiers() {
    g_keyDown[VK_SHIFT] = g_keyDown[0xa0] || g_keyDown[0xa1];
    g_keyDown[VK_CONTROL] = g_keyDown[0xa2] || g_keyDown[0xa3];
    g_keyDown[VK_MENU] = g_keyDown[0xa4] || g_keyDown[0xa5];
    g_altDown = g_keyDown[VK_MENU] != 0;
}

unsigned ControlCharacter(int vk, bool control) {
    if (control && vk >= 'A' && vk <= 'Z') return static_cast<unsigned>(vk - 'A' + 1);
    switch (vk) {
        case 0x1b: return 0x1b;
        case 0x0d: return 0x0d;
        case 0x08: return 0x08;
        case 0x09: return 0x09;
        default: return 0;
    }
}

unsigned PrintableCharacter(int sc, int vk, bool shift, bool capsLock) {
    if (vk >= 'A' && vk <= 'Z') return (shift != capsLock) ? static_cast<unsigned>(vk) : static_cast<unsigned>(vk - 'A' + 'a');
    if (vk >= '0' && vk <= '9') return shift ? static_cast<unsigned>(")!@#$%^&*("[vk - '0']) : static_cast<unsigned>(vk);
    if (vk >= 0x60 && vk <= 0x69) return static_cast<unsigned>('0' + vk - 0x60);
    if (vk == 0x20) return ' ';
    struct Pair {
        int sc;
        char plain;
        char shifted;
    };
    static const Pair kPairs[] = {
        {SDL_SCANCODE_MINUS, '-', '_'}, {SDL_SCANCODE_EQUALS, '=', '+'}, {SDL_SCANCODE_LEFTBRACKET, '[', '{'},
        {SDL_SCANCODE_RIGHTBRACKET, ']', '}'}, {SDL_SCANCODE_BACKSLASH, '\\', '|'}, {SDL_SCANCODE_SEMICOLON, ';', ':'},
        {SDL_SCANCODE_APOSTROPHE, '\'', '"'}, {SDL_SCANCODE_GRAVE, '`', '~'}, {SDL_SCANCODE_COMMA, ',', '<'},
        {SDL_SCANCODE_PERIOD, '.', '>'}, {SDL_SCANCODE_SLASH, '/', '?'}, {SDL_SCANCODE_KP_PLUS, '+', '+'},
        {SDL_SCANCODE_KP_MINUS, '-', '-'}, {SDL_SCANCODE_KP_MULTIPLY, '*', '*'}, {SDL_SCANCODE_KP_DIVIDE, '/', '/'},
        {SDL_SCANCODE_KP_PERIOD, '.', '.'},
    };
    for (const Pair& p : kPairs)
        if (p.sc == sc) return static_cast<unsigned>(shift ? p.shifted : p.plain);
    return 0;
}

unsigned MessageFor(int button, bool down, bool dbl) {
    if (button == 1) return down ? (dbl ? WM_LBUTTONDBLCLK : WM_LBUTTONDOWN) : WM_LBUTTONUP;
    if (button == 2) return down ? (dbl ? kWmRButtonDblClk : WM_RBUTTONDOWN) : WM_RBUTTONUP;
    return down ? (dbl ? kWmMButtonDblClk : kWmMButtonDown) : kWmMButtonUp;
}

Window* g_lastUnderCursor = nullptr;
bool g_layoutChanged = false;

}  // namespace

// Windows posts a WM_MOUSEMOVE when a window appears, disappears or moves under a motionless pointer. Under Wine only top-level windows
// do (the X server reports the pointer entering a new X window; child windows live inside their top-level's X window), which is what
// the callers in window.cpp mark.
void MarkLayoutChanged() { g_layoutChanged = true; }

void SynthesizeMouseMoveIfNeeded() {
    if (!g_layoutChanged) return;
    g_layoutChanged = false;
    if (CaptureWindow()) return;
    Window* under = WindowAtPoint(g_cursorPos, false, false, nullptr);
    if (under == g_lastUnderCursor || !under) {
        g_lastUnderCursor = under;
        return;
    }
    InputMouseMotion(g_cursorPos.x, g_cursorPos.y);
}

void InitInput() {
    memset(g_keyDown, 0, sizeof g_keyDown);
    memset(g_keyToggle, 0, sizeof g_keyToggle);
}

POINT CursorScreenPosition() { return g_cursorPos; }
void SetCursorScreenPosition(POINT p) { g_cursorPos = p; }
unsigned CurrentModifierMask() { return ModifierFlags(); }

void InputMouseMotion(int x, int y) {
    g_cursorPos.x = x;
    g_cursorPos.y = y;
    InputEvent e;
    memset(&e, 0, sizeof e);
    e.type = InputEvent::MouseMove;
    e.x = x;
    e.y = y;
    e.mk = ModifierFlags();
    e.time = PlatformTicksMs();
    QueueInputEvent(e);
}

namespace {

// Windows' rule (the system's double-click time and rectangle, 500 ms and 4x4 pixels): a second press of the same button close to
// the previous one in time and place is a double click; the press after a double click starts over.
int CountClicks(int button, int x, int y, unsigned timeMs) {
    constexpr unsigned kDoubleClickMs = 500;
    constexpr int kHalfRectangle = 2;
    static int lastButton = 0, lastX = 0, lastY = 0, count = 0;
    static unsigned lastTime = 0;
    const bool again = count == 1 && button == lastButton && timeMs - lastTime <= kDoubleClickMs && x >= lastX - kHalfRectangle &&
                       x <= lastX + kHalfRectangle && y >= lastY - kHalfRectangle && y <= lastY + kHalfRectangle;
    count = again ? 2 : 1;
    lastButton = button;
    lastX = x;
    lastY = y;
    lastTime = timeMs;
    return count;
}

}  // namespace

void InputMouseButton(int button, bool down, unsigned timeMs, int x, int y) {
    const int clicks = down ? CountClicks(button, x, y, timeMs) : 1;
    g_cursorPos.x = x;
    g_cursorPos.y = y;
    const unsigned flag = button == 1 ? MK_LBUTTON : button == 2 ? 2u : 0x10u;
    if (down) g_buttonMask |= flag; else g_buttonMask &= ~flag;
    const int vk = button == 1 ? 1 : button == 2 ? 2 : 4;
    g_keyDown[vk] = down ? 1 : 0;
    InputEvent e;
    memset(&e, 0, sizeof e);
    e.type = InputEvent::MouseButton;
    e.x = x;
    e.y = y;
    e.button = button;
    e.down = down;
    e.clicks = clicks;
    e.mk = ModifierFlags();
    e.time = PlatformTicksMs();
    QueueInputEvent(e);
}

void InputKey(int sdlScancode, bool down, bool repeat, unsigned) {
    const KeyInfo info = MapScancode(sdlScancode);
    if (!info.vk) return;
    if (const int sided = SideSpecificVk(sdlScancode)) g_keyDown[sided] = down ? 1 : 0;
    g_keyDown[info.vk] = down ? 1 : 0;
    RefreshGenericModifiers();
    if (down && !repeat && (info.vk == 0x14 || info.vk == 0x90 || info.vk == 0x91)) g_keyToggle[info.vk] ^= 1;
    InputEvent e;
    memset(&e, 0, sizeof e);
    e.type = InputEvent::Key;
    e.vk = info.vk;
    e.scan = info.scan;
    e.extended = info.extended;
    e.down = down;
    e.repeat = repeat;
    e.time = PlatformTicksMs();
    if (down && !g_altDown) {
        e.ch = ControlCharacter(info.vk, g_keyDown[VK_CONTROL] != 0);
        if (!e.ch && !g_keyDown[VK_CONTROL])
            e.ch = PrintableCharacter(sdlScancode, info.vk, g_keyDown[VK_SHIFT] != 0, g_keyToggle[0x14] != 0);
    }
    QueueInputEvent(e);
}

bool FinalizeInput(InputEvent* e) {
    memset(&e->msg, 0, sizeof e->msg);
    e->msg.time = e->time;
    switch (e->type) {
        case InputEvent::Key: {
            Window* target = FocusWindow();
            if (!target) target = ActiveWindow();
            if (!target) return false;
            const bool alt = g_altDown;
            const bool sys = alt || e->vk == 0x79;  // F10
            unsigned message = e->down ? (sys ? WM_SYSKEYDOWN : WM_KEYDOWN) : (sys ? WM_SYSKEYUP : WM_KEYUP);
            LPARAM lParam = 1 | (static_cast<LPARAM>(e->scan) << 16) | (e->extended ? 1u << 24 : 0u) | (alt ? 1u << 29 : 0u);
            if (!e->down) lParam |= (3u << 30);
            else if (e->repeat) lParam |= (1u << 30);
            lParam = static_cast<LPARAM>(static_cast<uint32_t>(lParam));
            e->msg.hwnd = ToHandle(target);
            e->msg.message = message;
            e->msg.wParam = static_cast<WPARAM>(e->vk);
            e->msg.lParam = lParam;
            e->msg.pt = g_cursorPos;
            if (e->down && e->ch) RecordKeyChar(e->msg.wParam, lParam, e->ch);
            return true;
        }
        case InputEvent::MouseMove:
        case InputEvent::MouseButton: {
            const POINT pt = {e->x, e->y};
            Window* target = nullptr;
            int hit = kHtClient;
            if (CaptureWindow()) {
                target = CaptureWindow();
            } else {
                target = WindowAtPoint(pt, false, true, &hit);
                g_lastUnderCursor = target;
            }
            if (!target || !IsLive(target)) return false;
            if (!IsEnabledTree(target)) return false;
            const bool isMove = e->type == InputEvent::MouseMove;
            const bool dbl = !isMove && e->down && e->clicks == 2 && (target->cls->style & CS_DBLCLKS);
            unsigned message = isMove ? WM_MOUSEMOVE : MessageFor(e->button, e->down, dbl);
            if (!isMove && e->down) {
                if ((target->style & WS_CHILD) || target->parent) {
                    const unsigned downMessage = MessageFor(e->button, true, false);
                    for (Window* w = target; w->parent && (w->style & WS_CHILD) && !(w->exStyle & kWsExNoParentNotify); w = w->parent) {
                        const POINT origin = ClientOriginOnScreen(w->parent);
                        Send(w->parent, kWmParentNotify, downMessage, MakeLParam(pt.x - origin.x, pt.y - origin.y));
                        if (!IsLive(target)) return false;
                    }
                }
                Window* top = TopLevelOf(target);
                const LRESULT activate = Send(target, WM_MOUSEACTIVATE, reinterpret_cast<WPARAM>(ToHandle(top)), MakeLParam(hit, static_cast<int>(message)));
                if (!IsLive(target)) return false;
                if (activate != 3 && activate != 4 && top && top != ActiveWindow() && IsLive(top)) ActivateTopLevel(top, true);
                if (activate == 2 || activate == 4) return false;
                if (!IsLive(target)) return false;
            }
            if (isMove || e->down) {
                Send(target, kWmSetCursor, reinterpret_cast<WPARAM>(ToHandle(target)), MakeLParam(hit, static_cast<int>(message)));
                if (!IsLive(target)) return false;
            }
            e->msg.hwnd = ToHandle(target);
            e->msg.pt = pt;
            if (hit == kHtClient) {
                const POINT origin = ClientOriginOnScreen(target);
                e->msg.message = message;
                e->msg.wParam = e->mk;
                e->msg.lParam = MakeLParam(pt.x - origin.x, pt.y - origin.y);
            } else {
                unsigned nc = kWmNcMouseMove;
                if (!isMove) nc = e->button == 1 ? (e->down ? (dbl ? WM_NCLBUTTONDBLCLK : kWmNcLButtonDown) : kWmNcLButtonUp)
                                : e->button == 2 ? (e->down ? kWmNcRButtonDown : kWmNcRButtonUp) : kWmNcMButtonDown;
                e->msg.message = nc;
                e->msg.wParam = static_cast<WPARAM>(hit);
                e->msg.lParam = MakeLParam(pt.x, pt.y);
            }
            return true;
        }
    }
    return false;
}

}  // namespace minwin

using namespace minwin;

// ---- keyboard state, cursor -------------------------------------------------------------------------------------------

extern "C" {

SHORT REB_STDCALL GetKeyState(int vk) {
    if (vk < 0 || vk > 255) return 0;
    SHORT state = g_keyDown[vk] ? static_cast<SHORT>(0x8000) : 0;
    if (g_keyToggle[vk]) state |= 1;
    return state;
}

SHORT REB_STDCALL GetAsyncKeyState(int vk) {
    if (vk < 0 || vk > 255) return 0;
    return g_keyDown[vk] ? static_cast<SHORT>(0x8000) : 0;
}

BOOL REB_STDCALL GetCursorPos(LPPOINT p) {
    *p = CursorScreenPosition();
    return TRUE;
}

}  // extern "C"
