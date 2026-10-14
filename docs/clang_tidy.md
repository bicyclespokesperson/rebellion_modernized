# Clang-tidy modernization

Use LLVM 22.1.4. `tools/clang_tools.sh tidy` checks the version; set `REB_CLANG_TIDY` when the binary is outside `PATH`.
The official [LLVM release](https://github.com/llvm/llvm-project/releases/tag/llvmorg-22.1.4) includes `clang-tidy`,
`clang-apply-replacements`, and the parallel `run-clang-tidy` driver. Keep downloaded tools outside tracked source.

Configure the maintained native target first and use its `compile_commands.json`. Select only compiled first-party
sources under `src/game`, `src/port`, and `tests`; skip third-party dependencies and archival targets. The root
`.clang-tidy` limits header diagnostics to maintained source. macOS runs using a separate LLVM distribution need the
Apple SDK explicitly because the Apple Clang compilation database may omit its implicit sysroot.

Apply one modernization family at a time. For example, on macOS:

The final formatting pass uses the same pinned LLVM version. `.clang-format` parses current C++ syntax and leaves
qualifier order unchanged, keeping this pass limited to presentation. Format tracked C/C++ sources and headers in
`src/game`, `src/port/native`, and the maintained `src/port/minwin_impl` implementation directories, plus the minwin
interface headers, native tests, stream test support and compiled port trace/simulation files. Exclude third-party code,
generated include data, `src/hybrid`, `src/standalone`, archival platform comparison tests and minwin layout checks.
Record the formatting-only commit in `.git-blame-ignore-revs`; use `git config blame.ignoreRevsFile .git-blame-ignore-revs`
locally to omit it from blame.

```bash
LLVM_ROOT="/path/to/LLVM-22.1.4-macOS-ARM64"
BUILD_DIR="build/native"
CHECK="modernize-use-nullptr"
SDK_PATH="$(xcrun --sdk macosx --show-sdk-path)"
uv run --with pyyaml "$LLVM_ROOT/bin/run-clang-tidy" \
  -clang-tidy-binary "$LLVM_ROOT/bin/clang-tidy" \
  -clang-apply-replacements-binary "$LLVM_ROOT/bin/clang-apply-replacements" \
  -p "$BUILD_DIR" -j 8 -checks="-*,${CHECK}" \
  -extra-arg=-isysroot -extra-arg="$SDK_PATH" \
  -export-fixes "work/tidy-${CHECK}" -fix \
  '.*/(src/(game|port)|tests)/.*'
```

On another host, use its native compilation database and SDK discovery. Review every replacement and any compiler
errors, build, and run the short save/reload smoke before committing each family. Run fix collections sequentially
because different translation units can propose edits to the same header. Do not pass `-format`; formatting stays
separate until the rewrite is complete. The configuration also specifies `FormatStyle: none`.

Bugprone/performance findings are triaged in [clang_tidy_findings.md](clang_tidy_findings.md). Diagnostics do not authorize
bulk signature changes. Run a survey without `-fix`; enable or apply additional checks only after reviewing their
assumptions and affected callers.

The A2 pass intentionally retains 13 first-declaration inline constructors in the tactical headers. Defaulting those
can change value-initialization behavior. Their repeated `modernize-use-equals-default` diagnostics are reviewed
exceptions; the other three modernization families have no outstanding diagnostics on the native host configuration.

## Additional automatic modernization checks

The configuration also enables 16 fixable checks: `modernize-concat-nested-namespaces`,
`modernize-deprecated-headers`, `modernize-deprecated-ios-base-aliases`, `modernize-loop-convert`,
`modernize-raw-string-literal`, `modernize-return-braced-init-list`, `modernize-shrink-to-fit`,
`modernize-type-traits`, `modernize-unary-static-assert`, `modernize-use-auto`, `modernize-use-bool-literals`,
`modernize-use-equals-delete`, `modernize-use-noexcept`, `modernize-use-starts-ends-with`,
`modernize-use-transparent-functors`, and `modernize-use-using`.
Loop conversion requires `MinConfidence: safe`; alias conversion skips `extern "C"` blocks.

The 2026-10-13 pass surveyed 526 compiled first-party files and applied clang-tidy's exported replacements without
formatting. Duplicate header diagnostics were consolidated before applying the replacements.

| Check | Fixed sites |
| --- | ---: |
| `modernize-use-auto` | 1249 |
| `modernize-use-using` | 184 |
| `modernize-deprecated-headers` | 93 |
| `modernize-return-braced-init-list` | 16 |
| `modernize-loop-convert` | 12 |
| `modernize-use-starts-ends-with` | 3 |
| `modernize-use-bool-literals` | 2 |

The other nine added checks found no eligible changes. Five braced-return diagnostics have no automatic fixes and
remain unchanged: three in `app_save_load.cpp`, one in `rebapp_globals.cpp`, and one in `dib_hd_tests.cpp`.

LLVM 22.1.4's alias fixes dropped pointee `const` from eight aliases in `minwin/base.h`, `minwin/dsound.h`, and
`minwin/winmm.h`. Spot fixes restored the original qualifiers; review alias changes for this issue before accepting
future fixes. The native build, all 18 CTest tests, and the two-process save/reload smoke passed after these repairs.

Two header replacements initially lacked fixes because the same source had multiple compilation commands. A
first-party compilation database retaining one native command per source let clang-tidy apply those replacements.
The survey, exported fixes, and validation logs are retained locally under `work/tidy-modernize/`.
