# Roadmap: aggressive modernization

Status: **correct, authoritative plan**; implementation started October 11, 2026.

This checklist is the source of truth for modernization work. Update it in the same change that completes, adds, splits,
or deliberately defers a roadmap item.

## Decision

Optimize for a maintainable, working, improved game. The reconstruction has served its purpose: use it as source
material, not as a specification for every observable detail. Fix bugs while rewriting, change interfaces and their
callers together, replace accidental ordering with explicit rules, and delete obsolete compatibility machinery.

Use C++23. Apply mechanical cleanup early. Then modernize by responsibility, with an independent adversarial review,
a host-platform build and a short automated smoke check before each commit. Save substantial manual playtesting for the
end.

The important adjustment to “rewrite each file and commit” is the unit of work: **one responsibility plus its clients**.
A file can be that unit, but a container family, ownership chain or initialization sequence often spans several files.
Splitting, merging and deleting files is encouraged. Already-clear code does not need rewriting just to mark it done.

## What we accept, and what still matters

Accept changed AI choices, random sequences, tie-breaking, timings, floating-point results, serialization bytes and
minor presentation differences. Fix known original bugs by default; no `classic_rules` switch or dual implementations.
Small differences are acceptable, but are not evidence of improvement by themselves. Review should identify a plausible
reason for a change instead of assuming that cleaner-looking code necessarily plays better.

Keep the recognizable game, both factions, working missions/economy/combat resolution, usable UI, and working saves.
Existing saves should import; modern saves must round-trip. Writing saves readable by the 1998 executable is not a goal.
New tactical 3D gameplay, multiplayer restoration, a new renderer and wholesale balance redesign are separate projects.

No byte-identical simulation gate, screenshot parity gate, expanded golden corpus, long UI comparisons, per-step
playthrough, or obligatory test for every rewritten function. The existing annotated tag `decompile-working-on-macos`
at `7df8d105c5db53cfa18bc5d227e11eaee6d903f6` is the known-good macOS baseline (annotation: “decompile working on macos”).
Old code and tools remain available there for archaeology; do not maintain a second active fidelity branch.

## Platform rule

Each change only needs to build and pass its relevant checks on the platform where the agent is working. A macOS agent
does not need Linux or Windows access; a Linux agent does not need to find a way to test macOS; the same rule applies on
Windows. Record the host platform and checks run, and do not claim checks that were not run. Platform discrepancies are
usually small and can be fixed by a later agent working on that platform. Cross-platform build status is tracked below,
but it is never a prerequisite for an individual change or a reason to block modernization work.

## A0 — Establish the modern build

- [x] Adopt `decompile-working-on-macos` as the known-good archaeology baseline. Keep development saves as disposable
      copies. Build and run commands are recorded in [native.md](../native.md).
- [x] Require C++23 and disable compiler extensions in the maintained CMake builds.
- [x] Standardize maintained builds on the Clang compiler family: Apple Clang, LLVM Clang, or clang-cl.
- [x] Update checked-in parser and syntax-check flags to C++23 so tooling reads the same language as the build.
- [x] Pin clang-format and clang-tidy to LLVM 22.1.4 through `tools/clang_tools.sh`.
- [x] Export the native compilation database.
- [x] Build and run the native game in C++23 on macOS (Apple Clang 17, arm64; 10-tick headless simulation smoke).
- [ ] Build and run the native game in C++23 on Linux.
- [ ] Establish a modern Windows x64 target using clang-cl and the Windows SDK, normal CRT linking and SDL/minwin. This
      is real port work: the retired Windows standalone target was i686, used `/NODEFAULTLIB`, and reconstructed startup/data;
      the current native platform selection only handles macOS and Linux. Provide the Windows OS adapter and resolve
      Win32/minwin naming and import boundaries. Do not promise Windows support based on a compiler flag alone.
- [x] Adapt the smoke harness to detect save write/close failures, reload its output in a second process, advance again,
      validate basic references, and exercise normal application teardown instead of the existing `ExitProcess` shortcut.
- [x] Run that startup/load/short-simulation/save/reload smoke on macOS (release and UBSan). Apple Clang 17's ASan
      runtime deadlocks before `main` on the current macOS host, including for an empty probe executable.
- [ ] Run that startup/load/short-simulation/save/reload smoke on Linux.
- [ ] Run that startup/load/short-simulation/save/reload smoke on Windows.

A0 and A1 may interleave where a platform target needs the new initialization path. Normal CRT startup also means
deliberately retiring conflicting CRT replacements and allocator overrides, rather than linking two versions together.

### C++23 support policy

Target current stable Clang toolchains and current desktop operating systems; preserving the existing macOS 11 deployment
floor is not a requirement. Target matrix: Apple Clang/libc++ on macOS arm64, LLVM Clang/libstdc++ on Linux x86-64, and
clang-cl with the Windows SDK on Windows x64. Retain existing macOS x86-64 and Linux arm64 builds where practical. GCC and
the MSVC compiler are out of scope. This matrix describes the eventual project state; each agent only builds the row
matching its host platform.

Pin concrete compiler, standard-library, SDK, CMake and OS versions after the first successful matrix build. “Latest”
describes selection policy, not a permanently floating CI dependency. Check a tiny compile/link probe for each C++23
facility we actually adopt. Prefer `std::expected`, `std::to_underlying`, spans, ordinary ranges and standard formatting
where supported. Do not make modules, `import std`, or exhaustive C++23 library coverage prerequisites. If a useful
facility is absent from one chosen library, use a simple alternative or a small local wrapper.

C++23 mode is feasible on these platforms; complete feature parity is a separate question. Apple documents OS runtime
requirements for some library facilities, and upstream Clang and libc++ track language and library support separately.
Check the chosen deployment target as well as compiler version. Sources checked for this proposal:
[Apple](https://developer.apple.com/xcode/cpp/), [Clang](https://clang.llvm.org/cxx_status),
[libc++](https://libcxx.llvm.org/Status/Cxx23.html), [GCC](https://gcc.gnu.org/projects/cxx-status.html),
[Microsoft standard modes](https://learn.microsoft.com/en-us/cpp/build/reference/std-specify-language-standard-version).

## A1 — Remove the machinery that makes rewriting expensive

The native compiler/linker now use typed program definitions and named startup work. Startup text is extracted into
[a stable asset pack](startup_assets.md), separate from C++ state. A1 is complete on macOS: application/game/display/UI/audio lifetimes are explicit, reconstructed stream layouts
and standard CRT replacements are retired, and allocations use the host C++ runtime.

- [x] Replace generated byte storage/assembler aliases for program globals with typed source definitions and meaningful
      defaults. The compiler/linker no longer read `symbols.csv`, original addresses or `native_data.json`.
- [x] Replace EXE startup-store replay with named initialization phases, including geometry, prototype/factory
      registration and legacy option defaults. Ordinary independent objects use normal C++ construction.
- [x] Own the delayed display service in a scoped native context; release sound, fonts and resource libraries explicitly
      before platform registries and startup assets are destroyed.
- [x] Move application/game ownership and borrowed service access into a small context. `GameContext` owns the app,
      flow, display, UI resources and audio; the flow/driver own tactical state, session and manager. Retire the borrowed
      display pointer and owning service globals. Callback/toolkit bridges borrow through the live app context.
- [x] Separate startup text from program initialization using a versioned asset schema independent of C++ symbols,
      pointer widths and object offsets. Extract only required strings from the user's installation; no extracted bytes
      are checked into source. Ordinary shipped resources/tables still come from the installation.
- [x] Delete `gen_native_data.py` and `gen_native_init.py`; the modern build no longer has a generated C++ stage.
- [x] Delete the remaining unused reconstructed global constructor/`atexit`/guard helpers after checking their runtime
      callers; archive them in Git. Retain actual initialization dependencies as named source behavior.
- [x] Retire active hybrid/hook/standalone CMake build machinery and make the native target the default. Original-layout
      assertions and size-scaled class-storage wrappers are removed; resolved containers are actual typed members.
- [x] Replace stand-in polymorphic views with their actual native types: strategic window, family table, notice/event
      payloads, data records and collected name nodes. Route network messages with an explicit sender side instead of
      reading a nonexistent common side field. Compile absent 3D-only interfaces out of the native auto-resolve target.
      Remove all 12 temporary vptr-sanitizer source exclusions.
- [x] Retire remaining obsolete calling-convention/vptr-retention helpers and reconstructed stream object-layout/CRT
      emulation from modern targets. Keep independently useful archaeology tools outside the build.
- [x] Enable ordinary exception support and default RTTI for the native target. Catch entry-point failures and contain
      exceptions at SDL/worker C callback boundaries; preserve pthread cancellation control flow.
- [x] Repair discovered zero-allocation dependencies in stream special-state flags, record data flags, drag-list hover
      color and copied object-reference flags. Replace animation sound-request byte clearing with typed assignment.
      Use a normal nothrow new expression in the recoverable allocation helper.
- [x] Fix native type errors exposed by RTTI/UBSan: align the shared stream lock through its API type,
      use common-base mission/facility persistence bodies, read actual system records and share AI role access.
- [x] Complete the member/raw-allocation audit and remove the transitional zero-filling `operator new` overrides.
      Allocation failures already follow the throwing-new contract; do not assume this proves every object is initialized.
- [x] **A1 exit:** a clean modern build/run no longer needs original C++ layout metadata, address-bound symbol
      identities or EXE startup-store replay. Historical names/comments in source are ordinary identifiers, not build inputs. It may still require original game assets.

Final validation: macOS arm64, Apple Clang 17; fresh C++23 build in `build/native_a1_finish`, focused string/file
stream round-trip, failure-state and frozen-buffer lifetime tests, and two-process save/reload simulation
(150336 → 150346 → 150356). Release and halt-on-error UBSan runs pass with `MallocScribble=1` and
`MallocPreScribble=1`; vptr checking is enabled for every maintained translation unit. The existing function-pointer
sanitizer exception remains in the native flags. Raw allocation sites were inventoried and their immediate
initialization paths reviewed; discovered omitted member defaults were repaired. This is source review plus executable
coverage, not a proof that every gameplay path is initialized. Independent reviews covered startup/service ownership,
stream clients and teardown. Linux/Windows checks remain future host work under the platform rule.

## A2 — Mechanical cleanup in bulk

- [x] Add a curated `.clang-tidy` for the four reviewed mechanical modernization checks and selected
      bugprone/performance diagnostics after surveying them.
- [x] Apply `modernize-use-nullptr` as a reviewed, buildable batch (nine sites; use value initialization for
      `pthread_t` because its host representation varies).
- [x] Apply `modernize-use-override` as a reviewed, buildable batch (declaration-only changes).
- [x] Apply `modernize-use-equals-default` as a reviewed, buildable batch. Retain first-declaration inline
      constructor bodies where defaulting could change value-initialization behavior (13 constructors in five tactical
      headers; their repeated tidy diagnostics are intentional).
- [x] Run `modernize-redundant-void-arg` across maintained source and tests; no eligible fixes. Remaining
      `(void)` prototypes are C-linkage platform APIs.
- [x] Survey bugprone and performance checks across maintained translation units. Fix the path-concatenation
      finding, make intentional pointer-sized copies explicit, and enable five selected diagnostics. Keep virtual-name
      near misses and by-value API changes as survey findings pending responsibility-level review.
      Commands and decisions: [clang_tidy.md](../clang_tidy.md), [clang_tidy_findings.md](../clang_tidy_findings.md).
- [x] Enable focused return, initialization and polymorphic-deletion errors in the modern context/display/stream
      files; restore deprecated-declaration and writable-string warnings there. Replace the last maintained member-offset
      picture read with typed access and retire the global invalid-`offsetof` suppression. Fix the exposed error-text
      format/copy issue and raw string transfer in ship notices; leave unrelated legacy diagnostics for their rewrites.

A2 validation: LLVM 22.1.4 processed 502 maintained source translation units and two test files, without formatting.
Each changed family passed an Apple Clang 17 macOS arm64 build and two-process save/reload smoke before commit.
The final integration passes the focused stream tests and both release and halt-on-error UBSan save/reload runs
with malloc pre-scribble/scribble enabled. Constructor exceptions and survey-only signature findings are recorded
in the linked tooling notes; Linux/Windows verification remains host follow-up.

Defer all clang-format runs until the rewrite is complete, including formatting touched files, to avoid churn during
semantic changes. The single final formatting pass is described below.

Run clang-tidy against real compilation commands. Start with fixable checks such as `modernize-use-nullptr`,
`modernize-use-override`, `modernize-use-equals-default` and `modernize-redundant-void-arg`; apply each family in a separate
commit. Sample its output, inspect the complete diff for outliers, build, and run the short smoke check. Run bugprone and
performance checks broadly for findings; only enable automatic fixes after checking their assumptions. Changes to loop
iteration, ownership, moves, signedness and string lifetime deserve semantic review even when a tool offers a fix.

Enable useful warnings and fix them as subsystems are touched. Do not turn a huge inherited warning backlog into an
up-front gate. Newly cleaned code should stay clean; remove suppressions and legacy flags with the code requiring them.

## A3 — Rewrite loop

Maintain a short worklist of responsibilities, not a speculative architecture document. Fan out this work across multiple luna or haiku subagents.
Seed it with the existing code map and `BUG(original)` sites. Each work item copies and completes this checklist:

- [ ] Read the responsibility, its callers and its lifetime/serialization dependencies. State the intended simpler API
      and behavior in a few sentences. Identify real invariants, not incidental properties of the old implementation.
- [ ] Rewrite it and migrate all affected clients in the same change. Rename, split, merge or delete as needed. Fix bugs
      encountered in the area immediately; summarize meaningful gameplay changes in the commit.
- [ ] Run selected tidy fixes, build on the host platform, and run the cheap relevant checks below. Leave clang-format
      until the end.
- [ ] Give an independent reviewer the diff, surrounding code and intended behavior. Ask them to find concrete failures,
      especially ownership/lifetime errors, invalidated iteration, lost callbacks, save corruption, startup cycles, integer
      errors and platform assumptions. They should inspect callers and error paths, not merely critique the diff's style.
- [ ] Address actionable findings, rerun affected checks, update this roadmap, and commit a coherent, buildable change.

One independent LLM review per semantic unit is the default; a second reviewer is useful for core ownership, startup or
serialization changes. Mechanical batches need a lighter review. Review supports judgment; it cannot demonstrate that
the game runs, which is why the small executable check remains.

Suggested order after A1/A2:

- [ ] Core types, strings, ownership and container families: remove shared friction and migrate consumers with each family.
- [ ] Save boundary and object identity: decouple the on-disk representation from subsequent object redesign.
- [ ] Rules, economy, missions, AI and events: fix original bugs and make gameplay code understandable.
- [ ] Simulation scheduling and randomness: remove UI-driven AI randomness and accidental timing dependencies.
- [ ] UI models, windows and platform interfaces: simplify callers and retire Win32-shaped abstractions where useful.
- [ ] Remaining glue and dead compatibility code: remove adapters, suppressions and obsolete build paths.

This is a priority order, not a strict topological traversal. Follow related callers across areas when it makes a change
smaller overall. A large responsibility can take several commits with a temporary adapter, but name its deletion task.

### Design defaults

- Standard containers and `std::string`; `unique_ptr` for exclusive ownership, handles/IDs for long-lived cross-object
  references, and references/non-owning pointers for bounded borrows. Avoid a blanket conversion to `shared_ptr`.
- Choose containers for access and lifetime needs. Preserve meaningful priority rules, but freely replace accidental
  iteration order. Do not let pointer/hash order silently decide gameplay; use an explicit tie-break where one matters.
- Make effects and dependencies explicit. C++23 does not guarantee left-to-right function argument evaluation: when order
  matters, write separate statements. Retire blanket reconstruction checks only after retaining this ordinary correctness
  rule. Do not preserve obscure call ordering solely for original-output parity.
- Separate gameplay RNG state from UI/advisor randomness, retain seed/state for reproductions, and save gameplay RNG state.
  Reproducibility within the modern build is useful; matching the original sequence or every platform's bytes is optional.
- Use defined arithmetic and clear rounding. Remove emulation helpers when obsolete; use unsigned types for intentional
  wrapping rather than relying on signed overflow. Keep ordinary floating-point semantics; fast-math is not a cleanup goal.
- Keep a legacy-save importer at the boundary. Use a versioned modern format when the representation changes; save IDs and
  values, never memory layouts or pointers. Preserve fields that actually encode gameplay state. Check semantic round-trip
  and reference integrity, not byte identity. Write via a temporary file and replace on success; keep original saves intact.
- Give uncertain fields honest descriptive names; do not invent a confident meaning just to eliminate `m_unk`. Investigate
  uncertain behavior when needed, but do not block a whole subsystem on perfect historical understanding.

## Verification budget

The default developer loop should take minutes, not a play session. Reuse the existing `REB_SIM` machinery and fixtures;
change its success criteria rather than building a new scenario framework or enlarging the golden corpus.

| When                               | Required check                                                                                                      |
| ---------------------------------- | ------------------------------------------------------------------------------------------------------------------- |
| Each semantic commit               | Incremental host build; short load → bounded ticks → save → reload → more ticks smoke; independent review           |
| Ownership, startup or save changes | Same smoke under ASan/UBSan on a supported host; focused invariant or round-trip check                              |
| Existing relevant automated tests  | Run the affected tests; no need to run unrelated legacy fidelity suites                                             |
| Each integration batch             | Host-platform build and short smoke; record platform discrepancies as later work                                    |
| End of the rewrite                 | Host-platform build, existing useful tests, one bounded sanitizer simulation, one structured manual acceptance pass |

The smoke has a timeout and checks successful loading, tick progress, successful save/reload and basic object-reference
integrity. It does not compare game outcomes against the reconstruction. Target well under a minute of runtime; measure
the existing harness before selecting a tick count. Configure sanitizer runs to fail on diagnostics (halt-on-error or
explicit report detection); today's `-fsanitize-recover=all` can otherwise produce a misleading successful exit.
Tiny direct tests are worthwhile for a discovered crash, tricky
arithmetic, lifetime or serialization bug; coverage percentages and mirrored implementation tests are not objectives.

Keep CI possible without publishing game data: compile in public jobs where feasible; asset-dependent runs use a local or
private installation. If a platform check is unavailable, leave its roadmap box unchecked. A failure found later on
another platform is an ordinary follow-up and does not invalidate a change that passed on its development platform.

- [ ] Check the root `.clang-format` options against the pinned formatter and C++23 syntax.
- [ ] Run one final formatting pass over first-party maintained code, excluding third-party, generated and archival code.
- [ ] Put the formatting-only pass in a dedicated commit and record it in `.git-blame-ignore-revs`.
- [ ] Run the host-platform build and automated checks on the formatted tree.

- [ ] On the current primary desktop, complete one substantial play session using representative saves to reach both
      factions, missions/research/recruitment, fleet movement, economy, combat resolution, and save/reload without waiting
      through a full campaign. Fix findings and repeat affected paths; there is no required second campaign or complete rerun
      for every fix.

## Recovery and completion

Keep commits small enough to understand but complete enough to build. Record purposeful behavior changes and the checks
run in commit messages. For a late bug, first reproduce it from a copied save with seed, actions and build recorded;
inspect the responsible code, or bisect when the regression is reproducible. Bisect is most useful with buildable commits
and a reusable fixture. Save-format transitions and nondeterministic failures can complicate it, so it is a recovery tool,
not a substitute for the minute-long smoke check.

- [ ] The old layout/startup coupling is gone.
- [ ] Core interfaces and ownership are clear.
- [ ] Known bug sites are fixed or explicitly deferred with a concrete reason.
- [ ] Every first-party area has been assessed; not every file needs a diff.
- [ ] The final host-platform acceptance pass succeeds.

No fidelity score, zero-warning crusade, exhaustive campaign certification, or simultaneous cross-platform sign-off is
required.

Recommended sequence: **C++23 host build → remove layout/startup coupling → bulk tooling fixes →
rewrite responsibility + clients → adversarial review + cheap executable check → commit → one clang-format pass →
final host checks and one acceptance pass.**
