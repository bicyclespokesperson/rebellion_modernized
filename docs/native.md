# Native builds: Linux and macOS, x86-64 and arm64 (phase 3, P3/P4)

One configuration builds all four: `cmake/native.cmake` (`-DREB_NATIVE=ON`). Everything that differs between the operating systems is in
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

## Build

```
tools/fetch_deps.sh freetype liberation sdl3                  # third_party/
tools/build_native.sh                                         # Linux x86-64, in the container (as before)
REB_NATIVE_IMAGE=... tools/build_native.sh --arch arm64       # Linux aarch64 cross build, build/native_arm64/rebellion
tools/native_sim.sh -a arm64                                  # the sim goldens on it under qemu-user
```

macOS (on a Mac with Xcode command-line tools, cmake, ninja, python3 with `pefile capstone unicorn clang`; not run yet, see below):

```
cmake -S . -B build/native -G Ninja -DREB_NATIVE=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo     # -DCMAKE_OSX_ARCHITECTURES=arm64|x86_64
cmake --build build/native --target reb_native_objects
python3 tools/gen_native_data.py --build build/native --exe <REBEXE.EXE> --symbols symbols --out build/native/gen   # and gen_native_init.py
cmake -S . -B build/native -DREB_NATIVE_GEN=ON && cmake --build build/native --target rebellion      # build/native/Rebellion.app
```

`tools/build_native.sh` is container-only (the generators need libclang 19 and the other Python packages of the image).

## arm64

* **Float to int**: `cvttsd2si` gives `0x80000000` for overflow and NaN; arm64 saturates. `reb_ftol` (`core/reb_x87.h`) has the x86 result on every
  target. The native x86-64 build run with `-fsanitize=float-cast-overflow` (`REB_NATIVE_SANITIZE=address,undefined,float-cast-overflow`) shows the
  places that rely on it; results in the phase 3 plan.
* No x86 intrinsics or inline assembly in the native build: the only `__asm__` left are empty compiler barriers (`vptr_store.h`, `atexit_keep.h`,
  `galaxy_display.cpp`); the x87 code in `reb_x87.h` and `crt/floor.cpp` is `#if` i386.
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

* aarch64 Linux cross build: all 504 objects compile, generators run (0 problems), link. `tools/native_sim.sh -a arm64` under qemu-user: all three
  sim goldens identical to the i686 goldens (17/17 files), same as x86-64 native.
* Float to int: `tools/find_float_casts.py` lists 29 conversions (25 in game code, now `reb_ftol` / new `reb_ftou`; 4 in the port: mouse
  coordinates and two bounded volume curves in dsound/smack, left alone). The `float-cast-overflow` build reported 0 overflows on the three sims
  and 15 UI scenarios (smoke, load_game, ui_tour, new_game_*, space_battle_autoresolve, ...), so the routing is protective, not a bug fix.
  `reb_ftol` returns INT32_MIN out of range (cvttsd2si); the original's `_ftol` truncates to 64 bits, so values in 2^31..2^63 wrap there;
  `reb_ftou` follows the 64-bit form.
* Unchanged: hybrid 10,706 hooks, difftest unit and integration 12,988 captures no FAIL, standalone_check OK, arg-order check clean, x86-64 sims 17/17.

## Needs a real Mac / arm64 hardware

The macOS build has never been compiled: Apple clang vs clang-19 diagnostics, SDL3's Cocoa flags and link frameworks in the static build,
`Rebellion.app` packaging and codesign, the generator step on macOS (libclang), the unit tests next to a system Arial, and the
`TerminateThread` cancellation behaviour. arm64 memory ordering (see above) needs hardware.
