# Higher resolutions and widescreen

Status: high-DPI rendering, the indexed HD artwork packs and the adaptive widescreen galaxy layout are implemented and have been checked in native macOS gameplay. The asset pipeline that builds the packs is documented in [HD artwork pack preparation](../hd_artwork.md).

## Current implementation (October 2026)

Menus and cutscenes keep the original 640×480 layout with 4:3 letterboxing.
During strategic gameplay, `widescreen=1` (the default) expands the logical canvas
to the display aspect ratio, from 640 to 1280 pixels wide and 480 high.
Set `widescreen=0` to keep the classic layout throughout. `upscale_filter=sharp` preserves the indexed artwork and renders
live FreeType text and line geometry at device resolution, up to 4× (2560×1920).
SDL performs the final fractional resize. `nearest` selects unfiltered presentation;
`integer` restricts the image to integer multiples. `render_scale=0` chooses the
internal scale automatically; `1` through `4` override it. For the complete original
pixel appearance, combine `render_scale=1` with `upscale_filter=integer`.
Bitmap-font labels retain their original authored pixels; the new glyph rendering
applies to live FreeType text.

The two gameplay droid stages also smooth their fully decoded animation poses in
`sharp` mode above 1×. Key-aware bilinear interpolation produces a cached 4× indexed
companion using the original palette. Logical pixels, silhouettes, hit testing,
poses, and timing remain unchanged. Each delta invalidates the cached companion;
only the final displayed pose is rebuilt. This applies to both factions' advisors
without a separate animation pack. `nearest`, `integer`, and `render_scale=1` retain
the original sprite pixels. Existing authored companions take precedence.

Cockpit edges stay anchored to the sides, the original galaxy and console remain
centered, and reconstructed frame bands fill the added width. Sector windows can
be moved by their title bars into the extra workspace; resizing keeps them inside
the usable panel area. Planet and unit dragging retains its original behavior.

This deliberately retains palette indices through drawing. Color-keyed sprites,
RLE-decoded bitmaps, palette animation, movies, and remapped artwork retain their
existing semantics. Mouse mapping uses the same viewport as presentation, including
letterbox offsets and Retina density. Resizing invalidates visible windows so text
is redrawn at the new scale. Alt+Enter still toggles desktop fullscreen.

A separate device surface preserves the logical screen for reads and legacy indexed
captures. GDI-created offscreen text also retains device detail through BitBlt;
tracked software bitmap writes preserve and compose indexed artwork detail;
untracked writes invalidate cached detail. Logical font advances and text
measurements stay unchanged. Sidecars are allocated lazily for GDI detail, rather
than for every game asset, with bounded allocation and nearest fallback.

The old proposal required every high-resolution frame to box-downsample exactly to
its 1× counterpart. That conflicts with newly rasterized glyphs and diagonals. The
useful requirements are unchanged 1× drawing, unchanged logical metrics/hit testing,
correct clipping/ROP/palette behavior, and visibly finer device detail.

No artwork is automatically redesigned. The runtime loads optional indexed companion
artwork as matching `.rhd` files from `hd_asset_dir`, which is absolute or relative to
the configuration file. Original logical pixels, palette indices, hit masks, and bitmap
bounds remain authoritative. Missing or malformed replacements fall back to the
original artwork. The companion follows software composition and GDI drawing, so
foreground markers, windows, text, and palette effects retain their layer order.
The packs are built offline: `tools/build_hd_pack.py` covers the STRATEGY.DLL
cockpit/galaxy resources and console buttons, and `tools/build_encyclopedia_icons.py`
covers portraits, planets, units, ships, facilities and mapped mission artwork. See
[HD artwork pack preparation](../hd_artwork.md). `tools/export_hd_assets.py` exports a
local bitmap inventory with nearest/Lanczos comparisons for choosing new candidates;
its manifest marks palette/transparency semantics unknown until audited, so blue/black
areas must not be read as transparency without checking the draw site.

Companion: [modernization decisions](modernization_aggressive.md).

## Goal

Make the game look good on modern displays: sharp at any window size, optionally smoothed or HD artwork, and
more screen space for the strategic workspace, without breaking the game's logic or its look.

## Rendering decisions

- **Logical surface:** an indexed buffer remains authoritative for game-visible
  reads, layout, clipping, and legacy `.idx` captures. Its height is 480; strategic
  gameplay can expand its width from 640 to 1280 to match the output aspect ratio.
- **Device surface:** a second indexed surface carries scaled artwork and freshly
  rasterized text/line strokes. Palette changes recolor both without RGB caching.
- **Offscreen drawing:** memory DCs acquire a device sidecar only when they contain
  detail to preserve. BitBlt transfers that detail with source palette mapping and
  raster operations. Direct DIB writes invalidate it explicitly; shadow comparison
  covers untracked writes. Oversized sidecars fall back to logical scaling.
- **Presentation:** SDL's Metal/OpenGL/Direct3D or software renderer uploads the
  device frame and fits it to one shared viewport used by mouse input too.
  The default uses linear final resampling of the already enlarged image; original
  1× input uses bounded nearest enlargement first. There is no custom shader,
  external xBRZ dependency, CRT effect, or runtime image generation.
- **Text:** FreeType glyphs are rasterized at the device scale, with the original
  logical advances/line breaks. Indexed glyph masks remain palette-safe; final
  fractional sampling supplies edge smoothing.
- **Cursor and testing:** cursor size and pixel-density alternates update across
  displays; F12 captures the next complete rendered frame as BMP. Set
  `REB_SCREENSHOT_PATH` to choose the capture path. Otherwise screenshots go to the
  SDL user-data directory under `screenshots/`; the game logs the saved path.

## Artwork and layout work

### Tier 3 — HD replacement assets (optional packs)

Optional packs contain **indexed 2× or 4× companions**, with the original palette
and logical bitmap intact. Replacements are identified by the exact original
RT_BITMAP bytes, including the DIB header, palette, and encoded source pixels.
An indexed representation was chosen after tracing the galaxy and object-picture
composition paths: a true-color overlay would not preserve arbitrary palette
remaps or the drawing order of software-composed windows.

The runtime pack format is deliberately small:

| Field | Encoding |
| --- | --- |
| Filename | 16 lowercase hexadecimal digits of source FNV-1a-64, followed by `.rhd` |
| Magic | 8 bytes: `REBHD01` or `REBHD02` followed by NUL |
| Logical dimensions | Two little-endian uint32 values: positive width and absolute height |
| Scale / byte count | Two little-endian uint32 values: scale 2 or 4 and payload size |
| Source fingerprint | Little-endian uint64 FNV-1a-64 of the complete original resource |
| Payload | Top-down, tightly packed palette indices, width × height × scale² bytes |

Files must match source dimensions, fingerprint, palette size, and exact byte count;
payloads are capped at 64 MiB. The FNV fingerprint is an identity check, not a security
signature. The offline manifest also records SHA-256 hashes, module/resource/language
identity, processing method, transparency rules, and review results. PNGs retain the
same indexed output for inspection; they are not decoded at runtime. No new image
codec or platform-specific renderer is required.

`REBHD01` uses the original logical pixel's color-key mask. `REBHD02` uses each
detail pixel's key index for visual transparency, allowing encyclopedia cutouts to
have finer or different outlines within the same logical bounds. Logical source
pixels and hit masks remain unchanged. Keyed composition initializes detail from
the destination before writing logical pixels, and format-derived intermediate
bitmaps retain the detail-keying mode.

Derived packs, source exports, and per-asset artwork stay under ignored `work/hd/`;
only generic tools and rendering code belong in the repository. The initial cockpit/galaxy, console-button, and encyclopedia-derived icon preparation
scopes are complete. Delta animations retain the original path and
invalidate detail when frames modify raw pixels; the gameplay droid stages rebuild
their filtered detail after decoding. Other animated artwork is unchanged.

### Tier 4 — adaptive widescreen strategic gameplay

The SDL drawable aspect ratio determines the gameplay canvas width. The platform
resizes its logical/device surfaces, sends a deferred root resize, and maps input
through the same viewport. Width is capped at 1280 (32:9 at height 480); taller
outputs retain the minimum 640×480 canvas. Scene entry enables the expanded canvas;
leaving strategic gameplay restores the original one.

The authored galaxy and its sector positions remain at their original size,
centered in the wider viewport. Reflected outer texture strips extend the background without duplicating its bright core; the
additional width provides usable space for sector and system windows. Faction-specific controls
and window docks remain at their original size, anchored to the appropriate edge
or bottom cockpit section. Original cockpit slices extend the frame without
stretching buttons. Roof, floor, corner, dock, and console cuts preserve the faction
geometry; quiet floor strips retain texture instead of stretching a single pixel
column. Existing sector windows and dialogs keep their logical size.
A larger logical encyclopedia and redesigned cutscene layouts are deferred.

## Remaining work

- Alternative bitmap scalers are deferred: evaluate them by asset class, since a whole-frame pixel-art scaler can
  distort text and painted imagery.
- Enhancing encyclopedia pages, intros and cutscenes is lower priority.
- Linux uses the same rendering path, and Windows has a build entrypoint and platform adapter, but neither has had
  host play verification. Do not claim Windows/Linux play verification from a macOS run.

Run `cmake --build build/native` and `ctest --test-dir build/native --output-on-failure`.
The rendering tests exercise fractional and integer mouse edges, original subpixel data
surviving presentation, screenshot readback, scaled clipping, palette-index drawing,
XOR restoration, offscreen text and direct-write invalidation, HD asset format, and
HD composition. The asset-dependent UI/save smoke remains `REB_UI_SMOKE=1 tools/modern_smoke.py ...`;
the offline pack-builder tests are listed in [hd_artwork.md](../hd_artwork.md).

Performance and visual quality at unusually
large outputs remain hardware-dependent; the renderer caps internal GDI resolution
at 4× and the gameplay canvas at 1280×480.
