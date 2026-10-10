# Phase 3 plan: native Linux and macOS

Measured Oct 9 2026 on the Phase 2 tree (standalone i686 rebellion.exe runs under Wine; deterministic saves byte-identical
to the original logic). Targets: Ubuntu x86-64, macOS arm64 + x86-64. 3D tactical stays stubbed (auto-resolve).

## Platform surface

137 files, 2,040 Win32 call sites, 211 distinct APIs. USER32 1,205 and GDI32 533 (ui + strategy_ui hold 1,546 of them),
KERNEL32 169, WINMM 53, SMACKW32 41, the rest small (registry 12, version 7, AVIFile 6, DDraw 5 — mode switch only,
DSound ~18 COM methods, DirectPlay ~15). Game logic (ai, events, economy, ...) has almost no Win32 calls.

The UI is self-drawn (software blitter, 84 callers) but every widget is an HWND: Win32 provides hit-testing, focus,
capture, clipping and paint scheduling (96 WndProcs, 162 `case WM_*` arms, ~23 custom messages, nested modal loops,
`SetWindowsHookEx`). **Decision: emulate the Win32 subset ("minwin", est. 9–10 k lines on SDL3) rather than rewrite
~41 k lines of UI.**

## 64-bit

Saves and .DAT are already field-by-field (681/691 Read/WriteValue32, ~486 field checksums) — original-save
compatibility on 64-bit is reachable. To fix: typed `ReadValue32<T>` with size asserts (~1,500 calls), 26 pointer→int
casts, 7 Get/SetWindowLong sites, 40 byte-offset punning sites, 55 untyped `DAT_` arrays, **31 `::operator new(0x..)` +
placement new (heap overflow on 64-bit)**, **14 manual vtable-slot index calls (wrong under the Itanium ABI)**, 84 plain
`long`. The data image and static-init table become generated C++. No shipped 32-bit Linux build (macOS has none);
i686 Linux only as a verification step. First x86-64 build with `-Werror=pointer-to-int-cast,int-to-pointer-cast,shorten-64-to-32`.

## Toolchain and x87

Calling-convention keywords become empty macros off i386 (484 `__thiscall`, 258 stdcall-family). i386-only asm in 5
files (`crt/x87_math.cpp`, `crt/floor.cpp`, `tactical/tactical_math.h`, two inline `call _CIpow`). clang everywhere with
`-fwrapv -fno-strict-aliasing -fsigned-char -ffp-contract=off -ffp-eval-method=double -fno-fast-math`.

x87 differences that matter: transcendentals (`fsin`/`fcos` in the tactical sim, `_CIpow`/`_CIacos`: 64-bit mantissa,
ignore precision control), excess precision of register-held locals, float→int overflow (x86 0x80000000 vs arm64
saturation). Plan: a `reb_x87` module — i386 keeps the asm; elsewhere a soft-float implementation of exactly those
instructions, validated bit-for-bit against arguments recorded on i686; `reb_ftol()` for conversions. Excess precision
is not emulated globally; goldens measure it. i686 Linux must set the FPU control word to 0x027F.

## Milestones

| # | Milestone | Exit criterion |
|---|---|---|
| P0a | Headless simulation harness (`REB_SIM=load:X,ticks:N,save:Y` driving the tick step directly, no clocks/input) + in-process scenario player on virtual time; golden corpus (3 saves × 2,000 ticks incl. auto-resolve); heap-shift probe in the standalone allocator | 5 runs byte-identical; heap probe identical or the pointer-ordered containers listed |
| P0b | Portability cleanups that are no-ops on i686 (vtable-index calls, raw `new`, typed I/O, pointer casts, punning, typed globals, `reb_x87` API, `__try`, asm guards) | difftest PASS; goldens + screenshots unchanged |
| P0c | minwin headers selected by `platform/win32.h`; layouts `static_assert`ed against the SDK on i686 | standalone with minwin headers + real import libs identical |
| P1 | minwin on SDL3 (window manager, GDI/palette, FreeType text, PE resources, config store, DSound mixer, Smacker decoder), i686-windows under Wine | no user32/gdi32/dsound/smackw32/ddraw/advapi32 imports; goldens identical; screenshots identical (glyphs aside) |
| P2 | Linux i686 ELF (verification only) | goldens byte-identical; Xvfb screenshots match P1 |
| P3 | Linux x86-64 (native floats) | ASan/UBSan clean on the scenarios; goldens identical or drift characterised (first-divergence float tracing); original saves load (nice to have) |
| P4 | macOS arm64 + x86-64 | same invariants as P3 on both architectures |
| P5 | Multiplayer over TCP/ENet behind CommMgr | two native instances finish a game (not DirectPlay-compatible) |

Every milestone leaves the Wine standalone byte-identical. Screenshots compare minwin's 8-bit framebuffer + palette.

## Decisions (user, Oct 9 2026)

1. **Float drift on 64-bit is acceptable** (exactness welcome but not required). No soft-x87 work up front: 64-bit builds
   use native floating point; P3/P4 exit on invariants (no crashes, ASan/UBSan clean, plausible battle/AI statistics,
   and goldens compared where they still match) instead of byte-identical goldens. The i686 goldens stay byte-exact.
   Revisit soft x87 only if drift causes visible divergence problems.
2. **Original save compatibility: nice to have, not required.** Keep it while it is free (don't widen saved fields
   without reason), but it doesn't gate any milestone.
3. **Libraries:** SDL3 (zlib), FreeType (FTL), own Smacker decoder; avoid LGPL.
4. **Font:** the user's Arial when present, Liberation Sans as fallback.
5. **Game data** read in place via a config file pointing at the install; multiplayer later over TCP.

## Hard parts

Window-manager semantics (message order, re-entrant SendMessage, WM_PAINT timing, modal loops); text metrics driving
line breaks; turning the 4 threads (MM timer, Smacker, sound events, DSound refill) into pump callbacks without audio
underruns; pointer-ordered iteration under a different allocator (P0a probe first); remaining vtable candidates
(`MissionJediTraining` slot 7, `HumanPlayer` slot 28).

## P0a results (measured Oct 9 2026)

**Harness** (`src/port/sim_harness.cpp`, standalone only; WinMain calls it instead of `RebellionApplication::Run` when `REB_SIM` is set):
`REB_SIM=load:<savefile>,ticks:<N>,save:<path>[,every:<k>][,log:<path>]`. It loads the save like `LoadPendingGame` + `BattleFlow::Restore`
(state 2 only), then calls `AppStepGame` (the main loop's call, `0x401bd0` -> `BattleFlow::Step` -> `GameStepDriver::Step`, confirmed) with
`m_waitElapsed` forced to 1 before every frame, so no wall clock, message pump or input is involved, until the game's tick counter reaches the
target and the frame phases of that tick are done; saves every k ticks (`<save>.t<tick>`) and at the end, writes `<save>.log` (frames,
per-checkpoint message-id counters of both controllers, battle-flow states entered) and exits. AI turns need nothing special: the
AI controller acts inside `GameStepDriver::Step` (scheduler/session phases). What it needs: the application object (WinMain builds it; no
window is created, but WinMain's display check and the registry values still run, so the exe runs under Xvfb/Wine as usual) and nothing else:
no DirectDraw, no strategic window. Stand-ins for the UI: (1) a loaded game starts paused (`m_pauseTick == tick`); the harness clears it, as
the player's un-pause does; (2) the human side's battle votes are answered in place of the vote/battle windows (auto-resolve for the type vote, then
confirm), the UI notices those windows would have queued are not created; (3) the strategic window state that follows the game in a save
file is neither read nor written, so harness saves are 85 bytes shorter than game saves and are not loadable by the UI without it
(`LoadGame` ignores that part). Tick rate: about 2,000 ticks in 20-60 s under Wine.

`tools/sim_run.sh [-e VAR=VAL] <save> <ticks> <outdir> [every]` runs one simulation in the dev container (`tools/container/run_sim.sh`);
`tools/sim_goldens.sh record|check` runs the corpus `tests/goldens/sim_goldens.txt` and compares sha256 of every save and of the harness
log against `tests/goldens/<name>.sha256` (hashes and command lines only).

**Goldens** (shipped saves SAVEGAME.001/.002/.004; 003 is the Phase 2 save scenario's start): `battle1` (3,000 ticks; an auto-resolved space
battle at about tick +2,250 goes through flow states 3-12, 21, 23), `alliance1` and `empire1` (2,000 ticks each; the other side's AI
plays, no battle within 12,000 ticks in these saves). **5 runs each: byte-identical** (all checkpoint saves and the logs).

**Heap probe** (`src/port/heap_probe.cpp`, behind the standalone's `operator new/delete`): `REB_HEAP_PROBE=<bytes>` shifts by leaking
that much first; `REB_HEAP_PROBE_MODE=jitter` leaks 8-512 bytes before every 5th allocation; `REB_HEAP_PROBE_MODE=reverse` serves
`operator new` from a private downward-growing arena with LIFO free lists, i.e. pointer order of later allocations inverts (the sample
`new` addresses in `rebellion.log.heap` confirm each mode took effect). All three give **saves identical to the goldens** on all three corpus entries: no
pointer-ordered container influences the saved strategic state in these 7,000 ticks (the small-hole reuse of the NT heap makes shift
and jitter weak probes; `reverse` is the strong one). Not covered: containers keyed by pointers that never reach a save (ordering effects on
UI only), `malloc` users (the game's CRT calls go to the UCRT heap unperturbed), uninitialised memory (`operator new` zero-fills in
the standalone, as in the earlier phases).

**Cross-check against the UI-driven loop.** `save_determinism` on the standalone (load SAVEGAME.003, click Save, 2 runs identical,
md5 c676b92c) saves at tick 8089; the harness from the same file at tick 8089 gives the same bytes for the whole game manager (the first
766,057 of 887,380 bytes: every strategic object) up to the stream position markers, which differ by a constant 15 because the
header name is 15 bytes longer in the UI save. The session tail (human controller notices and message ids) differs, as it
should: the UI-driven human controller has been clicked on and has queued messages. The hybrid cannot run `REB_SIM` (src/hybrid must
not call game code), so this comparison against the standalone's own UI-driven loop is the check of the harness.

## P0b results (measured Oct 9 2026)

Portability cleanups that change nothing on i686 (same 10,706 hooks, `check_callee_pop` 0, difftest unit + integration unchanged, standalone saves and
sim goldens byte-identical). The counts are the measured ones; the plan's numbers were estimates from a grep.

| Cleanup | Found | Done | Left (why) |
|---|---|---|---|
| Manual vtable-slot index calls | ~85 call sites in 27 files (`callVirtualSlot`/`CallVirtualSlot` ~75, `vtable[n]` / `table[slot]` 10) | all: each is now a call of a *declared* virtual. The slot numbers were checked against clang's MSVC-ABI vtable dump (`-Xclang -fdump-vtable-layouts`) | the iostream library's `IosVtable`/`StreambufVtable` are explicit slot structs, not index arithmetic |
| Raw-size `operator new` | 31 literal sizes, 11 named-constant sizes, 8 element-size expressions | 50 now use `sizeof(T)` | 3 opaque 3D-tactical classes (events 0x220 x2, order 0x84) and the 3D window (0x114): their constructors are not reconstructed |
| Typed stream I/O | ~1,490 `Read/WriteValue8/16/32`, `Stream*16/32` calls | all typed: `static_assert(sizeof(T) == width)`; the `void*` overloads are the raw primitives | only 3 sites failed the assert (partial 16-bit writes of an `int` / `uint32_t`, now `uint16_t` halves). No pointer, `long` or 64-bit field reaches a save |
| Pointer<->int casts | 27 flagged by clang, ~60 more `reinterpret_cast`s between `int` parameters and pointers (clang does not diagnose `reinterpret_cast`) | all | 19 deliberate truncations, kept explicit and greppable: `static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this))` emulating an uninitialised stack word, or an address used as an id |
| `Get/SetWindowLong` | 7 | reviewed: all `GWL_STYLE`/`GWL_EXSTYLE` values, none stores a pointer (`cool_window.cpp` already uses the `*Ptr` API) | |
| Byte-offset punning | ~100 | ~85 now use named fields (new overlays `TableEntryWithValue`, `SeedTableEntry`; `MissionCheckArgs`, `Side`, `Fleet`, `Role`, `Mission`, `ManuMgr`, `RulesMgr`, `CharacterMgr`, `MissionMgr`, `RebellionApplication` members) | 3D tactical (8), ICF-folded cross-class accessors (3), original bugs kept with a comment (reads past the end of `AllFacility` and `Character`; the menu "Return" handler reading a null item at 0x60), array-header and DIB-record offsets |
| Untyped `DAT_` globals | 50 (43 handler singletons, 6 guard bytes, 1 string) | 50, renamed in `symbols.csv` | |
| x87 / asm / calling conventions | 5 files, ~800 keyword uses | `core/reb_x87.h` (`reb_pow`, `reb_acos`, `reb_sin/cos/sqrt`, `reb_scaled_log10/pow`, `reb_ftol`) with the i386 asm unchanged and a libm `#else`; `crt/x87_math.cpp` and `crt/floor.cpp` are `#if i386`; `__try` is `REB_TRY/REB_FINALLY`; 265 files use `REB_THISCALL/REB_STDCALL/REB_CDECL` (`core/reb_abi.h`, empty off i386) | `crt/rand.cpp`, `qsort.cpp`, `atoi.cpp` define libc names; native builds must rename them (the `rand` sequence and the `qsort` order are game behaviour) |

`int` parameters that carried pointers became `intptr_t` (the same type on i686): the notice slots (`OnMessage`, `Virtual05-09`), `GameObj::Virtual29/31/47/48/55`,
`NotifyUnit*/NotifyOwner*` (formerly `Virtual49-53`, renamed in `symbols.csv`), the game hooks (`HookArg`), `PlayerController::Virtual25/76/92`,
`UiSendNotificationToSide`, `SpriteAnimator::Start/SetStart`, `CoolListItem::m_userData` (now a union with `m_userPointer`), the mission and
manufacturing manager entry points that took keys and contexts as ints, and a few more.

**Trial x86-64 compile.** `clang-cl --target=x86_64-pc-windows-msvc` with the xwin SDK headers over all 456 TUs of src/game, with
`-Werror=pointer-to-int-cast -Werror=int-to-pointer-cast -Werror=shorten-64-to-32` and a force-included wrapper that turns every
`reinterpret_cast` between a pointer and an integer narrower than a pointer into a `static_assert` (clang's own warning skips `reinterpret_cast`): 44 errors
in 14 files plus ~60 only the wrapper sees at the start, **0** at the end. The Windows data model is LLP64, which hides `long`. The LP64
trial (`clang++ --target=x86_64-linux-gnu`) cannot parse the 182 TUs that include `platform/win32.h` (the minwin work, P0c/P1) and finds, in the other 273,
only `long` in the iostream library (`filebuf_seekoff`, `ostream_operator_ls_long`, ...) and `_strnicmp`.

Not 64-bit clean, and not visible to these trials: the iostream model (`ios` holds a `streambuf*` and `long`s at 32-bit offsets; `IosOf` reads 32-bit
vbtable offsets), `HumanPlayer::m_alertList` (an opaque 0x10-byte array that holds an `AiNoticeQueue`), classes declared by size and padding,
and the `int`-typed pointer slots of the not-yet-reconstructed 3D tactical code.

**Checking virtual slots.** Replacing `vtable[n]` calls is only safe if the declared virtual really sits in slot n. `clang-cl -Xclang -fdump-vtable-layouts`
prints every class's MSVC-ABI layout with indices; a script that compares `Class:method=slot` pairs against that dump caught nothing wrong this time and is
cheap to rerun (compile each TU with the flag, merge the dumps, look the pairs up).

## P0c results (measured Oct 9 2026)

**minwin headers** (`src/port/minwin/`, 2,100 lines): `platform/win32.h` includes `port/minwin/minwin.h` instead of `<windows.h>` and the five SDK headers
when `REB_MINWIN` is defined. The standalone's CMake option is `REB_MINWIN` (`cmake/standalone.cmake`, default OFF; `REB_MINWIN=ON tools/build_standalone.sh`);
it applies to `reb_game` and `reb_tactical_stubs` only. The runtime objects (`src/standalone`, `src/port`) and the hybrid keep the SDK headers.

| Header | Holds |
|---|---|
| `core.h` | calling-convention macros (`WINAPI`... over `REB_STDCALL`), `MINWIN_IMPORT`, scalar and pointer-sized typedefs |
| `base.h` | handles (`struct HWND__ *`...), `GUID`/`IID`, `IUnknown`, `RECT`/`POINT`/`SIZE`, `HRESULT`, `LARGE_INTEGER`, `CRITICAL_SECTION`, `LOWORD`... |
| `kernel32.h` `gdi32.h` `user32.h` `winmm.h` `vfw.h` `comctl32.h` `advapi32.h` `version.h` | one per DLL: functions (`extern "C"`, `MINWIN_IMPORT ... REB_STDCALL`), structs, constants |
| `ddraw.h` `dsound.h` | COM interfaces as C++ structs with the virtuals in the SDK's vtable order, the structs, constants; `IID_*` as `extern const GUID` (linked from dxguid.lib) |
| `platform/dplay.h`, `platform/smacker.h` | already ours; they now use `REB_DLLIMPORT`/`REB_STDCALL` and plain virtual structs instead of `DECLARE_INTERFACE_`, in both header sets |

Typing rule: on Windows targets the scalar types are spelled exactly as the SDK spells them (`DWORD` = `unsigned long`, tags `tagRECT`, handles `struct HWND__ *`),
so C++ mangled names, overload resolution and code generation are those of the SDK build; elsewhere `LONG`/`DWORD` are `int32_t`/`uint32_t` and `LONG_PTR`/`WPARAM`/`LPARAM`
are `intptr_t`/`uintptr_t`, so every struct has the same layout on ILP32 and LP64. `MINWIN_IMPORT` is `__declspec(dllimport)` only on Windows (`REB_DLLIMPORT` in `core/reb_abi.h`);
the native implementation compiles with `MINWIN_IMPLEMENTATION` to define the functions instead. `GetWindowLongPtrA`/`SetWindowLongPtrA` alias the 32-bit calls on 32-bit Windows
and are real 64-bit functions elsewhere.

**Inventory** (`tools/minwin_inventory.py` -> `tools/minwin_inventory.txt`, regenerate with the script; needs the dev container): 583 distinct SDK/platform names appear in `src/game`
(user32 244 [92 functions], gdi32 84 [40], base types 65, kernel32 49 [38], winmm 37, DirectPlay 24, Smacker 20, dsound 17, ddraw 16, comctl32 10, advapi32 6, version 3, AVIFile 3), plus the
methods of 8 COM interfaces (IDirectDraw/2/Surface, IDirectSound/Buffer/3DListener/Notify, IDirectPlay2A/LobbyA). The declarations needed for them (transitive types, constants the
macros use) are 214 functions, 181 typedefs, 55 records, 241 constants. The inventory is by identifier name (an over-approximation); a method called through a typedef'd interface
pointer is not seen by it (`IDirectDrawSurface::Restore` surfaced as a compile error) -- the compile is the real test. `tools/minwin_scaffold.py NAME...` drafts the declarations of a
missing name (and its dependencies) from the SDK's AST; the headers are hand-maintained after that.

**Checks** (`tools/minwin_check.sh [--objects]`, one verdict line, all in the dev container):
1. `layout_check.cpp` (built into the standalone when `REB_MINWIN=ON`): `static_assert`s the size of all 40 structs, 312 field offsets and 241 constant values (and, on Windows, their types)
   listed in `layout_expected.inc`, which `tools/minwin_layout_gen.py` measures on the real SDK (the script also reports a stale table). Sizes/offsets are checked on i686 only; values also compile for x86-64 Linux.
   It caught: pack(1) of `WAVEFORMATEX`, and `CW_USEDEFAULT` (`-2147483648` is a `long long`, the SDK's is an `int`).
2. `tools/minwin_compare.py`: the two clang ASTs (SDK vs minwin) agree for every function (mangled name, parameter types, dllimport, noreturn), typedef, record (members in order) and COM
   interface (methods in vtable order): 214 functions, 181 typedefs, 55 records, 0 differences. It caught `ExitThread`'s `noreturn` (visible only in the generated code).
3. `tools/minwin_syntax.sh i686`: all 459 TUs of `src/game` compile against minwin.
4. `tools/minwin_objdiff.sh`: the 462 game objects built with and without `REB_MINWIN` have identical code, data and relocation targets (only debug info differs).

**Exit test** (standalone, `REB_MINWIN=ON`, real import libraries): `tools/pe_compare.py` against the SDK build: `.text .rdata .data .gfids .tls .rsrc .reloc` byte-identical (timestamp
and debug directory ignored); `tools/standalone_check.sh`: `STANDALONE OK: smoke clean, saves byte-identical to the original's`; `tools/sim_goldens.sh check`: battle1, alliance1, empire1 identical
to the goldens; smoke and load_game screenshots: `smoke_menu`, `load_galaxy`, `load_list` byte-identical to the SDK build's (the later `load_run_*` frames differ between *any* two runs, ON vs ON included: real-time
animation). Hybrid: `tools/build_hybrid.sh` unchanged (10,706 hooks); unit difftest over the shared regression store (12,864 captures): 12,318 PASS, 540 SKIP (not emulatable), 0 FAIL.

**LP64 syntax pass** (`tools/minwin_syntax.sh lp64`: `clang++ --target=x86_64-linux-gnu -std=c++20 -fsyntax-only -fms-extensions -fno-exceptions -fno-rtti -fcheck-new -fwrapv -fno-strict-aliasing -fsigned-char -DREB_MINWIN=1`):
454 of 459 TUs compile, none of the 5 failing is about Win32/DirectX/minwin: `_stricmp`/`_strnicmp` undeclared (`ui/cool_app.cpp`, `ui/cool_drag_list.cpp`, `core/game_string.cpp`) and the MSVC CRT headers
`<direct.h>` (`app/rebellion_application.cpp`) and `<io.h>` (`iostream/filebuf.cpp`). With `-Wpointer-to-int-cast -Wint-to-pointer-cast -Wshorten-64-to-32 -Wint-conversion -Wformat`: 3 `long`->`int` narrowings
(`ftell` in `app_cdcheck_version.cpp`, the iostream library's `long` and `atoi` forwarding to `atol`) and 5 `-Wformat-security` notes (`sprintf(buf, text)`), no pointer truncation. Pointer-sized Win32 types
(`WPARAM`, `LPARAM`, `LRESULT`, `HANDLE`, `GetWindowLongPtr`) are 64-bit there as intended.

## P1 results: window manager half (user32 + winmm timers on SDL3; measured Oct 9 2026)

Design, build and deviations: [docs/minwin_user.md](minwin_user.md). `REB_MINWIN_IMPL=ON tools/build_standalone.sh` links `src/port/minwin_impl/user` (+ `gdi_stub`, a throwaway
stand-in until the GDI component lands) and SDL3 3.4.18 (`tools/fetch_deps.sh`, sha256 pinned): `rebellion.exe` then imports KERNEL32, ADVAPI32, WINMM, VERSION, DDRAW, DSOUND, AVIFIL32, SMACKW32, DPLAYX and SDL3 -- no
USER32/GDI32/COMCTL32. The scenarios' xdotool input reaches the SDL window under Wine (borderless, at the origin of the Wine desktop). The game boots through the intro movies to the main menu, loads a game and runs
the whole `ui_tour` (all 19 screenshots taken; pixels are the stub's, text is missing). Default builds are unchanged: hybrid 10,706 hooks, `standalone_check` OK, `sim_goldens check` identical.

**Message-sequence comparison** (`REB_MSG_TRACE`; real-user32 standalone with `REB_MSG_TRACE_SHIM=ON` vs minwin_impl, `tools/msg_trace_compare.py`; the `*_x` scenarios are the stock ones plus extra Escape presses
in the first 22 s, because the intro movies do not play yet (real SMACKW32 cannot use our window handles)). Lines identical and in order, after dropping the noise kinds (WM_PAINT/ERASEBKGND/NCPAINT/TIMER counts, Wine's
IME/GETICON/STYLECHANGING, 0x310, WM_NOTIFY): `smoke` 93% (204 of 219), `load_game` 58%, `ui_tour` 16% (one shifted hunk keeps the rest misaligned: after the first 5 s the scenarios drift apart because of timing, so the
tail comparison is mostly of different game states). Every divergence found in `smoke` and its cause:

| Divergence | Cause |
|---|---|
| 9 extra messages on the main window after `WM_ACTIVATEAPP` (`WM_WINDOWPOSCHANGING/CHANGED` x3, `WM_NCCALCSIZE`, `WM_DISPLAYCHANGE`) | the real ddraw `SetCooperativeLevel`/`SetDisplayMode` resizes the real window under Wine; minwin has no ddraw yet |
| `WM_KEYUP`, `WM_CHAR`, `WM_MOUSEMOVE` order around the movie windows; an extra `WM_NULL` | the intro movie ends at a different time (the real Smack thread cannot draw into our handles) and the SDL window reports an initial pointer position |
| `WM_SHOWWINDOW` of `CoolStrobeButton` #7-#9 in a different order | `CoolWindow::m_linkedWindows` is an AVL tree keyed by HWND value; Wine's handles and our pointers order differently (a pointer-ordered container in game code) |
| 3 trailing key messages | scenario timing |

Fixed on the way (all found by the comparison): `WM_NOTIFYFORMAT` comes from the tooltip's `TTM_ADDTOOLA`, not from window creation; `WM_CHILDACTIVATE` after `WM_NCCALCSIZE`; children of hidden parents change only their visibility flag; destroying
an invisible-by-parent child sends only `WM_SHOWWINDOW`; focus may be set to hidden windows (Wine/Windows only refuse disabled ones); `WM_CANCELMODE` before disabling; top-levels activate before `WM_WINDOWPOSCHANGED`;
`WM_PALETTEISCHANGING` after the realizing handler returns; synthesized `WM_MOUSEMOVE` when a window appears under the pointer; cross-thread `SendMessage` (the movie thread) marshalled to the owner thread.


## P1 component libraries (`src/port/minwin_impl/`, measured Oct 9 2026)

Six self-contained libraries, each with unit tests that run natively (x86-64 clang) and as i686-windows under Wine, no SDL: `config`, `res`, `gdi`, `text`, `dsound`, `smack`.
`src/port/minwin_impl/run_tests.sh native|i686 [target]` builds and runs them in the dev container (`CMakeLists.txt` there is a stand-alone project; the libraries are `mw_config mw_res mw_gdi mw_text mw_dsound mw_smack`).
Sources of third-party code: `tools/fetch_deps.sh` (FreeType 2.13.2 — Wine's version, chosen so glyph bitmaps match Wine; Liberation fonts 2.1.5; SDL 3.4.18 source and VC zip; sha256-pinned, into gitignored `third_party/`). Game data is read in place (`MW_INSTALL_DIR` for the tests that need it).
The Win32 names are defined with `MINWIN_IMPLEMENTATION` (plain `extern "C"` stdcall, declared by `port/minwin/*.h`); the window manager's seam is `gdi_api.h` (window DCs, `Screen()`, `CurrentPalette()`).
Wiring left to the integrator: `mwds::SetEventSignaler(SetEvent)` (DirectSound notifications), `mwsmk::SetDeviceContextProvider(GetDC, ReleaseDC)` (SmackBufferSetPalette), `minwin_user::NotifyPaletteChanged` (called by RealizePalette), and pulling `mwds::Mixer::Global().Render()` from the audio backend.

| Library | Defines (names from `tools/minwin_inventory.txt`) | Coverage |
|---|---|---|
| `config` | advapi32 `RegOpenKeyExA RegQueryValueExA RegSetValueExA RegCloseKey GetUserNameA` | 5/5; INI file (`install_dir`, `data_dir`, `font_path`, `window_scale`, `[registry]` values; location per OS, `REBELLION_CONFIG`) |
| `res` | kernel32 `LoadLibraryA FreeLibrary GetModuleHandleA FindResourceA LoadResource LockResource FreeResource SizeofResource`; user32 `LoadStringA LoadCursorA LoadIconA LoadAcceleratorsA`; version `GetFileVersionInfoSizeA GetFileVersionInfoA VerQueryValueA`, `VerLanguageNameA`; case-insensitive path translation | all used; `LoadBitmap`/`LoadImage`/menus/dialogs are not used by the game (its `LoadBitmap` is its own function) and not implemented |
| `gdi` | all 40 gdi32 functions of the inventory + user32 `FillRect` | 8-bit surfaces: window/memory/info DCs, DIB sections, DDBs, pens/brushes, palettes with the 20 static colours, identity-palette realization, `SetSystemPaletteUse`, all 256 ROP3s, `R2_*`, clip lists. Not implemented (not in the inventory): StretchBlt/StretchDIBits, GetPixel/SetPixel, AnimatePalette, regions, pens wider than 1, hatch/pattern brushes, dithering of RGB brushes |
| `text` | gdi32 `CreateFontIndirectA GetTextExtentPoint32A GetTextMetricsA TextOutA`, user32 `DrawTextA` | flags used by the game: `DT_LEFT/CENTER/RIGHT/VCENTER/BOTTOM/SINGLELINE/WORDBREAK/CALCRECT/NOCLIP/NOPREFIX/EDITCONTROL` (+ `DT_EXPANDTABS`, `DT_EXTERNALLEADING`) |
| `dsound` | `DirectSoundCreate`, `IDirectSound`, `IDirectSoundBuffer` (all methods), `IDirectSoundNotify`, `IDirectSound3DListener` (accepts parameters, nothing is spatialised), `IID_*` | circular `Lock`/`Unlock`, cursors, notifications (positions and stop), volume/pan/frequency, primary buffer rules, `DuplicateSoundBuffer`; `mwds::Stream` for push audio |
| `smack` | the 15 `Smack*` functions of the inventory + `SmackGetTrackData` | picture, palette, audio, rectangles, `Smack`/`SmackBuf` fields |

**Verification against Wine / the real DLL** (`gdi/tests/compare_wine.sh`, `text/tests/run_wine_text.sh`, `smack/tests/run_wine_smack.sh`, `res/tests/compare_pefile.sh`):

- `res`: for all 18 EXE/DLLs of the install, every resource (type, name, language, size, FNV hash of the data) is identical to pefile's (17,500 resources); synthetic PEs and the real TEXTCOMM/TEXTSTRA/REBEXE.EXE resources tested.
- `gdi`: 14 operation sequences on 8-bit DIB sections (colour resolution incl. `PALETTERGB`/`PALETTEINDEX`, every ROP of `BitBlt`/`PatBlt`, blits between different colour tables and overlapping blits, 400 random + 100 tiled lines, solid/XOR/dotted pens, `Rectangle` with pen/brush/null pen/XOR rubber band, `SetDIBitsToDevice` with scan windows and a foreign colour table) give **bit-identical surfaces** under the real gdi32 (Wine), the i686 build under Wine and the x86-64 build. Findings that are Wine-specific are copied (tie-breaking of the line rasteriser, `Rectangle` extents, negative `PatBlt`/`FillRect` extents mirror around the inclusive start, `SetDIBitsToDevice` return value).
- `text`: 365 strings from the game's string tables/encyclopedia and its 11 distinct Arial fonts (Liberation Sans stands in for Arial in Wine too): text metrics 11/11, `GetTextExtentPoint32` 4,015/4,015, prefix widths 44/44, `DrawText` `CALCRECT` with word break and edit control 99/99 and single-line 88/88 are identical; fixed-width word-wrap scans identical except runs of two or more spaces at the break (22 of 24 scans identical, 2 differ in the reported width). Glyph bitmaps: identical for 10 of 11 fonts with FreeType 2.13.2 (the version in Wine's container); the 8 px font differs in 141 of 161,280 pixels (the stem of some capitals is hinted one pixel to the left); with 2.13.3 (the first pin, since replaced by 2.13.2) the 24 and 30 px fonts also differed in 39 and 95 pixels (hinting changed between the releases). Text is rendered monochrome, as Wine does on a palettised display.
- `smack`: the decoder (written from the format description; the only reference for bit layouts was the DLL's behaviour) reproduces **every frame of all 15 shipped movies** (`MDATA.000-202`, 19,000+ frames): picture, palette, the decoded PCM of the frame (`SmackGetTrackData`: 11,025 Hz 16-bit stereo, Huffman-DPCM), changed rectangles, and the observable `Smack`/`SmackBuf` state (`m_newPalette` is announced before the frame is decoded, `SmackGoto` is 1-based and announces a palette only if it changed, `SmackBuf` is bottom-up with `m_reversed = 1`), for both the i686 build under Wine and x86-64.
- `dsound`: no reference implementation was available; 15 tests pin the documented semantics (return codes, control flags, circular regions, notification timing, the game's half-buffer music refill pattern is gapless).

**Known deviations.** GDI: dotted/dashed pens on diagonal lines have a different dash phase from Wine (axis-aligned lines are identical, the only dotted lines the game draws). Text: the "System" stock font is drawn with Liberation Sans, `DT_END_ELLIPSIS`-style flags and `DT_EXPANDTABS` tab stops (a guess: 8 average widths) are untested. Smack: SMK4 is rejected (all shipped movies are SMK2); `SmackBufferSetPalette` realizes the movie palette as 256 `PC_NOCOLLAPSE` entries instead of RAD's remapping; blits stretch by integer zoom factors only. DirectSound: the mixer's write cursor leads by 15 ms and the output rate is the backend's (default 44,100 Hz), with linear resampling.


## Long-run fidelity against the original (measured Oct 9 2026)

`tools/long_compare.sh [-s alliance|battle] [-t TICK] [-n RUNS] [-k SECONDS]` (slow, on demand, not part of `integrate.sh`) runs one UI-driven
scenario on the original logic (hybrid with `REB_DISABLE_HOOKS=all`), the hybrid with all hooks and the standalone, all runs concurrently
(one container each), and `tools/long_compare_report.py` compares saves byte for byte and screenshots pixel for pixel. Scenarios:
`long_alliance` (Alliance game "Rebels", tick 95,075, the Imperial AI plays, no battle; default target tick 98,575 = 3,500 ticks) and
`long_battle` (Empire game "JAS1", tick 150,326, an Alliance fleet arrives at Clak'dor VII around tick 152,576, the script chooses Simulate
Results; default target 154,500 = 4,200 ticks, one auto-resolved battle plus AI activity of both sides).

**Making the save tick deterministic.** The shims (`src/hybrid/determinism.cpp`, mirrored in `src/standalone/determinism.cpp`) gained
`REB_QPC_FAST_UNTIL_TICK=<n>` (the virtual QPC steps 1 ms per call until the game manager's tick counter reaches n, so ticks are cheap, then
`REB_QPC_STEP_NS`), `REB_QPC_START_KEY=<vk>` (the counter stands still from the moment a game exists until the key is seen down, so idle
frames while the script sets the speed do not move game time), `REB_QPC_HOLD_UNTIL_KEY=<vk>` (after tick n the counter stands still again
until the key is pressed a second time, immediately before the script opens the Saved Games screen: the ticks the save needs, ~2, are
counted from the key, not from wall time), `REB_CLOCK_FROM_QPC=1` (timeGetTime/GetTickCount read the virtual counter instead of counting
calls) and, in the hybrid, `REB_ENABLE_HOOKS=<hex list>` (only these hooks are installed; the inverse of `REB_DISABLE_HOOKS`, which cannot
express "all but a few" within the environment size). The scripts press Scroll Lock (VK 145), which the game ignores. Both gates poll
`GetAsyncKeyState`; the tick counter is read from `g_gameManager+0xc`. The tick at the save is exact in every run (header word 0x6a:
154,502 in all 9 battle runs; 96,302 in all alliance runs). The scenarios also turn the music off in the Saved Games screen first and
`REB_NO_TIMERS=1` stops the multimedia timers. Screenshots are only taken while the clock is held (a picture taken while the game runs
depends on the moment), and are identical within a trajectory (below).

**The original does not reproduce its own long runs.** Even `orig` x `orig` differs after a few hundred ticks. Cause, found with
`tools/trace_divergence.patch` (temporary trace points in game code; apply, build, run twice, compare; the first sim-visible difference is
the first differing `RUN` event): the AI breaks ties with `g_sessionRandom` (`AiRankedList::IsLess` -> `SessionRandomNext`), the same
generator the droid advisor draws from (`AllianceAdvisor/EmpireAdvisor::ProcessIdle`, `AdvisorBase::PickAdvice`, called by
`HumanPlayer::NextAdvice` from the music-end callback and by the cue machinery after droid animation steps). When an advisor draw lands one
draw earlier or later relative to the AI's draws, the AI ranks sectors/systems differently (first visible: an `AdvisorTask20` destination
order, event kind 0x214, for a different manager key), and the simulations drift: sizes of the saves differ by kilobytes and the
game-manager serial counters (`GameHookNextSerialA/B`, header words at 0x72/0x76) differ. Advisor idle draws are driven by UI animation and
music, which run on wall-clock/async schedules. Further sources removed on the way: multimedia timers (`REB_NO_TIMERS`), the per-call
clock (`REB_CLOCK_FROM_QPC`), idle frames before the start (start key), music (scenario).

**Results (Oct 9, concurrent runs on one machine).**

| Scenario / config | runs | outcome |
|---|---|---|
| alliance, 3,500 ticks, first shims | orig 2, hybrid 2, standalone 2 | all six saves differ (sizes 1,262,052-1,282,422); same-config pairs too |
| alliance, 3,500 ticks, + start key, no timers, clock from QPC | 3 x 3 | all nine differ in size; orig_1/orig_2 within 9,960 bytes |
| alliance, 1,225 ticks (`-t 96300`), no music switch | orig 4, hybrid 4, standalone 4 | modal trajectory (1,252,262 bytes) reached by orig 4/4, hybrid 2/4, standalone 1/4; all seven runs on it agree within 45-57 bytes of UI state (16-bit message stamps, one 5 KB region); three more trajectories |
| same, music off, final scenario | orig 3, hybrid 4, standalone 4 | hybrid 4/4 on the modal trajectory (<= 52 bytes apart), orig 1/3 on it (the other two on trajectories standalone also reaches), standalone on 4 distinct trajectories (2 shared with orig); screenshots identical across the modal runs |
| battle, 4,200 ticks (with and without music) | 3 + 3 + 3 | all runs differ in size, but **orig 3/3 and every hybrid/standalone run (6/6, 5/5) form two stable groups**: header serials 0x142e5/0x1ea8c (original code) vs 0x142dd/0x1ea88 (hybrid and standalone), same tick 154,502 |

So on a trajectory the three configurations agree (alliance), and the standalone and hybrid always agree with each other. The battle scenario shows
a *systematic* difference between the original code and the reconstructions, appearing at the daily update between ticks 153,762 and 153,822
(bisected by save tick; `tools/bisect_save.py`'s first round was ambiguous: no single chunk of 1,785 hooks restores the original, and
module-level `REB_ENABLE_HOOKS` subsets of core modules stall the game). The first difference of the event/random trace of the original code
(`REB_ENABLE_HOOKS` of the trace functions only) against the hybrid is not a game-logic result but timing: the Empire droid advisor's idle draw
after event 513 at tick 150,797 happens 172 ticks later in the original code (tick 150,969) and 221 ticks later in the hybrid (151,018), a
constant offset that then shifts every later advisor draw against the AI's draws (AI target choice 0x214 differs at tick 151,380). Hypothesis:
droid animation progress per game tick depends on wall-clock rate (paint/idle cadence), and the original runs fewer ticks per second than the
reconstruction, so its animations advance further per tick; this would explain a stable orig-vs-reconstruction split with no logic difference.
Not tested: pinning animation to virtual time (the standalone/hybrid would need to suppress or virtualise WM_PAINT/SpriteStage cadence).
No reconstruction bug was identified, and none is excluded.



## P1 integration results (measured Oct 9 2026)

`REB_MINWIN=ON REB_STANDALONE_DIR=build/impl REB_CMAKE_ARGS=-DREB_MINWIN_IMPL=ON tools/build_standalone.sh` (needs `tools/fetch_deps.sh`; the incremental build is `cmake --build build/impl` in the dev container).
**Imports: KERNEL32 (120 functions) and SDL3 (37) only**; no USER32/GDI32/COMCTL32/ADVAPI32/VERSION/WINMM/DDRAW/DSOUND/SMACKW32/AVIFIL32/DPLAYX.
Wiring: `gen_minwin_imports.py` defines the `__imp__` slots of every implemented function (also the Smack and DirectPlay ones); `glue/wiring.cpp` connects dsound->SetEvent, smack->GetDC/ReleaseDC and the SDL audio backend;
the data image's own `IID_IDirectDraw2/IDirectSoundNotify/IDirectSound3DListener` are used (`MW_IIDS_IN_GAME_DATA`); the C++ library is the static libcpmt (the game objects only need `_ALLOW_RUNTIME_LIBRARY_MISMATCH`).
DirectDraw: `ddraw/ddraw_api.cpp` (IDirectDraw/IDirectDraw2, exclusive mode covers the display with the same window-position messages and `WM_DISPLAYCHANGE` as Wine's ddraw); MCI, AVIFile and DirectPlay answer "unavailable" (no providers);
`timeGetTime`/`timeSetEvent` are the determinism shims', backed by `user/mmtimer.cpp`.
HWNDs are small integers allocated like Wine's server does (slot = 0x20 + 2*index, generation in the high word, LIFO reuse), see window.cpp.
Finding: Wine's handle order is *not* creation order (recycled slots carry a higher generation and sort after fresh ones; the pre-history of the wineserver shifts the slot numbers by a run-dependent constant), so exact equality with Wine's values is impossible; the model reproduces the pattern (fresh before recycled).

Scenarios under Wine with `REB_MINWIN_IMPL` (no "Unhandled" in any): smoke, ui_tour (20 shots), load_game, new_game_rebel, space_battle_autoresolve, video_intro (movies play with sound). Screenshot comparison against the real-Win32 standalone, `REB_DETERMINISTIC=1`, Xvfb depth 8 and Liberation fonts for both
(`work/s/bin`-style tooling: per-pixel differences): the text areas are pixel-identical (0 of 2,750 px in the main menu label; with a 24-bit display Wine anti-aliases text and differs, a Wine artefact the original 8-bit mode does not have). What remains is animation and timing:
smoke 2.6k px (animated cockpit icons), load_game 2.2-2.3k px (blinking map markers, a hover label), ui_tour 250-1,700 px for 15 of 20 shots (plus one shot of a double-click, fixed by our own double-click detection: Windows' 500 ms / 4x4 rule, not SDL's count),
`tour_04`/`tour_17`: the Game Controls screen appears about two seconds later than under Wine (creating it is slower in the impl build; not profiled).
Message traces (smoke): 91% of lines identical and in order; open divergences: Wine broadcasts `WM_PALETTECHANGED` on every palette-changing `RealizePalette` (movie windows) while ours does so only after the first (the gdi component reports "unchanged" for the movie palette, cause not found),
one extra `WM_WINDOWPOSCHANGING` after `WM_DISPLAYCHANGE`, movie/key timing.
Audio: `REB_AUDIO_CAPTURE` of video_intro: non-silent, peak 32765; the menu music (MDATA.300) in the capture correlates 1.000 with the file resampled 11,025 -> 44,100 Hz (exact). No capture of Wine's own DirectSound output was possible in the container.
Save fidelity: `REB_STANDALONE_DIR=build/impl tools/standalone_check.sh --no-build`: STANDALONE OK, saves differ from the original-logic run in 64 bytes (the tick/checksum tolerance of that script; the default build is byte-identical). Default builds: `standalone_check` OK (byte-identical), `sim_goldens check` identical (battle1, alliance1, empire1).


**Follow-up (Oct 9, later): the split was sound timing plus two reconstruction bugs.**

- *Clock calls are not the cause.* `REB_CLOCK_TRACE=<n>` (n >= 2) logs the virtual-clock calls per caller every n ticks (callers in the
  DLL are reported as the original address of their hook) and the first 4,000 calls of every caller after the start key. Per 250
  ticks the original code and the hybrid make exactly the same calls: 251 `Stopwatch::ElapsedSeconds` + 1 `Stopwatch::Reset` QPC calls
  per tick, and identical `SpriteStage::Tick` timeGetTime counts (= main-loop iterations). A per-iteration clock mode is therefore not
  needed and was not built.
- *Sound was the wall-clock leak.* The droids' talking steps (`AnimationStep04/13`, `advanceDeltaFrames`) end when
  `IDirectSoundBuffer::GetStatus` stops reporting PLAYING, and DirectSound plays in wall-clock time (the container's ALSA null device),
  so a talking step lasted 36-43 ticks depending on how fast the build ran; every later advisor draw moved with it. `REB_VIRTUAL_SOUND=1`
  (`src/port/virtual_sound.h`, both shims, on in `long_compare.sh`) patches the driver's DirectSound/buffer vtables so the playing
  state follows the virtual counter (a one-shot buffer plays (bytes - position) / nAvgBytesPerSec, a looping one until Stop). Failing
  DirectSoundCreate instead is not an option: `SoundManager::SetPrimaryBufferFormat` dereferences the device unchecked (original bug).
  With it, every configuration reproduces itself byte for byte (3/3 orig, 3/3 hybrid, 3/3 standalone, both scenarios).
- *Logic bugs, found with `tools/bisect_save.py --enable-only`* (install only one chunk of hooks per run; with the plain
  disable-one-chunk mode no chunk restored the original because reconstructed callers call DLL functions directly, so several chunks
  each reached the same buggy function): `RulesMgr::ResumeTrip` passed BeginTravel's from/to keys swapped (battle);
  `AiSystemPlanner::MoveTeams` did not clear its in-place flag for travelling or unavailable members, and the shared
  `aitask::AddTargetSystem` picked sectors from the wrong selector (every original copy takes the *next* sector from the task's
  just-emptied selector, AiTask3's also the first one; BUG(original)) (alliance). To find the call:
  bisect the save tick with `REB_ENABLE_HOOKS=<culprit>` vs the original, capture the culprit with a skip that lands in that window,
  and look for the failing capture with difftest. A mixed run also exposed `IntPair::Set` (0x51c900, really the constructor) not
  returning `this`.

| Scenario / config (REB_VIRTUAL_SOUND) | runs | outcome |
|---|---|---|
| battle, before the fixes | 3 x 3 | each configuration byte-identical across its runs; orig 1,366,554 vs hybrid/standalone 1,367,450 bytes |
| alliance, before the fixes | 3 x 3 | each configuration byte-identical across its runs; orig 1,275,468 vs hybrid/standalone 1,272,066 bytes |
| alliance, MoveTeams + AddTargetSystem fixed | 3 x 3 | **one trajectory, all nine runs**; hybrid byte-identical to the original; standalone ~55 bytes apart (zero-filled `new`); all screenshots identical |
| battle, ResumeTrip fixed | 3 x 3 | **one trajectory, all nine runs** (1,366,554 bytes); orig vs hybrid/standalone differ in 9-11 bytes of uninitialised heap memory near the end of the save (the standalone writes zeros there: zero-filled `new`); screenshots identical except the standalone's battle popup (2,884 px). Rerun after all fixes (2 x 3): same |

Remaining: the 9-11 bytes near the end of the battle save are uninitialised memory that the game serialises (orig, hybrid and
standalone each write different garbage there; not traced to its record yet). Under heavy machine load (load average ~28) the
wall-clock-scripted clicks can miss the main menu and a run writes no save; rerun with fewer runs.



## P1 pixel and message-trace comparison against the real-Win32 standalone (measured Oct 9 2026)

Tools: `tools/pixel_run.sh` (scenarios on `build/standalone` and `build/impl`, 8-bit Xvfb, Liberation fonts, `tools/pixel_compare.py` per scenario, diff images
reference | impl | differences), `tools/msg_trace_run.sh` (the message-trace comparison; the reference needs `REB_MSG_TRACE_SHIM=ON`, see the script), `REB_MINWIN_DUMP=Z:/out`
(the impl writes its 8-bit frame and palette when the scenario's `shot` asks: `<shot>.idx`), `REB_CLOCK_RATE_LOG=<file>` (clock calls per real second and the game tick, both builds).
Method notes:

* **Palette-index space.** The 8-bit X screenshot of Wine's own display and the one of the SDL window are both lossy and *different* (an entry that is not one of the 20 static colours
  is matched by Wine's palette code per entry, by the SDL window's blit per pixel: index 195 `(79,80,161)` is blue in the reference and black in the impl, index 246 `(255,251,240)` black in
  the impl and white in the reference). `pixel_compare.py` therefore checks, with the impl's dump, that every impl palette index maps to **one** reference colour (the most common one)
  and counts the reference pixels that deviate: those are real differences in what was drawn. The "rgb px" column is the plain screenshot difference.
* **Clock.** Under `REB_DETERMINISTIC=1` the game's clock advances per call, so the animation phase and, in `new_game_rebel`, the whole intro trajectory depend on the loop's speed (the
  intro was at different stages in the two builds: 52k px). The comparison therefore runs with real clocks; the run-to-run floor of the *reference itself* is then the measure of what is
  timing (`ref vs ref` below). A frozen clock (`REB_PIXEL_CLOCK=frozen`: the virtual clock stands still from the moment a game exists) makes every loaded-game frame reproducible; it shows
  one constant 103 px difference (the droid advisor sprite: each build froze at a different droid frame) and the residuals below, but it makes the double-clicks of tour_03/tour_04 flaky, so the table uses real clocks.
* One scenario at a time: ten concurrent games slow the movies enough that the scripted Escape presses land on other movies (new_game_rebel/battle then differ by 50-60k px).

| scenario | shots | before (virtual clock, rgb px) | after (real clock, rgb px / palette-index px) | reference vs reference (rgb px, two runs) |
|---|---|---|---|---|
| smoke | 1 | 1,975-3,024 | 874 / 713 | 773 |
| ui_tour | 20 | 275-14,352 (tour_04: a stray sector window), tour_17 99k (Game Controls late) | 254-3,186 / **0 for 9 shots**, 32-34 for 5 (a 2-px strip), 53-86 for 3, 293 (tooltip), 400, 505 | 0 for 14 shots, 34 for 4, 260-292 for 2 |
| load_game | 9 | 509-2,388 | 296-2,298 / 0-628 | 0-359 (two shots 51k: a battle popup in one run) |
| new_game_rebel | 11 | 51,947-52,213 (different intro stage) | 2,037-2,606 / 306-852 | 197-846 |
| space_battle_autoresolve | 7 | 1,841-2,388 (sequential; 61k when concurrent) | 2,036 / 253 for the popup shot; 24-46k for the 4 shots after_20s, closed, result_a, result_b | 504-52k (the battle popup's timing differs between two runs of the reference) |

What the remaining differences are (all documented, none is a drawing bug of the impl that was found):

| Residual | Size | Cause |
|---|---|---|
| animation phase | 30-250 px per shot at fixed places (droid at (558,350), map sparkles, the day counter's digits) | the real-clock phase of the animations; the same places differ between two reference runs |
| `Version: 1.01.00 English` label | 505 px, `load_list`, `tour_17` | Wine's `VerLanguageNameA(0x409)` returns `English`; Windows and the impl return `English (United States)` (unit test `res_test`). The impl keeps Windows' answer |
| tooltip border | about 140 px per tooltip shot | Wine's 8-bit display dithers the `WS_BORDER` colour (top/right edges grey, left/bottom edges grey/white); the impl draws the same pattern with a different phase |
| ship/troop/facility bars | 12-16 px strips in the sector, system and personnel windows (53-86 px per shot) | the reference dithers a solid fill whose colour is not an exact member of Wine's X colormap; the impl draws the exact palette entry (what Windows does when the entry is in the system palette) |
| `Mission Messages` tooltip (`tour_13`) | 400 px | after a click the impl restarts the tooltip's show timer, as the real control did for the GID and Game Controls buttons (the reference traces show `TTN_SHOW` again 500 ms after the click) but not for this one (cause not found) |
| battle timing | the 4 battle shots listed above | the popup appears at a different moment in two runs of the *reference* |

Fixes made (biggest first):

1. **Synthesized `WM_MOUSEMOVE`** (window appears/disappears/moves under a motionless pointer) only for top-level windows other than tooltips, as under Wine (child windows live inside one X window).
   The impl delivered a stray hover to the galaxy map after `alt+w`: the Fleet window screen differed by 14k px (a sector window was opened by the next click) and the hover label
   ("Dolomar Sector", "Fakir Sector") followed the wrong position.
2. **Tooltips**: `tooltips_class32` now behaves like the real control (`user/tooltip.cpp`), drawn with the same text pixels and box geometry; the stub drew nothing (2.2k px per shot with a tooltip).
3. **Palette**: `RealizePalette` reports a palette that only re-maps entries as realized (Windows' result), which makes the movie palettes broadcast `WM_PALETTECHANGED` like Wine; `SmackBufferSetPalette`
   realizes the buffer's initial (black) palette; the ddraw mode switch sends Wine's extra `WM_WINDOWPOSCHANGING` and the `WM_PALETTECHANGED` broadcast (both open items of the P1 results).
4. **The Game Controls screen "2 s later"** is an artefact of the virtual clock, not a slow window manager. `REB_CLOCK_RATE_LOG` of the whole ui_tour: both builds make about 140,000 `timeGetTime` and 70,000 `QueryPerformanceCounter`
   calls per second in the main loop, and both stop calling the clock for 2-3 s while the screen is built (ref 3 s, impl 2.3 s). With real clocks `tour_17` is the same state in both builds (only the version label differs). In the
   pre-game phase (movies, menu) the impl makes about 20x more `QueryPerformanceCounter` calls than the real Smacker (194M against 9M per ui_tour run: `smack_clock.cpp` polls), which only matters for the per-call virtual clock.
5. **The reference trace shim lost windows**: `RebTracedRegisterClassA` wrapped a class only the first time its name was registered, but the game registers every Cool class once per instance handle; about 170 CoolStrobeButton windows (and all their messages) were missing
   from the reference log, which is why `ui_tour` showed 16% identical lines.

Message-sequence comparison after the fixes (`tools/msg_trace_run.sh`, real clocks, identical lines in order after dropping the noise kinds): **smoke 98.1%** (206 of 210; was 91-93%), **ui_tour 94.6%** (5,817 of 6,147; was 16%, 17.9% with the old shim),
**load_game 96.6%** (1,059 of 1,096; was 58%). The rest: the second `WM_PALETTECHANGED` of every movie window (sent by Wine after the activation), repeated `WM_NCHITTEST`/`WM_SETCURSOR` around a mouse move, `WM_SHOWWINDOW` order of
sibling buttons, a few `WM_WINDOWPOSCHANGED` of a hidden window.

Default builds unchanged: `standalone_check` OK (byte-identical saves), `sim_goldens check` identical (3/3), the impl build's `standalone_check` OK (64-byte tick difference), `tools/native_sim.sh` 17/17 files identical to the i686 goldens,
`run_tests.sh native` and `i686` pass.


## P3 results: native Linux x86-64 (measured Oct 9 2026)

Details, findings and how to build: [docs/native_linux.md](native_linux.md). `build/native/rebellion` (clang-19, SDL3 and FreeType from `third_party/`, `src/port/minwin_impl`, POSIX kernel32/CRT in
`src/port/native`) runs the game to the main menu, loads and saves games, runs new games and the UI tour. The three simulation goldens (7,000 ticks incl. an auto-resolved battle) are **byte-identical to the
i686 goldens** (no float drift measured); ASan/UBSan are clean on those and on 15 UI scenarios after fixing 15 findings, most of them classes that mirror the 32-bit layout (two of which changed simulation results). The data image and the static-initializer
table are generated from the EXE as typed C++ definitions (`tools/gen_native_data.py`, `tools/gen_native_init.py`). P2 (i686 Linux) was skipped.

**Residual bytes, standalone differences, minwin dsound (Oct 9, later).**

- *Every residual save byte is one field.* The 55 (alliance) and 9-11 (battle) differing bytes are `UiNotice::m_unk3c`, the 16-bit field at +0x3c
  that `UiNotice::Write` serialises right after the notice's 32-bit creation tick (`UiNotice::Read/Write`, 0x4c4e80/0x4c4f30). Neither constructor
  (0x4c4ce0, 0x4c4d90) ever sets it, and the original's `operator new` hands out recycled heap memory, so the original saves leftovers of earlier
  allocations: ASCII fragments of freed strings (`ry`, `ti`, `ed`: texts of earlier messages) and pointer halves. The hybrid shares the original's
  heap and mostly reproduces the leftovers (alliance: byte-identical to the original in 6 of 7 hybrid alliance runs measured, one run differed in 65 of
  these fields, so they are not a function of the trajectory alone), the standalone's zero-filling `operator new` saves 0. Classification:
  uninitialised memory the original writes, XFAIL class; it cannot be reproduced without reproducing the original's heap, so it is not.
  `tools/long_compare_report.py` finds a notice by its serialised header (0, owner 0-4, tick, the field, unk40 0/1, unk28) and reports these bytes
  as `+Nu` instead of counting them; every other differing byte is still counted.
- *The standalone's battle popup said "The Side fleet ... Side forces" (2,884 px).* `Side`, `Character` and `ManuMgr` (and their subclasses)
  replace `ObjectBase` slot 0 (`WriteName`) with `AppendSideName` (0x52d920), `AppendCharacterName` (0x4ed060) and `AppendManuMgrName` (0x527b40);
  only the hybrid, which keeps the original vtables, ever reached them. The standalone's compiled vtables fell back to `GameObj::WriteName` (the
  record's name, "Side"; no force title for characters; no queue length for managers). The three classes now declare the override, and the popup is
  identical to the original's. The hybrid is unaffected.
- *REB_VIRTUAL_SOUND in the minwin dsound* (`minwin_impl/dsound/dsound_api.cpp`): `MinwinDirectSoundSetVirtualClock(now_us)`, called by the
  standalone shim when `REB_DETERMINISTIC` and `REB_VIRTUAL_SOUND` are set, switches a secondary buffer's `GetStatus` to the model of
  `src/port/virtual_sound.h` (one-shot: (bytes - position) / nAvgBytesPerSec of virtual time from the last Play, looping: until Stop;
  SetCurrentPosition and Stop keep the position). The mixer still renders in real time, and `GetCurrentPosition` and the notification events stay
  real, as in the Wine runs. The native port only has to call the same function with its virtual clock. On the impl build the alliance run
  reproduces itself with it (five runs: one size and one save) and not without it (a different save).
- *The impl build also needed the Smack clock.* `SmackWait`, polled by the game while a movie plays, read `QueryPerformanceCounter`, which in the
  standalone is the shim's virtual counter (one step per call): the intro movies ran the virtual counter 186 s ahead before the game existed,
  the first game tick fired at game creation, and the impl's simulation was one tick out of phase with the original from the start (first differing
  event at tick 95,080: a timer event ran before instead of after a type-queue event; found with `REB_CLOCK_TRACE` and the RUN/random trace of
  `tools/trace_divergence.patch`, whose event_queue hunk no longer applies). `smack_clock.cpp` now reads the real counter through `GetProcAddress`.
  With it the impl build's alliance save equals the original's (same size, same serial counters) apart from the same 55 uninitialised bytes.
  `REB_CLOCK_TRACE=<n>` exists in the standalone shim too (per caller, `Z:\out\clock_trace.log`, comparable with the hybrid's totals).
- *BattleInProgressWindow::OnCreate1 crashed in the impl build* (the Empire long_battle never saved): the original copies a `CoolLabel` bit for
  bit and deletes its `String` twice (original bug, invisible in the original's small-block heap, which leaves freed blocks intact). The impl's UCRT
  heap overwrites the freed String's vptr; the first delete is now skipped (`fixedText.m_text = nullptr`), the sharing is kept.
- `sim_goldens check` reports battle1 as different, with and without this branch's `src/game` changes (identical hashes built without them): the
  golden predates the `RulesMgr::ResumeTrip` fix and needs re-recording (`sim_goldens.sh record`).
- Not fixed: the impl build's screenshots differ from the Wine runs' (its own GDI text and palette, 7k-60k px per shot), by design of the impl.

| Scenario / config (Oct 9, after these fixes) | runs | outcome |
|---|---|---|
| alliance, default builds | 3 x 3 | one trajectory; hybrid = original (byte-identical in 2 of 3, +65u in one), standalone +55u; all screenshots identical |
| battle, default builds | 3 x 3 | one trajectory (1,366,554 bytes); hybrid +11u, standalone +9u/+10u against the original; **all screenshots identical, the popup too** |
| alliance, minwin_impl standalone (`REB_STANDALONE_DIR=build/impl`) | 3 + 3 (orig) | one trajectory, standalone +55u; one run lost its save under load; Wine-vs-impl screenshots differ (impl's own GDI) |
| battle, minwin_impl standalone | 3 + 3 (orig) | one trajectory (1,366,554 bytes), standalone +10u in all three runs |

