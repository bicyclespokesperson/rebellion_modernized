#!/usr/bin/env bash
# Syntax-only compile of every TU under src/game (plus the tactical stubs) against the minwin headers, in the dev container.
#   tools/minwin_syntax.sh i686    clang-cl --target=i686-pc-windows-msvc, flags of the standalone build (must be error free)
#   tools/minwin_syntax.sh lp64    clang++ --target=x86_64-linux-gnu with the port flags of docs/phase3_plan.md (a survey)
# Diagnostics go to build/minwin/syntax_<mode>/<tu>.log; the summary lists the files with errors and the error kinds.
set -uo pipefail
source "$(dirname "$0")/paths.sh"
mode="${1:-i686}"
out="$ROOT/build/minwin/syntax_$mode"
rm -rf "$out"; mkdir -p "$out"
case "$mode" in
  i686) driver="clang-cl --target=i686-pc-windows-msvc -fms-compatibility-version=19.40 /imsvc/opt/xwin/crt/include /imsvc/opt/xwin/sdk/include/ucrt /imsvc/opt/xwin/sdk/include/um /imsvc/opt/xwin/sdk/include/shared /Zs /Zc:sizedDealloc- /GR- /EHs-c- /arch:IA32 /clang:-fcheck-new -Wno-microsoft-cast -D_ACRTIMP= -D_CRTIMP= -D_DCRTIMP= -DREB_HYBRID=1 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_NO_CRT_STDIO_INLINE -D_CRT_SECURE_NO_WARNINGS -DREB_STANDALONE=1 -Datoi=reb_atoi -Datol=reb_atol" ;;
  lp64) driver="clang++ --target=x86_64-linux-gnu -std=c++23 -fsyntax-only -fms-extensions -fno-exceptions -fno-rtti -fcheck-new -fwrapv -fno-strict-aliasing -fsigned-char -ffp-contract=off -Wno-everything -DREB_STANDALONE=1" ;;
  *) echo "usage: $0 i686|lp64" >&2; exit 2 ;;
esac
cat > "$out/one.sh" <<EOF
#!/usr/bin/env bash
f="\$1"
log="$out/\$(echo "\$f" | tr '/' '_').log"
$driver ${REB_SYNTAX_EXTRA:-} -DREB_MINWIN=1 -DREB_NO_TACTICAL_3D=1 -Isrc/game -Isrc "\$f" > "\$log" 2>&1 || true
EOF
chmod +x "$out/one.sh"
docker run --rm -u "$(id -u):$(id -g)" -v "$ROOT:$ROOT" -w "$ROOT" rebellion-dev bash -c "find src/game -name '*.cpp' | sort | xargs -P \$(nproc) -n 1 $out/one.sh"
total=$(find src/game -name '*.cpp' | wc -l)
failed=$(grep -l "error:" "$out"/*.log 2>/dev/null | wc -l)
echo "$mode: $failed of $total TUs with errors (logs in $out)"
grep -h "error:" "$out"/*.log 2>/dev/null | sed -E 's/^[^ ]*: error: //; s/'"'"'[^'"'"']*'"'"'/X/g' | sort | uniq -c | sort -rn | head -40 > "$out/summary.txt"
cat "$out/summary.txt"
