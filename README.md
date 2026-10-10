# Star Wars: Rebellion — decompilation

A faithful source reconstruction of *Star Wars: Rebellion* (1998) that runs natively on Linux and macOS. Requires your own
copy of the game: no game data is in this repository, and builds read the game files from your installation.

Status: the strategic game (galaxy, AI, missions, economy, characters, battles resolved automatically, saves) is
reconstructed and verified against the original; 3D tactical combat is not reconstructed, so battles are always
auto-resolved. Multiplayer is not supported in the native builds.
See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the approach, [docs/phase3_plan.md](docs/phase3_plan.md) for the
port, and [CLAUDE.md](CLAUDE.md) for the working process.

## Builds

| Build | What it is | Doc |
|---|---|---|
| hybrid | proxy `SMACKW32.DLL` loaded by the original `REBEXE.EXE` under Wine; replaces reconstructed functions in place (the verification harness) | [ARCHITECTURE.md](docs/ARCHITECTURE.md) |
| standalone | `rebellion.exe` built only from the sources (i686, Wine) | [phase2_plan.md](docs/phase2_plan.md) |
| native | Linux / macOS executable on SDL3, x86-64 and arm64 | [native.md](docs/native.md), [native_linux.md](docs/native_linux.md) |

## Setup

1. Copy the installed game folder to `orig_game_files/Star Wars - Rebellion/` (contains `REBEXE.EXE`).
2. Tools (no root needed): Docker, `uv`, `mise`.
   ```sh
   DOCKER_BUILDKIT=0 docker build -t rebellion-dev docker   # Wine, Xvfb, clang-19, MSVC SDK headers (xwin)
   tools/fetch_deps.sh                                       # SDL3, FreeType, Liberation fonts (pinned, into third_party/)
   ```

## Play natively (Linux)

```sh
tools/build_native.sh --image      # once: the native build container
tools/build_native.sh              # build/native/rebellion
```

The game reads `config.ini` from `~/.config/rebellion/` (macOS: `~/Library/Application Support/Rebellion/`;
`$REBELLION_CONFIG` overrides). The minimum is the path of your game installation:

```ini
install_dir=/path/to/Star Wars - Rebellion
```

Optional keys: `font_path` (an `arial.ttf`; otherwise system Arial, then the bundled Liberation Sans). Saves go to
`SaveGame/` inside the installation, as in the original. macOS builds on a Mac are described in
[docs/native.md](docs/native.md) (not yet tested on real Mac hardware).

## Development

Reconstruction work uses the hybrid build and Ghidra:

```sh
mise install                                         # JDK 21 for Ghidra
# Ghidra 12.1 unpacked to tools/ghidra, then:
tools/ghidra_headless.sh ghidra_proj Rebellion -import "orig_game_files/Star Wars - Rebellion/REBEXE.EXE"
uv venv -p 3.12 tools/.venv && uv pip install --python tools/.venv --no-index \
    --find-links tools/ghidra/Ghidra/Features/PyGhidra/pypkg/dist pyghidra
tools/ghidra_server.sh start && tools/gq -f tools/py/apply_symbols.py && tools/gq --save
mkdir -p work/game && cp -r "orig_game_files/Star Wars - Rebellion/." work/game/
tools/build_hybrid.sh
tools/run_game.sh tests/scenarios/smoke.txt     # screenshot in work/run/smoke/
```

Verification: `tools/integrate.sh` (merge + full regression), `tools/standalone_check.sh`, `tools/sim_goldens.sh check`,
`tools/native_sim.sh`, `tools/long_compare.sh`.

Note: building the Windows targets pulls the MSVC CRT and Windows SDK headers via `xwin`, which accepts Microsoft's
license.
