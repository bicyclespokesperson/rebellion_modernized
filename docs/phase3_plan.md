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
