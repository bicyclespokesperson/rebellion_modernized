# How this was built

This is a source-level reconstruction of *Star Wars: Rebellion* (1998, Coolhand Interactive / LucasArts) that runs
natively on macOS, Linux and Windows. The strategic game logic was reconstructed from `REBEXE.EXE`; the Win32
platform underneath it (windowing, DirectDraw, DirectSound, Smacker video, DirectPlay) was replaced.

## The original binaries

| | |
|---|---|
| Code | All game code is in `REBEXE.EXE` (MSVC 5.0, linker 5.10, `/GX`, no RTTI on game classes, identical-function folding on). Image base 0x400000, no relocations. |
| Game functions | ~14,080 (2.0 MB) at 0x401000–0x615345, plus compiler-generated helpers at 0x628e10–0x62908f. ~3,560 of them (virtual methods reached only via vtables, static initializers) were not found by Ghidra's auto-analysis and were created from `symbols/missed_functions.csv`. |
| Library code | 0x6153b0–0x628e0f: the old iostream library (LIBCI), then LIBCMT and the EH runtime. |
| Not code | ~11,350 `Unwind@` EH funclets (0x629090+), compiler-generated and never decompiled. |
| Other DLLs | Resource-only (bitmaps, waves, strings, dialogs) except `SMACKW32.DLL` (RAD Smacker) and `d3drm.dll` (Direct3D Retained Mode). |

## How the reconstruction worked

The executable was analysed in Ghidra and rewritten in C++ function by function. Each reconstructed function was
first loaded into the running original game in place of its machine code and checked against the original, both by
replaying captured calls and by playing. Once every function was reconstructed, the same sources were linked into a
standalone executable without `REBEXE.EXE`, and then ported from Win32/DirectX to SDL3 through `minwin`, a small
platform layer that implements the subset of Win32 the game uses. The original toolchain, ABI and verification
harnesses have since been retired, and the code modernized (see
[modernization decisions](design/modernization_aggressive.md)); they remain in the repository history. No
decompiler output is committed.

The original 3D tactical mode was not reconstructed. Since 2026-10-19 `src/game/space_battle/` is the only battle
simulator: it runs live for Take Command and headless for Simulate Results and AI battles, behind the strategic
`BattleSetup` / `BattleResults` seam in `src/game/app/`, with its SDL presenter in `src/port/native/space_battle/`.
See [the battle design](design/tactical_combat.md).

## Source layout

```
src/game/             the reconstructed game: strategy, AI, missions, economy, UI, saves, the space-battle simulator
src/port/minwin/      the Win32 subset the game is written against (headers)
src/port/minwin_impl/ its SDL3/FreeType implementation: windows and input, GDI, DirectDraw, sound, Smacker, config
src/port/native/      entry point, OS adapters (Linux, macOS, Windows), LAN, smoke tests, battle presenter
tools/                build, asset, AI-match and analysis tooling; gq is the Ghidra query client
symbols/              symbols.csv (names and addresses of the original functions), imports, modules
```

## Analysis setup

To reproduce the Ghidra project against your own `REBEXE.EXE` (untracked: `ghidra_proj/`, `tools/ghidra/`):

```sh
MISE_ENV=ghidra mise install                         # JDK 21 for Ghidra
# Ghidra 12.1 unpacked to tools/ghidra, then:
tools/ghidra_headless.sh ghidra_proj Rebellion -import "orig_game_files/Star Wars - Rebellion/REBEXE.EXE"
uv venv -p 3.12 tools/.venv && uv pip install --python tools/.venv --no-index \
    --find-links tools/ghidra/Ghidra/Features/PyGhidra/pypkg/dist pyghidra
tools/ghidra_server.sh start && tools/gq -f tools/py/apply_symbols.py && tools/gq --save
```
