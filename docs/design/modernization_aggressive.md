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

### Current responsibility worklist

Settings dictionary: own names and values in a standard container, retain newest-first duplicate lookup, and return
settings copies as ordinary values. Copying preserves lookup priority; malformed, missing or overflowing numeric values
leave existing settings unchanged. Spaces and dashes remain legacy separators, and negative stored values convert to
signed floating-point values correctly. This dictionary has no save representation.

- [x] Read responsibility, clients and lifetime/serialization dependencies; define API and invariants.
- [x] Replace intrusive list ownership with `std::vector`/`std::string` and migrate settings-copy clients.
- [x] Run selected tidy checks, host builds, focused parser/copy tests and save/reload smoke.
- [x] Complete independent review of callers, parser progress, copy order and error paths.
- [x] Address findings, verify and commit the coherent change.

Elapsed-time clock: use `std::chrono::steady_clock` for construction/reset-to-now and elapsed seconds. The app's wait and
tactical frame timing borrow this value; it is not serialized. Retire split performance-counter words, truncated
frequency, platform fallback and reconstructed padding.

- [x] Read responsibility, timing callers and lifetime/serialization dependencies; define API and invariants.
- [x] Replace platform counter emulation with a private monotonic time point and retain timing clients.
- [x] Run selected tidy checks, host builds and save/reload smoke.
- [x] Complete independent review of clients and confirm no raw layout dependency.
- [x] Address findings, verify and commit the coherent change.

Validation for these completed units: macOS arm64, Apple Clang 17; release and halt-on-error UBSan builds, stream and
settings CTests, and two-process simulation/save/reload (150336 → 150346 → 150356). LLVM 22.1.4 selected tidy checks
report no findings in the rewritten implementations. Formatting remains deferred.

Integer rules math and map distance: retain the small helper API, define signed ceiling division, use wide ratio and
coordinate intermediates, and compute the floor square root without overflowing squares. Zero division throws
`std::domain_error`; results outside `int` throw `std::overflow_error`. Exact roots (including 4) now produce correct
travel distances, and ratio products no longer wrap before division. Recordless ship/squad loss calculations avoid zero
denominators. Invalid loaded travel parameters use the same checked division policy; valid truncation remains unchanged.

- [x] Read responsibility, gameplay callers and data/serialization dependencies; define arithmetic invariants.
- [x] Rewrite arithmetic and migrate map-distance/travel and zero-denominator fleet clients.
- [x] Run selected tidy checks, host builds, boundary/exception tests and save/reload smoke.
- [x] Complete independent review of integer ranges, signed rounding and callers.
- [x] Address zero-denominator findings, verify and commit the coherent change.

Math validation: macOS arm64 release and halt-on-error UBSan builds; all three CTests (streams, settings, integer math)
and malloc pre-scribble/scribble two-process save/reload smoke pass. Boundary tests cover signed ceiling quadrants,
`INT_MIN`/`INT_MAX`, `UINT64_MAX` square root, wide products with representable results and rejected invalid arithmetic.
Selected LLVM 22.1.4 tidy checks report no findings in the changed implementations. Formatting remains deferred.

String values and legacy codec: remove the custom `String` class wholesale. Gameplay, notices and UI now use
`std::string` directly; localized resources and the legacy uint16-length save representation have explicit free functions.
Read failures preserve the destination, oversized writes fail before emitting bytes, and ordinary allocation failures
throw. Nullable C-string boundaries coalesce to empty values. Remove the raw label copy and movie-notice byte offsets.
Embedded NUL bytes imported from a save remain in the value rather than being truncated during the next write.

- [x] Read responsibility, clients, text borrows and save dependencies; choose plain standard values.
- [x] Remove the wrapper and migrate all maintained clients, resource loads and legacy codecs.
- [x] Run selected tidy checks, host builds, string boundary/value tests and save/reload smoke.
- [x] Complete independent review of ownership, cursors, nullable inputs and codec failures.
- [x] Fix findings and commit the coherent change.

Queues and stacks: own stable nodes directly with `std::list<std::unique_ptr<LinkNode>>`. Pop/dequeue explicitly transfer
ownership; reject null, duplicate or foreign insertions. Detach all views before node destruction and reject insertion
from destructor callbacks while clearing. Copies of nodes start detached; owners cannot be copied or moved. Public link
views remain temporary clients of owned storage; migrate them with the remaining container families.

- [x] Read responsibility, clients, mutation and destruction rules.
- [x] Replace queue/stack ownership and add shared link membership checks for sorted-list interoperability.
- [x] Run host builds, transfer/copy/reentrant-clear tests and save/reload smoke.
- [x] Complete independent review of ownership, links and callbacks.
- [x] Fix reentrant insertion and commit the coherent change.

Gameplay and presentation RNG: AI draws use the GameRoot generator, whose state already saves/loads. Advisors retain a
separate presentation stream; restore its session field rather than discarding it. Use standard bounded distributions,
retain seed warm-up state, define wide modular steps and reject invalid saved generator states. This deliberately changes
AI random sequences and fixes out-of-range draws; save representation remains the existing int32 generator state.

- [x] Read all random clients, ownership, seed and persistence dependencies.
- [x] Split gameplay/presentation draws and replace overflow-prone range calculations.
- [x] Run host builds, range/state/independence tests and save/reload smoke.
- [x] Complete independent review of bounds, restored state and missing-generator behavior.
- [x] Preserve the absent-generator lower-bound fallback and commit the coherent change.

Validation for these three units: macOS arm64, Apple Clang 17 release and halt-on-error UBSan builds; all six CTests and
malloc pre-scribble/scribble two-process simulation/save/reload pass (150336 → 150346 → 150356). Independent reviews
covered string clients/codecs, reentrant container teardown and both RNG streams. LLVM 22.1.4 selected tidy checks run
on the core implementations. Formatting remains deferred.

Ordered containers and identity: replace AVL balancing and intrusive list ownership with standard containers owning
stable nodes. Tree queries remain logarithmic; sorted lists retain insertion-time comparisons, including AI tie breaks.
`Remove` transfers ownership to its caller, while `Clear` destroys held nodes. Reject foreign membership, duplicate
identity and exhausted generated IDs; composite family/ID trees use their actual comparator for identity.

- [x] Read ownership-transfer, composite-key lookup and mutation clients.
- [x] Replace trees and sorted-list storage and migrate binary lookup clients.
- [x] Complete final host builds, malformed-count and ownership tests, and save/reload smoke.
- [x] Complete independent reviews; fix detach/delete mismatch and composite-key identity findings.
- [x] Commit the checked container and serialization integration.

Save and stream boundary: use typed input/output interfaces and preserve explicit legacy field encodings. Write a sibling
temporary save and replace the destination only after serialization and close succeed. Propagate failed restores, reject
bad position markers and check count widths. Reallocate legacy stream buffers using offsets within their own allocation;
zero new capacity so seeking into unwritten space cannot expose heap bytes.

- [x] Read save/load callers, stream lifetime, marker and collection codecs.
- [x] Implement atomic replacement, typed restore/save results and checked codecs.
- [x] Complete final host builds, failure preservation/stream tests and save/reload smoke.
- [x] Independently review failure cleanup, close state, count symmetry and seek growth.
- [x] Commit after resolving the final count-reader and seek-test findings.

Gameplay and UI responsibilities: fix live original bug sites rather than preserve erroneous behavior. Changes include
AI ranked misses, restored notice groups, sector scan field order, target-sector selection, rescue flags, planner type
checks, finished states, retained surplus orders, single application of effects, record totals, random facility selection,
and failed allocation/resource/event cleanup. Jedi training now checksums its persisted teacher key through its existing
base implementation. Death Star sabotage deliberately retains the current fresh ranked selection on each pass; the old
comment described a retention branch absent from the maintained implementation.

UI fixes cover invisible selection rows, empty lists, manufacturing rows, advisor controls, drag-image lifetime,
paint/bitmap lifetime, scrolling and font bounds. Movie callbacks post completion before teardown; paths use ordinary
strings and callback GUIDs are owned values. Message templates parse bounded resource records. All 320 shipped TEXTSTRA
message templates use direct field codes 1–4; undefined high-bit fields are safely skipped rather than indexing object memory.

- [x] Read annotated sites and their callers across rules/AI/events/UI/audio/video/multiplayer.
- [x] Fix live bugs and remove annotations already made obsolete by earlier changes.
- [x] Complete final host build and cheap simulation/save/reload checks.
- [x] Independently review gameplay/UI changes and callback/ownership behavior.
- [x] Commit the validated behavior fixes and record the intentional gameplay changes.

Bitmap responsibility: own software bitmap bytes with a vector, validate file/resource extents and decode RLE8 once at
load. Rendering and hit tests then share the uncompressed path, preserving row orientation and transparency. Remove the
unbounded runtime RLE walkers; fix palette expansion, clipping, row/column zero and destination coordinates.

- [x] Read resource/file ownership, pixel consumers, orientation and all RLE commands.
- [x] Replace RLE walkers and bitmap storage, migrate consumers and fix blitter bugs.
- [x] Complete final host builds, bounded decoder tests and desktop rendering checks.
- [x] Review pixel extent, palette, constructor and reload failures; fix actionable findings.
- [x] Commit the validated bitmap unit.

Validation for this integration: macOS arm64, Apple Clang 17 release and halt-on-error UBSan; all eight CTests pass.
Malloc pre-scribble/scribble simulation/save/reload passes in both builds (150336 → 150346 → 150356). Desktop intro,
main-menu bitmaps and quit confirmation render correctly. Selected LLVM 22.1.4 tidy review found only an override annotation,
which is corrected. The broader desktop acceptance and final formatting remain pending.

Final A3 integration:

- [x] Complete the host build and all eight useful native tests in release and halt-on-error UBSan.
- [x] Complete the bounded simulation/save/reload smoke with malloc scribble enabled.
- [ ] Finish the structured desktop acceptance pass below and fix findings.
- [x] Complete the dedicated formatting commit and post-format checks below.

Final ownership review found and repaired fleet-row insertion/removal that bypassed the owning sorted list. Explicit
insertion now updates owned storage and borrowed links together; removal detaches ownership before deletion. Button
animation frames and their bitmaps now use standard exclusive ownership with stable borrowed frame pointers.

Desktop acceptance remains open: the macOS automation service captures the SDL window and sends keyboard input, but
coordinate clicks fail with `noWindowsAvailable`, including with only one running instance and after rebinding/relaunch.
Using a uniquely identified test bundle avoids that service error, but message tracing shows that clicks and drags at
different requested coordinates all deliver the same stale SDL mouse position (629,12), so reliable interaction is still
blocked. The main menu has no keyboard start/load navigation. Intro/menu/confirmation rendering was inspected; this does
not substitute for the substantial gameplay acceptance pass. The disposable process and temporary default configuration
were cleaned up. Linux/Windows testing remains work for agents on those hosts.

Final formatting: LLVM 22.1.4 processed 896 maintained C/C++ sources and headers; 874 changed in formatting-only commit
`0204a34`, recorded in `.git-blame-ignore-revs`. Third-party, generated and archival files are excluded. Current-language
parsing is enabled and qualifier order is left unchanged. The repeat formatter check, post-format release and strict UBSan
builds, all eight CTests in both builds and malloc-scribble save/reload smoke pass (150336 → 150346 → 150356).

### Container clients and retirement of the remaining generic bases

Use ordinary iteration over standard owning storage. Nodes no longer carry predecessor/successor or next/previous
links, and containers no longer maintain borrowed head/tail/top pointers or duplicate list counts. Keep bounded borrows
and stable identity where clients need them; increment an iterator before removing its current entry. ID cursors that
span callbacks locate their position through the owning registry instead of walking an object's hidden links.

- [x] Inventory link traversal and migrate gameplay, UI, multiplayer, audio and tactical clients to standard iteration.
- [x] Remove node links, node-only traversal methods, copied list views and obsolete rewind operations.
- [x] Delete `NodeQueue`, `NodeStack`, `OrderedTree` and `AnimationStepQueue`; use typed standard storage for posted
      notifications, production keys, advisor payload groups, sound events, animation steps and popup entries.
- [x] Replace notification sentinel casts with the actual typed payload group and bound notice payload borrows to dispatch.
- [x] Review mutation, ordering, ownership and save readers; retain useful ownership/ID tests and add erase-during-iteration coverage.
- [x] Complete host release/strict UBSan builds, useful tests and bounded save/reload, then commit this migration.

The inherited generic owners `AvlTree`, `IdAvlTree`, `AvlSet`, `IntKeyedSet`, `IdValueTree`, `SortedLinkList` and
`IdSortedLinkList` have been retired. Keep shared abstractions when they make ownership or domain behavior clearer;
removing every custom class is not a goal. Domain objects compose `IdRegistry<T>`, backed by
`std::map<uint32_t, unique_ptr<T>>`, to share ID allocation, exhaustion, duplicate rejection and ownership transfer.
Standard maps already provide a balanced tree; a separate AVL implementation is unnecessary.

- [x] Migrate ID registries and composite object-key registries to concrete entry types, preserving legacy field codecs.
      Share ordinary ID ownership through composition; retain explicit multi-registry teardown where destructors call back
      across registries.
- [x] Migrate ordered event, window, sound and tactical owners to typed standard storage and domain comparisons.
      Store integer-keyed event kinds, status tallies and advisor/player notice values directly in standard maps.
- [x] Replace inherited sorted UI rows and AI ranking owners. Preserve UI grouping and existing ranking insertion behavior;
      use strict comparisons for standard ordered storage, including null pointers and floating-point NaNs. Existing random
      ranking ties remain in explicit insertion scans, never in a standard sorting comparator.
- [x] Delete the seven generic owner classes and `core/containers.*`. Payload identity and polymorphic lifetime support
      remain in `core/entry_identity.*`; these bases carry no container storage or traversal links.
- [x] Replace message-index casts between unrelated notice layouts with a common notice interface for text, keys and
      actions, including the confirmation notice's independently stored text.
- [x] Test registry duplicate rejection, ID exhaustion, ownership transfer and destructor callbacks; test domain ordering,
      copying and save codecs, including reference counts larger than 65535 and malformed input. Cover ranked-item
      duplicate-pointer rejection and detach ownership, including winner/missing-candidate deletion in the selector.
- [x] Complete the host release/strict UBSan builds, all eight tests and bounded save/reload for this retirement checkpoint.
- [ ] Reconsider linked storage when it improves a particular client. Prefer vectors, deques or value entries when they
      fit access and lifetime needs; use `vector<unique_ptr<T>>` where stable pointees are useful.
- [ ] Adopt standard algorithms where they clarify sorting, partitioning, bulk transformation, aggregation or lookup.
      Keep straightforward loops when they already express the operation clearly; algorithm-count targets are not a goal.
- [ ] Remove empty legacy payload bases where concrete types can supply their own polymorphic lifetime without obscuring
      domain relationships. Generate explicit random tie-break keys before any future replacement of ranking insertion
      scans with standard sorting.
- [ ] Give confirmation notices a stable object key if their message-index Open action is needed. Their retained subject
      order is a borrowed pointer with no proven lifetime; it must not be reinterpreted as key bits or dereferenced after
      the order is destroyed. The common notice interface currently exposes an unset key for this notice.

Validation for this retirement checkpoint: macOS release and halt-on-error UBSan builds pass all eight native tests,
including the ranking regression test. Both builds pass two-process save/reload smoke with malloc scribbling
(ticks 150336 → 150346 → 150356). The first integration smoke caught ranked-item removal destroying an object that
its caller expected to delete; removal now detaches ownership, and the focused test covers this contract. Independent
reviews checked registry lifetime, codecs, event ordering and detach/delete callers across AI, UI, multiplayer, audio
and tactical code. Changed C++ files pass the pinned LLVM 22.1.4 formatter.

Mission-panel crash follow-up: the reported executable still contained the retired `IdAvlTree` owner. Its duplicate
check used the overlay's drawing-depth comparator, so the mission and team pages (both 355 pixels high at y=0) compared
equal despite IDs 0x32 and 0x33. The team page was rejected, and character-list construction dereferenced its missing
background. The composed registry introduced in `5e81240` uses ID identity and already fixes this failure. A targeted
host probe with the actual STRATEGY.DLL pages verified both registrations and both character-list constructors in release
and halt-on-error UBSan, with malloc scribbling enabled. Temporary diagnostic code was removed after verification.
Rebuilt the normal `build/native` executable; all eight tests and its two-process malloc-scribble save/reload smoke pass.

Fleet-collapse follow-up: replacing intrusive traversal lost the item-specific visibility filtering. Fleet state still
defaulted to collapsed, but hidden ship rows were drawn, laid out and hit-tested. Lists now expose a borrowed visible
range and visibility-aware navigation, while owning storage and sorting retain every row. The optional `REB_UI_SMOKE=1`
check exercises collapse → expand → collapse, rendered pixels, hit testing, keyboard navigation, hidden selections and
sorting with actual fleet rows. Host release and halt-on-error UBSan save/reload smoke pass this check.

Battle-report follow-up: an empty participant section contains headings and blank rows without images. Its fill-color
lookup now checks the image itself before reading it, fixing the reported battle-end null dereference. The window reads
the actual `Notice1` payload rather than a layout stand-in, and vote confirmation carries that notice type through the UI
facade. Total-destruction artwork now requires both no winner and no retreating side; a retreat with no winner uses the
side-specific fleet artwork. The optional native UI smoke covers empty sections for both factions and all four kinds,
plus summary-art selection for destruction, retreat and victory/defeat.
Host release and halt-on-error UBSan builds pass the expanded UI check and two-process malloc-scribble save/reload smoke
(ticks 150336 → 150346 → 150356); independent review and pinned LLVM formatting/tidy checks pass.

Ship-status overlap follow-up: clearing dynamic label text reset its bottom edge to absolute zero, producing a negative
height at nonzero row positions. Unsigned status-row heights then wrapped and placed later rows over earlier ones.
Text/resource/font resets now collapse unfixed bounds at the label's own origin; status rows use signed heights and
reserve at least one normal line, including completely blank rows. The optional UI check reproduces the old failure
and verifies nonzero origins, empty values, blank rows, wrapping and rendered text. Host release and halt-on-error UBSan
save/reload smoke pass with malloc scribbling, including the existing fleet and battle-report checks.

Report audit follow-up: the assault/fleet popup now reads its actual `UiTextNotice` / `Notice15` payload rather than
the unrelated `PlayerMessage` layout stand-in. Text, artwork, navigation keys, faction data and participant sets come
from their real owners; detailed sets are accessed only for Notice15. Vote submission also carries `UiNotice*`
through the facade and session. Message dispatch uses the common text accessor, including Notice8, which does not
inherit the text-notice layout. Assault completion suppresses a stale result if its system has disappeared; the
Dagobah story report chooses artwork from Luke's typed story flags. Battle, assault/fleet, mission, vote, message-index,
advisor and encyclopedia report paths were reviewed. Native UI smoke creates the real assault popup in both faction
skins and checks title and body pixels separately, plus missing-target suppression and both Dagobah artwork branches.
Creating and disposing that fixture also found an uninitialized galaxy offscreen bitmap pointer; the constructor now
initializes it, making cleanup safe before native window creation.

HQ victory follow-up: victory mode and captured-character flags use typed fields instead of original 32-bit memory
offsets. HQ-only games do not require story-character availability or capture. The completed-step publication path
recomputes victory after setup, catching Alliance HQ destruction independently of its cause. Native UI regression
checks exercise HQ-only and standard predicates and both report payload shapes; release and halt-on-error UBSan
save/reload smoke pass with malloc scribbling, and all eight host tests pass in both builds.

Host-only validation and the existing desktop acceptance gap still apply; Linux and Windows are work for agents
running on those hosts.

### Per-responsibility workflow

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

- [x] Check the root `.clang-format` options against the pinned formatter and C++23 syntax.
- [x] Run one final formatting pass over first-party maintained code, excluding third-party, generated and archival code.
- [x] Put the formatting-only pass in a dedicated commit and record it in `.git-blame-ignore-revs`.
- [x] Run the host-platform build and automated checks on the formatted tree.

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

- [x] The old layout/startup coupling is gone.
- [ ] Core interfaces and ownership are clear.
- [ ] Known bug sites are fixed or explicitly deferred with a concrete reason.
- [ ] Every first-party area has been assessed; not every file needs a diff.
- [ ] The final host-platform acceptance pass succeeds.

No fidelity score, zero-warning crusade, exhaustive campaign certification, or simultaneous cross-platform sign-off is
required.

Recommended sequence: **C++23 host build → remove layout/startup coupling → bulk tooling fixes →
rewrite responsibility + clients → adversarial review + cheap executable check → commit → one clang-format pass →
final host checks and one acceptance pass.**
