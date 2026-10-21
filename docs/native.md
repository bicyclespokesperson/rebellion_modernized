# Native builds: macOS, Linux and Windows

Building and configuring are in the [README](../README.md): `./create_build.sh` configures `build/native` and
`./launch_game.sh` compiles and launches; the config keys are listed there. The top-level `CMakeLists.txt` includes
`cmake/native.cmake`. Everything that differs between operating systems is in `cmake/native_platform.cmake`,
`src/port/native/platform{.h,_linux.cpp,_macos.cpp,_windows.cpp}` and `packaging/macos/`. The executable is
`build/native/rebellion` on Linux, `build/native/Rebellion.app` on macOS and `build/native/rebellion.exe` on Windows.

Ninja and Makefile builds copy their compilation database to an ignored `compile_commands.json` at the repository
root for editor tooling.

## The OS deltas

| What | Linux | macOS | Where |
|---|---|---|---|
| config file directory (`config.ini`; `$REBELLION_CONFIG` overrides) | `$XDG_CONFIG_HOME/rebellion`, else `~/.config/rebellion` | `~/Library/Application Support/Rebellion` | `platform_*.cpp` `UserConfigDir` |
| saves, game data | `install_dir` of the config file (the player's own game directory, `SaveGame/` inside it) on both: nothing OS-specific | | |
| system font directories (Arial search) | `~/.fonts`, `~/.local/share/fonts`, `$XDG_DATA_HOME/fonts`, `/usr/share/fonts`, `/usr/local/share/fonts` | `/System/Library/Fonts/Supplemental` (has Arial), `/Library/Fonts`, `~/Library/Fonts` | `SystemFontDirs` |
| bundled fallback fonts (Liberation Sans) | `<executable dir>/fonts` | `Rebellion.app/Contents/Resources/fonts` | `BundledFontDir`, `reb_native_platform_package` |
| thread id / name / priority | `gettid`, `pthread_setname_np(thread, name)`; priority is not raised | `pthread_threadid_np`, `pthread_setname_np(name)`; priority is not raised | `CurrentThreadId`, `SetCurrentThreadName`, `RaiseCurrentThreadPriority` |
| link libraries | `pthread dl m`; SDL3 finds X11/Wayland/ALSA/Pulse | `pthread m`; SDL3 links Cocoa/Metal/CoreAudio frameworks itself | `REB_PLATFORM_LIBS` |
| deployment target | n/a | 11.0 for arm64 and x86-64 (set before `project()`) | top-level `CMakeLists.txt` |
| packaging | plain binary `build/native/rebellion` | `Rebellion.app` (Info.plist from `packaging/macos`, Liberation fonts, the game's icon converted to `Rebellion.icns` or `REB_BUNDLE_ICON`, ad-hoc `codesign`) | `reb_native_platform_package` |
| cross builds | `--arch arm64`: clang-19 + Ubuntu cross libc/libstdc++, SDL3 with only its dummy drivers (`REB_SDL_HEADLESS`) | none (macOS is built on a Mac or a macOS runner) | `cmake/toolchain-linux-aarch64.cmake` |

Windows uses `%APPDATA%\Rebellion\config.ini`, the Windows SDK and the MSVC-compatible CRT, and builds with `clang-cl`.

The SDL-backed compatibility functions use C++ linkage in `reb::compat` on every platform. Game and port callers
qualify these calls explicitly; they must not define plain Windows API symbols or use global API-renaming macros.
Resource module handles belong to the PE reader, not the Windows loader. Real OS calls stay separate in platform
implementation files that use SDK declarations. The Windows link does not permit duplicate definitions.

Building `rebellion` or `reb_battle_runner` checks the compatibility objects with `llvm-nm` (or `nm` on POSIX);
keep LLVM's symbol tools installed alongside Clang on Windows. The same check is available as the
`reb_compat_symbols` build target and the `native_compat_symbols` CTest. It rejects bare compatibility API and GUID
symbols, including decorated 32-bit Windows names, while allowing genuine host imports.

`create_build.sh` refuses to run there without the MSVC environment (`INCLUDE`/`LIB`), which is why Git Bash must be
started from the developer prompt; `launch_game.sh` converts the paths it exports to Windows form with `cygpath`.

Not deltas, because they work the same: path translation (`mw::res::ResolvePath` tries the exact name first, so the
case-insensitive default of APFS works; case-sensitive volumes fall back to a directory scan), `clock_gettime(CLOCK_MONOTONIC)`, `mmap`.

## What is where

| Piece | Where | Notes |
|---|---|---|
| kernel32 on POSIX | `src/port/native/kernel32_posix.cpp` | only what has no standard-library form: `CloseHandle` for the descriptor handles of `win_compat.cpp`, `GetCurrentThreadId`, QPC (`CLOCK_MONOTONIC`) and a constant `GetLastError`. Threads, events and locks are `std::thread` / `std::mutex` / `std::condition_variable` in the callers; `port/native/thread_sync.h` has the event group and the timed join the audio threads use. `GetTickCount` is `native_clock::TickMs()`. `timeGetTime/timeSetEvent` are in `main.cpp` (the window manager's pump-driven timers). |
| MSVC CRT names | `src/port/native/crt_posix.cpp`, `crt_windows.cpp`, `crt/` | `_sopen/_read/_write/_lseek/_close/_setmode/_chdir/_mkdir/_chdrive`, `_stricmp/_strnicmp`, `<io.h>`/`<direct.h>` shims. All paths go through `mw::res::ResolvePath` (backslashes, drive letters, any case). There is no text mode: every file the game opens is opened binary. Game installation paths use the explicit `OpenGameFile` adapter. |
| iostream library | `src/game/iostream` | streams own typed `ios`, input/output views and virtual `streambuf` implementations |
| program globals | typed definitions in `src/game` | |
| initialization | `src/port/native/game_context.cpp` | named dependencies and scoped platform teardown |
| entry | `src/port/native/main.cpp` | asset owner, `minwin_wiring::Install()`, scoped context, `RunApplication` and exception boundary |
| startup text | `tools/extract_startup_assets.py` | a [versioned asset pack](design/startup_assets.md) extracted from the installation by `create_build.sh` |
| config | `src/port/minwin_impl/config` | on POSIX the derived `*Location` registry values are `C:` + install dir, so the game's "drop the drive letter, chdir" code works |

## Desktop behaviour

The presentation path is `src/port/minwin_impl/user/present.{h,cpp}`, small hooks in `user/platform.cpp`, `user/misc.cpp`
(cursor) and `user/message.cpp` (idle pacing), `audio/sdl_audio.cpp`, and `src/port/native/first_run.{h,cpp}`.

| Topic | Behaviour |
|---|---|
| Test overrides | `REB_MINWIN_SCALE`, `REB_MINWIN_FULLSCREEN`, `REB_MINWIN_PACING_US`, `SDL_RENDER_DRIVER`, `SDL_AUDIODRIVER` |
| Window | Decorated, resizable (minimum 640x480), high pixel density on (macOS Retina). The renderer fits a device-resolution frame to the output. Gameplay expands to the output aspect ratio, from 640×480 to 1280×480 logical pixels; other scenes retain 4:3. The default sharp mode uses fractional fitting; nearest and integer modes are configurable. Live text and line geometry render at device scale; the gameplay canvas keeps a logical height of 480. SDL's default renderer (GPU) is tried first, then `software` |
| Fullscreen | Borderless desktop fullscreen (`SDL_SetWindowFullscreen`), toggled by Alt+Enter (the key pair is swallowed, the game never sees the Return) |
| Mouse | One shared presentation viewport (accounts for the letterbox offset, the selected filter and pixel density), floored, clamped to 0..639 / 0..479. `REB_MINWIN_TRACE=mouse` prints window and game coordinates |
| Cursors | The game's own cursors are enlarged by the current integer scale (nearest neighbour; hot spot scaled) and rebuilt when the scale changes (resize, fullscreen, moving to another display); with pixel density above 1 an alternate image at the pixel scale is attached |
| Mouse capture | `SDL_CaptureMouse` follows the game's `SetCapture`/`ReleaseCapture` (synchronised once per message pump); nothing is grabbed otherwise, also not in fullscreen |
| Close / Alt+F4 | The window's close button, `SDL_EVENT_QUIT` and Alt+F4 (when the window manager does not take it) post `WM_SYSCOMMAND/SC_CLOSE`: the game's quit confirmation, as in the original |
| Focus | Focus lost or minimised: keys and mouse buttons still held are released toward the game (no stuck keys), then `WM_ACTIVATEAPP(FALSE)`; regained or restored with focus: `WM_ACTIVATEAPP(TRUE)` |
| Window dragging | Native game windows use live dragging; Windows builds keep the original outline mode |
| Idle pacing | The game's main loop is `PeekMessage(PM_REMOVE)` until empty, one game step, repeat. When the queue is empty at nesting depth 0, `PeekMessage` sleeps so that two consecutive empty returns are at least 250 us apart (`REB_MINWIN_PACING_US`; 0 disables; off with `REB_DETERMINISTIC=1`). Do not make it coarser: game progress per real second depends on the loop's latency (the game's tick scheduler re-arms from "now"); 2000 us was measurably slower |
| Audio | SDL3 audio stream at 44.1 kHz s16 stereo, device buffer 1024 frames (23 ms, `audio_buffer`), rendered in SDL's audio callback by the mixer. No device: a message on stderr and the real-time pacing thread keeps play cursors and refill threads running |
| First run | `CheckInstallation()` (before anything else): config file present, `install_dir` set and a folder, `REBEXE.EXE` (its resources are read at run time), the 14 resource DLLs (`ALBRIEF ALSPRITE CDDERR COMMON EMBRIEF EMSPRITE ENCYBMAP ENCYTEXT GOKRES REBDLOG STRATEGY TEXTCOMM TEXTSTRA TEXTTACT`), `GData/`, `EData/`, `MDATA/MDATA.000` (in `data_dir`), `SaveGame/` (created if missing). Anything wrong: `SDL_ShowSimpleMessageBox` plus stderr naming the setting, the file and the config path, exit status 1. `SMACKW32.DLL`, `TACTICAL.DLL`, `VOICEFX*.DLL` are not needed natively |

Encyclopedia artwork uses `EData` under `data_dir` (or `install_dir`); an explicit registry `EncyLocation` still
overrides this. The graphics handle registry stays alive until process exit because global game font tables delete
their fonts during static cleanup (`gdi_shutdown_test` covers the order).

Known rough edges: the game does not know the window can be resized or go fullscreen (its own "windowed" command-line
flag is unrelated); the idle loop wakes up to 4,000 times a second, about 9% of a core at the galaxy screen; vsync is
off by default (`vsync=1`); when minimised the loop keeps pacing at 250 us.

## Developer menu

**D** in the shuttle cockpit or on the strategic screen opens a hidden developer menu. It chooses the AI controller
for the Alliance and Empire independently: **Classic** (the game default), **Codex Strategist** or **Claude Admiral**
([ai_matches.md](ai_matches.md)). Choices are saved as `alliance_ai` and `empire_ai` and apply to new games. In a running
single-player game, changing an AI side replaces its controller immediately; changing the human side sets its
preference for future AI control. The star marks the running personality for an AI side, or the saved preference for a
human side and in the cockpit. **Switch sides**, on the strategic screen, exchanges human/AI control and rebuilds the
faction UI; existing orders continue, open panels and controller planning state reset.

**Enhancements** (`src/game/app/enhancements.h`, config key `enhancements`, default on) toggles the port's interface
additions as a group: ETAs and completion days, speed keys 1-4, Repeat mission, the decoys menu entry, extra ship damage
and fleet capacity detail, maintenance totals and economy tooltips, the Saved Games delete cross and named questions,
"Captured - location unknown", and the newer idle markers. Off, those views behave as in the original. It is a local
display preference: simulation, saves and LAN traffic never read it. Panels whose layout differs pick it up when reopened.

## Smoke tests

All need a game installation and run without a window (`SDL_VIDEODRIVER=dummy`). The base check is
`tools/modern_smoke.py`: two processes that load a save, advance, save and reload it.

```sh
uv run tools/modern_smoke.py --exe build/native/Rebellion.app/Contents/MacOS/Rebellion \
  --config work/macos/config.ini --load "<install_dir>/SaveGame/SAVEGAME.003" --output-dir work/smoke
```

Prefix it with these to add checks:

- `REB_UI_SMOKE=1`: fleet-list layout, rendering, hit testing, keyboard navigation, selection and sorting; battle
  participant lists and outcome artwork; status tables; Notice7/Notice15 and assault/story report windows; HQ-only and
  standard victory conditions. Logs `UI visibility checks passed`. The HD review variables (`REB_HD_REVIEW_DIR`,
  `REB_FACILITY_REVIEW_DIR` and others) are in [hd_artwork.md](hd_artwork.md).
- `REB_SWITCH_SIDES_SMOKE=1`: the developer menu click, deferred switching for both factions, queued-order and
  speed/pause preservation, then the same after reload.

These advance the game themselves, so they run as one process:

- **Repeat mission** (`src/game/strategy_ui/mission_repeat.{h,cpp}`) plays a sabotage mission from the "Rebels Fast
  Start" save to its report, presses Repeat, saves and reloads while the team travels home, and checks the second
  mission (about 20 s; `REB_MISSION_REPEAT_REVIEW_DIR` adds captures):

  ```sh
  REBELLION_CONFIG=... SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy REB_DETERMINISTIC=1 REB_MISSION_REPEAT_SMOKE=1 \
    REB_SIM="load:<install_dir>/SaveGame/SAVEGAME.002,ticks:10,save:/tmp/repeat.sav" \
    build/native/Rebellion.app/Contents/MacOS/Rebellion   # exit 0 and "Mission repeat checks passed" in /tmp/repeat.sav.log
  ```

- **Saved Games** (`REB_SAVE_SLOTS_SMOKE=1`, same form) opens the screen over the loaded game, answers each question
  both ways, deletes slot 5 and saves into slot 6. It writes into `<install_dir>/SaveGame`, so point `REBELLION_CONFIG`
  at a scratch install (symlinked game files and a copied `SaveGame` holding saves 1-5 only);
  `REB_SAVE_SLOTS_REVIEW_DIR` adds captures.
- **Campaign space battle** forces a pending battle, votes Take Command, issues orders, saves and reloads mid-battle,
  finishes with Get Immediate Result and checks the strategic results. `REB_LIVE_BATTLE_SMOKE=1`, which the script
  sets, is the test-only exception to the dummy-driver live gate.

  ```sh
  python3 tools/live_battle_smoke.py --exe build/native/Rebellion.app/Contents/MacOS/Rebellion \
    --config /path/to/config.ini --output-dir work/live-smoke
  ```

LAN smokes are in [multiplayer.md](multiplayer.md).

## Object cast regression checks

The native object-cast audit reads the actual compilation database with libclang, including class inheritance and
pointer depth. It checks game and platform sources and the project headers they include; no game assets are needed.

```sh
uv run tools/test_audit_object_casts.py
uv run tools/audit_object_casts.py --build build/native --check
uv run tools/audit_object_casts.py --build build/native --include-downcasts --json work/cast-audit/inventory.json
```

The check rejects unrelated class-pointer casts, inheritance casts made with `reinterpret_cast` (which can lose base
pointer adjustments), pointer-to-pointer output aliases, and casts involving incomplete record types. Parse errors
also fail the check. The checked-in `tools/object_cast_exceptions.json` contains individual reviewed platform API
exceptions with reasons: opaque handles, versioned DirectSound/DirectDraw buffers, DIB/palette buffers and socket
addresses. Removed exceptions fail the full check too. Do not add an exception for a game object accessed through
another class's methods. The macOS arm64 workflow runs these checks after building the native objects; configurations
with different conditional source paths may need their platform exceptions reviewed separately.

`--include-downcasts` inventories ordinary static and C-style downcasts for review. A cast from a base class to the
wrong subclass can still compile, so those require family/registry checks and runtime coverage. The audit does not
track pointers through `void*`, raw byte arithmetic, or function-pointer conversions; it is not proof that every
runtime downcast is safe.

For asset-backed runtime coverage, build the native target with `-DREB_NATIVE_SANITIZE=undefined`, then run:

```sh
uv run tools/native_ubsan_survey.py \
  --exe build/native_ubsan/Rebellion.app/Contents/MacOS/Rebellion \
  --config work/macos/config.ini \
  --load "orig_game_files/Star Wars - Rebellion/SaveGame/SAVEGAME.003" \
  --output-dir work/cast-audit/survey --ticks 1000 --timeout 180
```

Use the corresponding executable path on other native hosts. The survey runs a loaded save and reload with actual
UI and side-switch checks, three fresh galaxy sizes with different AI combinations, and a fresh-campaign reload.
It records sanitizer reports, saves, reference validation, normal teardown and executable identity, and fails for
any report or incomplete case. Output stays under ignored `work/`; installation assets and saves stay out of Git.

## Linux container tooling

The `rebellion-native` container (`docker/Dockerfile.native`) provides sanitizer builds, the arm64 cross build and
Xvfb-driven scenario runs:

```sh
tools/fetch_deps.sh                      # third_party/: SDL3, FreeType, Liberation fonts (sha256 pinned)
tools/build_native.sh --image            # once: the rebellion-native image
tools/build_native.sh                    # build/native/rebellion (RelWithDebInfo)
REB_SANITIZE=1 tools/build_native.sh     # build/native_asan/rebellion (ASan + UBSan)
REB_NATIVE_IMAGE=... tools/build_native.sh --arch arm64   # build/native_arm64/rebellion
tools/run_native.sh tests/scenarios/smoke.txt            # Xvfb + xdotool; screenshots in work/run_native/smoke/
tools/run_native.sh --exe build/native_asan/rebellion --out work/run_native/x_asan tests/scenarios/load_game.txt
python3 tools/native_triage.py work/run_native/x_asan/game.log   # distinct sanitizer reports with counts
```

`tools/container/native_gdb.sh <rundir>` and `REB_GDB=1 tools/run_native.sh ...` give backtraces; `tools/save_diff.py`
compares saves. Useful environment: `REB_MINWIN_SCALE`, `REB_MINWIN_TRACE`, `REB_MSG_TRACE=<file>`,
`SDL_RENDER_DRIVER=software`, `REB_AUDIO_CAPTURE=<file>`.

The image has no window manager. To check fullscreen, close, focus and minimise:

```sh
docker build -t rebellion-native-wm - <<< $'FROM rebellion-native\nUSER root\nRUN apt-get update && apt-get install -y --no-install-recommends openbox wmctrl x11-xserver-utils'
REB_NATIVE_IMAGE=rebellion-native-wm REB_WM=openbox REB_XVFB_GEOMETRY=1920x1080x24 REB_MINWIN_SCALE=1 REB_MINWIN_TRACE=mouse \
  tools/run_native.sh tests/scenarios/native_window.txt          # scaling, Alt+Enter, mouse mapping at the edges
REB_NATIVE_IMAGE=rebellion-native-wm REB_WM=openbox REB_XVFB_GEOMETRY=1920x1080x24 \
  tools/run_native.sh tests/scenarios/native_desktop.txt         # auto scale, close button, Alt+F4, focus, minimise
tools/run_native.sh tests/scenarios/native_cpu.txt; cat work/run_native/native_cpu/cpu.log   # CPU per thread
```

The container's ALSA default is a `null` PCM that consumes audio as fast as it is written, so SDL's audio thread spins
a core there; use `SDL_AUDIODRIVER=dummy` for CPU measurements.

## arm64

* **Float to int**: `cvttsd2si` gives `0x80000000` for overflow and NaN; arm64 saturates. `reb_ftol`
  (`core/reb_x87.h`) gives the x86 result on every target; `tools/find_float_casts.py` lists the conversions, and a build with
  `-DREB_NATIVE_SANITIZE=address,undefined,float-cast-overflow` reports the ones that overflow.
* No x86 intrinsics or inline assembly: the remaining `__asm__` is limited to empty compiler barriers.
* `char` is signed (`-fsigned-char`), `-ffp-contract=off` so arm64 does not fuse multiply-adds, `-fwrapv`.
* **Memory ordering**: arm64 is weakly ordered, x86 is not. The game's two helper threads (sound refill, movie) share
  a few plain flags with the main thread, written for x86. qemu-user runs them with the host's ordering, so only real
  arm64 hardware can show a race there.

## CI

All three workflows run by hand (`workflow_dispatch`) and use no game data; tests that read the installation skip
when `MW_INSTALL_DIR` is unset.

- `.github/workflows/linux.yml` (ubuntu-24.04, x86-64 and aarch64 cross): compiles every native object and runs the
  `src/port/minwin_impl` unit tests on x86-64.
- `.github/workflows/macos.yml` (macos-14 arm64, macos-15-intel x86-64): compiles every native object, runs the object
  cast checks on arm64 and the `src/port/minwin_impl` unit tests.
- `.github/workflows/windows.yml` (windows-latest, `clang-cl` and the Windows SDK): compiles the native objects and
  runs the resource-free tests. The Windows build has not yet been through a startup/load/save/reload smoke on a
  Windows host, so Windows support is not considered verified.

macOS builds happen on macOS runners or a Mac: the Xcode and Apple SDKs agreement does not permit extracting the SDK
to cross-compile on Linux.
