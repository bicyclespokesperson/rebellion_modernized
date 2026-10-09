#!/usr/bin/env bash
# Fetch the third-party libraries the port links (docs/phase3_plan.md P1) into third_party/ (gitignored; never commit
# binaries). Each archive is pinned by version and sha256; a mismatch aborts. Idempotent: skips what is already present.
#   SDL3 (zlib licence) -- the official Visual C++ development zip: headers, lib/<arch>/SDL3.lib, SDL3.dll.
set -euo pipefail
source "$(dirname "$0")/paths.sh"

SDL3_VERSION=3.4.18
SDL3_VC_SHA256=78d84602ae616cfe26b33a73b7d3b9b6b56e8a6650e53d92c9c6ca938f747acd

fetch() {  # fetch <url> <sha256> <destination file>
  local url="$1" sha="$2" dest="$3"
  [[ -f "$dest" ]] || curl -fsSL --retry 3 -o "$dest" "$url"
  echo "$sha  $dest" | sha256sum -c --quiet - || { echo "checksum mismatch: $dest" >&2; rm -f "$dest"; exit 1; }
}

third_party="$ROOT/third_party"
mkdir -p "$third_party/downloads"

sdl3_dir="$third_party/SDL3-$SDL3_VERSION"
if [[ ! -f "$sdl3_dir/lib/x86/SDL3.lib" ]]; then
  zip="$third_party/downloads/SDL3-devel-$SDL3_VERSION-VC.zip"
  fetch "https://github.com/libsdl-org/SDL/releases/download/release-$SDL3_VERSION/SDL3-devel-$SDL3_VERSION-VC.zip" "$SDL3_VC_SHA256" "$zip"
  unzip -q -o "$zip" -d "$third_party"
fi
ln -sfn "SDL3-$SDL3_VERSION" "$third_party/SDL3"
echo "SDL3 $SDL3_VERSION: $third_party/SDL3"
