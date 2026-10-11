# Native Linux x86-64 build (phase 3, P2/P3)

macOS, arm64 and the list of OS differences: see [native.md](native.md).

`build/native/rebellion` is the game built from the same `src/game` sources as the Windows builds, for the host ABI (x86-64, Itanium
C++ ABI, LP64) and with `src/port/minwin_impl` (SDL3, FreeType) as the platform. No Wine, no i686. The player's licensed game files are read
in place (`install_dir` in the config file, see `src/port/minwin_impl/config/config.h`).

## Build and run

```
tools/fetch_deps.sh                      # third_party/: SDL3 and FreeType sources, Liberation fonts (sha256 pinned; gitignored)
tools/build_native.sh --image            # once: the rebellion-native container (docker/Dockerfile.native, FROM rebellion-dev)
tools/build_native.sh                    # build/native/rebellion (RelWithDebInfo)
REB_SANITIZE=1 tools/build_native.sh     # build/native_asan/rebellion (ASan + UBSan, game and port objects)
tools/run_native.sh tests/scenarios/smoke.txt            # Xvfb + xdotool in the container; screenshots in work/run_native/smoke/
tools/run_native.sh --exe build/native_asan/rebellion --out work/run_native/x_asan tests/scenarios/load_game.txt
python3 tools/native_triage.py work/run_native/x_asan/game.log   # distinct sanitizer reports with counts
```

Running by hand: write a config file (`install_dir=/path/to/Star Wars - Rebellion`, optional `font_path=`, `window_scale=`, `fullscreen=`; see native.md "Playing on a desktop"),
`REBELLION_CONFIG=<file> build/native/rebellion` (the working directory is changed to `install_dir` by the game itself). Useful
environment: `REB_MINWIN_SCALE`, `REB_MINWIN_TRACE`, `REB_MSG_TRACE=<file>`, `SDL_RENDER_DRIVER=software`, `REB_AUDIO_CAPTURE=<file>`.
`tools/container/native_gdb.sh <rundir>` and `REB_GDB=1 tools/run_native.sh ...` give backtraces; `tools/save_diff.py` compares saves.

The script extracts the [versioned startup text pack](design/startup_assets.md) from the local installation, then
configures and builds the native target with Clang/C++23. `--objects-only` skips asset extraction. CMake itself can
compile without an installation; runtime smoke requires assets. The native target uses normal exception support and
RTTI, with the host C++ allocator and typed startup/service ownership.

## What is where

| Piece | Where | Notes |
|---|---|---|
| kernel32 on POSIX | `src/port/native/kernel32_posix.cpp` | events, semaphores, threads (pthreads; `TerminateThread` is `pthread_cancel`), critical sections (a recursive pthread mutex behind `LockSemaphore`), QPC/`GetTickCount` (`CLOCK_MONOTONIC`), `FindFirstFileA` with wildcards, `_lopen/_lread/_lclose`, `DeleteFileA`, `GetDriveTypeA` (always fixed: the CD check passes with the install directory), `GetComputerNameA`. `timeGetTime/timeSetEvent` are in `main.cpp` (the window manager's pump-driven timers). |
| MSVC CRT names | `src/port/native/crt_posix.cpp`, `crt/` | `_sopen/_read/_write/_lseek/_close/_setmode/_chdir/_mkdir/_chdrive`, `_stricmp/_strnicmp`, `<io.h>`/`<direct.h>` shims. All paths go through `mw::res::ResolvePath` (backslashes, drive letters, any case). There is no text mode: every file the game opens is opened binary, so no CRLF translation is needed. Game installation paths use the explicit `OpenGameFile` adapter. |
| standard CRT | host C++ runtime | reconstructed standard-library replacements and global name redirects are retired; game seed generation remains an explicit game primitive |
| iostream library | `src/game/iostream` | streams own typed `ios`, input/output views and virtual `streambuf` implementations; native vtable/vbtable shims are retired |
| program globals | typed definitions in `src/game` | no byte storage, assembler aliases or symbol metadata generation |
| initialization | `src/port/native/game_context.cpp` | named dependencies and scoped platform teardown; no EXE initializer replay |
| entry | `src/port/native/main.cpp` | asset owner, `minwin_wiring::Install()`, scoped context, `RunApplication` and exception boundary |
| tactical 3D | `src/port/native/tactical_absent.cpp` | the virtuals of `TacticalObjectManager` that are declared but never reconstructed; battles are auto-resolved |
| config | `src/port/minwin_impl/config` | on POSIX the derived `*Location` registry values are `C:` + install dir, so the game's "drop the drive letter, chdir" code works |

## Program state and assets

Globals have real C++ definitions, normal object construction and source-level array capacities. Named startup work
preserves derived metrics and registers animation/table prototypes. The delayed display service belongs to a scoped
native context. Sound, font and resource state is drained before the platform's lazy registries are destroyed.

Startup text uses a stable asset pack extracted by `tools/extract_startup_assets.py`. Its runtime format has no C++
symbols, object offsets or pointer widths. The old native-data and initializer generators are deleted; historical
implementation is available in Git. Ordinary resource DLLs and game tables are still loaded from the installation.

## Porting fixes found by running (all no-ops on i686)

The classes that mirror the original layout by size and padding were the main source. Found by ASan/UBSan (`-fsanitize=address,undefined`, recoverable):

* storage for embedded objects declared by their 32-bit size (`CoolFrameButton::m_frames`, `HumanPlayer::m_alertList`, `SpriteAnimator::m_steps`,
  `CoolGameWindow::m_overlays`, `SeedFamilyTableEntry::m_items`, and `ObjectTypeDb`'s stack name lists): now members of their actual host-native types;
* raw offsets into such objects (`m_frames + 8`, `ios + 0x2c`, `ios + 0x30`, filebuf `+0x4c`);
* the iostream wrapper layouts and critical-section storage (typed composition and native `CRITICAL_SECTION` members);
* 32-bit pointer differences and `int` parameters carrying pointers (`strstreambuf_doallocate`, `Mission::CheckMemberDefault`);
* an array cookie of 4 bytes before 8-aligned elements (`MissionMgr` type table, `AnimationStep` frame lists);
* a `LARGE_INTEGER*` punned over two `uint32_t` fields (`Stopwatch`);
* classes with fake leading virtual slots (`ForeignSlots` in `system_accessors.cpp`): a virtual call must be through a declared virtual, because
  Itanium vtables give a virtual destructor two entries;
* the ICF-folded accessor `GameObj::GetDepartureKey` applied to a `Command` (`EventBase::m_key` at the same offset).

## Results (measured Oct 9 2026)

**State: the native executable runs the game.** Boots through the intro movies (Smacker decoder, audio through SDL: `REB_AUDIO_CAPTURE` of
`video_intro` peaks at 32765), main menu, Saved Games, loading each shipped save (SAVEGAME.001-.004), the galaxy screen with the game
clock running, a new Rebel and a new Empire game, saving (SAVEGAME.005 written natively; loading it back gives the same galaxy), the
auto-resolve battle flow, and the UI tour (all windows). Not available: the 3D tactical battle (stubbed everywhere, as in the Wine
standalone), multiplayer (DirectPlay answers "unavailable"), AVI playback.

**Simulation goldens (`tools/native_sim.sh`; the 3 goldens of `tests/goldens`: 3,000 + 2,000 + 2,000 ticks, 500-tick checkpoints, one auto-resolved space
battle through flow states 3-12, 21, 23): every save (checkpoints and final) is byte-identical to the i686 goldens, and so are the logs
(frame counts, message counters, flow states) except the first line, which names the loaded file.** No float drift showed up at all in these
7,000 ticks of AI, economy and mission logic. Native floats (SSE, `-ffp-contract=off`) give the same results as the x87 build here; the
earlier divergences were all port bugs (below), found by comparing against the i686 run tick by tick (saves every tick, then a trace of
random draws, executed events and a checksum of all game objects after every event, then function-entry traces of the divergent tick).
A native run is deterministic run to run (ASLR does not matter: no pointer-ordered container influences the saves), and independent of the
virtual clock's step.

**Sanitizers** (`REB_SANITIZE=1`: ASan + UBSan with recovery, `-fno-sanitize=function` because reconstructed game code stores function pointers under other types): clean (0 reports) on the three simulations above (7,000 ticks, 5 min) and on these scenarios: smoke, load_game, ui_tour,
new_game_rebel, new_game_empire, space_battle_autoresolve, encyclopedia, fleet_window, battle_window, object_menu, mission_dialog, manufacture_explore,
game_controls, system_forces, save_game_slot. The saves of the sanitized and the plain build are identical. Everything the sanitizers reported on the way, with its fix:

| Finding | Where | Kind | Fix |
|---|---|---|---|
| a `ptrdiff_t` truncated to `int` between two allocations | `strstreambuf_doallocate` | game, 64-bit | `ptrdiff_t` |
| embedded objects in byte arrays sized for 32 bits (6 sites) | `CoolFrameButton::m_frames`, `HumanPlayer::m_alertList`, `SpriteAnimator::m_steps`, `CoolGameWindow::m_overlays`, `SeedFamilyTableEntry::m_items`, `ObjectTypeDb`'s stack name lists | game, 64-bit | actual host-native member types |
| raw offsets into those objects and into `ios`/`filebuf` | `m_frames + 8`, `ios + 0x2c/0x30`, `filebuf + 0x4c` | game, 64-bit | named fields |
| 4-byte array cookies in front of 8-aligned elements | `MissionMgr` type table, `AnimationStep` frame lists | game, 64-bit | cookie = alignment |
| `LARGE_INTEGER*` over two `uint32_t` fields | `Stopwatch::Reset` | game, 64-bit | local (32-bit build keeps the original call: difftest compares the API arguments) |
| pointers in `int` parameters/outputs | `Mission::CheckMemberDefault` | game, 64-bit | `intptr_t` |
| stand-in classes for objects the game really builds as orders and notices (`RenameRequest` for the order `TextMessage`, `ConfirmRequest` for `Notice8`): fields at 32-bit offsets and a virtual call through a made-up vtable slot (Itanium vtables differ) | renaming a fleet or system (Return in the rename field), confirming a move order (`object_menu`, `move_drag_confirmed` crashed: SEGV in `String::Assign`) | game, 64-bit | the stand-ins removed; the real classes with their own members and `GetTeam()` |
| a String passed by value in three `int` argument words | `PlayerController::Virtual87` and overrides | game, 64-bit | `StringWords` (three pointer-sized words; the same stack bytes on i686) |
| object layouts described twice (`PlayerNotice` stub vs the real `PostedEvent`/`AiNotice`; `TrainingOrderCheck`/`StoryOrderCheck` vs `MissionCheckArgs`) | notice routing, Jedi training and story mission checks | game, 64-bit: **changed simulation results** (notices never reached the AI; a mission completed too early) | derive from the real base; field copies |
| fake leading virtual slots (`ForeignSlots`) | `System::FindManufacturingManagerBySlot123` and two siblings | game, ABI: Itanium vtables differ | call the declared virtual |
| ICF-folded cross-class accessors (`GameObj::GetDepartureKey` on a `Command`, `ObjectRefNode::GetKey` on a payload) | orders, UI notifications | game, 64-bit | named field |
| LP64 iostream wrapper layout and critical-section alignment | `iostream.h`, `ios.cpp` | typed composition with host-native `CRITICAL_SECTION` members |
| alignment of globals (16 for arrays of 16+ bytes; the compiler emits aligned SSE stores) | generated data | port | `alignas` |
| a Win32 handle used after `CloseHandle` (the mixer signals a closed notification event) | `kernel32_posix.cpp` | port (the game does it on Windows too) | live-object registry |
| a window destroyed by its own procedure, then read | `minwin::CallProc` | port | read before the call |
| the label shared by two objects, freed, drawn, freed again | `BattleInProgressWindow::OnCreate1` | **original bug** (benign on the NT heap) | `fixedText` gives its string up |

**Original bugs kept** (marked `BUG(original)`): everything the sanitizers did not flag.

**Argument evaluation order** (unspecified in C++: right to left in MSVC and clang-cl, left to right in the Itanium ABI). `tools/find_arg_order.py`
finds 282 argument lists in `src/game` with two or more calls (the first version's 215 skipped const methods, lambdas were counted wrongly).
It proves 248 order-independent by analysis: every call has a visible body that writes nothing but its own locals and calls only such functions
(virtual calls count when all overriders do). The other 34 were read by hand (callees, not names) and are order-independent too: out-key getters writing
only their own local key, by-value `String` copies (allocation only), independent Win32 lookups, `++local` next to a getter. They are listed with the reason in
`tools/arg_order_allow.txt`. No list drew random numbers or advanced a cursor, so nothing needed hoisting and no i686 code changed.
`tools/check_arg_order.sh` (about 40 s, a CMake configure plus libclang) fails on any new unclassified list; `tools/integrate.sh` runs it.
Not covered by the gate (informational, `REB_ARGCHECK_ARGS=--reads`): one side-effecting call next to a plain read of memory it might change
(`f(m_x, Mutate(&m_x))`); 74 lists, all reviewed by their callee names as benign (key getters, Win32, string temporaries).

**Unit tests**: the i686 builds are unchanged: hybrid 10,706 hooks, `check_callee_pop` 0 mismatches, `standalone_check` OK (saves byte-identical to the original's),
`sim_goldens check` identical. See the end of this section for the difftest numbers. `src/port/minwin_impl` unit tests (`config_test`) pass natively.

## Remaining

* No i686 Linux verification build (P2): not needed for any result above.
* 3D tactical stays out of scope; the multiplayer lobby shows no providers.
* ASan/UBSan runs cover the scenarios above, not every UI path (the long scenarios `long_*`, `mp_lobby*`, `soak_*`).
* macOS (P4) needs the SDL3 build flags (`cmake/native.cmake` already avoids Linux-only pieces except the container), the CRT names of `crt_posix.cpp`,
  and `-fsanitize=function` decisions.
* `TerminateThread` is `pthread_cancel` (asynchronous): fine for the game's two threads at shutdown, not a general implementation.
* The pointer-order test (`REB_HEAP_PROBE`-like) is not ported; ASLR gives it for free (two native runs agree).
