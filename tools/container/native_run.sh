#!/usr/bin/env bash
# Run a shell command in the native-build container (docker/Dockerfile.native) from the checkout root, with the primary checkout
# (original game files, third_party) mounted read-only. Optional environment: REB_DOCKER_ARGS (extra docker run arguments, e.g.
# "-e DISPLAY=:1"), REB_NATIVE_IMAGE.
#   tools/container/native_run.sh 'command ...'
set -euo pipefail
source "$(dirname "$0")/../paths.sh"
# shellcheck disable=SC2086
exec docker run --rm -u "$(id -u):$(id -g)" -v "$ROOT:$ROOT" -v "$MAIN:$MAIN:ro" -w "$ROOT" ${REB_DOCKER_ARGS:-} "${REB_NATIVE_IMAGE:-rebellion-native}" bash -c "$1"
