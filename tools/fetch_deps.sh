#!/usr/bin/env bash
# Fetch the pinned third-party sources the port builds from into third_party/ (gitignored). Idempotent: a dependency whose
# directory exists is left alone. Every archive is verified against the sha256 below before it is unpacked.
#   tools/fetch_deps.sh [freetype] [liberation] [sdl3] [freetype-ref]      (default: all but freetype-ref)
# freetype-ref is FreeType 2.13.2, the version Wine (Ubuntu 24.04) renders with: building the text module against it
# (-DMW_FREETYPE_DIR=third_party/freetype-2.13.2) reproduces Wine's glyph bitmaps exactly (src/port/minwin_impl/text/tests).
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
dest="$root/third_party"
mkdir -p "$dest/.dl"

# name|version|url|sha256|archive-top-directory
deps=(
  "freetype|2.13.3|https://download.savannah.gnu.org/releases/freetype/freetype-2.13.3.tar.xz|0550350666d427c74daeb85d5ac7bb353acba5f76956395995311a9c6f063289|freetype-2.13.3"
  "freetype-ref|2.13.2|https://download.savannah.gnu.org/releases/freetype/freetype-2.13.2.tar.xz|12991c4e55c506dd7f9b765933e62fd2be2e06d421505d7950a132e4f1bb484d|freetype-2.13.2"
  "liberation|2.1.5|https://github.com/liberationfonts/liberation-fonts/files/7261482/liberation-fonts-ttf-2.1.5.tar.gz|7191c669bf38899f73a2094ed00f7b800553364f90e2637010a69c0e268f25d0|liberation-fonts-ttf-2.1.5"
  "sdl3|3.4.18|https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz|9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3|SDL3-3.4.18"
)

want=("$@")
[[ ${#want[@]} -eq 0 ]] && want=(freetype liberation sdl3)

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
