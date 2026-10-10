#!/usr/bin/env bash
# Runs the project's pinned clang-format or clang-tidy. Override discovery with REB_CLANG_FORMAT or REB_CLANG_TIDY.
set -euo pipefail

readonly LLVM_VERSION="22.1.4"

case "${1:-}" in
  format)
    TOOL_NAME="clang-format"
    TOOL_OVERRIDE="${REB_CLANG_FORMAT:-}"
    ;;
  tidy)
    TOOL_NAME="clang-tidy"
    TOOL_OVERRIDE="${REB_CLANG_TIDY:-}"
    ;;
  *)
    echo "usage: $0 format|tidy [arguments...]" >&2
    exit 2
    ;;
esac
shift

TOOL_PATH="$TOOL_OVERRIDE"
if [[ -z "$TOOL_PATH" ]]; then
  TOOL_PATH="$(command -v "$TOOL_NAME-${LLVM_VERSION%%.*}" 2>/dev/null || true)"
fi
if [[ -z "$TOOL_PATH" ]]; then
  TOOL_PATH="$(command -v "$TOOL_NAME" 2>/dev/null || true)"
fi
if [[ -z "$TOOL_PATH" ]]; then
  echo "$TOOL_NAME $LLVM_VERSION is required" >&2
  exit 1
fi

TOOL_VERSION="$("$TOOL_PATH" --version)"
TOOL_VERSION_NUMBER="$(sed -nE 's/.*version ([0-9]+([.][0-9]+)+).*/\1/p' <<< "$TOOL_VERSION")"
if [[ "$TOOL_VERSION_NUMBER" != "$LLVM_VERSION" ]]; then
  echo "$TOOL_PATH is not the pinned $TOOL_NAME $LLVM_VERSION: $TOOL_VERSION" >&2
  exit 1
fi

exec "$TOOL_PATH" "$@"
