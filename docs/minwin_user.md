# minwin_impl/user: the window manager on SDL3

USER32 of `src/port/minwin` implemented natively (`src/port/minwin_impl/user/`) plus the winmm multimedia timers. The
game's windows are SDL3's; the native build imports no `USER32`, `GDI32` or `COMCTL32`.

Environment: `REB_MINWIN_SCALE=n` (window size multiple), `REB_MINWIN_TRACE=outlines` (draw every window's rectangle
into the presented frame), `=stacks` (nested message loops note the code addresses on the stack in the message trace),
`=mouse` (window and game coordinates). `REB_AUDIO_CAPTURE=<file>` writes the mixed audio (s16le stereo 44100)
instead of using a device.

## What is implemented

| File | Contents |
|---|---|
| `window.cpp` | classes (`RegisterClassA`, `GetClassInfoA`, the built-in `tooltips_class32`), window tree with z-order, `CreateWindowExA` (message order below), `DestroyWindow`, `ShowWindow`, `SetWindowPos`/`MoveWindow`, `EnableWindow`, window words (`GWL_*`, extra bytes), `SetParent`/`GetWindow`, coordinate mapping, `WindowFromPoint`/`ChildWindowFromPointEx`, visible-region computation (`WS_CLIPCHILDREN`/`WS_CLIPSIBLINGS`: rect lists) |
| `focus.cpp` | activation (`WM_ACTIVATEAPP`, `WM_NCACTIVATE`, `WM_ACTIVATE`), focus (`WM_KILLFOCUS`/`WM_SETFOCUS`, IME context messages), capture (`WM_CAPTURECHANGED`) |
| `message.cpp` | `SendMessage` (re-entrant, same thread; other threads are marshalled), `PostMessage`, `GetMessage`/`PeekMessage` with Windows' order (posted, input, `WM_QUIT`, `WM_PAINT`, `WM_TIMER`), `DispatchMessage`, `TranslateMessage` (`WM_CHAR`), `TranslateAccelerator` (`RT_ACCELERATOR`), hooks (`WH_MOUSE`, `WH_KEYBOARD`, `WH_GETMESSAGE` with `CallNextHookEx`), `SetTimer`, `DefWindowProc`, the message trace |
| `paint.cpp` | update rectangles (union), `WM_PAINT`/`WM_NCPAINT`/`WM_ERASEBKGND` scheduling, `BeginPaint`/`EndPaint`, `GetDC`/`GetWindowDC`/`ReleaseDC` (DCs come from `gdi_api.h` with origin and visible-region clip), exposure of uncovered areas |
| `input.cpp`, `platform.cpp` | SDL events to `WM_MOUSEMOVE`/`BUTTON*`/`DBLCLK`/`KEYDOWN`/`KEYUP`/`CHAR` with Windows' `wParam`/`lParam`, hit-testing (`WM_NCHITTEST`), `WM_PARENTNOTIFY`/`WM_MOUSEACTIVATE`/`WM_SETCURSOR`, `GetKeyState`/`GetAsyncKeyState`, cursors, the SDL window and present loop (`present.cpp`), `MessageBoxA` |
| `misc.cpp`, `cursor.cpp` | metrics, caret (XOR overlay), `LoadStringA`/`LoadAcceleratorsA`/`LoadBitmapA` (through kernel32's resource API), `RT_GROUP_CURSOR` to SDL colour cursors, `wsprintfA`, rectangle helpers |
| `mmtimer.cpp` | `timeSetEvent`/`timeKillEvent` |
| `tooltip.cpp` | the `tooltips_class32` control: tools (windows or rectangles), initial/re-show/auto-pop timers, the popup under the pointer (`TTM_ACTIVATE/SETDELAYTIME/ADDTOOLA/DELTOOLA/UPDATETIPTEXTA/RELAYEVENT`); no `TTN_*` notifications |

**Creation/destruction order** (taken from the real user32 trace): `WM_NCCREATE`, `WM_NCCALCSIZE`, `WM_CREATE`, `WM_SIZE`, `WM_MOVE`, `WM_PARENTNOTIFY` (to the parent),
`ShowWindow` when `WS_VISIBLE`; `ShowWindow` = `WM_SHOWWINDOW`, `SetWindowPos` (`WM_WINDOWPOSCHANGING`, `WM_NCCALCSIZE` on size/frame change, `WM_CHILDACTIVATE` for children,
exposed windows get `WM_NCPAINT` + `WM_ERASEBKGND` immediately, `WM_WINDOWPOSCHANGED`, which `DefWindowProc` turns into `WM_SIZE`/`WM_MOVE`), top-levels are activated before `WM_WINDOWPOSCHANGED`.
Destruction: `WM_PARENTNOTIFY`, hide, `WM_DESTROY` pre-order over the tree, then `WM_NCDESTROY` children first.
A button press sends `WM_PARENTNOTIFY` up the parent chain, then `WM_MOUSEACTIVATE` (forwarded to the parents by `DefWindowProc`), then `WM_SETCURSOR`, then the message.


## Message trace (`REB_MSG_TRACE`)

`REB_MSG_TRACE=<file>` makes `src/port/msg_trace.cpp` log one line per window-procedure entry (depth, class, window
serial, message, `wParam`, `lParam`; handles become serials in order of first appearance, pointers `ptr`;
`REB_MSG_TRACE_HANDLES=1` appends raw handles). `tools/msg_trace_compare.py a.log b.log` normalises device contexts,
drops the message kinds whose count is timing or host dependent (listed with their counts on both sides) and diffs
the rest.

## Deviations from Windows (deliberate or known)

* **Single thread.** The thread that initialises the runtime owns every window and queue. A `SendMessage` from another thread (the movie thread) is queued
  and executed by the owner from its pump, the sender waits (2 s at most). `GetDC`/`ReleaseDC`/`GetAsyncKeyState` from other threads run unsynchronised.
* **Multimedia timers** (`timeSetEvent`) fire from the pump (`PeekMessage`/`GetMessage`), at most once per call and timer, not on a timer thread.
* **No non-client area is drawn**; `WS_BORDER`/`WS_DLGFRAME`/caption only affect the client rectangle (insets 1/3/19 px).
* **Update regions are one rectangle** per window (union); a hidden or fully covered window's update region is dropped. Uncovered areas are invalidated per
  window, so `WM_PAINT` counts differ from Windows'.
* **Tooltips** (`tooltip.cpp`): the messages the game uses, drawn like the real control (white-ish `COLOR_INFOBK` box, text pixel-identical, border grey on the top/right edges and dithered on the left/bottom as Wine's 8-bit display renders `WS_BORDER`); no `TTN_*` notifications and no balloon/track modes. A button message hides the tip and restarts the show timer (the real control shows the tip again 500 ms after a click that leaves the pointer on the tool).
* `WM_GETICON`, `WM_STYLECHANGING/CHANGED` on activation and IME messages come from Wine's shell/IME integration and are not emulated (`WM_DISPLAYCHANGE`, the extra `WM_WINDOWPOSCHANGING` and the `WM_PALETTECHANGED` broadcast after a mode switch are, by `ddraw_api.cpp`).
* **Pointer motion from window changes**: a window appearing, disappearing or moving under a motionless pointer posts a `WM_MOUSEMOVE` only for top-level windows other than tooltips (Windows does it for every window; under Wine only the X windows' crossing events do, and child windows live inside one X window). Anything else makes the hover label of the galaxy map follow a stale pointer position.
* `GetCursorPos` is the last position SDL reported; there is no `SetCursorPos`.
* The 64-bit port keeps `HWND` = pointer to the window object, window extra bytes are rounded up to pointer size.
