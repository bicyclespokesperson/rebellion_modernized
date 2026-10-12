# Native builds: Linux and macOS, x86-64 and arm64 (phase 3, P3/P4)

The top-level `CMakeLists.txt` builds the native executable by default; `tools/build_native.sh` drives Linux builds and
`tools/build_macos.sh` drives macOS builds. Both include `cmake/native.cmake`. Everything that differs between the operating systems is in
`cmake/native_platform.cmake`, `src/port/native/platform{.h,_linux.cpp,_macos.cpp}` and `packaging/macos/`. The Linux x86-64 details (what is
where, globals, sanitizer findings) are in [native_linux.md](native_linux.md); this file lists the deltas and the arm64/macOS status.

## The OS deltas (all of them)

| What | Linux | macOS | Where |
|---|---|---|---|
| config file directory (`config.ini`; `$REBELLION_CONFIG` overrides) | `$XDG_CONFIG_HOME/rebellion`, else `~/.config/rebellion` | `~/Library/Application Support/Rebellion` | `platform_*.cpp` `UserConfigDir` |
| saves, game data | `install_dir` of the config file (the player's own game directory, `SaveGame/` inside it) on both: nothing OS-specific | | |
| system font directories (Arial search) | `~/.fonts`, `~/.local/share/fonts`, `$XDG_DATA_HOME/fonts`, `/usr/share/fonts`, `/usr/local/share/fonts` | `/System/Library/Fonts/Supplemental` (has Arial), `/Library/Fonts`, `~/Library/Fonts` | `SystemFontDirs` |
| bundled fallback fonts (Liberation Sans) | `<executable dir>/fonts` | `Rebellion.app/Contents/Resources/fonts` | `BundledFontDir`, `reb_native_platform_package` |
| thread id / name | `gettid`, `pthread_setname_np(thread, name)` | `pthread_threadid_np`, `pthread_setname_np(name)` | `CurrentThreadId`, `SetCurrentThreadName` |
| link libraries | `pthread dl m`; SDL3 finds X11/Wayland/ALSA/Pulse | `pthread m`; SDL3 links Cocoa/Metal/CoreAudio frameworks itself | `REB_PLATFORM_LIBS` |
| deployment target | n/a | 11.0 for arm64 and x86-64 (set before `project()`) | top-level `CMakeLists.txt` |
| packaging | plain binary `build/native/rebellion` | `Rebellion.app` (Info.plist from `packaging/macos`, Liberation fonts, ad-hoc `codesign`) | `reb_native_platform_package` |
| cross builds | `--arch arm64`: clang-19 + Ubuntu cross libc/libstdc++, SDL3 with only its dummy drivers (`REB_SDL_HEADLESS`) | none (macOS is built on a Mac or a macOS runner) | `cmake/toolchain-linux-aarch64.cmake` |

Size: `platform_linux.cpp` 50 lines, `platform_macos.cpp` 39, `platform.h` 31, `native_platform.cmake` 54, `Info.plist.in` 18. No `#ifdef __APPLE__` /
`__linux__` remains in `src/port` or `src/game` (the shared `config.cpp` and `font_files.cpp` call `reb::platform::`; their Windows branches
are the i686 builds'). Not deltas, because they work the same: path translation (`mw::res::ResolvePath` tries the exact name first, so the
case-insensitive default of APFS works; case-sensitive volumes fall back to a directory scan), `clock_gettime(CLOCK_MONOTONIC)`, POSIX
threads and condition variables, `mmap`.

Known semantic difference: `TerminateThread` is `pthread_cancel` with asynchronous cancellation on Linux; macOS does not honour the
asynchronous type, so the cancel acts at the next cancellation point (`nanosleep`, condition waits: every place the game's two threads block).

## Playing on a desktop (window, audio, pacing, first run)

The hidden developer menu opens with **3** on the strategic screen. **Switch sides** exchanges human/AI control in a
single-player game and rebuilds the faction UI. Existing orders continue; open panels and controller planning state reset.
For its integration check, set `REB_SWITCH_SIDES_SMOKE=1` when running `tools/modern_smoke.py`. Each process exercises the
menu click, deferred switching, both factions, queued-order preservation and speed/pause preservation before advancing and
saving; the second process loads that save and repeats the check.

For an asset-dependent list regression check without desktop input, prefix a `tools/modern_smoke.py` invocation with
`REB_UI_SMOKE=1`. After loading the save, each process checks fleet-row collapse/expansion, layout, rendered pixels,
hit testing, keyboard navigation, selection and sorting. It uses the configured installation's STRATEGY.DLL resources
and logs `UI visibility checks passed` before running the usual simulation and save/reload checks.
It also rebuilds empty battle-participant lists for both sides and all four kinds, and checks the outcome artwork IDs
for total destruction, retreat and victory/defeat.
Status-table checks cover blank labels/values, minimum row spacing, multiline wrapping, rendered text and label resets
at nonzero coordinates.
Report checks exercise actual Notice7 and Notice15 payloads, including empty participant sections, and create the
assault report window in both faction skins to verify separately that its title and body render. They also check
missing assault targets and the story report's artwork before and after Luke completes Dagobah. Victory checks
temporarily change and restore loaded game state to verify HQ-only victory without character captures, Alliance
control of Coruscant, and the additional capture requirements in standard mode.

All of it is in the presentation path: `src/port/minwin_impl/user/present.{h,cpp}` (new), small hooks in `user/platform.cpp`, `user/misc.cpp`
(cursor) and `user/message.cpp` (idle pacing), `audio/sdl_audio.cpp`, and `src/port/native/first_run.{h,cpp}`. Game code and simulation are untouched.

| Topic | Behaviour |
|---|---|
| Config keys | `window_scale` (0/absent: largest integer scale that fits the display's usable area, leaving 48 px for the title bar), `fullscreen`, `renderer`, `vsync`, `audio_buffer` (`config/config.h`). Environment overrides for tests: `REB_MINWIN_SCALE`, `REB_MINWIN_FULLSCREEN`, `REB_MINWIN_PACING_US`, `SDL_RENDER_DRIVER`, `SDL_AUDIODRIVER` |
| Window | Decorated, resizable (minimum 640x480), high pixel density on (macOS Retina). The renderer presents the 640x480 frame with `SDL_LOGICAL_PRESENTATION_INTEGER_SCALE` and a nearest-neighbour texture: always an integer scale, black bars. SDL's default renderer (GPU) is tried first, then `software`; the Windows (Wine) builds keep `software` |
| Fullscreen | Borderless desktop fullscreen (`SDL_SetWindowFullscreen`), toggled by Alt+Enter (the key pair is swallowed, the game never sees the Return) |
| Mouse | `SDL_RenderCoordinatesFromWindow` (accounts for the letterbox offset, the scale and the pixel density), floored, clamped to 0..639 / 0..479. `REB_MINWIN_TRACE=mouse` prints window and game coordinates. Unit of the check: windowed 1x and fullscreen 2x, edges (0, 1, 2, 639, 479 and outside) |
| Cursors | The game's own cursors are enlarged by the current integer scale (nearest neighbour; hot spot scaled) and rebuilt when the scale changes (resize, fullscreen, moving to another display); with pixel density above 1 an alternate image at the pixel scale is attached |
| Mouse capture | `SDL_CaptureMouse` follows the game's `SetCapture`/`ReleaseCapture` (synchronised once per message pump); nothing is grabbed otherwise, also not in fullscreen |
| Close / Alt+F4 | The window's close button, `SDL_EVENT_QUIT` and Alt+F4 (when the window manager does not take it) post `WM_SYSCOMMAND/SC_CLOSE`: the game's quit confirmation, as in the original |
| Focus | Focus lost or minimised: keys and mouse buttons still held are released toward the game (no stuck keys), then `WM_ACTIVATEAPP(FALSE)`; regained or restored with focus: `WM_ACTIVATEAPP(TRUE)` |
| Idle pacing | The game's main loop is `PeekMessage(PM_REMOVE)` until empty, one game step, repeat. When the queue is empty at nesting depth 0, `PeekMessage` sleeps so that two consecutive empty returns are at least 250 us apart (`REB_MINWIN_PACING_US`; 0 disables; off with `REB_DETERMINISTIC=1`). Do not make it coarser: game progress per real second depends on the loop's latency (see Results) |
| Audio | SDL3 audio stream at 44.1 kHz s16 stereo, device buffer 1024 frames (23 ms, `audio_buffer`), rendered in SDL's audio callback by the mixer. No device: a message on stderr and the real-time pacing thread keeps play cursors and refill threads running |
| First run | `CheckInstallation()` (before anything else): config file present, `install_dir` set and a folder, `REBEXE.EXE` (its resources are read at run time), the 14 resource DLLs (`ALBRIEF ALSPRITE CDDERR COMMON EMBRIEF EMSPRITE ENCYBMAP ENCYTEXT GOKRES REBDLOG STRATEGY TEXTCOMM TEXTSTRA TEXTTACT`), `GData/`, `EData/`, `MDATA/MDATA.000` (in `data_dir`), `SaveGame/` (created if missing). Anything wrong: `SDL_ShowSimpleMessageBox` plus stderr naming the setting, the file and the config path, exit status 1. `SMACKW32.DLL`, `TACTICAL.DLL`, `VOICEFX*.DLL` are not needed natively |

Testing recipe (no window manager exists in the `rebellion-native` image; the checks of fullscreen, close, focus and minimise need one):

```
docker build -t rebellion-native-wm - <<< $'FROM rebellion-native\nUSER root\nRUN apt-get update && apt-get install -y --no-install-recommends openbox wmctrl x11-xserver-utils'
REB_NATIVE_IMAGE=rebellion-native-wm REB_WM=openbox REB_XVFB_GEOMETRY=1920x1080x24 REB_MINWIN_SCALE=1 REB_MINWIN_TRACE=mouse \
  tools/run_native.sh tests/scenarios/native_window.txt          # scaling, Alt+Enter, mouse mapping at the edges
REB_NATIVE_IMAGE=rebellion-native-wm REB_WM=openbox REB_XVFB_GEOMETRY=1920x1080x24 \
  tools/run_native.sh tests/scenarios/native_desktop.txt         # auto scale, close button, Alt+F4, focus, minimise
tools/run_native.sh tests/scenarios/native_cpu.txt; cat work/run_native/native_cpu/cpu.log   # CPU per thread (driver action `cpu <s>`)
```

The driver (`tools/container/native_driver.sh`) gained the actions `cpu`, `wmove`, `wclick`, `minimize`, `restore`, `close`, `geometry`, `focus-away`,
`focus-back` and the environment `REB_XVFB_GEOMETRY`, `REB_WM`. The container's ALSA default is a `null` PCM that consumes audio as fast as it is
written, so under it SDL's audio thread spins a core; use `SDL_AUDIODRIVER=dummy` (paced in real time) for CPU measurements.

### Measured (Oct 9 2026, Linux x86-64 in the container, Xvfb)

* **CPU** at the galaxy screen (`load_game`, game speed Fast, `tests/scenarios/native_cpu.txt`, SDL dummy audio): the main thread went from **98% to 8-9%**
  of a core; the whole process from 116% to 27%, of which about 16% are Mesa llvmpipe worker threads of the container's software GL that a desktop
  with a GPU does not have. With the container's ALSA default (a `null` PCM that swallows audio as fast as it is written) SDL's audio thread spins
  at 100%: that is the container, not the game.
* **Game speed is unchanged only for a fine pacing period.** Game progress per real second depends on the loop's latency (the game's own tick scheduler
  re-arms from "now"). Resources 72 s after setting speed Fast, same save: no pacing 147 / 1549; 100 us 147 / 1549; 250 us 147 / 1549 (default);
  500 us 146 / 1535; 1000 us 144 / 1518; 2000 us 141 / 1476 (about 5% slower). Main-thread CPU: 12% (100 us), 9% (250 us), 8% (1000 us).
* **Sim goldens** (`tools/native_sim.sh`, virtual clocks, pacing off): 17/17 identical to the i686 goldens.
* **Window**: windowed 1x and fullscreen (2x on 1920x1080) mouse mapping exact at 0, 1, 2, 639/479 and outside the frame (clamped); automatic scale on
  1920x1080 is 2 (1280x960 decorated window); close button, Alt+F4 and focus loss/regain/minimise/restore ran without a crash and the quit
  confirmation appears; a key held while the focus leaves is released toward the game (one `WM_KEYUP`).
* **Sanitizers** (`REB_SANITIZE=1`, SDL dummy audio): 44 scenarios of `tests/scenarios` (all but `mp_*`, `long_*`, `*_long`, `soak_*`) and the four `native_*`
  scenarios: 0 reports, except the two below.

### Rough edges

* Real audio devices, HiDPI (macOS Retina, Wayland fractional scaling) and a real GPU renderer were not exercised: this machine's container has no sound
  card, no display server of the host and no GPU. The code paths are SDL's documented ones (`SDL_RenderCoordinatesFromWindow`, pixel density, alternate
  cursor images).
* The game does not know the window can be resized or go fullscreen; its own "windowed" command-line flag is unrelated.
* The idle loop wakes up to 4,000 times a second (about 9% of a core at the galaxy screen); the floor is the game's own busy `PeekMessage` loop.
* No vsync by default (tearing is possible with a GPU renderer); `vsync=1` enables it. When the window is minimised the loop keeps pacing at 250 us.

## Build

```
tools/fetch_deps.sh freetype liberation sdl3                  # third_party/
tools/build_native.sh                                         # Linux x86-64, in the container (as before)
REB_NATIVE_IMAGE=... tools/build_native.sh --arch arm64       # Linux aarch64 cross build, build/native_arm64/rebellion
tools/native_sim.sh -a arm64                                  # the sim goldens on it under qemu-user
```

macOS (on a Mac with Xcode command-line tools, CMake and `uv`):

```sh
REB_GAME_EXE="/path/to/Star Wars - Rebellion/REBEXE.EXE" tools/build_macos.sh
```

The script downloads pinned dependencies, creates `build/tools-venv` for Python asset extraction and Ninja, extracts the
startup asset pack and original application icon from your EXE, configures the native CMake build, and links and signs
`build/native/Rebellion.app`. The icon is converted locally to `Rebellion.icns` and bundled for Finder and the Dock;
original game artwork stays out of git. `REB_BUNDLE_ICON` can point CMake at a different `.icns` file.
`REB_GAME_EXE` defaults to `orig_game_files/Star Wars - Rebellion/REBEXE.EXE` in this checkout.

Set `install_dir` in a config file as described in the README. `tools/run_macos.command` is a double-clickable launcher;
it defaults to `work/macos/config.ini`, or uses `$REBELLION_CONFIG` when set. The installation must be writable for saves.

**Historical host verification (Oct 10, 2026):** on Apple silicon, Apple clang compiled the game and port objects, the ARM64 app linked
and passed signature verification, and the intro and main menu rendered. A headless saved game loaded, advanced ten ticks, and saved.
This records the earlier build state; current validation should use the native CMake build and a host smoke run.

Empire gameplay sector selection, the message droid and centered exit dialogs were manually verified after two repairs:
the galaxy's show handler now clears mouse mode bit 8, matching the original executable, and DirectDraw restores a minimized
window when reacquiring exclusive mode. Task switching also preserves the active top-level window while changing keyboard
focus during deactivation, preventing a nested reactivation before minimization finishes. `focus_test` reproduces that
sequence, fails against the previous implementation, and passes with the repair.

Encyclopedia artwork uses `EData` under `data_dir` (or `install_dir` when `data_dir` is absent); an explicit registry
`EncyLocation` still overrides this default. Native game windows use live dragging, while Windows builds keep the original
outline mode. The outline reconstruction also uses the original white pen, so its preview is visible.
Encyclopedia pictures and live system-window dragging were manually verified in the rebuilt Mac app. The input trace
also confirms repeated window-position changes while the mouse button is held, before release.

The graphics handle registry stays alive until process exit because global game font tables delete their fonts during
static cleanup. Destroying the registry earlier caused a shutdown crash in `FontTable::~FontTable` / `DeleteObject`.
`gdi_shutdown_test` reproduces that destruction order, crashes before the repair, and exits successfully afterward;
the regular GDI tests also pass. The rebuilt Mac app was also launched on the desktop and exited through its normal
quit confirmation with status 0 and no crash output.

`tools/build_native.sh` remains the Linux container build. Windows and Linux host validation are deferred; current requested validation
is macOS host-only.

## arm64

* **Float to int**: `cvttsd2si` gives `0x80000000` for overflow and NaN; arm64 saturates. `reb_ftol` (`core/reb_x87.h`) has the x86 result on every
  target. The native x86-64 build run with `-fsanitize=float-cast-overflow` (`REB_NATIVE_SANITIZE=address,undefined,float-cast-overflow`) shows the
  places that rely on it; results in the phase 3 plan.
* No x86 intrinsics or inline assembly in the native build: the remaining `__asm__` is limited to empty compiler barriers
  (`game_string.cpp`, `galaxy_display.cpp`); the x87 code in `reb_x87.h` and `crt/floor.cpp` is `#if` i386.
* `char` is signed (`-fsigned-char`), `-ffp-contract=off` so arm64 does not fuse multiply-adds, `-fwrapv`.
* **Memory ordering**: arm64 is weakly ordered, x86 is not. The game's two helper threads (sound refill, movie) share a few plain flags with the
  main thread, written for x86. qemu-user runs them with the host's ordering, so only real arm64 hardware can show a race there.

## CI

`.github/workflows/linux.yml` (ubuntu-24.04: x86-64 and aarch64 cross, compile every object of the native configuration; unit tests of
`src/port/minwin_impl` on x86-64) and `.github/workflows/macos.yml` (macos-14 arm64 and macos-15-intel x86-64: compile every object; the same unit
tests). Both fetch dependencies with `tools/fetch_deps.sh`. Neither uses game data: the tests that read the installation skip when
`MW_INSTALL_DIR` is unset, and linking (which reads `REBEXE.EXE`) is not part of CI.

osxcross / the Apple SDK was not set up: the Xcode and Apple SDKs agreement licenses the SDK for use on Apple-branded hardware, so extracting
it to cross-compile on Linux is not permitted. macOS builds therefore happen on macOS runners or a Mac.

## Results (Oct 9 2026)

* Historical aarch64 Linux cross build: all 504 objects compiled and linked. `tools/native_sim.sh -a arm64` under qemu-user: all three
  sim goldens identical to the i686 goldens (17/17 files), same as x86-64 native.
* Float to int: `tools/find_float_casts.py` lists 29 conversions (25 in game code, now `reb_ftol` / new `reb_ftou`; 4 in the port: mouse
  coordinates and two bounded volume curves in dsound/smack, left alone). The `float-cast-overflow` build reported 0 overflows on the three sims
  and 15 UI scenarios (smoke, load_game, ui_tour, new_game_*, space_battle_autoresolve, ...), so the routing is protective, not a bug fix.
  `reb_ftol` returns INT32_MIN out of range (cvttsd2si); the original's `_ftol` truncates to 64 bits, so values in 2^31..2^63 wrap there;
  `reb_ftou` follows the 64-bit form.
* Unchanged: hybrid 10,706 hooks, difftest unit and integration 12,988 captures no FAIL, standalone_check OK, arg-order check clean, x86-64 sims 17/17.

## Needs a real Mac / arm64 hardware

The macOS ARM64 build and menu launch have been verified (see Build above). Remaining checks include full gameplay,
real audio, Retina mouse/cursor scaling, Intel macOS, and `TerminateThread` cancellation behaviour. arm64 memory ordering (see above) needs hardware.
