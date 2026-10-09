#!/usr/bin/env bash
# Fetch the pinned third-party sources the port builds from into third_party/ (gitignored). Idempotent: a dependency whose
# directory exists is left alone. Every archive is verified against the sha256 below before it is unpacked.
#   tools/fetch_deps.sh [freetype] [sdl3]      (default: all)
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
dest="$root/third_party"
mkdir -p "$dest/.dl"

# name|version|url|sha256|archive-top-directory
deps=(
  "freetype|2.13.3|https://download.savannah.gnu.org/releases/freetype/freetype-2.13.3.tar.xz|0550350666d427c74daeb85d5ac7bb353acba5f76956395995311a9c6f063289|freetype-2.13.3"
  "sdl3|3.4.18|https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz|9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3|SDL3-3.4.18"
)

want=("$@")
[[ ${#want[@]} -eq 0 ]] && want=(freetype sdl3)

fetch() {
  local name="$1" version="$2" url="$3" sha="$4" top="$5"
  if [[ -d "$dest/$top" ]]; then echo "$name $version: present"; return; fi
  local archive="$dest/.dl/$(basename "$url")"
  [[ -f "$archive" ]] || curl -fsSL --retry 3 -o "$archive" "$url"
  echo "$sha  $archive" | sha256sum -c --quiet - || { echo "$name: checksum mismatch, removing $archive" >&2; rm -f "$archive"; exit 1; }
  tar -C "$dest" -xf "$archive"
  echo "$name $version: fetched"
}

for d in "${deps[@]}"; do
  IFS='|' read -r name version url sha top <<<"$d"
  for w in "${want[@]}"; do
    [[ "$w" == "$name" ]] && fetch "$name" "$version" "$url" "$sha" "$top"
  done
done
exit 0
