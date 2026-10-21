# Clang-tidy modernization

Use LLVM 22.1.4. `tools/clang_tools.sh tidy` checks the version; set `REB_CLANG_TIDY` when the binary is outside `PATH`.
The official [LLVM release](https://github.com/llvm/llvm-project/releases/tag/llvmorg-22.1.4) includes `clang-tidy`,
`clang-apply-replacements`, and the parallel `run-clang-tidy` driver. Keep downloaded tools outside tracked source.

Configure the maintained native target first and use its `compile_commands.json`. Select only compiled first-party
sources under `src/game`, `src/port`, and `tests`; skip third-party dependencies and archival targets. The root
`.clang-tidy` limits header diagnostics to maintained source. macOS runs using a separate LLVM distribution need the
Apple SDK explicitly because the Apple Clang compilation database may omit its implicit sysroot.

Apply one modernization family at a time. For example, on macOS:

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
separate. The configuration also specifies `FormatStyle: none`.

Run bugprone and performance surveys without `-fix`; diagnostics do not authorize bulk signature changes. Two kinds
of warning are known: `bugprone-virtual-near-miss` reports on hooks such as `CheckActivityStatus3` and
`CheckRequestImpl2` are intentional, distinct recovered virtual slots, not missing overrides; and
`performance-unnecessary-value-param` alone does not make borrowing by const reference safe (custom `String` values
can carry ownership), so do not bulk-convert by-value parameters to const references. Change such a signature only
while reviewing ownership across all its declarations and call sites. Enable or apply other checks only after
reviewing their assumptions and affected callers.

## Formatting

Formatting uses the same pinned LLVM version. `.clang-format` parses current C++ syntax and leaves qualifier order
unchanged, keeping a formatting pass limited to presentation. Format tracked C/C++ sources and headers in `src/game`,
`src/port/native`, and the maintained `src/port/minwin_impl` implementation directories, plus the minwin interface
headers, native tests, stream test support and compiled port trace/simulation files. Exclude third-party code and
generated include data. Record formatting-only commits in `.git-blame-ignore-revs`; use
`git config blame.ignoreRevsFile .git-blame-ignore-revs` locally to omit them from blame.

## Additional automatic modernization checks

The configuration also enables 16 fixable checks: `modernize-concat-nested-namespaces`,
`modernize-deprecated-headers`, `modernize-deprecated-ios-base-aliases`, `modernize-loop-convert`,
`modernize-raw-string-literal`, `modernize-return-braced-init-list`, `modernize-shrink-to-fit`,
`modernize-type-traits`, `modernize-unary-static-assert`, `modernize-use-auto`, `modernize-use-bool-literals`,
`modernize-use-equals-delete`, `modernize-use-noexcept`, `modernize-use-starts-ends-with`,
`modernize-use-transparent-functors`, and `modernize-use-using`.
Loop conversion requires `MinConfidence: safe`; alias conversion skips `extern "C"` blocks.

LLVM 22.1.4's alias fixes can drop pointee `const` (they did for eight aliases in `minwin/base.h`, `minwin/dsound.h`
and `minwin/winmm.h`); review alias changes for this before accepting them. Header replacements can lack fixes when
the same source has several compilation commands; a first-party compilation database with one native command per
source avoids that.
