#!/usr/bin/env bash
# Runs the GDI operation scenarios (gdi_ops.cpp) three ways and diffs the resulting 8-bit surfaces:
#   real    gdi_dump_real.exe (i686, the real gdi32/user32) under Wine           -> reference
#   ours32  gdi_dump_ours.exe (i686, minwin_impl/gdi) under Wine
#   ours64  gdi_dump_ours (native x86-64, minwin_impl/gdi)
# Usage: compare_wine.sh [scenario-filter]
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../../.." && pwd)"
filter="${1:-}"
"$root/src/port/minwin_impl/run_tests.sh" i686 gdi_dump_real gdi_dump_ours >/dev/null
"$root/src/port/minwin_impl/run_tests.sh" native gdi_dump_ours >/dev/null
out="$root/build/minwin_impl/i686"
docker run --rm -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" -v "$root:$root" -w "$out" rebellion-dev bash -c \
  "xvfb-run -a wine ./gdi_dump_real.exe real.txt $filter >/dev/null 2>&1; xvfb-run -a wine ./gdi_dump_ours.exe ours32.txt $filter >/dev/null 2>&1; chown \$HOST_UID:\$HOST_GID real.txt ours32.txt"
(cd "$root/build/minwin_impl/native" && ./gdi_dump_ours "$out/ours64.txt" $filter >/dev/null)
python3 "$root/src/port/minwin_impl/gdi/tests/diff_dumps.py" "$out/real.txt" "$out/ours32.txt" "$out/ours64.txt"
