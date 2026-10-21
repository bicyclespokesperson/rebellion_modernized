# Modernization decisions

The policy behind turning the reconstruction into a maintainable modern game, adopted October 11, 2026. The bulk
rewrite is done: the native build uses C++23, original layout/startup coupling and the hybrid build are gone, the
generic reconstructed containers and `String` class are replaced by standard types, and one formatting pass has run
(commit `0204a34`, recorded in `.git-blame-ignore-revs`). Individual changes are recorded in Git history. This file
keeps the decisions that still guide new work and the items that remain open.

## Decision

Optimize for a maintainable, working, improved game. The reconstruction has served its purpose: use it as source
material, not as a specification for every observable detail. Fix bugs while rewriting, change interfaces and their
callers together, replace accidental ordering with explicit rules, and delete obsolete compatibility machinery.

The unit of work is **one responsibility plus its clients**. A file can be that unit, but a container family,
ownership chain or initialization sequence often spans several files. Splitting, merging and deleting files is
encouraged. Already-clear code does not need rewriting just to mark it done. Each semantic change gets an independent
adversarial review, a host-platform build and a short automated smoke check before commit.

## What we accept, and what still matters

Accept changed AI choices, random sequences, tie-breaking, timings, floating-point results, serialization bytes and
minor presentation differences. Fix known original bugs by default; no `classic_rules` switch or dual implementations.
Small differences are acceptable, but are not evidence of improvement by themselves. Review should identify a plausible
reason for a change instead of assuming that cleaner-looking code necessarily plays better.

Keep the recognizable game, both factions, working missions/economy/combat resolution, usable UI, and working saves.
Existing saves should import; modern saves must round-trip. Writing saves readable by the 1998 executable is not a goal.
Multiplayer restoration, a new renderer and wholesale balance redesign are separate projects.

No byte-identical simulation gate, screenshot parity gate, expanded golden corpus, long UI comparisons, per-step
playthrough, or obligatory test for every rewritten function. The existing annotated tag `decompile-working-on-macos`
at `7df8d105c5db53cfa18bc5d227e11eaee6d903f6` is the known-good macOS baseline (annotation: “decompile working on macos”).
Old code and tools remain available there for archaeology; do not maintain a second active fidelity branch.

## Platform rule

Each change only needs to build and pass its relevant checks on the platform where the agent is working. A macOS agent
does not need Linux or Windows access; a Linux agent does not need to find a way to test macOS; the same rule applies on
Windows. Record the host platform and checks run, and do not claim checks that were not run. Platform discrepancies are
usually small and can be fixed by a later agent working on that platform. Cross-platform status is never a prerequisite
for an individual change or a reason to block work.

### C++23 support policy

Target current stable Clang toolchains and current desktop operating systems; preserving the existing macOS 11 deployment
floor is not a requirement. Target matrix: Apple Clang/libc++ on macOS arm64, LLVM Clang/libstdc++ on Linux x86-64, and
clang-cl with the Windows SDK on Windows x64. Retain existing macOS x86-64 and Linux arm64 builds where practical. GCC and
the MSVC compiler are out of scope. This matrix describes the eventual project state; each agent only builds the row
matching its host platform. The CMake build requires C++23 with compiler extensions off; clang-format and clang-tidy are
pinned to LLVM 22.1.4 through `tools/clang_tools.sh` (see [clang_tidy.md](../clang_tidy.md)).

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

## Design defaults

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
- Keep shared abstractions when they make ownership or domain behavior clearer; removing every custom class is not a
  goal. Keep straightforward loops when they already express an operation clearly; algorithm-count targets are not a goal.

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
explicit report detection); the build's `-fsanitize-recover=all` can otherwise produce a misleading successful exit.
On macOS, Apple Clang 17's ASan runtime has deadlocked before `main`, so macOS sanitizer runs have used UBSan.
Tiny direct tests are worthwhile for a discovered crash, tricky
arithmetic, lifetime or serialization bug; coverage percentages and mirrored implementation tests are not objectives.

Keep CI possible without publishing game data: compile in public jobs where feasible; asset-dependent runs use a local or
private installation. A failure found later on another platform is an ordinary follow-up and does not invalidate a
change that passed on its development platform. Enable useful warnings and fix them as code is touched; newly cleaned
code should stay clean, and suppressions go with the code requiring them.

For a late bug, first reproduce it from a copied save with seed, actions and build recorded; inspect the responsible
code, or bisect when the regression is reproducible.

## Open items

- [ ] **Desktop acceptance pass.** On the current primary desktop, complete one substantial play session using
      representative saves to reach both factions, missions/research/recruitment, fleet movement, economy, combat
      resolution, and save/reload without waiting through a full campaign. Fix findings and repeat affected paths.
- [ ] **Remaining exit criteria.** Core interfaces and ownership are clear, and every first-party area has been
      assessed (not every file needs a diff). The layout/startup coupling is gone, and no `BUG(original)` annotations
      remain.
- [ ] **Linux host validation.** The manually dispatched `linux` workflow compiles the native objects and runs unit tests
      without game data; no C++23 build, run or save/reload smoke has been recorded on a Linux host.
- [ ] **Windows runtime validation.** The clang-cl target has a Windows OS adapter, `create_build.sh` support from Git
      Bash, and a manually dispatched `windows` workflow that compiles and runs resource-free tests. Runtime smoke on a
      Windows host remains unverified.
- [ ] Give order-confirmation notices (`Notice8`) a stable object key if their message-index Open action is needed. Their
      subject order is a borrowed pointer with no proven lifetime; do not reinterpret it as key bits or dereference it
      after the order is destroyed.
- [ ] Generate explicit random tie-break keys before replacing AI ranking insertion scans with standard sorting.

No fidelity score, zero-warning crusade, exhaustive campaign certification, or simultaneous cross-platform sign-off is
required.
