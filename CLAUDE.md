# Star Wars: Rebellion decompilation

Faithful source reconstruction of `REBEXE.EXE` (1998, MSVC 5, 32-bit), verified function-by-function
against the original running under Wine. Read [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) first;
[docs/code_map.md](docs/code_map.md) says which code belongs to which subsystem, and
[docs/library_code.md](docs/library_code.md) covers the statically linked CRT/iostream library.

The game files are the user's licensed copy. Never commit them, or raw Ghidra output, to git.

## Tools (run from the repo root)

| Command | Purpose |
|---|---|
| `tools/ghidra_server.sh start\|stop\|status` | Long-lived Ghidra session over the analyzed EXE (main checkout only) |
| `tools/gq '<python>'` / `tools/gq -f file.py` | Query Ghidra. Globals: `program fapi listing fm addr() func() decomp()`, `name_func() name_data()`. `tools/gq --save` persists. Never `--quit` while others work. |
| `work/decomp/<addr>.c` (main checkout) | Snapshot of the decompilation of every function — grep it, cheaper than `decomp()`. Regenerated periodically; `decomp()` is always current |
| `tools/sync_types.py` | Push the layouts of classes declared under `src/game` into Ghidra as structures (decompiler then shows field names) |
| `tools/sym.py set <addr> func\|data <Name> ["(sig)"]` | Name a function/global: updates `symbols/symbols.csv` (source of truth) and Ghidra |
| `tools/build_hybrid.sh` | Build `build/hybrid/smackw32.dll` (in the dev container) |
| `tools/run_game.sh [--orig] <scenario>` | Run the game headless with this checkout's DLL; output in `work/run/<scenario>/` |
| `REB_CAPTURE=0x5f35b0:3:10:50 tools/run_game.sh <scenario>` | Capture 3 calls of an original function — after skipping 10, every 50th — into `work/run/<scenario>/captures/` (fields after the address are optional) |
| `tools/difftest.py [-j N] <captures>/*.cap` | Replay captures: original vs reconstruction (`-j` in parallel). `--orig-only` shows what the original does; `--help` lists --poke/--entry/... overrides |
| `REB_DISABLE_HOOKS=all\|0x..,0x..` | Run with some reconstructions disabled (bisecting) |
| `tools/build_native.sh [--image]` / `REB_SANITIZE=1 ...` | Native Linux x86-64 `rebellion` (build/native, ASan+UBSan in build/native_asan), docs/native_linux.md |
| `tools/run_native.sh <scenario>` / `tools/native_sim.sh` | Scenarios (Xvfb + xdotool) and the simulation goldens on the native build; `tools/native_triage.py` summarises sanitizer reports |

Never kill processes by name pattern (it kills your own shell); use the scripts' stop commands.

## Decompiling a function or class

1. **Understand it.** Read `work/decomp/<addr>.c`, the disassembly (`tools/gq 'for i in listing.getInstructions(func(0x..).getBody(), True): print(i.getAddress(), i)'`),
   its callers and callees. Work class-by-class or module-by-module (`symbols/modules.csv`), not on scattered functions.
2. **Name it.** `tools/sym.py set 0x005f35b0 func String::String "(char const *)"` for every function you
   reconstruct or call; globals as `g_camelCase`, vtables as `Class::vftable` (data). Only a class with
   *multiple* polymorphic bases has several vtables, named `Class::vftable{Base}` (e.g. `DatRecord::vftable{IdNode}`
   and `DatRecord::vftable{NameSource}`); single inheritance always uses plain `Class::vftable`. Compiler-generated scalar deleting destructors, if you name them, are
   `Class::scalar_deleting_destructor` (Ghidra rejects backticks); they are never hooked. A signature is
   required only when several original functions share a qualified name (overloads); write it as the C++
   parameter list (`(void)`, `(char const *)`, `(int,int)`), spacing is ignored.
3. **Write it** in `src/game/<module>/`. The code must be ordinary portable-ish C++:
   - Same behaviour as the original, including quirks (truncation to 16 bits, unchecked indices, odd
     return values). Mark original bugs with `// BUG(original): ...`, do not fix them in game code.
   - Classes mirror the original memory layout exactly: `REB_ASSERT_SIZE(X, 0x..)` and
     `REB_ASSERT_OFFSET(X, m_field, 0x..)` from `core/layout.h` (enforced only on 32-bit builds — never
     write code that relies on layout beyond what the original did). Unknown fields: `uint8_t m_unk10[0x20]; // +0x10`.
     Comment field offsets (`// +0x08`).
   - Declare every virtual method in vtable order, including ones you don't reconstruct (they still need
     `symbols.csv` rows so the vtable resolves; placeholder names like `Virtual05` are fine).
   - Reconstructed functions that correspond to originals must have external linkage (no `static`, no
     anonymous namespace) — the hook table references them by name.
   - Data members stay public (layout asserts use `offsetof`).
   - Taking the address of a reconstructed function (callbacks, factory registries) is fine: the post-link
     step rewrites such pointers to the original address, so pointer identity matches the original.
   - Return types: flags and predicates in this codebase almost always return a full int (0/1 in EAX) and
     callers test all of EAX — declare them `int`, not `bool`, unless the original visibly sets only AL.
     A function that ends by calling another (tail call / forwarder, `call X; ret`) returns X's EAX: if any caller
     uses it, declare it with X's return type and `return X(...)` — a `void` declaration passes difftest on its own
     but breaks callers (only `--integration` catches it).
   - Functions you call but don't reconstruct: just declare them (`int16_t FUN_00479ac0();` or a named
     declaration with a `symbols.csv` row). Calling conventions follow MSVC defaults: free functions
     `__cdecl`, members `__thiscall`, Win32 callbacks `__stdcall`.
   - Globals: `extern` declarations only (they live in the original image during the hybrid phase).
   - Win32/DirectX: include `platform/win32.h` only (never `<windows.h>` directly) and call the APIs by
     their real names, as the original does. Use pointer-sized types where Win32 does (`LONG_PTR`,
     `WPARAM`/`LPARAM`, `GetWindowLongPtr`/`SetWindowLongPtr`, `DWORD_PTR`) even though the original is
     32-bit — the port is 64-bit (see docs/platform_layer.md).
   - No STL, no exceptions, no RTTI, no `dynamic_cast`. `operator new` returns NULL on failure, as in the
     original — keep the original's null checks (the build uses `-fcheck-new` so clang keeps them). Use the CRT the original uses (`malloc`, `sprintf`,
     `strcpy`...); they resolve to the game's own CRT.
   - Prefer clear names and small helpers over comments; comment only the non-obvious *why*.
   - Do not reconstruct 3D tactical combat code (out of scope — see docs/code_map.md).
4. **Build:** `tools/build_hybrid.sh`. The generator reports `N hooks`; link errors name any symbol that has
   no definition and no address.
5. **Verify:**
   - Capture real calls with a scenario that exercises the code (`tests/scenarios/`; docs/ui_navigation.md
     lists what each reaches — e.g. `load_game` for a game in progress, `ui_tour` for the main windows,
     `space_battle_autoresolve` for a battle; new games take ~5 min because of an unskippable intro), then
     `tools/difftest.py work/run/<scenario>/captures/*.cap` must print PASS for every capture. Capture
     several calls with different `skip` values to cover different paths.
   - Captures run the fully original game by default (`REB_DISABLE_HOOKS=all` is implied), so every call
     reaches the original address. Writes to a function's own incoming argument slots are ignored
     (MSVC uses them as scratch); the slot count comes from your declared signature, so declare it exactly.
   - `tools/check_callee_pop.py` compares every hook's `RET n` with the original's — a free function or static member
     whose original pops its arguments is `__stdcall`. It covers functions no capture reaches (a wrong convention
     corrupts the caller's stack in live runs only).
   - A difftest SKIP is not a pass: it means the original could not be emulated (usually DirectX/COM
     behaviour) — such functions are checked with smoke runs only. Say so in your report.
   - Always finish with `tools/run_game.sh tests/scenarios/smoke.txt` and look at the screenshot.
6. **Feed Ghidra:** run `tools/sync_types.py` after adding or changing class layouts, so callers you
   read next decompile with field names.
7. **Commit** on your branch with a message listing the functions reconstructed and the difftest results.

## Conventions

- Branches: `JS/<topic>`. Commit trailers per the session instructions.
- `symbols/symbols.csv` merges with `merge=union`; after merging run `tools/gq -f tools/py/apply_symbols.py && tools/gq --save`.
- Python tools: type annotations, pathlib, argparse; uv scripts (`#!/usr/bin/env -S uv run --script`).
- The hybrid runtime (`src/hybrid/`) must not call game code or the game CRT.
