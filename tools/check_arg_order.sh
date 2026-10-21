#!/usr/bin/env bash
# Guard against argument-evaluation-order dependence in src/game: clang-cl (MSVC ABI)
# evaluates call arguments right to left, Clang on macOS and Linux left to right.
# MSVC and clang-cl evaluate call arguments right to left, clang for the Itanium ABI (the native build) left to right; an argument list with
# two or more calls where any of them has a side effect behaves differently. tools/find_arg_order.py classifies every such list: all calls
# read-only (fine), listed in tools/arg_order_allow.txt with a reason (reviewed), or unclassified (this check fails).
#   tools/check_arg_order.sh            one verdict line; the unclassified sites, if any, are listed before it (exit status 1)
#   tools/check_arg_order.sh --all      also list the allowlisted and the read-only sites
# Needs only a CMake configure (no objects) in the rebellion-native container; about half a minute.
set -euo pipefail
source "$(dirname "$0")/paths.sh"
bdir="${REB_ARGCHECK_DIR:-build/argcheck}"
mkdir -p "$ROOT/$bdir"
mode=--check
[[ "${1:-}" == --all ]] && mode=--all
docker run --rm -u "$(id -u):$(id -g)" "${REB_MOUNTS[@]}" -w "$ROOT" rebellion-native bash -c \
  "cmake -S . -B $bdir -G Ninja -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 -DREB_NATIVE=ON > $bdir/configure.log 2>&1 \
   || { tail -20 $bdir/configure.log; exit 2; }; python3 tools/find_arg_order.py $bdir --repo $ROOT $mode ${REB_ARGCHECK_ARGS:-}"
