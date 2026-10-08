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
  original relative layout, with a label for every mangled name (covers the 18 two-type globals) and ~560 pointer
  fixups (code → function symbols, .rdata → copied blocks, data → labels). CRT/LIBCI ranges excluded. Vtables and EH
  tables are compiler-generated.
- **Static init:** generated `static_init_table.cpp` calling the initializers in the original table order (0x6a6004,
  1,781 entries) from an entry wrapper. Tactical ID initializers (1,667, pure constant arithmetic) are evaluated in
  Unicorn at generation time and baked into the data image. The 88 atexit thunks are reconstructed.
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

## Milestones

| # | Milestone | Exit criterion |
|---|---|---|
| M0 | Categorized gap report in report.json + `tools/standalone_gaps.py` | Numbers reproducible per build |
| M1 | Replace the 646 stale declarations; empty `aliases.csv`; one type per global | Hook count unchanged, −646 references |
| M2 | combat_strat, saveload, the remaining large non-tactical functions, named ICF stubs, atexit thunks | No reference to non-tactical game code; difftest PASS |
| M3 | Tactical fence (captures, reachable reconstruction, stubs, vote fallback) | Hybrid with stubs: auto-resolve OK; "tactical" resolves abstractly |
| M4 | LIBCI + CRT fidelity pieces | difftest PASS; save/load scenarios OK |
| M5 | `REB_OWN_DATA=1` hybrid: generated data image, vtable patching off | Scenarios match the normal hybrid |
| M6 | Standalone `rebellion.exe` (clang-cl/lld-link, static UCRT, real import libs, init table, entry wrapper) | Zero absolute symbols; differential runs pass |
| M7 | Determinism harness (can start with M1) | Repeated hybrid runs identical; heap probe recorded |

## Risks

combat_strat module boundaries are low-confidence; pointer-ordered containers (heap probe); 0x5aefc0 reachability;
ICF-folded function-pointer comparisons (lld folds a different set); multiplayer abstract-battle sync unchecked;
~210 heuristic function starts may hide functions (the M6 zero-absolute-symbol link settles it).
