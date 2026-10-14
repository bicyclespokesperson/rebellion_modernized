# Clang-tidy survey findings

The initial read-only survey used the pinned LLVM 22.1.4 clang-tidy runner with `build/native_a1_finish/compile_commands.json` and the macOS SDK sysroot. It processed 502 translation units selected from `src/game`, `src/port`, and `tests`; project headers were included in diagnostics. The checks were `bugprone-use-after-move`, `bugprone-dangling-handle`, `bugprone-sizeof-expression`, `bugprone-suspicious-memset-usage`, `bugprone-virtual-near-miss`, `performance-for-range-copy`, `performance-unnecessary-value-param`, and `performance-inefficient-string-concatenation`.

After collapsing repeated header diagnostics, the survey found 46 warning locations: 26 unnecessary value parameters, 17 virtual near-miss reports, two pointer-sized `sizeof` expressions, and one string concatenation warning. The other requested checks produced no warnings.

The path construction in `src/port/minwin_impl/text/font_files.cpp` now appends the separator and filename to the existing string, avoiding intermediate concatenations while preserving the resulting path. The Smack state storage copies in `src/port/minwin_impl/smack/smack_api.cpp` now spell the extent as `sizeof(SmackState*)`; both continue to copy exactly one pointer representation into or out of the opaque state slot.

The virtual near-miss warnings are survey-only. Hooks such as `CheckActivityStatus3` and `CheckRequestImpl2` describe distinct recovered virtual slots, as shown by their slot comments; they are not missing overrides.

The 26 by-value parameter warnings are also survey-only. ABI/address identity constraints have been retired. Before changing these signatures, audit ownership, copy behavior, lifetime, callbacks, and all declarations/call sites as part of the responsible API or ownership rewrite. In particular, custom `String` values can carry ownership semantics, and the tidy warning alone does not establish that borrowing by const reference is safe. Avoid bulk signature edits based only on this warning.

The raw survey output is retained at `work/a2/clang-tidy-bugprone-performance.log` for detail and reproducibility. The two port changes were rechecked with the relevant bug-prone and concatenation checks; output is in `work/a2/clang-tidy-port-recheck.log` with no warnings.
