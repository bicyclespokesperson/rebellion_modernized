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
| P3 | Linux x86-64 + soft x87 | goldens identical to Wine i686 (or every diff explained by first-divergence float tracing); ASan/UBSan clean; original 1998 saves load and re-save byte-identical |
| P4 | macOS arm64 + x86-64 | goldens identical across architectures |
| P5 | Multiplayer over TCP/ENet behind CommMgr | two native instances finish a game (not DirectPlay-compatible) |

Every milestone leaves the Wine standalone byte-identical. Screenshots compare minwin's 8-bit framebuffer + palette.

## Decisions (proposed defaults, pending the user)

1. Bit-exact 64-bit simulation (soft x87 + goldens) vs accepting float drift.
2. Original save compatibility on 64-bit: yes (cheap; no saved field may be widened).
3. Libraries: SDL3 (zlib), FreeType (FTL), own Smacker decoder and own soft x87 (avoid LGPL).
4. Font: the user's Arial when present, Liberation Sans otherwise.
5. Game data read in place via a config file pointing at the install; multiplayer later over TCP.

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
