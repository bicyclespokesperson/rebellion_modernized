#!/usr/bin/env bash
# Build and run the minwin_impl unit tests in the dev container (docs/phase3_plan.md P1).
#   run_tests.sh native [target...]   host-architecture build (x86-64 clang), tests run directly
#   run_tests.sh i686 [target...]     clang-cl i686-windows build; every *_test.exe runs under Wine (Xvfb)
# Build trees: build/minwin_impl/<mode>. Extra cmake arguments via MW_CMAKE_ARGS. Test working directory is the build tree.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
mode="${1:-native}"
shift || true
targets="${*:-}"
out="build/minwin_impl/$mode"
mkdir -p "$root/build"

case "$mode" in
  native) toolchain="" ;;
  i686) toolchain="-DCMAKE_TOOLCHAIN_FILE=$root/cmake/toolchain-win32-clangcl.cmake" ;;
  *) echo "usage: $0 native|i686 [target...]" >&2; exit 2 ;;
esac

docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp -v "$root:$root" -w "$root" rebellion-dev bash -c \
  "set -e; cmake -S src/port/minwin_impl -B $out -G Ninja $toolchain -DCMAKE_BUILD_TYPE=Release ${MW_CMAKE_ARGS:-} >/dev/null; \
   cmake --build $out ${targets:+--target $targets} 2>&1 | tail -n 40"

if [[ "$mode" == native ]]; then
  docker run --rm -u "$(id -u):$(id -g)" -v "$root:$root" -w "$root/$out" rebellion-dev ctest --output-on-failure 2>&1 | tail -n 80
else
  docker run --rm -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" -v /usr/share/fonts/truetype/liberation:/usr/share/fonts/truetype/liberation:ro -v "$root:$root" -w "$root/$out" rebellion-dev bash -c \
    "export MW_FONT_DIR='Z:\\usr\\share\\fonts\\truetype\\liberation'; for t in \$(find . -maxdepth 2 -name '*_test.exe' | sort); do echo == \$t; xvfb-run -a wine \$t 2>&1 | tr -d '\r' || echo FAILED \$t; done; \
     chown -R \$HOST_UID:\$HOST_GID . 2>/dev/null; true"
fi
