// Raw input as the platform layer reports it, and the pieces of the queue/input code that talk to each other.
#pragma once
#include "port/minwin_impl/user/user_internal.h"

namespace minwin {

struct InputEvent {
    enum Type { MouseMove, MouseButton, Key } type;
    int x, y;            // screen coordinates (mouse)
    int button;          // 1 left, 2 right, 3 middle
    bool down;
    int clicks;
    int vk;              // keys
    int scan;
    bool extended;
    bool repeat;
    unsigned ch;         // character TranslateMessage will generate (0: none)
    unsigned time;
    unsigned mk;         // MK_* flags when the event happened (mouse)
    bool finalized;
    bool keep;
    MSG msg;             // the message to deliver, valid after FinalizeInput
};

struct AcceleratorTable {
    int count;
    AccelEntry entries[1];
};

// Turns a raw event into the message(s) Windows would produce at retrieval time: hit-testing (WM_NCHITTEST), WM_PARENTNOTIFY,
// WM_MOUSEACTIVATE, window activation and WM_SETCURSOR happen here, in that order. False discards the event.
bool FinalizeInput(InputEvent* event);
void QueueInputEvent(const InputEvent& event);
void RecordKeyChar(WPARAM vk, LPARAM lParam, unsigned ch);
void SetQuit(int code);
unsigned PlatformTicksMs();
void WaitForPlatformEvent(unsigned maxMs);

// Fed by the platform layer.
void InputMouseMotion(int x, int y);
void InputMouseButton(int button, bool down, int clicks, int x, int y);
void InputKey(int sdlScancode, bool down, bool repeat, unsigned shiftMask);
void InputText(const char* utf8);
void InputFocusChanged(bool gained);
unsigned CurrentModifierMask();

}  // namespace minwin
