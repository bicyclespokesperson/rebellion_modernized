# minwin_impl/user: the window manager on SDL3 (phase 3, P1)

USER32 of `src/port/minwin` implemented natively (`src/port/minwin_impl/user/`, ~3,300 lines) plus the winmm multimedia timers. With
`REB_MINWIN_IMPL=ON` the standalone `rebellion.exe` imports no `USER32.dll`, `GDI32.dll` or `COMCTL32.dll`; its windows are SDL3's.
The first target is i686 Windows under Wine (the same exe, run by the same scenarios).

## Build and run

```
tools/fetch_deps.sh                       # SDL3 3.4.18 VC zip -> third_party/SDL3 (gitignored; sha256 pinned in the script)
REB_MINWIN=ON REB_STANDALONE_DIR=build/impl REB_CMAKE_ARGS=-DREB_MINWIN_IMPL=ON tools/build_standalone.sh
REB_STANDALONE_DIR=build/impl tools/run_game.sh --standalone tests/scenarios/smoke.txt   # SDL3.dll is mounted next to the exe
```

`REB_MINWIN_IMPL=OFF` (default) keeps the real import libraries; nothing else changes (`standalone_check`, `sim_goldens` pass; see the
P1 results in `docs/phase3_plan.md`). The game objects are the same as in the `REB_MINWIN` build: they still call through
`__imp__Name@N` slots. `tools/gen_minwin_imports.py` generates the slots for the implemented functions from the minwin headers
(`gen/minwin_imports.cpp`), with a `static_assert` of every stack size; an implemented-nowhere function is an unresolved `_Name@N` at link time.
With `REB_MINWIN_IMPL=ON` the executable links every component of `src/port/minwin_impl` (config, res, gdi, text + FreeType, dsound, smack, ddraw,
`stubs/unavailable.cpp`, `audio/sdl_audio.cpp`, `glue/wiring.cpp`) and imports only KERNEL32 and SDL3 (`tools/exe_imports.py build/impl/rebellion.exe`).
The gdi stub is gone. Set the game up with a config file (`REBELLION_CONFIG`, see config/config.h); `tools/run_game.sh --standalone` writes one.
`REB_AUDIO_CAPTURE=<file>` writes the mixed audio (s16le stereo 44100) instead of using a device; `REB_XVFB_DEPTH=8` and `REB_WINE_FONTS=liberation`
make Wine's own gdi32 draw like ours (8-bit display, Liberation Sans for Arial), which is what pixel comparisons against the real-Win32 standalone need.

Environment: `REB_MINWIN_SCALE=n` (window size multiple), `REB_MINWIN_TRACE=outlines` (draw every window's rectangle into the presented
frame), `=stacks` (nested message loops note the game code addresses on the stack in the message trace; `tools/map_lookup.py` resolves them).

## What is implemented

| File | Contents |
|---|---|
| `window.cpp` | classes (`RegisterClassA`, `GetClassInfoA`, the built-in `tooltips_class32`), window tree with z-order, `CreateWindowExA` (message order below), `DestroyWindow`, `ShowWindow`, `SetWindowPos`/`MoveWindow`, `EnableWindow`, window words (`GWL_*`, extra bytes), `SetParent`/`GetWindow`, coordinate mapping, `WindowFromPoint`/`ChildWindowFromPointEx`, visible-region computation (`WS_CLIPCHILDREN`/`WS_CLIPSIBLINGS`: rect lists) |
| `focus.cpp` | activation (`WM_ACTIVATEAPP`, `WM_NCACTIVATE`, `WM_ACTIVATE`), focus (`WM_KILLFOCUS`/`WM_SETFOCUS`, IME context messages), capture (`WM_CAPTURECHANGED`) |
| `message.cpp` | `SendMessage` (re-entrant, same thread; other threads are marshalled), `PostMessage`, `GetMessage`/`PeekMessage` with Windows' order (posted, input, `WM_QUIT`, `WM_PAINT`, `WM_TIMER`), `DispatchMessage`, `TranslateMessage` (`WM_CHAR`), `TranslateAccelerator` (`RT_ACCELERATOR`), hooks (`WH_MOUSE`, `WH_KEYBOARD`, `WH_GETMESSAGE` with `CallNextHookEx`), `SetTimer`, `DefWindowProc`, the message trace |
| `paint.cpp` | update rectangles (union), `WM_PAINT`/`WM_NCPAINT`/`WM_ERASEBKGND` scheduling, `BeginPaint`/`EndPaint`, `GetDC`/`GetWindowDC`/`ReleaseDC` (DCs come from `gdi_api.h` with origin and visible-region clip), exposure of uncovered areas |
| `input.cpp`, `platform.cpp` | SDL events to `WM_MOUSEMOVE`/`BUTTON*`/`DBLCLK`/`KEYDOWN`/`KEYUP`/`CHAR` with Windows' `wParam`/`lParam`, hit-testing (`WM_NCHITTEST`), `WM_PARENTNOTIFY`/`WM_MOUSEACTIVATE`/`WM_SETCURSOR`, `GetKeyState`/`GetAsyncKeyState`, cursors, the 640x480 SDL window, present loop (palette to XRGB, letterboxed), `MessageBoxA` |
| `misc.cpp`, `cursor.cpp` | metrics, caret (XOR overlay), `LoadStringA`/`LoadAcceleratorsA`/`LoadBitmapA` (through kernel32's resource API), `RT_GROUP_CURSOR` to SDL colour cursors, `wsprintfA`, rectangle helpers |
| `mmtimer.cpp` | `timeSetEvent`/`timeKillEvent` |

**Creation/destruction order** (taken from the real user32 trace): `WM_NCCREATE`, `WM_NCCALCSIZE`, `WM_CREATE`, `WM_SIZE`, `WM_MOVE`, `WM_PARENTNOTIFY` (to the parent),
`ShowWindow` when `WS_VISIBLE`; `ShowWindow` = `WM_SHOWWINDOW`, `SetWindowPos` (`WM_WINDOWPOSCHANGING`, `WM_NCCALCSIZE` on size/frame change, `WM_CHILDACTIVATE` for children,
exposed windows get `WM_NCPAINT` + `WM_ERASEBKGND` immediately, `WM_WINDOWPOSCHANGED`, which `DefWindowProc` turns into `WM_SIZE`/`WM_MOVE`), top-levels are activated before `WM_WINDOWPOSCHANGED`.
Destruction: `WM_PARENTNOTIFY`, hide, `WM_DESTROY` pre-order over the tree, then `WM_NCDESTROY` children first.
A button press sends `WM_PARENTNOTIFY` up the parent chain, then `WM_MOUSEACTIVATE` (forwarded to the parents by `DefWindowProc`), then `WM_SETCURSOR`, then the message.

## Message-sequence comparison (`REB_MSG_TRACE`)

`src/port/msg_trace.cpp` logs one line per window-procedure entry (depth, class, window serial, message, `wParam`, `lParam`; handles become serials in order
of first appearance, pointers `ptr`). The real-user32 build (`REB_MSG_TRACE_SHIM=ON`, needs `REB_MINWIN`) wraps every registered window procedure
(`src/standalone/msg_trace_shim.cpp`); the minwin_impl build logs from its own dispatcher. `tools/msg_trace_compare.py ref.log impl.log` normalises device contexts,
drops the message kinds whose count is timing or host dependent (listed with their counts on both sides) and diffs the rest. Results and causes of the divergences
are in `docs/phase3_plan.md` (P1 window manager).

## Deviations from Windows (deliberate or known)

* **Single thread.** The thread that initialises the runtime owns every window and queue. A `SendMessage` from another thread (the movie thread) is queued
  and executed by the owner from its pump, the sender waits (2 s at most). `GetDC`/`ReleaseDC`/`GetAsyncKeyState` from other threads run unsynchronised.
* **Multimedia timers** (`timeSetEvent`) fire from the pump (`PeekMessage`/`GetMessage`), at most once per call and timer, not on a timer thread.
* **No non-client area is drawn**; `WS_BORDER`/`WS_DLGFRAME`/caption only affect the client rectangle (insets 1/3/19 px).
* **Update regions are one rectangle** per window (union); a hidden or fully covered window's update region is dropped. Uncovered areas are invalidated per
  window, so `WM_PAINT` counts differ from Windows'.
* **Tooltips**: `tooltips_class32` accepts the `TTM_*` messages the game uses, answers `WM_NOTIFYFORMAT` on `TTM_ADDTOOLA` and draws nothing; no `TTN_*` notifications.
* `WM_GETICON`, `WM_STYLECHANGING/CHANGED` on activation, IME messages and `WM_DISPLAYCHANGE` come from Wine's shell/IME/ddraw integration and are not emulated.
* `GetCursorPos` is the last position SDL reported; there is no `SetCursorPos`.
* The 64-bit port keeps `HWND` = pointer to the window object, window extra bytes are rounded up to pointer size.
