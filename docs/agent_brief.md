# Module agent brief (template)

You are reconstructing one module of Star Wars: Rebellion (REBEXE.EXE, MSVC 5, 32-bit) in a function-by-function
decompilation. You work in your own git worktree; the primary checkout (Ghidra server, original game files,
work/decomp/ decompilation snapshot, shared game directory, regression captures) is found automatically by the tools.

FIRST read CLAUDE.md and docs/ARCHITECTURE.md and follow the workflow in CLAUDE.md exactly. Look at existing
code under src/game/ for style and for classes you can reuse (`git log` shows what was merged recently). Read
the relevant parts of docs/code_map.md for your module.

## Rules of engagement
- Scope: only the address range(s) you are given, plus functions you determine belong to the same class/source
  file. Note any functions you think are mis-assigned by the code map.
- Classes from other modules that you need: if a header for them exists under src/game, include and extend it
  minimally (add only what you need). If none exists, create one in the owning module's directory (per
  docs/code_map.md) with only the members you need, unknown parts as padding, and say so in your report.
- Names of classes outside your range: the agent owning that address range names them. Ask the owner directly
  (SendMessage) with the exact addresses and your proposed names; meanwhile declare such classes minimally in a
  clearly named `*_decl.h` header that can be dropped at merge.
- Do the work yourself: do NOT spawn sub-agents (the account's usage budget allows only 2–3 agents in total).
- Behaviour must match the original exactly, including quirks; mark original bugs `// BUG(original):`.
- Verification: every reconstructed function must PASS difftest on at least one capture, or be listed in your
  report as SKIP (not emulatable) / not captured (not reached by any scenario) with the reason. Use the
  scenarios in tests/scenarios/ (see docs/ui_navigation.md if present for what each reaches). Capture with
  several `skip` values for functions with multiple paths. Up to 64 capture targets per run.
- Do not edit tools/, src/hybrid/, docker/, CMakeLists.txt, or CLAUDE.md. If the tooling blocks you, work around
  it if possible and report the problem precisely.
- When done: `tools/build_hybrid.sh`, all difftests of your captures pass, `tools/run_game.sh tests/scenarios/smoke.txt`
  screenshot looks right, `tools/sync_types.py`, then commit on your branch. Commit message: what you
  reconstructed + difftest summary, ending with the two trailer lines given in your task.

## Final report (your last message)
1. Functions reconstructed: address, name (a compact table).
2. Verification per function: PASS (n captures) / SKIP (why) / not captured (why).
3. Smoke result.
4. Classes/headers you created or extended outside your module.
5. Problems with tools or instructions, and suggestions (be specific).
6. Your branch name and worktree path.
