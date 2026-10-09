#!/usr/bin/env bash
# Run clang-cl in the dev container with the i686 Windows target and the real SDK headers (xwin), the way the hybrid and
# standalone builds compile game code. Arguments are passed through (clang-cl syntax). Used by the minwin tools
# (tools/minwin_inventory.py, tools/minwin_check.sh). With REB_CC=clang++ the container's plain clang++ is run instead
# (for the LP64 syntax pass), with no SDK flags.
set -euo pipefail
source "$(dirname "$0")/paths.sh"
x=/opt/xwin
sdk="--target=i686-pc-windows-msvc -fms-compatibility-version=19.40 /imsvc$x/crt/include /imsvc$x/sdk/include/ucrt /imsvc$x/sdk/include/um /imsvc$x/sdk/include/shared"
if [[ "${REB_CC:-clang-cl}" == clang++ ]]; then sdk=""; fi
exec docker run --rm -u "$(id -u):$(id -g)" -v "$ROOT:$ROOT" -w "$ROOT" rebellion-dev ${REB_CC:-clang-cl} $sdk "$@"
