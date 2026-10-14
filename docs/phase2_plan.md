# Phase 2 plan: standalone executable

Measured on the Oct 8 2026 hybrid build (`build/hybrid/gen/report.json`, `symbols/*.csv`, a scan of REBEXE.EXE).
Goal: the same sources link into an executable that runs without REBEXE.EXE (i686 under Wine first; Phase 3 ports).

## Remaining game code

- Non-tactical: 1,175 functions / 81.5 KB left. 705 are scalar deleting destructors (clang emits its own). The real
  logic is **combat_strat** (228 functions, 48 KB, 0% — strategic battle queue `game_rules_battle` 0x513f70–0x518b20,
  `ObjF9_combat_rules_mgr`, `planet_assault_resolver`, `combat_rules_14`, `cmd_225_resolver`) and **saveload**
  (66 functions, 6.6 KB: `game_session_saveload` 0x409230–0x40a050, `saveload_state` 0x568160).
- Tactical: 2,840 functions / 254 KB left, mostly 3D presentation — stubbed, not reconstructed (see M3).

## Original references (2,119)

| Category | Count |
|---|---|
| Reconstructed functions still called through `FUN_`/stale declarations | 646 (620 addresses) |
| Unreconstructed non-tactical functions | 261 addresses, 25 KB |
| Unreconstructed tactical functions (3D vtable slots, seam calls, 88 `LAB_` atexit thunks) | 176 addresses |
| Win32 imports | 212 |
| LIBCI iostream | 51 symbols / 40 functions |
| CRT | 35 |
| Globals: .bss 613, .data 90, .rdata 30; 18 addresses declared under two types | |

## Decisions

- **Data:** generate, don't hand-write. `tools/gen_data_image.py` emits the game-owned .data/.bss as one block at the
  original relative layout, with a label for every mangled name (covers the 19 two-type globals) and the pointer
  fixups (code → function symbols, .rdata → copied blocks, data → labels; 18 in the data as measured, see M5). CRT/LIBCI ranges excluded. Vtables and EH
  tables are compiler-generated.
- **Static init:** generated `static_init_table.cpp` calling the initializers in the original table order (0x6a6004,
  1,781 entries) from an entry wrapper. Tactical ID initializers (1,667, pure constant arithmetic) are evaluated in
  Unicorn at generation time and baked as stores into the data block. The 88 atexit thunks are reconstructed.
  Done: `tools/gen_static_init.py` (table 0x6a6000-0x6a7bd8, 111 calls, 1,670 baked, 0 unhandled, validated in the
  emulator; details in docs/library_code.md, "Static initializers").
- **LIBCI iostream:** reconstruct (game code depends on its layout: virtual-base `ios`, `x_lockc` +0x34, critical
  section +0x38, strstreambuf freeze semantics). **CRT:** static UCRT, except reconstructed fidelity pieces: `rand/srand`
  (per-thread seed, initial 1), `qsort` (VC5 order of equal elements), `_CIpow`, `_CIacos`, `floor`, `__ftol`.
- **Tactical fence:** reconstruct whatever auto-resolve reaches (capture all referenced tactical leftovers over the
  auto-resolve scenarios plus a Death Star battle — 0x5aefc0 is suspect); everything else becomes a logged stub in
  `tactical_3d_absent.cpp` (stubs carry original names, so they are hooks in the hybrid and testable now). Under
  `REB_NO_TACTICAL_3D`, BattleFlow maps a "tactical" vote to auto-resolve; the dialog stays faithful.

## Verification

Determinism shims in both builds (fixed-step QueryPerformanceCounter/timeGetTime/GetTickCount, fixed `time()`).
First prove repeated hybrid runs give byte-identical saves, then probe heap-address sensitivity (shift the heap and
compare saves). Exit criterion for the standalone build: saves byte-identical and screenshots pixel-identical with the
hybrid on soak_fast, load_game→save, auto-resolve and new-game scenarios. difftest stays the per-function gate.

## Determinism (M7, measured Oct 8 2026)

Switches (all opt-in, `src/hybrid/determinism.cpp`, passed through by `tools/run_game.sh`): `REB_DETERMINISTIC=1` patches the
game's IAT slots for QueryPerformanceCounter/Frequency, timeGetTime, GetTickCount, GetLocalTime and GetSystemTime and
the CRT `time()` (0x61b120, a CRT function in the EXE: it is jump-patched; it also reads GetLocalTime, which is fixed
too). `REB_CLOCK_STEP_US` (default 1000) is the virtual step per timeGetTime/GetTickCount call, `REB_QPC_STEP_NS`
(default 1000) per QueryPerformanceCounter call, `REB_NO_TIMERS=1` stops timeSetEvent callbacks, `REB_HEAP_PROBE=<bytes>`
(+ `REB_HEAP_PROBE_SBH=1`) shifts heap addresses, `REB_CLOCK_TRACE=1` logs clock callers. Runner:
`tools/determinism_check.sh [-n RUNS] [-t TAG] [-e VAR=VAL] [scenario]` (default scenario `save_determinism.txt`: load
RebelsFastStart, save SAVEGAME.005) runs the scenario N times and compares every save byte for byte; it refuses to
call a run identical when no save was written.

What drives nondeterminism: the strategic simulation advances one tick whenever the `m_waitTimer` Stopwatch (QPC) passes
0.25 s, and the main loop polls it once per iteration. Loaded games are not frozen: ticks run in the background, and
the tick counter (copied into ~8 places of the save, plus the checksums) at the moment a wall-clock-scripted click
saves varies run to run.

- Real clocks / per-call clocks at 1 ms per QPC call: the loop runs ~80,000 iterations/s, so ~300 ticks/s elapse and the
  save differs on every run (tick 11,055 to 11,227; 15,000-byte diffs, sizes differ by 48 bytes), because every
  scripting jitter of tens of ms moves the tick by tens. Freezing all clocks (step 0) changes the game's behaviour
  (UI animations stall) and the saves still differ (sizes differ by hundreds of bytes). Freezing only QPC freezes the simulation, but the save
  is a simulation command and never runs.
- Fix: a slow QPC (`REB_QPC_STEP_NS=1000`, one tick per ~250,000 iterations, ~3 s) while timeGetTime/GetTickCount stay at
  1 ms per call so the UI still animates. A tick boundary now rarely falls near an input event.
- Hybrid baseline: byte-identical saves in 7 of 8 runs (887,465 bytes). The 8th differs from the others in exactly the
  tick counter (8089 instead of 8090), its copies and checksums: a tick boundary landed on the click. Re-run on a
  mismatch that is only the tick; a larger `REB_QPC_STEP_NS`/longer scenario lowers the odds further.
- After the LIBCI reconstruction (M4) the same scenario writes a byte-identical SAVEGAME.005 (md5 6eec1db1...): the
  reconstructed ostream/filebuf write path reproduces the original library's output.
- Not sources: `time()` (fixed), multimedia timers (`REB_NO_TIMERS=1` made no difference to the pattern), the heap.
- Heap probe (18 MB address-space reservation before the CRT heap exists, then 1,690-byte allocations from both the
  CRT's NT heap and, with `REB_HEAP_PROBE_SBH=1`, its small-block heap; the probe allocations land at 0x1691548 and
  0x1691BF0): saves under the probe are byte-identical to each other and to the baseline run with the same tick. Heap
  addresses do not reach the save file for this scenario (save path only). The probe does not move whole regions,
  and the pointer-ordered container risk in the plan remains untested for scenarios that run the simulation.
- For the standalone comparison use the same switches in both builds; the scenario must not depend on wall-clock time
  beyond tick boundaries. Remaining gap: input is still injected by wall-clock xdotool (iteration-keyed injection was
  tried: iteration rates differ by an order of magnitude between intro and menus, so it was dropped).

## M5: the hybrid that owns its data (measured Oct 8 2026)

`tools/build_hybrid.sh --own-data` (CMake `REB_OWN_DATA=ON`) puts the game's globals in the DLL instead of binding them to
REBEXE.EXE addresses. Stages: container builds the objects and `gen_link_symbols.py --own-data` (which then leaves game-owned
globals unbound and writes `gen/data_refs.json`); the host runs `tools/gen_data_image.py` (needs the original EXE: capstone +
pefile via uv) to write `gen/data_image.s`; the container assembles and links it. `tools/check_data_image.py --exe <EXE>` validates the
result (every label at its original offset, block bytes equal to the EXE's except the relocated pointers, every patch site holds a
block address). `REB_DATA_TRIPWIRE=1` (run-time) is the leak detector described below. The default build is unchanged.

**What is owned.** `tools/own_data.py` holds the boundaries, found from the references of the library functions: the EXE's
.data/.bss are `[0x6a6000-0x6a7c10)` static-initializer tables (the CRT's `_initterm` walks them until M6), `[0x6a7c10-0x6ac970)` game
.data, `[0x6ac970-0x6b059c)` LIBCI RTTI + LIBCMT .data (RTTI descriptors, locale tables, `_iob`, small-block-heap state, math tables),
`[0x6b059c-0x6be708)` game .bss and `[0x6be708-0x6bfe50)` LIBCMT/LIBCI .bss. The two game segments (77,516 bytes) become one 92,920-byte
block in the original relative layout (the CRT gap is zero-filled, so one displacement relocates any original address). Pooled
literals (`""` at 0x6a8714/0x6a8f48 and the `%ld`/`%lu` formats at 0x6ac1c0/c4) are game data that the CRT code also reads; they are
owned like the rest and the CRT's references are shifted too.

**Image contents.** 823 labels (790 in the block at 772 distinct addresses, 33 for `.rdata` globals; covers the 19 two-type
addresses), 17 `.rdata` copies (692 bytes: float tables, rects, GUIDs; vtables/vbtables stay in the EXE because vtable patching points at
them) and 18 pointer fixups, all block-to-block (the static data holds almost no pointers: the 560 estimated earlier counted
printable-string bytes; strings are told apart by a printable-run mask). No static
data points at code or `.rdata` strings (those tables are built at run time by the static initializers), so the "hooked function symbol /
copied string" fixup paths exist but fire 0 times today.

**Original code that is not reconstructed (step 2).** REBEXE.EXE has no `.reloc` section, so references are found by disassembly:
`gen_data_image.py` sweeps `.text` per function partition and lists every `imm32`/`disp32` that holds a block address:
8,174 sites (5,775 displacements, 2,399 immediates; 4,968 distinct targets) plus one pointer in `.rdata`. By owner: 4,173 in
reconstructed functions (dead once hooked, but patched too so `REB_DISABLE_HOOKS` still works), 3,982 in tactical code (3,386 distinct
globals; stays original), 15 in other unreconstructed game code, 4 in CRT. The alternative, limiting M5 to globals that no
unreconstructed code references, would own only 1,575 of the 4,968 referenced addresses (32%) and leave the whole tactical and
DirectDraw-init data behind, so it was not chosen.

Chosen: **shift the references at load.** `src/hybrid/own_data.cpp` adds `block - 0x6a7c10` to each listed dword (`reb_code_refs`, 8,175
entries) in `DllMain`, before hooks are installed; the EXE's copy of the block becomes dead. The block's initial bytes are the EXE's file
contents, and the static initializers run later in the EXE's CRT start-up, so nothing is copied at run time. Cost: 8,175 four-byte
writes (about 1 ms); the DLL loads at a fixed base anyway. Weakness: a reference the disassembly cannot see (an address built by
arithmetic from something outside the block, or a data-in-code misparse) would silently read the stale EXE copy. Evidence against: every
site's bytes were checked, every one of the 4,968 targets lies inside a defined Ghidra data item, and the **tripwire** (`REB_DATA_TRIPWIRE=1`)
marks the 17 whole pages of the EXE copy (68 KB of the 77.5 KB; the 3 edge pages share space with the CRT) `PAGE_NOACCESS` and logs the
instruction that touches one. Negative control: `REB_OWN_DATA_NO_PATCH=1` skips the shifts and the tripwire fires within the smoke
scenario (write from DLL code, from `0x59b41c`, `0x5dda66`); with the shifts it never fires.
Limits of this mode: `difftest`/`REB_CAPTURE` snapshots assume the original layout, so they stay on the default build.

**Verification (own-data build).** smoke (menu screenshot correct), load_game and soak_fast: no `Unhandled`, no tripwire hit;
`tools/determinism_check.sh -n 4`: four byte-identical `SAVEGAME.005` files with md5 `c676b92c...` (the original-logic save; one
earlier pair gave the known tick-boundary variant `d0f0adea...`, which the default build produced in the same session too).

**Vtables (step 3).** `tools/vtable_gaps.py` reads the built DLL: 1,018 reconstructed vtables, 24,034 slots. 985 are complete (every slot
a hooked reconstruction or a pure virtual, whose slot is `_purecall`); 33 have unreconstructed slots (242 slots, 103 distinct functions):
28 of them only tactical (231 slots, the fenced-off 3D classes: `MovingObject`, `FormationBase`, ...) or `icf_return_zero` (5), and 5
with game-code slots (6 slots, 2 functions: 0x40f340, 0x5f6590). Slots left unreconstructed are still valid in the hybrid (they hold
the original's address), so a DLL vtable works for original code that calls through it; what the redirection protects is pointer
identity (original code comparing a vptr with a vtable address) and objects the unreconstructed constructors build with the original
vtable. `REB_OWN_VTABLES=1` (experiment: `patch_vtables.py --keep-own-vtables`, combine with `--own-data`) leaves every vptr store on our
copies. Result: smoke, load_game and soak_fast still run clean, but `save_determinism` fails (the Options screen never opens, no save is
written). A bisect over the 1,013 vtables (keep own copies for a range, rebuild, run) shows that keeping every vtable except `AiPlayer::vftable` works
and keeping only `AiPlayer::vftable` breaks it: the original's slot 8 is 0x51ebb0 (`mov eax,1; ret 4`, ICF-shared with
`GameManager::NoteDispatch`), `AiPlayer` does not override it, and the inherited `PlayerController` slot (0x48b440) returns 0. The vptr
redirect hides gaps of exactly this kind (an override the reconstruction never declared, or declared with the wrong body). So vtable
patching stays on. `tools/vtable_gaps.py --exe <EXE> [--list]` also compares every slot with the original vtable's: of 24,034 slots,
5,571 are our own copies of ICF-folded stubs (equivalence not checkable by address), 131 in 70 vtables name a different original function
with other machine code (the AiPlayer slot is one; the others are candidates to review: expected to be ICF aliases with different bytes
only where relative calls differ, otherwise missing overrides), and the rest match. Turning the redirect off by default needs: the 33
incomplete vtables closed (M2/M3), that candidate list reviewed, and soak/save scenarios green with `REB_OWN_VTABLES=1`.

## M6: the first standalone `rebellion.exe` (measured Oct 8 2026)

`tools/build_standalone.sh` builds `build/standalone/rebellion.exe` (i686 PE, clang-cl + lld-link in the dev container, static UCRT,
no REBEXE.EXE at run time) from the same sources: CMake `REB_STANDALONE` (`cmake/standalone.cmake`), `REB_NO_TACTICAL_3D` implied.
`tools/run_game.sh --standalone <scenario>` runs it under Wine with the install directory's game data and the real SMACKW32.DLL.
`tools/map_lookup.py` turns Wine crash addresses into functions through the link map; the exe writes registers, memory behind the
registers and the code addresses found on the stack to `REB_LOG` on an unhandled exception (`src/standalone/crash_log.cpp`).

**Stages** (like `build_hybrid.sh --own-data`): (1) container: compile, `gen_link_symbols.py --standalone` writes `gen/data_refs.json`
(every original global the objects reference, also CRT/LIBCI data and vtables; the hooked-function table); (2) host: `gen_data_image.py
--standalone` writes `gen/data_image.s` and `gen_resources.py` copies the EXE's resource section into `gen/rebexe.res` (RCDATA animation
scripts/message templates, the save-file name strings, icons, cursors, version; generated at build time from the install, never
committed); (3) container: link. A failed link prints the unresolved symbols grouped by category
(`tools/standalone_link_report.py` -> `gen/link_report.txt`, the categories of `standalone_gaps.py`).

**What the standalone image is made of.** The same 92,920-byte data block as M5 (now the only definition: no run-time patching, no
`reb_code_refs`), plus: the referenced `.rdata` globals copied *with their pointers relocated* (vtables: slots become our function
symbols; `const char* const` globals: the pointer target is copied or becomes a block label; unreconstructed targets would be
`reb_orig_<addr>` link errors), fresh storage for the two LIBCI data globals in the CRT's `.bss`, the original's static-initializer
table (1,781 entries; reconstructed initializers by symbol, `jmp`/`call;jmp` thunks followed; run from a `.CRT$XCU` entry before the
game's `WinMain`) and import libraries made by `gen_import_libs.py` for the DLLs the SDK has no `.lib` for (SMACKW32 and DPLAYX: by
ordinal, as the EXE imports them; D3DRM by name). Our own vtables are used everywhere (no vptr redirect to the EXE).

**Link result: 0 unresolved symbols.** Everything the reconstructed code still referenced by original address was already
reconstructed, stubbed (the 164 tactical 3D stubs) or provided by a library. What had to be added: the 12 scalar deleting destructors
of the LIBCI vftables (`src/standalone/iostream_vtable_functions.cpp`: the LIBCI classes are layout structs, nothing defined them),
`icf_return_zero`/`icf_empty_function`, `_isprint/_isspace/_ispunct` (UCRT), and a bring-up `_CIpow` (calls `pow`; the CRT fidelity
agent's reconstruction replaces it). `filebuf` reads the text-mode flag of a handle through `_setmode` instead of the old CRT's
`__pioinfo` (the UCRT's layout differs; the pending-CR flag has no public equivalent and reads as clear). `operator new` zero-fills
(the original's fresh heap pages were zero; this makes the standalone's uninitialised reads reproducible).

**Bugs the standalone build exposed** (all invisible in the hybrid, whose vptr redirect and original data hide them):
`System::SetRecord` (slot 10, 0x50e1c0, shared with `Character::SetRecord`: sets the object's id from its record) was not overridden,
so new games gave systems allocator ids 0x500+ and crashed in `SystemNeighbourTable::Build`; `Fleet::ShouldNumberNameFromRecord` (0x40f340) must
return 1; `g_coolDragListClassName` is an `.rdata` pointer to a `.data` string that the data image copied unrelocated (the drag list
window was never created, ui_tour crashed opening the system window). `tools/vtable_gaps.py` skipped every multiple-inheritance vtable
(`Class::vftable{Base}`, all GameObj classes); it compares them now, which also found the candidates listed below.

**Vtable candidates, resolved (Oct 9).** Against the disassembly, only one was a real gap: `MissionJediTraining` slot 7 is the
original `MissionPalace::Checksum` forwarder (0x56dee0 -> 0x56da70 -> 0x56aab0), so the Teacher key is *not* part of the
checksum (Read/Write do include it; `BUG(original)`); the class now overrides `Checksum` with the rule-base sum. The rest are
behaviourally equal: slot 48 of Facility/CapShip/Fleet/CombatUnit (slot 47 for Fleet) is 0x4f29a0 -> 0x51ebb0 (`return 1`), `HumanPlayer`
slot 28 is 0x486870 -> 0x5ae1e0 (`ret 4`), slot 133 (0x52d5e0 -> `Fleet::ShouldNumberNameFromRecord`, returns 1) and 137 (0x4f2a70 -> 0x524b10, returns 1) are
forwarders to such stubs; `DefenseFacility`/`ShipGroup` slots that name `_purecall` are abstract in the original. Left as is, tactical only:
`CapitalShipFormation`/`FighterFormation` slot 6 return 3/2 in the original, `TacticalSpecialForceUnit` slot 1 returns 8, `GroupOrder*`.

**Static init (done).** `tools/build_standalone.sh` runs `gen_static_init.py --standalone` on the host (from the user's EXE, like the data
image; `gen/static_init_table.cpp` is never committed): the 111 reconstructed initializers become calls (bound to the objects' symbols with
`/alternatename`, since clang-cl keeps the `\x01` of an asm label literally), the other 1,670 are baked stores into `reb_data_block`, all in the
original order; the entry runs it from a `.CRT$XCU` entry. The interim `reb_init_table` of the data image is gone.
`tools/check_init_state.sh` verifies it: the original game with every hook disabled is captured at WinMain's first instruction
(`REB_CAPTURE=0x4165b0:1`), the standalone writes its game-owned data (`REB_DUMP_INIT_STATE`, after the table ran, before WinMain), and
`tools/compare_init_state.py` compares the two segments the data image owns (0x6a7c10-0x6ac970, 0x6b059c-0x6be708) after undoing the
relocation of pointers into the block (25 of them). Result: 0 differing dwords except 107 vtable pointers (to our own vtable copies; each
reference value is a named `vftable`).

**`battle_closed` (936 px) was timing.** With `REB_DETERMINISTIC=1` the hybrid and the standalone (before and after the full init table)
have a pixel-identical `battle_closed`; what still differs between *any* two runs, hybrid vs hybrid included, is a 10-pixel-high
strip of the title/status line at y 20-29 (`battle_after_20s`/`battle_result_a`, 45-55 px). Not an init-table effect.

**Regression guard.** `tools/standalone_check.sh` (last stage of `tools/integrate.sh`) builds the standalone, runs smoke (no `Unhandled`)
and compares the save of `save_determinism` with the original code's save of the same session (equal, or a tick-only difference of at
most 70 bytes at equal size, retried once); one `STANDALONE OK|FAIL` line, about 6 minutes.

**Scenarios under Wine** (real clocks, `tools/run_game.sh --standalone`): 49 of the 51 scenarios of `tests/scenarios/` (all but `fast_play_missions_long` and `load_game_long`) ran to the
end of their script without a crash or `Unhandled` in `wine.log`, including main menu (smoke), `load_game` (60 s of play), `save_game_slot`
(writes `SAVEGAME.005`, same size as the hybrid's), `new_game_rebel`/`new_game_empire` (new game, agent-droid intro, 60 s of play),
`soak_fast`, `ui_tour`, `space_battle_autoresolve` and `space_battle_tactical_vote` (the stubbed tactical vote falls back to auto-resolve),
`video_intro` (Smacker movies through the real SMACKW32.DLL) and the multiplayer lobby scenarios (DirectPlay through Wine's dplayx).

**Differential runs against the hybrid.** `src/standalone/determinism.cpp` defines the game's import slots for the clock APIs ahead of the
import libraries (the hybrid patches the IAT; same switches: `REB_DETERMINISTIC`, `REB_CLOCK_STEP_US`, `REB_QPC_STEP_NS`,
`REB_NO_TIMERS`), so `REB_DETERMINISTIC=1` runs of both builds see the same virtual time. `tools/compare_runs.py <dirA> <dirB>` compares
screenshots pixel by pixel and the saves byte by byte; `tools/determinism_check.sh -s` repeats a scenario on the standalone exe.
`save_determinism` (load, save `SAVEGAME.005`): the saves of two hybrid and two standalone runs are **byte-identical** (887,465 bytes, md5
`c676b92c...`, the value M5 recorded) and the three screenshots pixel-identical. Screenshots of hybrid vs standalone, one run each:
`soak_fast` 4/4, `new_game_rebel` 11/11, `load_game` 9/9, `space_battle_autoresolve` 6/7 identical (`battle_closed`: 936 pixels differ, a
timing artefact until a second hybrid run shows otherwise). Without the shims, hybrid and standalone saves of `save_game_slot` differ in 46
bytes (tick counters and checksums).

**Not done / differences from the hybrid.** The 1,665 tactical ID initializers of the table are not run (they are 0 entries: the
generated `static_init_table.cpp` of the CRT/init work replaces the interim table); UCRT replaces the VC5 CRT (`rand`, `qsort`,
`floor`, `_ftol`, `sprintf`, locale: the fidelity pieces are the CRT agent's); the heap probe of M7 is not implemented for the standalone
(the heap is the UCRT's); `difftest`/captures are hybrid-only.

## Milestones

| # | Milestone | Exit criterion |
|---|---|---|
| M0 | Categorized gap report in report.json + `tools/standalone_gaps.py` | Numbers reproducible per build |
| M1 | Replace the 646 stale declarations; no `aliases.csv` (deleted in M3); one type per global | Hook count unchanged, −646 references |
| M2 | combat_strat, saveload, the remaining large non-tactical functions, named ICF stubs, atexit thunks | No reference to non-tactical game code; difftest PASS |
| M3 | Tactical fence (captures, reachable reconstruction, stubs, vote fallback) | Hybrid with stubs: auto-resolve OK; "tactical" resolves abstractly. Done: see docs/code_map.md "Tactical fence (M3)" |
| M4 | LIBCI + CRT fidelity pieces | difftest PASS; save/load scenarios OK. LIBCI done (src/game/iostream, 0 references left); CRT fidelity pieces done (src/game/crt; decisions for the rest in docs/library_code.md) |
| M5 | `REB_OWN_DATA=1` hybrid: generated data image, vtable patching off | Scenarios match the normal hybrid. Data image done (above); vtable patching still on |
| M6 | Standalone `rebellion.exe` (clang-cl/lld-link, static UCRT, real import libs, init table, entry wrapper) | Zero absolute symbols; differential runs pass. First exe built and measured, see "M6" above (saves identical on `save_determinism` up to the tick; the full generated init table is in; CRT fidelity pieces are open) |
| M7 | Determinism harness (can start with M1) | Repeated hybrid runs identical; heap probe recorded |

## Risks

combat_strat module boundaries are low-confidence; pointer-ordered containers (heap probe); 0x5aefc0 reachability;
ICF-folded function-pointer comparisons (lld folds a different set); multiplayer abstract-battle sync unchecked;
~210 heuristic function starts may hide functions (the M6 zero-absolute-symbol link settles it).
