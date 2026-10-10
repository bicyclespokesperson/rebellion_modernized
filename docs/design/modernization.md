# Design: codebase modernization (preliminary)

Status: proposal, not started. Written Oct 2026 after phases 1–3 (faithful reconstruction, standalone build, native
Linux/macOS port). Companion: [high_resolution.md](high_resolution.md).

## Goal

Turn the faithful reconstruction into a codebase that is pleasant to change: real names and types, owned memory,
standard containers, testable game logic, no dependence on the 1998 binary layout. It is the base for gameplay fixes,
quality-of-life features, higher resolutions and an eventual from-scratch tactical mode.

Non-goals: changing gameplay as a side effect, a rewrite in another language, multiplayer.

## Where the code is today (measured)

| | |
|---|---|
| Size | 797 files, ~171 k lines in `src/game` |
| Binary-layout coupling | 771 `REB_ASSERT_SIZE`, 1,076 `REB_ASSERT_OFFSET`; class layouts mirror MSVC 5 32-bit |
| Placeholder names | 118 `m_unkNN`/`m_padNN` fields, 129 `VirtualNN` slots, 68 `FUN_xxxxxxxx` references |
| Memory | ~736 raw `new`/`delete` sites, NULL-returning `operator new` with original null checks |
| Containers | ~106 hand-rolled tree/list/queue classes (id-ordered AVL trees, intrusive lists) |
| Strings | the game's own `String` class, `sprintf`/`strcpy` in 19 places, ANSI code page |
| Original bugs kept on purpose | 158 `BUG(original)` sites |
| Symbols | 13,862 rows in `symbols/symbols.csv` tie functions to original addresses (hooks, difftest) |
| Platform | the game talks Win32 to `minwin` (our Win32 subset on SDL3); multiplayer is DirectPlay |

What already helps: field-by-field save I/O (typed, size-checked), explicit argument evaluation order (checked by
`tools/check_arg_order.sh`), no pointer-ordered iteration that affects saves (heap probes), and a deterministic headless
simulation harness (`REB_SIM`) with goldens on i686, x86-64 and arm64.

## The central decision: what is the oracle once the layout goes?

Today correctness is defined per function: difftest replays the original binary's calls against ours. That only works
while our classes have the original layout and our functions the original names, i.e. exactly what modernization removes.

**Proposal: freeze the faithful reconstruction as the oracle and verify the modern mainline behaviourally against it.**

* Tag the current tree as `faithful-1.0` and keep it buildable (hybrid, standalone, native). It stays the reference for
  "what did the original do", and difftest/hybrid work continues there if anyone wants to fix residual fidelity issues.
* The modern mainline is verified by *behaviour*, not by layout:
  * **Simulation goldens** (`REB_SIM`): extend the corpus from 3 to ~20 saves covering every mission type, both sides,
    space and planetary battles, Death Star, late game; thousands of ticks each. Byte-identical saves vs the oracle.
  * **Save compatibility:** the modern build must load every save the oracle writes and write saves the oracle loads
    (byte-identical while the format is unchanged).
  * **Long UI-driven comparisons** (`tools/long_compare.sh`) and **screenshot parity** on the scripted scenarios.
  * **Unit tests** for logic that becomes testable once it no longer needs the binary (rules, AI evaluators, economy).
* Each modernization step must keep all of the above green. Steps that intentionally change behaviour (bug fixes) are
  separate commits that update goldens with a written reason.

Alternative considered: modernize in place while keeping the hybrid working. Rejected: every rename, layout change or
container swap would have to stay hook-compatible, which forbids most of the useful changes.

## Stages

| # | Stage | Content | Exit criterion |
|---|---|---|---|
| M0 | Oracle and harness | Tag `faithful-1.0`; grow the golden corpus; in-process scenario player for the native build (no xdotool); golden runs in CI for the parts that need no game data, locally for the rest | Corpus green on oracle and mainline; scenario player replays the UI scenarios |
| M1 | Leave the binary | Drop the hybrid/difftest/hook machinery from the mainline (it stays on the tag); remove `REB_ASSERT_*`, symbol-name constraints, `__thiscall`/calling-convention shims, the i686-only paths; one native build configuration | Goldens and save compatibility unchanged |
| M2 | Names and types | Replace `m_unk*`, `VirtualNN`, `FUN_*` with real names (most are understood by now; the rest get named from use); enums and strong id types for object keys/families, message ids, ranges; `const` correctness; split the big multi-purpose classes along the seams the decompilation already documents | No placeholder names left outside documented unknowns |
| M3 | Memory and containers | Ownership: `std::unique_ptr` for owned objects, references/non-owning pointers elsewhere; replace the id-ordered AVL trees with `std::map`/sorted vectors **with the same ordering**, intrusive lists with `std::vector`/`std::list`; the game's `String` → `std::string` at the boundaries first, then inside | Goldens unchanged (iteration order is behaviour: every container swap must keep it) |
| M4 | Serialization | Move save/load into a dedicated module: a versioned format (header with version), the original format as "version 0" reader/writer kept for compatibility; checksums kept where multiplayer/save validation uses them | Old saves load; new saves round-trip; golden saves unchanged in version-0 mode |
| M5 | Original bugs policy | Triage the 158 `BUG(original)` sites: crashes/UB/leaks are fixed unconditionally; gameplay-affecting bugs (e.g. AI rescue never keeping a target, bombardment picking the first facility) become fixes behind a `classic_rules` option, default "fixed" or "classic" per decision | Every site classified; fixes covered by new goldens |
| M6 | Platform boundary | Keep `minwin` as the platform layer initially. Later, if the high-resolution work needs it, replace the Win32-shaped UI host with a native one (see high_resolution.md, tier 4) | Decided per need, not up front |

M2–M4 can run in parallel per subsystem (the code map's module boundaries hold). Suggested order by value: rules,
economy, AI and events first (logic, testable, high churn for gameplay changes), UI last (tied to the platform work).

## Determinism rules for the modern code

These are what the faithful work learned the hard way; the mainline keeps them as written rules plus checks:

* Iteration order of every container that feeds game logic or saves is part of behaviour.
* One side-effecting call per argument list (`tools/check_arg_order.sh` stays in CI).
* Random draws come from the game's generators, in the original order; the AI shares a generator with the advisor droid,
  so UI timing can change AI choices — keep that isolated or decide to separate the generators deliberately (a
  behaviour change, M5-style).
* Floating point: `-ffp-contract=off`, no fast-math; float→int via `reb_ftol`.
* No pointer values in saves, hashes or ordering.

## Tooling

clang-format (one style pass per stage, never mixed with logic changes), clang-tidy with a curated check set
(modernize-*, bugprone-*, a few performance-*), sanitizers in CI for the native build, unit tests with a small framework
(doctest or Catch2, permissive licences), `tools/sim_goldens.sh` and the save-compatibility test as the merge gate.

## Risks

* **Losing the oracle.** Mitigated by the tag and by never updating goldens without a reason in the commit.
* **Silent ordering changes** in container swaps. Mitigated by the corpus size and by porting containers one family at a
  time with a golden run per family.
* **Game data in source.** Some globals are initialised from the EXE at build time (data image, static-init table). A
  modern codebase wants them as source, but they are the game's data: keep generating them from the user's EXE at build
  time (decision needed if a data-free build is ever wanted).
* **Scope.** ~171 k lines; done incrementally per subsystem it never blocks playing the game.

## Decisions to make before starting

1. Freeze-and-verify (recommended) vs in-place with the hybrid kept alive.
2. Bug policy default: classic or fixed.
3. Whether the modern build must keep writing version-0 saves by default (compatibility with the original game) or may
   switch to a new format.
4. C++ standard (C++20 recommended) and minimum compilers (Apple clang of the oldest supported macOS).
