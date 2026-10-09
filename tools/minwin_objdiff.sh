#!/usr/bin/env bash
# Compile the game objects of the standalone build twice (REB_MINWIN=OFF against the Windows SDK, ON against src/port/minwin)
# and compare them object by object: the section bytes (code and constants) and the relocated symbol names must be equal, only
# debug info may differ. Output: build/minwin/objdiff/summary.txt. The first run builds both trees (a few minutes).
#   tools/minwin_objdiff.sh [--keep]    (--keep reuses already built trees)
set -uo pipefail
source "$(dirname "$0")/paths.sh"
work="$ROOT/build/minwin/objdiff"
mkdir -p "$work"
container() {
  docker run --rm -u "$(id -u):$(id -g)" -v "$ROOT:$ROOT" -w "$ROOT" rebellion-dev bash -c "$1"
}
for variant in off on; do
  flag=OFF; [[ $variant == on ]] && flag=ON
  dir="build/minwin/obj_$variant"
  if [[ "${1:-}" != --keep || ! -d "$ROOT/$dir" ]]; then
    container "cmake -S . -B $dir -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-win32-clangcl.cmake -DCMAKE_BUILD_TYPE=Release -DREB_STANDALONE=ON -DREB_MINWIN=$flag > $work/configure_$variant.log 2>&1 && cmake --build $dir --target reb_game reb_tactical_stubs > $work/build_$variant.log 2>&1" || { echo "build $variant failed (see $work)"; exit 1; }
  fi
done
cat > "$work/dump.sh" <<'EOF'
#!/usr/bin/env bash
# Prints a canonical text form of one object: section headers, raw section contents, relocation names.
obj="$1"
llvm-objdump-19 -s -r --section=.text --section=.rdata --section=.data --section=.bss --section=.xdata "$obj" 2>/dev/null \
  | grep -v "file format\|^$" | sed -E 's/[A-Za-z0-9_\/.-]*\.(obj|cpp)//g'
EOF
chmod +x "$work/dump.sh"
container "cd build/minwin/obj_off && find CMakeFiles -name '*.obj' | sort" > "$work/objects.txt"
container "
n=0; bad=0
while read -r o; do
  a=\$($work/dump.sh build/minwin/obj_off/\$o | md5sum); b=\$($work/dump.sh build/minwin/obj_on/\$o | md5sum)
  n=\$((n+1)); if [ \"\$a\" != \"\$b\" ]; then bad=\$((bad+1)); echo \"DIFF \$o\"; fi
done < $work/objects.txt
echo \"objects: \$n, different: \$bad\"" | tee "$work/summary.txt"
