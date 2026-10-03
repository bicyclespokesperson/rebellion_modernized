# Architecture

Goal: a source-level reconstruction of *Star Wars: Rebellion* (1998, Coolhand Interactive / LucasArts) that
runs natively on modern Linux and macOS. The game logic is decompiled faithfully from `REBEXE.EXE`; the
platform layer (Win32 windowing, DirectDraw, DirectSound, Smacker video, DirectPlay) is replaced.

## The binary

| | |
|---|---|
| Code | All game code is in `REBEXE.EXE` (MSVC 5.0, linker 5.10, `/GX`, no RTTI on game classes, identical-function folding on). Image base 0x400000, no relocations. |
| Game functions | ~10,665 at 0x401000–0x615345, plus a few compiler-generated helpers at 0x628e10–0x62908f |
| Library code | 0x6153b0–0x628e0f: old iostream library (LIBCI) then LIBCMT + EH runtime — see [library_code.md](library_code.md) |
| Not code | ~11,350 `Unwind@` EH funclets (0x629090+) — compiler-generated, never decompiled |
| Other DLLs | Resource-only (bitmaps, waves, strings, dialogs) except `SMACKW32.DLL` (RAD Smacker) and `d3drm.dll` (Microsoft Direct3D Retained Mode) |

## Approach: hybrid first, then standalone

**Phase 1 — Hybrid (current).** A proxy `SMACKW32.DLL` (REBEXE imports it statically) is loaded into the
original game running under Wine. It forwards the Smacker exports to the real DLL (`smackorig.dll`) and on
load patches every reimplemented function in `REBEXE.EXE` with a `jmp` to the reconstruction. The game is
playable at every step, so each function is verified in its real context.

Link-time glue (`tools/gen_link_symbols.py`) keeps the source free of addresses:

- Reconstructed code references original functions and globals by name. Names come from
  `symbols/symbols.csv`; Ghidra default names (`FUN_00401000`, `DAT_006be668`) resolve by pattern. Any such
  symbol that is *referenced but not defined* becomes an absolute COFF symbol at the original address, so
  calls go straight into `REBEXE.EXE`.
- Any symbol that is *defined* and names an original function gets a hook-table entry.
- Overloads are told apart by the `signature` column (e.g. `String::String` + `(char const *)`).
- `tools/patch_vtables.py` rewrites reconstructed constructors/destructors to store the original vtable
  addresses (`Class::vftable` rows), so objects are indistinguishable from ones the original code builds.

The DLL links no C runtime: reconstructed code calls the game's own statically linked CRT (heap, `rand`
state, FILE handles are shared with the original). The small runtime in `src/hybrid` uses only kernel32 and
must not call into the game (it runs in `DllMain`, before the game's CRT initializes).

Compiler settings mirror the original's ABI: clang-cl targeting i686-pc-windows-msvc (MSVC C++ ABI:
thiscall, vtable layout), `/arch:IA32` (x87 floating point like the original), no RTTI, no exceptions
(the game never throws; `operator new` returns NULL), non-sized `operator delete`.

**Phase 2 — Standalone.** When every game function is reconstructed, the same sources link into an
executable without `REBEXE.EXE`: globals get definitions, the CRT becomes the host libc (with an
MSVC-compatible `rand` for fidelity), and Windows APIs are behind a platform layer.

**Phase 3 — Port.** Replace the platform layer with SDL (window/input/audio), a software 8-bit blitter,
a Smacker decoder (libsmacker/ffmpeg), and 64-bit-clean data structures (explicit serialization for save
files and `.DAT` loading, no layout punning). Targets: Linux x86-64, macOS arm64/x86-64.

## Scope decision: 3D tactical combat

The 3D tactical space battle mode (Direct3D Retained Mode) is **out of scope** for faithful decompilation
(user decision). Its code is identified and fenced off (see [code_map.md](code_map.md)); the port resolves
battles with the game's own auto-resolve path, and a fresh tactical mode may be written later on top of the
same battle data. This also removes the hardest porting dependency (D3DRM).

## Verification

1. **Differential testing** (`tools/difftest.py`). The hybrid DLL can capture real calls
   (`REB_CAPTURE=addr[:count[:skip]],...`): CPU state at entry plus a snapshot of all process memory. The
   tester replays each capture in Unicorn twice — original machine code vs. reconstruction — and compares
   return value, ESP (calling convention), callee-saved registers, x87 stack, every byte written outside the
   callee's frame, and the sequence of Win32 calls (stubbed: they return 0). Win32/COM calls are stubbed, so
   functions whose behaviour depends on what DirectX returns are covered by smoke tests instead.
2. **Smoke/scenario tests** (`tools/run_game.sh tests/scenarios/*.txt`): scripted input + screenshots under
   Wine/Xvfb. `REB_DISABLE_HOOKS=all` or a list of addresses bisects a regression to one function.

## Repository layout

```
src/game/        reconstructed game code (portable target; no addresses, no hybrid-only code)
src/hybrid/      hybrid runtime: DllMain, hook installer, capture, proxy .def
symbols/         symbols.csv (names/addresses — source of truth), imports.csv, library_functions.csv, modules.csv
tools/           build/run/test tooling; gq = Ghidra query client
docker/          dev image: Wine, Xvfb, clang-19/lld, MSVC CRT/SDK headers via xwin
tests/scenarios/ input scripts for run_game.sh
docs/            this file, library_code.md, code_map.md
```

Untracked, in the main checkout only: `orig_game_files/` (the user's licensed copy), `work/game` (a writable
game copy used by every run), `ghidra_proj/`, `tools/ghidra/`, `refs/` (open-rebellion, StarWarsRebellionEditor.NET).
