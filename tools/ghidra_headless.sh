#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
export GHIDRA_MAXMEM="${GHIDRA_MAXMEM:-12G}"
exec mise exec -C "$root" -- "$root/tools/ghidra/support/analyzeHeadless" "$@"
