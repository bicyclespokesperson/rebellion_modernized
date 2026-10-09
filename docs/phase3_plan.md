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
