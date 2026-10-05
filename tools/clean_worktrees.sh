#!/usr/bin/env bash
# Remove agent worktrees whose branch is fully merged into master and that have no uncommitted changes.
# Locked worktrees (a running agent holds the lock) are never touched.
set -uo pipefail
source "$(dirname "$0")/paths.sh"
cd "$MAIN"
git worktree list --porcelain | awk '/^worktree /{w=$2} /^locked/{print w}' > /tmp/.locked_worktrees.$$
for w in .claude/worktrees/*; do
  [[ -d "$w" ]] || continue
  if grep -qx "$MAIN/$w" /tmp/.locked_worktrees.$$; then echo "locked (agent running): $(basename "$w")"; continue; fi
  b=$(git -C "$w" rev-parse --abbrev-ref HEAD)
  if git merge-base --is-ancestor "$b" master && [[ -z "$(git -C "$w" status --short | grep -v '^??')" ]]; then
    git worktree remove -f "$w" && git branch -D "$b" -q && echo "removed $(basename "$w")"
  else
    echo "kept (unmerged work): $(basename "$w")"
  fi
done
rm -f /tmp/.locked_worktrees.$$
