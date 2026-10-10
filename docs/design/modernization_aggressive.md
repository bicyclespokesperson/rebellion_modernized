# Design: aggressive modernization

Status: second proposal, October 11, 2026; implementation not started. Alternative to
[the fidelity-first plan](modernization.md), not an additional set of requirements.

## Decision

Optimize for a maintainable, working, improved game. The reconstruction has served its purpose: use it as source
material, not as a specification for every observable detail. Fix bugs while rewriting, change interfaces and their
callers together, replace accidental ordering with explicit rules, and delete obsolete compatibility machinery.

Use C++23. Apply mechanical cleanup early. Then modernize by responsibility, with an independent adversarial review,
a build and a short automated smoke check before each commit. Save substantial manual playtesting for the end.

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

## A0 — Establish the modern build

1. Use `decompile-working-on-macos` as the existing known-good baseline and record the build/run commands.
   Use copies of saves for development runs.
2. Enable C++23 on our targets, with the standard required and extensions disabled as the target end state. Keep narrowly
   needed legacy compiler extensions temporarily; remove them as their users disappear. Do not impose our flags on SDL
   or FreeType. Export compilation databases and pin clang-format/clang-tidy versions.
3. Build the existing native game in C++23 on macOS and Linux. Adapt generator parsing flags too: changing the top-level
   CMake setting does not prove the Python/libclang generators understand the same source language.
4. Establish a modern Windows x64 target using clang-cl and the MSVC STL/SDK, normal CRT linking and SDL/minwin. This is
   real port work: the current Windows standalone target is i686, uses `/NODEFAULTLIB`, and reconstructs startup/data;
   the current native platform selection only handles macOS and Linux. Provide the Windows OS adapter and resolve
   Win32/minwin naming and import boundaries. Do not promise Windows support based on a compiler flag alone.
5. Adapt the existing smoke harness to check save write/close failures and reload its output in a second process,
   advance again, and validate basic references. Its current save-success return is insufficient. Exercise normal
   application teardown too; the existing `ExitProcess` shortcut does not test shutdown ownership. Finish one such
   startup/load/short-simulation/save/reload run on each modern platform before the broad rewrite loop.

Steps A0 and A1 may interleave where the Windows target needs the new initialization path. Keep macOS/Linux building
during that work; establish Windows before large interface changes accumulate. Normal CRT startup also means deliberately
retiring conflicting CRT replacements and allocator overrides, rather than linking two versions of them together.

### C++23 support policy

Target current stable toolchains and current desktop operating systems; preserving the existing macOS 11 deployment
floor is not a requirement. Initial matrix: Apple Clang/libc++ on macOS arm64, Clang/libstdc++ on Linux x86-64, and
clang-cl/MSVC STL on Windows x64. Retain existing macOS x86-64 and Linux arm64 builds where practical; they need not each
receive manual playthroughs. Supporting GCC or MSVC's compiler as additional frontends is optional.

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

The current native build still depends on `symbols/symbols.csv`, original global addresses, compiled symbol names,
`native_data.json`, and static initializer stores recovered from the EXE. See `tools/gen_native_data.py` and
`tools/gen_native_init.py`. Removing layout assertions alone does not remove this coupling.

- Replace reconstructed program globals and startup calls with typed definitions and explicit initialization/shutdown.
  Group ownership in a small application/game context and pass the services each subsystem needs. Avoid merely replacing
  dozens of globals with one global service locator.
- Separate shipped game data from program initialization. Continue extracting proprietary tables/resources from the
  user's installation where needed, but emit a stable asset schema independent of C++ symbols and object offsets.
  Do not check extracted proprietary bytes into source. Delete the old generators once no live build depends on them.
- Preserve real initialization dependencies explicitly; remove incidental dependencies on translation-unit order,
  constructor side effects, or original memory addresses. Break cycles by moving shared types or responsibilities and
  passing dependencies explicitly, not by introducing an interface for every class.
- Remove hybrid/hook/difftest build requirements, original-layout assertions and obsolete calling conventions from the
  modern targets. Archive their implementation in Git. Keep archaeology tools only where still useful independently.
- Restore normal allocation and error handling. Audit code relying on zero-filled allocations before switching it to
  ordinary `new`/`make_unique`; initialize members explicitly. Use RAII; enable normal exception support, catch failures
  at application/callback boundaries, and use explicit results for routine recoverable errors. No exceptions may unwind
  through C callbacks. RTTI can use the compiler default; there is no need to emulate the old ABI.

Exit: a clean modern build/run no longer needs original C++ layouts, function names or EXE startup-store replay. It may
still require original game assets. This is the one enabling migration worth doing ahead of the broad cleanup.

## A2 — Mechanical cleanup in bulk

Add a curated `.clang-tidy`. Defer all clang-format runs until the rewrite is complete, including formatting touched
files, to avoid churn during semantic changes. The single final formatting pass is described below.

Run clang-tidy against real compilation commands. Start with fixable checks such as `modernize-use-nullptr`,
`modernize-use-override`, `modernize-use-equals-default` and `modernize-redundant-void-arg`; apply each family in a separate
commit. Sample its output, inspect the complete diff for outliers, build, and run the short smoke check. Run bugprone and
performance checks broadly for findings; only enable automatic fixes after checking their assumptions. Changes to loop
iteration, ownership, moves, signedness and string lifetime deserve semantic review even when a tool offers a fix.

Enable useful warnings and fix them as subsystems are touched. Do not turn a huge inherited warning backlog into an
up-front gate. Newly cleaned code should stay clean; remove suppressions and legacy flags with the code requiring them.

## A3 — Rewrite loop

Maintain a short worklist of responsibilities, not a speculative architecture document. Seed it with the existing code
map and `BUG(original)` sites. Each work item follows this loop:

1. Read the responsibility, its callers and its lifetime/serialization dependencies. State the intended simpler API and
   behavior in a few sentences. Identify real invariants, not incidental properties of the old implementation.
2. Rewrite it and migrate all affected clients in the same change. Rename, split, merge or delete as needed. Fix bugs
   encountered in the area immediately; summarize meaningful gameplay changes in the commit.
3. Run selected tidy fixes. Build and run the cheap checks below; leave clang-format until the end.
4. Give an independent reviewer the diff, surrounding code and intended behavior. Ask them to find concrete failures,
   especially ownership/lifetime errors, invalidated iteration, lost callbacks, save corruption, startup cycles, integer
   errors and platform assumptions. They should inspect callers and error paths, not merely critique the diff's style.
5. Address actionable findings, rerun affected checks, and commit a coherent, buildable change. Use another review pass
   only for material revisions or unresolved findings; no recursive debate over speculative concerns.

One independent LLM review per semantic unit is the default; a second reviewer is useful for core ownership, startup or
serialization changes. Mechanical batches need a lighter review. Review supports judgment; it cannot demonstrate that
the game runs, which is why the small executable check remains.

Suggested order after A1/A2:

| Area | Main payoff |
| --- | --- |
| Core types, strings, ownership and container families | Remove shared friction; migrate consumers with each family |
| Save boundary and object identity | Decouple the on-disk representation from subsequent object redesign |
| Rules, economy, missions, AI and events | Fix original bugs; make gameplay code understandable |
| Simulation scheduling and randomness | Remove UI-driven AI randomness and accidental timing dependencies |
| UI models, windows and platform interfaces | Simplify callers and retire Win32-shaped abstractions where useful |
| Remaining glue and dead compatibility code | Remove adapters, suppressions and obsolete build paths |

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

| When | Required check |
| --- | --- |
| Each semantic commit | Incremental host build; short load → bounded ticks → save → reload → more ticks smoke; independent review |
| Ownership, startup or save changes | Same smoke under ASan/UBSan on a supported host; focused invariant or round-trip check |
| Existing relevant automated tests | Run the affected tests; no need to run unrelated legacy fidelity suites |
| Each integration batch | macOS/Linux/Windows builds and the short smoke; catch platform breakage before starting another batch |
| End of the rewrite | Full build matrix, existing useful tests, one bounded sanitizer simulation, one structured manual acceptance pass |

The smoke has a timeout and checks successful loading, tick progress, successful save/reload and basic object-reference
integrity. It does not compare game outcomes against the reconstruction. Target well under a minute of runtime; measure
the existing harness before selecting a tick count. Configure sanitizer runs to fail on diagnostics (halt-on-error or
explicit report detection); today's `-fsanitize-recover=all` can otherwise produce a misleading successful exit.
Tiny direct tests are worthwhile for a discovered crash, tricky
arithmetic, lifetime or serialization bug; coverage percentages and mirrored implementation tests are not objectives.

Keep CI possible without publishing game data: compile in public jobs where feasible; asset-dependent runs use a local or
private installation. If a platform check is unavailable, record it as unverified rather than silently claiming coverage.
Do not finish the project with an untested Windows target.

The root `.clang-format` was copied from [Meneldor](https://github.com/bicyclespokesperson/meneldor_chess_engine/blob/main/.clang-format).
Once the rewrite is complete, check its options against the pinned formatter and C++23 syntax, then run it once over first-party maintained code, excluding
third-party, generated and archival code. Put that formatting-only pass in a dedicated commit and record it in
`.git-blame-ignore-revs`. Run the final build and automated checks on the formatted tree before manual acceptance.

At the end, do one substantial play session on the primary desktop, using representative saves to reach both factions,
missions/research/recruitment, fleet movement, economy, combat resolution, and save/reload without waiting through a full
campaign. Briefly launch/load/save and check input/audio on the other two OSes. Fix findings and repeat affected paths;
there is no required second campaign or complete rerun for every fix.

## Recovery and completion

Keep commits small enough to understand but complete enough to build. Record purposeful behavior changes and the checks
run in commit messages. For a late bug, first reproduce it from a copied save with seed, actions and build recorded;
inspect the responsible code, or bisect when the regression is reproducible. Bisect is most useful with buildable commits
and a reusable fixture. Save-format transitions and nondeterministic failures can complicate it, so it is a recovery tool,
not a substitute for the minute-long smoke check.

Finish when modern targets build/run on the three OSes, the old layout/startup coupling is gone, core interfaces and
ownership are clear, known bug sites have been fixed or explicitly deferred, and the final acceptance pass succeeds.
Every first-party area should have been assessed; not every file needs a diff. Deferred items get a concrete reason, not
a compatibility mode. No fidelity score, zero-warning crusade, or exhaustive campaign certification is required.

Recommended sequence: **C++23 build → remove layout/startup coupling and establish Windows x64 → bulk tooling fixes →
rewrite responsibility + clients → adversarial review + cheap executable check → commit → one clang-format pass →
final checks and one acceptance pass.**
