# Sourced by tools: ROOT = this checkout (may be a git worktree), MAIN = the primary checkout, which owns the
# shared, untracked state (orig_game_files/, work/game, the Ghidra project/server, refs/).
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MAIN="$(cd "$(git -C "$ROOT" rev-parse --path-format=absolute --git-common-dir)/.." && pwd)"
