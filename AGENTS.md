# Repository guidance

- Commit and push completed changes at logical checkpoints without waiting for a separate request or confirmation. Include only changes belonging to the task; do not commit unrelated user work. Respect an explicit request to leave changes uncommitted or unpushed.
- prefer commit/revert to git stash to avoid race conditions between worktrees
- Committing directly to master is alright

## Current build and validation

- The native build is the default. Use Clang with C++23 through the top-level CMake project.
- Keep platform-specific behavior in the native platform layer; shared game code should use the host-native types and ordinary C++ lifetimes.
- Validate on the agent's host platform: configure/build the native target and run the available smoke check. Access to other platforms is never a prerequisite.
- Do not require original-binary symbol naming, hybrid builds, difftest captures, or no-STL/no-exceptions constraints for current modernization work.
- Do not format files unless the task asks for formatting.

## Historical material

The decompilation references in `docs/ARCHITECTURE.md`, `docs/code_map.md`, `docs/library_code.md`, and `docs/agent_brief.md` can still explain the original game and earlier reconstruction decisions. Their old hybrid build, symbol naming, calling-convention, capture, and difftest procedures are historical context only. Follow the modernization roadmap and the current build instructions above when those sources conflict.

Use branches named `JS/<topic>`. Never commit licensed game files, extracted asset packs, or raw Ghidra output.
