#!/usr/bin/env bash
# Fetch the pinned third-party dependencies of the port (docs/phase3_plan.md P1) into third_party/ (gitignored; never
# commit them). Idempotent: a dependency whose directory exists is left alone. Every archive is verified against the
# sha256 below before it is unpacked; a mismatch aborts.
#   tools/fetch_deps.sh [freetype] [liberation] [sdl3] [sdl3-vc]      (default: all)
# FreeType is pinned to 2.13.2, the version Wine (Ubuntu 24.04) renders with: the text module then reproduces Wine's glyph
# bitmaps exactly (src/port/minwin_impl/text/tests). sdl3-vc is the official Visual C++ development zip (headers,
# lib/x86/SDL3.lib, SDL3.dll) used by the i686-windows build; third_party/SDL3 links to it.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
dest="$root/third_party"
mkdir -p "$dest/.dl"

# name|version|url|sha256|archive-top-directory
deps=(
  "freetype|2.13.2|https://download.savannah.gnu.org/releases/freetype/freetype-2.13.2.tar.xz|12991c4e55c506dd7f9b765933e62fd2be2e06d421505d7950a132e4f1bb484d|freetype-2.13.2"
  "liberation|2.1.5|https://github.com/liberationfonts/liberation-fonts/files/7261482/liberation-fonts-ttf-2.1.5.tar.gz|7191c669bf38899f73a2094ed00f7b800553364f90e2637010a69c0e268f25d0|liberation-fonts-ttf-2.1.5"
  "sdl3|3.4.18|https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz|9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3|SDL3-3.4.18-src"
  "sdl3-vc|3.4.18|https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-devel-3.4.18-VC.zip|78d84602ae616cfe26b33a73b7d3b9b6b56e8a6650e53d92c9c6ca938f747acd|SDL3-3.4.18"
)

want=("$@")
[[ ${#want[@]} -eq 0 ]] && want=(freetype liberation sdl3 sdl3-vc)

fetch() {
  local name="$1" version="$2" url="$3" sha="$4" top="$5"
  if [[ -d "$dest/$top" ]]; then echo "$name $version: present"; return; fi
  local archive="$dest/.dl/$(basename "$url")"
  [[ -f "$archive" ]] || curl -fsSL --retry 3 -o "$archive" "$url"
  echo "$sha  $archive" | sha256sum -c --quiet - || { echo "$name: checksum mismatch, removing $archive" >&2; rm -f "$archive"; exit 1; }
  case "$archive" in
    *.zip) unzip -q -o "$archive" -d "$dest" ;;
    *)
      # The source tarball and the VC zip both unpack to SDL3-<version>; keep the source tree apart.
      if [[ "$top" == *-src ]]; then
        mkdir -p "$dest/$top" && tar -C "$dest/$top" --strip-components=1 -xf "$archive"
      else
        tar -C "$dest" -xf "$archive"
      fi ;;
  esac
  echo "$name $version: fetched"
}

for d in "${deps[@]}"; do
  IFS='|' read -r name version url sha top <<<"$d"
  for w in "${want[@]}"; do
    [[ "$w" == "$name" ]] && fetch "$name" "$version" "$url" "$sha" "$top"
  done
done
[[ -d "$dest/SDL3-3.4.18" ]] && ln -sfn "SDL3-3.4.18" "$dest/SDL3"
exit 0
