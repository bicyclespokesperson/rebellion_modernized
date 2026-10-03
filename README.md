# Star Wars: Rebellion — decompilation

A faithful source reconstruction of *Star Wars: Rebellion* (1998), with the goal of running it natively on
Linux and macOS. Requires your own copy of the game. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for
the approach and [CLAUDE.md](CLAUDE.md) for the working process.

## Setup

1. Copy the installed game folder to `orig_game_files/Star Wars - Rebellion/` (contains `REBEXE.EXE`).
2. Tools (no root needed): Docker, `uv`, `mise`.
   ```sh
   mise install                                         # JDK 21 for Ghidra
   DOCKER_BUILDKIT=0 docker build -t rebellion-dev docker   # Wine, Xvfb, clang-19, MSVC SDK headers (xwin)
   # Ghidra 12.1 unpacked to tools/ghidra, then:
   tools/ghidra_headless.sh ghidra_proj Rebellion -import "orig_game_files/Star Wars - Rebellion/REBEXE.EXE"
   uv venv -p 3.12 tools/.venv && uv pip install --python tools/.venv --no-index \
       --find-links tools/ghidra/Ghidra/Features/PyGhidra/pypkg/dist pyghidra
   tools/ghidra_server.sh start && tools/gq -f tools/py/apply_symbols.py && tools/gq --save
   mkdir -p work/game && cp -r "orig_game_files/Star Wars - Rebellion/." work/game/
   ```
3. Build and run the hybrid game (original EXE + reconstructed functions):
   ```sh
   tools/build_hybrid.sh
   tools/run_game.sh tests/scenarios/smoke.txt     # screenshot in work/run/smoke/
   ```

Note: building pulls the MSVC CRT and Windows SDK headers via `xwin`, which accepts Microsoft's license.
