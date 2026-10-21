# Repository guidance

- Commit and push completed changes at logical checkpoints without waiting for a separate request or confirmation. Include only changes belonging to the task; do not commit unrelated user work. Respect an explicit request to leave changes uncommitted or unpushed.
- prefer commit/revert to git stash to avoid race conditions between worktrees
- Committing directly to master is alright

## Current build and validation

- The native build is the default. Use Clang with C++23 through the top-level CMake project.
- Configure once with `./create_build.sh` (macOS, Linux, or Git Bash from a VS developer prompt on Windows), then
  `./launch_game.sh` compiles incrementally and launches (`--no-build`, `--no-hd`). Prerequisites (CMake, Ninja,
  uv, Clang, Linux SDL headers) are listed in README.md; `mise install` provides CMake, Ninja and uv.
  Java for Ghidra lives in `mise.ghidra.toml` (`MISE_ENV=ghidra`).
- Keep platform-specific behavior in the native platform layer; shared game code should use the host-native types and ordinary C++ lifetimes.
- Validate on the agent's host platform: build the native target and run the save/reload smoke, `tools/modern_smoke.py` (`--exe`, `--config`, `--load`, `--output-dir`); `docs/native.md` lists the other smokes. Access to other platforms is never a prerequisite, and avoid using github actions for testing.
- Original-binary symbol naming, ABI fidelity and no-STL/no-exceptions constraints no longer apply. Design rules for modernization work are in `docs/design/modernization_aggressive.md`.
- Do not format files unless the task asks for formatting.

## Historical material

`docs/ARCHITECTURE.md` summarizes how the game was reconstructed from the original executable. The hybrid build,
symbol-naming, calling-convention, capture and difftest procedures used then are retired; their documentation is
only in the repository history.

Never commit licensed game files, extracted asset packs, or raw Ghidra output.

## HD artwork

Rebuild the encyclopedia-derived icon pack from the repository root:

```sh
uv run --locked --python 3.12 tools/build_encyclopedia_icons.py \
  --install-dir "orig_game_files/Star Wars - Rebellion" --no-overrides
```

Generated art lives in ignored `work/hd/`: never commit it, `.rhd` files, extracted images or masks; only scripts and
recipes are committed. Mappings, recipes, review and validation: `docs/hd_artwork.md`.

## Space battles

Since 2026-10-19, `src/game/space_battle/` is the only battle simulator, used live and headless through the
strategic seam in `src/game/app/`. The legacy `src/game/tactical/` module, original 3D stubs and engine-selection
setting are retired. Keep new battle work in the simulator, battle UI and native presenter layers.

## Tactical battle sprites

Build the Blender-rendered battle sprite pack with
`uv run --locked --python 3.12 tools/build_tactical_sprites.py --install-dir "orig_game_files/Star Wars - Rebellion"`.
Everything under `work/hd/tactical/` (extracted meshes, `.blend` models, renders, the pack) stays uncommitted; commit
only recipes in `tools/tactical_model_recipes/`. Sources, pack format, remodel status and how to remodel a class:
`docs/tactical_sprites.md` (keep its status table current).
