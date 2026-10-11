# Star Wars: Rebellion — decompilation

A faithful source reconstruction of *Star Wars: Rebellion* (1998) that runs natively on Linux and macOS. Requires your own
copy of the game: no game data is in this repository, and builds read the game files from your installation.

Status: the strategic game (galaxy, AI, missions, economy, characters, battles resolved automatically, saves) is
reconstructed and verified against the original; 3D tactical combat is not reconstructed, so battles are always
auto-resolved. Multiplayer is not supported in the native builds.
See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the approach, [docs/phase3_plan.md](docs/phase3_plan.md) for the
port, and [CLAUDE.md](CLAUDE.md) for the working process.

Active work: [aggressive modernization roadmap](docs/design/modernization_aggressive.md). Related future work:
[higher resolutions and upscaling](docs/design/high_resolution.md).

## Builds

| Build | What it is | Doc |
|---|---|---|
| native | Linux / macOS executable on SDL3, x86-64 and arm64; the active CMake build | [native.md](docs/native.md), [native_linux.md](docs/native_linux.md) |

The former hybrid and standalone Windows builds are retained in Git history and supporting scripts for reference, but are no longer
buildable from the active CMake project.

## Setup

1. Copy the installed game folder to `orig_game_files/Star Wars - Rebellion/` (contains `REBEXE.EXE`).
2. Tools (no root needed): Docker, `uv`, `mise`.
   ```sh
   DOCKER_BUILDKIT=0 docker build -t rebellion-dev docker   # Wine, Xvfb, clang-19, MSVC SDK headers (xwin)
   tools/fetch_deps.sh                                       # SDL3, FreeType, Liberation fonts (pinned, into third_party/)
   ```

## Launch the game (macOS / Linux)

Once built, run from the repo root:

```sh
./launch_game.sh
```

The launcher checks for the native executable and uses `$REBELLION_CONFIG`, your platform's user config, or the existing
local config (`work/macos/config.ini` on macOS, `work/launch/config.ini` on Linux), in that order. If no config exists,
it creates `work/launch/config.ini` pointing to `orig_game_files/Star Wars - Rebellion/`. Missing builds, configs and game
files produce setup instructions. Saves go to the installation's `SaveGame/` folder.

On macOS, build first with `tools/build_macos.sh` (Xcode command-line tools, CMake and `uv` required;
see [native.md](docs/native.md)). Linux build instructions follow.

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

Optional keys (all in the same file, before any `[registry]` section):

| Key | Default | Meaning |
|---|---|---|
| `window_scale` | largest integer scale that fits the display | window size as a multiple of 640x480 (1-8); the window can also be resized, the picture always stays at an integer scale, centred, with black bars |
| `fullscreen` | `0` | `1` starts fullscreen (desktop resolution, integer-scaled, letterboxed, nearest-neighbour); **Alt+Enter** toggles at any time |
| `data_dir` | `install_dir` | where `MDATA/` lives if it is not inside the installation |
| `font_path` | system Arial, then bundled Liberation Sans | an `arial.ttf` |
| `renderer` | SDL's choice (GPU if available, else software) | SDL render driver name, e.g. `software` |
| `vsync` | `0` | `1` synchronises presentation with the display |
| `audio_buffer` | `1024` | audio device buffer in sample frames (raise to 2048 if you hear crackling) |

If the config file is missing, `install_dir` is wrong, or the installation lacks a file the game needs (`REBEXE.EXE`, the `*.DLL`
resource libraries, `GData`, `EData`, `MDATA/MDATA.000`), the game shows a message box that says what to put where and exits.
The close button and **Alt+F4** go through the game's own quit confirmation. Saves go to `SaveGame/` inside the installation, as in the
original. The game keeps one CPU core mostly idle (about 10% at the galaxy screen). macOS builds on a Mac are described in
[docs/native.md](docs/native.md) (ARM64 build and menu launch verified on real Mac hardware).

## Development

Reconstruction work uses Ghidra and the native build:

```sh
mise install                                         # JDK 21 for Ghidra
# Ghidra 12.1 unpacked to tools/ghidra, then:
tools/ghidra_headless.sh ghidra_proj Rebellion -import "orig_game_files/Star Wars - Rebellion/REBEXE.EXE"
uv venv -p 3.12 tools/.venv && uv pip install --python tools/.venv --no-index \
    --find-links tools/ghidra/Ghidra/Features/PyGhidra/pypkg/dist pyghidra
tools/ghidra_server.sh start && tools/gq -f tools/py/apply_symbols.py && tools/gq --save
tools/build_macos.sh                         # macOS; Linux: tools/build_native.sh
tools/run_native.sh tests/scenarios/smoke.txt
```

Native run and simulation tools are documented in [native_linux.md](docs/native_linux.md).
