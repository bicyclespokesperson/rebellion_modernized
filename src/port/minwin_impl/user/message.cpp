// The message queue: SendMessage (re-entrant), PostMessage, GetMessage/PeekMessage with Windows' priorities (sent, posted,
// input, quit, paint, timers), DispatchMessage, TranslateMessage, accelerators, hooks, SetTimer and DefWindowProc.
#define MINWIN_IMPLEMENTATION 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "port/minwin_impl/res/modules.h"
#include "port/minwin_impl/user/input.h"
#include "port/minwin_impl/user/user_internal.h"
#include "port/msg_trace.h"

namespace minwin::present {
void PaceIdleLoop();  // present.cpp
}

namespace minwin {
namespace {

struct Posted {
    MSG msg;
    Posted* next;
};
struct Input {
    InputEvent event;
    Input* next;
};
struct Timer {
    Window* window;
    UINT_PTR id;
    unsigned interval;
    unsigned due;
    TIMERPROC proc;
    Timer* next;
};

Posted* g_postedHead = nullptr;
Posted* g_postedTail = nullptr;
Input* g_inputHead = nullptr;
Input* g_inputTail = nullptr;
Timer* g_timers = nullptr;
UINT_PTR g_nextTimerId = 0x1000;
bool g_quitPending = false;
int g_quitCode = 0;
int g_depth = 0;
DWORD g_lastMessagePos = 0;
Hook* g_hooks[16];
Hook* g_currentHook = nullptr;

struct KeyChar {
    WPARAM vk;
    LPARAM lParam;
    unsigned ch;
    bool valid;
};
KeyChar g_keyChars[8];
int g_keyCharNext = 0;

bool InRange(UINT message, UINT min, UINT max) { return (min == 0 && max == 0) || (message >= min && message <= max); }

bool MatchesWindow(const Window* target, const Window* filter) {
    return !filter || target == filter || (target && IsDescendant(filter, target));
}

LRESULT CallProc(Window* w, UINT msg, WPARAM wp, LPARAM lp) {
    if (!w->cls->builtin && msgtrace::Enabled()) msgtrace::Log(w->cls->name, w->handle, msg, wp, lp, g_depth);
    WNDPROC proc = w->proc;
    // The procedure may destroy its own window (a popup menu dismissing itself): `w` is not touched afterwards.
    const bool builtin = w->cls->builtin;
    if (!builtin) ++g_depth;
    LRESULT result;
    if (proc)
        result = proc(ToHandle(w), msg, wp, lp);
    else
        result = DefaultWindowProc(w, msg, wp, lp);
    if (!builtin) --g_depth;
    if (msg == WM_QUERYNEWPALETTE) FlushPaletteNotification();
    return result;
}

void AppendPosted(const MSG& m) {
    Posted* node = static_cast<Posted*>(Alloc(sizeof(Posted)));
    node->msg = m;
    if (g_postedTail) g_postedTail->next = node; else g_postedHead = node;
    g_postedTail = node;
}

void RemovePosted(Posted* node, Posted* before) {
    if (before) before->next = node->next; else g_postedHead = node->next;
    if (g_postedTail == node) g_postedTail = before;
    Free(node);
}

void RemoveInput(Input* node, Input* before) {
    if (before) before->next = node->next; else g_inputHead = node->next;
    if (g_inputTail == node) g_inputTail = before;
    Free(node);
}

Hook* NewestLiveHook(int type) {
    for (Hook* h = g_hooks[type]; h; h = h->next)
        if (!h->dead) return h;
    return nullptr;
}

LRESULT InvokeHook(Hook* hook, int code, WPARAM wp, LPARAM lp) {
    Hook* saved = g_currentHook;
    g_currentHook = hook;
    const LRESULT r = hook->proc(code, wp, lp);
    g_currentHook = saved;
    return r;
}

bool RunMouseHook(const MSG& m, bool remove) {
    if (!NewestLiveHook(WH_MOUSE)) return false;
    MOUSEHOOKSTRUCT info;
    memset(&info, 0, sizeof info);
    info.pt = m.pt;
    info.hwnd = m.hwnd;
    info.wHitTestCode = kHtClient;
    return CallHooks(WH_MOUSE, remove ? kHcAction : kHcNoRemove, m.message, reinterpret_cast<LPARAM>(&info)) != 0;
}

bool RunKeyboardHook(const MSG& m, bool remove) {
    if (!NewestLiveHook(WH_KEYBOARD)) return false;
    return CallHooks(WH_KEYBOARD, remove ? kHcAction : kHcNoRemove, m.wParam, m.lParam) != 0;
}

void RunGetMessageHook(MSG* m, bool remove) {
    if (!NewestLiveHook(WH_GETMESSAGE)) return;
    CallHooks(WH_GETMESSAGE, kHcAction, remove ? PM_REMOVE : PM_NOREMOVE, reinterpret_cast<LPARAM>(m));
}

#if defined(_WIN32)
extern "C" const char __ImageBase;
#endif

// REB_MINWIN_TRACE=stacks: the code addresses on the stack (tools/map_lookup.py turns them into function names).
void NoteCodeAddresses(const char* what) {
    static int mode = -1;
    if (mode < 0) {
        const char* text = getenv("REB_MINWIN_TRACE");
        mode = text && strstr(text, "stacks") ? 1 : 0;
    }
    if (!mode || !msgtrace::Enabled()) return;
#if defined(_WIN32)
    const uint8_t* base = reinterpret_cast<const uint8_t*>(&__ImageBase);
#else
    return;
    const uint8_t* base = nullptr;
#endif
    uint32_t peOffset, sizeOfCode, baseOfCode;
    memcpy(&peOffset, base + 0x3c, 4);
    memcpy(&sizeOfCode, base + peOffset + 24 + 4, 4);
    memcpy(&baseOfCode, base + peOffset + 24 + 20, 4);
    const uint32_t codeBegin = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(base)) + baseOfCode;
    const uint32_t codeEnd = codeBegin + sizeOfCode * 2;
    uintptr_t local = 0;
    const uint32_t* stack = reinterpret_cast<const uint32_t*>(&local);
    char line[400];
    int length = snprintf(line, sizeof line, "stack(%s %x-%x):", what, codeBegin, codeEnd);
    int found = 0;
    for (int i = 0; i < 1024 && found < 24 && length < 360; ++i)
        if (stack[i] >= codeBegin && stack[i] < codeEnd) {
            length += snprintf(line + length, sizeof line - static_cast<size_t>(length), " %x", stack[i]);
            ++found;
        }
    msgtrace::Note(line);
}

bool IsMouseMessage(UINT m) { return m >= kWmMouseFirst && m <= kWmMouseLast; }
bool IsKeyMessage(UINT m) { return m >= WM_KEYDOWN && m <= kWmSysChar && m != WM_CHAR && m != 0x103 && m != kWmSysChar; }

}  // namespace

int NestingDepth() { return g_depth; }

// minwin's window manager is single-threaded: the thread that initialises the runtime owns every window and queue. The game's
// movie thread sends messages to its parent window; those requests are executed by the owner thread, from its message pump,
// and the sender waits (as SendMessage across threads does on Windows).
namespace {

struct CrossThreadRequest {
    Window* window;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
    LRESULT result;
    volatile LONG done;
    volatile LONG abandoned;
    CrossThreadRequest* next;
};

const DWORD g_ownerThread = GetCurrentThreadId();
CRITICAL_SECTION g_requestLock;
bool g_requestLockReady = false;
CrossThreadRequest* g_requests = nullptr;

void EnsureRequestLock() {
    if (!g_requestLockReady) {
        InitializeCriticalSection(&g_requestLock);
        g_requestLockReady = true;
    }
}

}  // namespace

bool IsOwnerThread() { return GetCurrentThreadId() == g_ownerThread; }

void ProcessCrossThreadRequests() {
    if (!g_requestLockReady) return;
    for (;;) {
        EnterCriticalSection(&g_requestLock);
        CrossThreadRequest* request = g_requests;
        if (request) g_requests = request->next;
        LeaveCriticalSection(&g_requestLock);
        if (!request) return;
        request->result = IsLive(request->window) ? Send(request->window, request->message, request->wParam, request->lParam) : 0;
        InterlockedExchange(&request->done, 1);
        if (request->abandoned) Free(request);
    }
}

LRESULT SendFromOtherThread(Window* w, UINT msg, WPARAM wp, LPARAM lp) {
    EnsureRequestLock();
    CrossThreadRequest* request = static_cast<CrossThreadRequest*>(Alloc(sizeof(CrossThreadRequest)));
    request->window = w;
    request->message = msg;
    request->wParam = wp;
    request->lParam = lp;
    EnterCriticalSection(&g_requestLock);
    CrossThreadRequest** tail = &g_requests;
    while (*tail) tail = &(*tail)->next;
    *tail = request;
    LeaveCriticalSection(&g_requestLock);
    for (int waited = 0; !request->done && waited < 5000; ++waited) Sleep(1);
    if (!request->done && InterlockedExchange(&request->abandoned, 1) == 0) return 0;  // the owner frees it when it gets to it
    const LRESULT result = request->result;
    Free(request);
    return result;
}
unsigned GetTicksMs() { return PlatformTicksMs(); }

LRESULT Send(Window* w, UINT msg, WPARAM wp, LPARAM lp) {
    if (!w) return 0;
    return CallProc(w, msg, wp, lp);
}

bool Post(Window* w, UINT msg, WPARAM wp, LPARAM lp) {
    MSG m;
    memset(&m, 0, sizeof m);
    m.hwnd = ToHandle(w);
    m.message = msg;
    m.wParam = wp;
    m.lParam = lp;
    m.time = GetTicksMs();
    const POINT p = CursorScreenPosition();
    m.pt = p;
    AppendPosted(m);
    return true;
}

// SDL reports the text a key press produced after the key event; it replaces the US-layout guess made when the key was queued.
void InputText(const char* utf8) {
    if (!utf8 || !*utf8 || !g_inputTail) return;
    InputEvent& e = g_inputTail->event;
    if (e.type != InputEvent::Key || !e.down || e.finalized || !e.ch) return;
    const unsigned char c = static_cast<unsigned char>(utf8[0]);
    if (c >= 0x20 && c < 0x7f) e.ch = c;
    else if (c == 0xc2 || c == 0xc3) e.ch = ((c & 0x1f) << 6) | (static_cast<unsigned char>(utf8[1]) & 0x3f);
}

void QueueInputEvent(const InputEvent& e) {
    Input* node = static_cast<Input*>(Alloc(sizeof(Input)));
    node->event = e;
    // Consecutive unprocessed mouse moves collapse into the newest one, as Windows does.
    if (e.type == InputEvent::MouseMove && g_inputTail && g_inputTail->event.type == InputEvent::MouseMove && !g_inputTail->event.finalized) {
        g_inputTail->event = e;
        Free(node);
        return;
    }
    if (g_inputTail) g_inputTail->next = node; else g_inputHead = node;
    g_inputTail = node;
}

void PurgeMessagesFor(Window* w) {
    Posted* before = nullptr;
    for (Posted* n = g_postedHead; n;) {
        Posted* next = n->next;
        if (n->msg.hwnd == ToHandle(w)) RemovePosted(n, before); else before = n;
        n = next;
    }
}

void PurgeTimersFor(Window* w) {
    Timer** link = &g_timers;
    while (*link) {
        if ((*link)->window == w) {
            Timer* dead = *link;
            *link = dead->next;
            Free(dead);
        } else {
            link = &(*link)->next;
        }
    }
}

LRESULT CallHooks(int type, int code, WPARAM wp, LPARAM lp) {
    Hook* hook = NewestLiveHook(type);
    if (!hook) return 0;
    return InvokeHook(hook, code, wp, lp);
}

bool HasHook(int type) { return NewestLiveHook(type) != nullptr; }

void SetQuit(int code) {
    g_quitPending = true;
    g_quitCode = code;
}

// ---- retrieval -----------------------------------------------------------------------------------------------------------

static Timer* DueTimer(Window* filter, unsigned now) {
    for (Timer* t = g_timers; t; t = t->next)
        if (static_cast<int>(now - t->due) >= 0 && (!t->window || MatchesWindow(t->window, filter))) return t;
    return nullptr;
}

static unsigned NextTimerDelay(unsigned now, unsigned limit) {
    unsigned best = limit;
    for (Timer* t = g_timers; t; t = t->next) {
        const int remaining = static_cast<int>(t->due - now);
        if (remaining <= 0) return 0;
        if (static_cast<unsigned>(remaining) < best) best = static_cast<unsigned>(remaining);
    }
    return best;
}

bool NextMessage(MSG* out, Window* filter, UINT min, UINT max, bool remove, bool wait) {
    ProcessCrossThreadRequests();
    FlushPaletteNotification();
    if (g_depth > 0 && msgtrace::Enabled()) {
        char note[64];
        snprintf(note, sizeof note, "nested %s at depth %d", wait ? "GetMessage" : "PeekMessage", g_depth);
        msgtrace::Note(note);
        NoteCodeAddresses("nested");
    }
    for (;;) {
    restart:
        PumpPlatform();

        Posted* before = nullptr;
        for (Posted* n = g_postedHead; n;) {
            Window* target = FromHandle(n->msg.hwnd);
            if (n->msg.hwnd && !target) {
                Posted* next = n->next;
                RemovePosted(n, before);
                n = next;
                continue;
            }
            if (!InRange(n->msg.message, min, max) || (filter && !MatchesWindow(target, filter))) {
                before = n;
                n = n->next;
                continue;
            }
            *out = n->msg;
            if (remove) RemovePosted(n, before);
            RunGetMessageHook(out, remove);
            return true;
        }

        SynthesizeMouseMoveIfNeeded();
        Input* inputBefore = nullptr;
        for (Input* n = g_inputHead; n;) {
            InputEvent& e = n->event;
            if (!e.finalized) {
                e.finalized = true;
                e.keep = FinalizeInput(&e);
            }
            if (!e.keep) {
                Input* next = n->next;
                RemoveInput(n, inputBefore);
                n = next;
                continue;
            }
            Window* target = FromHandle(e.msg.hwnd);
            if (!e.msg.hwnd || !target || !InRange(e.msg.message, min, max) || (filter && !MatchesWindow(target, filter))) {
                if (e.msg.hwnd && !target) {
                    Input* next = n->next;
                    RemoveInput(n, inputBefore);
                    n = next;
                    continue;
                }
                inputBefore = n;
                n = n->next;
                continue;
            }
            *out = e.msg;
            bool discard = false;
            if (IsMouseMessage(out->message)) discard = RunMouseHook(*out, remove);
            else if (IsKeyMessage(out->message)) discard = RunKeyboardHook(*out, remove);
            if (remove || discard) {
                RemoveInput(n, inputBefore);
            }
            if (discard) goto restart;
            RunGetMessageHook(out, remove);
            return true;
        }

        if (g_quitPending && InRange(WM_QUIT, min, max) && !filter) {
            memset(out, 0, sizeof *out);
            out->message = WM_QUIT;
            out->wParam = static_cast<WPARAM>(g_quitCode);
            out->time = GetTicksMs();
            if (remove) g_quitPending = false;
            RunGetMessageHook(out, remove);
            return true;
        }

        if (InRange(WM_PAINT, min, max)) {
            if (Window* w = FindWindowNeedingPaint()) {
                if (!filter || MatchesWindow(w, filter)) {
                    memset(out, 0, sizeof *out);
                    out->hwnd = ToHandle(w);
                    out->message = WM_PAINT;
                    out->time = GetTicksMs();
                    out->pt = CursorScreenPosition();
                    RunGetMessageHook(out, remove);
                    return true;
                }
            }
        }

        if (InRange(WM_TIMER, min, max)) {
            const unsigned now = GetTicksMs();
            if (Timer* t = DueTimer(filter, now)) {
                memset(out, 0, sizeof *out);
                out->hwnd = ToHandle(t->window);
                out->message = WM_TIMER;
                out->wParam = t->id;
                out->lParam = reinterpret_cast<LPARAM>(t->proc);
                out->time = now;
                out->pt = CursorScreenPosition();
                if (remove) t->due = now + t->interval;
                RunGetMessageHook(out, remove);
                return true;
            }
        }

        if (!wait) return false;
        WaitForPlatformEvent(NextTimerDelay(GetTicksMs(), 10));
        ProcessCrossThreadRequests();
    }
}

}  // namespace minwin

using namespace minwin;

// ---- Win32 entry points ------------------------------------------------------------------------------------------------

extern "C" {

BOOL REB_STDCALL PeekMessageA(LPMSG msg, HWND hwnd, UINT min, UINT max, UINT flags) {
    Window* filter = FromHandle(hwnd);
    if (hwnd && !filter) return FALSE;
    if (!NextMessage(msg, filter, min, max, (flags & PM_REMOVE) != 0, false)) {
        if ((flags & PM_REMOVE) && NestingDepth() == 0) present::PaceIdleLoop();
        return FALSE;
    }
    if (flags & PM_REMOVE) g_lastMessagePos = static_cast<DWORD>(MakeLParam(msg->pt.x, msg->pt.y));
    return TRUE;
}

BOOL REB_STDCALL GetMessageA(LPMSG msg, HWND hwnd, UINT min, UINT max) {
    Window* filter = FromHandle(hwnd);
    if (hwnd && !filter) return -1;
    NextMessage(msg, filter, min, max, true, true);
    g_lastMessagePos = static_cast<DWORD>(MakeLParam(msg->pt.x, msg->pt.y));
    return msg->message != WM_QUIT;
}

DWORD REB_STDCALL GetMessagePos(void) { return g_lastMessagePos; }

LRESULT REB_STDCALL DispatchMessageA(const MSG* msg) {
    if (msg->message == WM_TIMER && msg->lParam) {
        TIMERPROC proc = reinterpret_cast<TIMERPROC>(msg->lParam);
        proc(msg->hwnd, WM_TIMER, msg->wParam, GetTicksMs());
        return 0;
    }
    Window* w = FromHandle(msg->hwnd);
    if (!w) return 0;
    if (msg->message == WM_PAINT && !w->hasUpdate) return 0;
    return Send(w, msg->message, msg->wParam, msg->lParam);
}

BOOL REB_STDCALL TranslateMessage(const MSG* msg) {
    if (msg->message != WM_KEYDOWN && msg->message != WM_SYSKEYDOWN) return FALSE;
    for (KeyChar& k : g_keyChars) {
        if (!k.valid || k.vk != msg->wParam || k.lParam != msg->lParam) continue;
        k.valid = false;
        if (!k.ch) return FALSE;
        Post(FromHandle(msg->hwnd), msg->message == WM_KEYDOWN ? WM_CHAR : kWmSysChar, k.ch, msg->lParam);
        return TRUE;
    }
    return FALSE;
}

LRESULT REB_STDCALL SendMessageA(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (reinterpret_cast<uintptr_t>(hwnd) == static_cast<uintptr_t>(-1) || reinterpret_cast<uintptr_t>(hwnd) == 0xffff) {
        for (Window* t = FirstTopLevel(); t;) {
            Window* next = t->next;
            Send(t, msg, wp, lp);
            t = next;
        }
        return 1;
    }
    Window* w = FromHandle(hwnd);
    if (!w) return 0;
    if (!IsOwnerThread()) return SendFromOtherThread(w, msg, wp, lp);
    return Send(w, msg, wp, lp);
}

BOOL REB_STDCALL PostMessageA(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (reinterpret_cast<uintptr_t>(hwnd) == static_cast<uintptr_t>(-1) || reinterpret_cast<uintptr_t>(hwnd) == 0xffff) {
        for (Window* t = FirstTopLevel(); t; t = t->next) Post(t, msg, wp, lp);
        return TRUE;
    }
    if (!hwnd) {
        Post(nullptr, msg, wp, lp);
        return TRUE;
    }
    Window* w = FromHandle(hwnd);
    if (!w) return FALSE;
    return Post(w, msg, wp, lp);
}

void REB_STDCALL PostQuitMessage(int code) { SetQuit(code); }

LRESULT REB_STDCALL DefWindowProcA(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Window* w = FromHandle(hwnd);
    if (!w) return 0;
    return DefaultWindowProc(w, msg, wp, lp);
}

// ---- timers ---------------------------------------------------------------------------------------------------------------

UINT_PTR REB_STDCALL SetTimer(HWND hwnd, UINT_PTR id, UINT elapse, TIMERPROC proc) {
    Window* w = FromHandle(hwnd);
    if (hwnd && !w) return 0;
    if (elapse < 10) elapse = 10;
    Timer* t = nullptr;
    if (w)
        for (Timer* e = g_timers; e; e = e->next)
            if (e->window == w && e->id == id) { t = e; break; }
    if (!t) {
        t = static_cast<Timer*>(Alloc(sizeof(Timer)));
        t->window = w;
        t->id = w ? id : g_nextTimerId++;
        t->next = g_timers;
        g_timers = t;
    }
    t->interval = elapse;
    t->due = GetTicksMs() + elapse;
    t->proc = proc;
    return t->id;
}

BOOL REB_STDCALL KillTimer(HWND hwnd, UINT_PTR id) {
    Window* w = FromHandle(hwnd);
    for (Timer** link = &g_timers; *link; link = &(*link)->next) {
        if ((*link)->window == w && (*link)->id == id) {
            Timer* dead = *link;
            *link = dead->next;
            Free(dead);
            return TRUE;
        }
    }
    return FALSE;
}

// ---- hooks --------------------------------------------------------------------------------------------------------------

HHOOK REB_STDCALL SetWindowsHookExA(int type, HOOKPROC proc, HINSTANCE module, DWORD thread) {
    if (type < 0 || type >= 16 || !proc) return nullptr;
    Hook* h = static_cast<Hook*>(Alloc(sizeof(Hook)));
    h->type = type;
    h->proc = proc;
    h->module = module;
    h->thread = thread;
    h->next = g_hooks[type];
    g_hooks[type] = h;
    return reinterpret_cast<HHOOK>(h);
}

BOOL REB_STDCALL UnhookWindowsHookEx(HHOOK handle) {
    Hook* target = reinterpret_cast<Hook*>(handle);
    for (int type = 0; type < 16; ++type)
        for (Hook* h = g_hooks[type]; h; h = h->next)
            if (h == target) { h->dead = true; return TRUE; }
    return FALSE;
}

LRESULT REB_STDCALL CallNextHookEx(HHOOK handle, int code, WPARAM wp, LPARAM lp) {
    Hook* current = handle ? reinterpret_cast<Hook*>(handle) : g_currentHook;
    if (!current) return 0;
    for (Hook* h = current->next; h; h = h->next)
        if (!h->dead) return InvokeHook(h, code, wp, lp);
    return 0;
}

// ---- accelerators ---------------------------------------------------------------------------------------------------------

}  // extern "C"

namespace minwin {

LRESULT SendCommandFromAccelerator(HWND hwnd, unsigned command) {
    return SendMessageA(hwnd, WM_COMMAND, MAKEWPARAM(command, 1), 0);
}

}  // namespace minwin

namespace {

struct TranslatedKey {
    WPARAM vk;
    bool valid;
};
TranslatedKey g_translatedKey;

}  // namespace

extern "C" int REB_STDCALL TranslateAcceleratorA(HWND hwnd, HACCEL table, LPMSG msg) {
    const std::vector<mw::res::Accelerator>* accel = mw::res::AcceleratorsFromHandle(table);
    if (!accel || !msg) return 0;
    if ((msg->message == WM_KEYUP || msg->message == WM_SYSKEYUP) && g_translatedKey.valid && g_translatedKey.vk == msg->wParam) {
        g_translatedKey.valid = false;
        return 1;
    }
    if (msg->message != WM_KEYDOWN && msg->message != WM_SYSKEYDOWN) return 0;
    const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    const bool control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    for (const mw::res::Accelerator& e : *accel) {
        if (!(e.flags & 1)) continue;  // ASCII entries match WM_CHAR, which the game's table does not use
        if (e.key != msg->wParam) continue;
        if (((e.flags & 4) != 0) != shift || ((e.flags & 8) != 0) != control || ((e.flags & 0x10) != 0) != alt) continue;
        g_translatedKey.vk = msg->wParam;
        g_translatedKey.valid = true;
        minwin::SendCommandFromAccelerator(hwnd, e.command);
        return 1;
    }
    return 0;
}

namespace minwin {

// ---- DefWindowProc -------------------------------------------------------------------------------------------------------------

static void NcInsetsFor(DWORD style, int* l, int* t, int* r, int* b) {
    int frame = 0;
    if (style & WS_BORDER) frame += 1;
    if (style & WS_DLGFRAME) frame += 3;
    if (style & 0x40000) frame += 4;
    *l = *r = *b = frame;
    *t = frame + ((style & 0xc00000) == 0xc00000 ? 19 : 0);
}

static void SendSizeAndMove(Window* w, const WindowPos* pos) {
    if (!(pos->flags & kSwpNoClientMove))
        Send(w, WM_MOVE, 0, MakeLParam(w->x + w->clientLeft, w->y + w->clientTop));
    if (!IsLive(w)) return;
    if (!(pos->flags & kSwpNoClientSize))
        Send(w, WM_SIZE, w->iconic ? kSizeMinimized : kSizeRestored, MakeLParam(w->clientWidth, w->clientHeight));
}

LRESULT DefaultWindowProc(Window* w, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_NCCREATE:
            return TRUE;
        case kWmNcCalcSize: {
            RECT* r = wp ? &reinterpret_cast<NcCalcSizeParams*>(lp)->rgrc[0] : reinterpret_cast<RECT*>(lp);
            int l, t, rr, b;
            NcInsetsFor(w->style, &l, &t, &rr, &b);
            r->left += l;
            r->top += t;
            r->right -= rr;
            r->bottom -= b;
            if (r->right < r->left) r->right = r->left;
            if (r->bottom < r->top) r->bottom = r->top;
            return 0;
        }
        case kWmNcHitTest: {
            const POINT p = {static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp))};
            const RECT client = ClientRectOnScreen(w);
            if (p.x >= client.left && p.x < client.right && p.y >= client.top && p.y < client.bottom) return kHtClient;
            const RECT outer = WindowRectOnScreen(w);
            if (p.x >= outer.left && p.x < outer.right && p.y >= outer.top && p.y < outer.bottom)
                return (w->style & 0xc00000) == 0xc00000 && p.y < client.top ? kHtCaption : kHtBorder;
            return kHtNowhere;
        }
        case kWmSetCursor: {
            if (w->parent) {
                if (Send(w->parent, kWmSetCursor, wp, lp)) return TRUE;
            }
            if (LOWORD(lp) == kHtClient && w->cls->cursor) {
                SetCursor(w->cls->cursor);
                return TRUE;
            }
            return 0;
        }
        case WM_MOUSEACTIVATE: {
            if (w->parent) {
                if (const LRESULT r = Send(w->parent, WM_MOUSEACTIVATE, wp, lp)) return r;
            }
            return MA_ACTIVATE;
        }
        case WM_ERASEBKGND: {
            HBRUSH brush = w->cls->background;
            if (!brush || reinterpret_cast<uintptr_t>(brush) <= 32) return 0;
            RECT r = MakeRect(0, 0, w->clientWidth, w->clientHeight);
            FillRect(reinterpret_cast<HDC>(wp), &r, brush);
            return 1;
        }
        case WM_PAINT: {
            ValidateWindow(w);
            return 0;
        }
        case kWmSetText:
            Free(w->text);
            w->text = DuplicateString(reinterpret_cast<const char*>(lp));
            return TRUE;
        case kWmGetText: {
            if (!wp) return 0;
            char* out = reinterpret_cast<char*>(lp);
            strncpy(out, w->text ? w->text : "", wp - 1);
            out[wp - 1] = 0;
            return static_cast<LRESULT>(strlen(out));
        }
        case kWmGetTextLength:
            return w->text ? static_cast<LRESULT>(strlen(w->text)) : 0;
        case WM_CLOSE:
            DestroyWindow(ToHandle(w));
            return 0;
        case kWmWindowPosChanged:
            SendSizeAndMove(w, reinterpret_cast<const WindowPos*>(lp));
            return 0;
        case WM_ACTIVATE:
            if (wp != 0 && !w->iconic) SetFocus(ToHandle(w));
            return 0;
        case kWmNcActivate:
            return TRUE;
        case kWmNotifyFormat:
            return 1;
        case WM_SYSCOMMAND:
            switch (wp & 0xfff0) {
                case 0xf060: Send(w, WM_CLOSE, 0, 0); break;  // SC_CLOSE
                case SC_MINIMIZE: ShowWindow(ToHandle(w), SW_MINIMIZE); break;
                case SC_RESTORE: ShowWindow(ToHandle(w), 9); break;
                default: break;
            }
            return 0;
        default:
            return 0;
    }
}

void RecordKeyChar(WPARAM vk, LPARAM lParam, unsigned ch) {
    KeyChar& k = g_keyChars[g_keyCharNext];
    g_keyCharNext = (g_keyCharNext + 1) % 8;
    k.vk = vk;
    k.lParam = lParam;
    k.ch = ch;
    k.valid = true;
}

}  // namespace minwin
