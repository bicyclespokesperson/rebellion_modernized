# Higher resolutions and artwork roadmap

Status: high-DPI rendering and the gameplay-focused indexed HD pilot and adaptive widescreen galaxy layout are implemented. Both faction layouts and the local artwork pack have been checked in native macOS gameplay; portrait, planet, and unit artwork remain future work.

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

No artwork is automatically redesigned. `tools/export_hd_assets.py` exports a
local bitmap inventory, original BMP/indexed PNG, nearest and Lanczos comparisons,
and contact sheets. For example:

```sh
uv run tools/export_hd_assets.py \
  --module 'orig_game_files/Star Wars - Rebellion/STRATEGY.DLL' \
  --out work/hd/strategy --scale 2
```

Use repeated `--id` flags for a small pilot. The inspected local STRATEGY.DLL contains
1,042 bitmaps; resources 902/903 are the galaxy backdrops, and 900/901 are faction
frames. The exported manifest records source hashes and language IDs and explicitly
marks palette/transparency/animation semantics unknown until audited. It is an
**artwork preparation inventory, not a runtime replacement pack**. In particular,
blue/black areas must not be interpreted as transparency without checking the draw
operation. No local exports or licensed source data belong in git.

`tools/build_hd_pack.py` prepares optional indexed companion artwork. The runtime
loads matching `.rhd` files from `hd_asset_dir`, which is absolute or relative to the
configuration file. Original logical pixels, palette indices, hit masks, and bitmap
bounds remain authoritative. Missing or malformed replacements fall back to the
original artwork. The companion follows software composition and GDI drawing, so
foreground markers, windows, text, and palette effects retain their layer order.
The current local STRATEGY.DLL pilot has 68 resources: four cockpit/galaxy resources
(900–903) and all 64 console-button states. Its reproducible recipe is
`tools/hd_gameplay_recipe.json`; build and configuration instructions are in
[HD artwork pack preparation](../hd_artwork.md). The manifest records the 64 button
states as `visual_candidate_accepted` after offline contact-sheet and palette/key-mask
review. All four larger cockpit/galaxy assets are `runtime_scene_accepted` after
comparison and native gameplay review. Common button states were also seen in game;
not every alternate state was exercised interactively. The
next artwork candidates are portraits, planets, and unit/ship art.

Written Oct 2026. Companion: [aggressive modernization](modernization_aggressive.md).

## Validation

On macOS, the native release and UBSan builds pass all 18 CTests and the
load/advance/save/reload smoke check with UI smoke enabled. Rendering regressions
cover device text and lines, clipping, palette mapping, raw bitmap invalidation,
cached-text scale changes, fractional fitting, mouse mapping, and screenshots.
They also cover source-palette mapping for HD bitmap draws and detail preservation
through 4×→1×→4× display transitions. Resizing preserves custom display titles and
does not replay display-mode activation audio; sector slot placement and toggles
use the same widescreen offsets as existing panels.
The asset-dependent UI smoke also constructs the real galaxy window, resizes it
640→854→1280→640, checks the background and offscreen buffers, and repaints all
console/message buttons. The side-switch smoke repeats this for both factions.
Sector dragging is covered both by window messages and queued mouse input,
including capture/release, panel bounds, and preserving marker dragging.
All six offline pack-builder tests pass.
The droid checks exercise real base images and delta frames from both faction DLLs,
including detail invalidation/regeneration and transparent-cell preservation.
Five offline sequence-export tests cover cumulative decoding, malformed resources,
and palette/key-mask rules. Representative animation review covers 71 frames across
four sequences; native gameplay screenshots cover all four advisors. On this host,
a synthetic 106×133 sprite benchmark took about 8 ms for the first palette/frame,
1 ms for a changed frame, and under 1 μs when reusing the cached frame.
ASan tests time out before execution on this host, consistent with the existing
Apple Clang runtime limitation recorded in the modernization roadmap.

Desktop computer-use checks covered both factions, saved-game loading, galaxy
display modes, sector selection, 4:3↔widescreen transitions, a 2560×1440 window,
sector clamping when narrowing, title-bar dragging into the expanded workspace,
options transitions, F12 capture, and normal quit. The pinned SDL Cocoa patch uses
event coordinates for buttons and drags, and avoids premature release from physical
button-state reconciliation during injected drags. Changed button positions also
deliver mouse motion first so the game's hover-based selection works with injected
input.

Linux uses the same rendering path but was not executed in this validation run.
The experimental Windows adapter, build script, and CI workflow are present;
Windows compilation and runtime remain unverified. See [Windows build status](../native_windows.md).

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
| Magic | 8 bytes: `REBHD01` followed by NUL |
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

Use [flowers-upscale](../../.agents/skills/flowers-upscale/SKILL.md) to select and
review the treatment per asset. Derived packs, source exports, and per-asset artwork
stay under ignored `work/hd/`; only generic tools and rendering code belong in git.
The initial cockpit/galaxy and console-button preparation scope is complete. Portrait,
planet, and unit/ship artwork are the next candidates; intro, encyclopedia, and cutscene
enhancement remain lower priority. Delta animations retain the original path and
invalidate detail when frames modify raw pixels; the gameplay droid stages rebuild
their filtered detail after decoding. Other animated artwork is unchanged.

#### Asset selection and resolution

Prioritize by time visible in normal play and visible improvement at the intended
display size. **Encyclopedia art is low priority**: it is rarely on screen. Do not
make enhancing the encyclopedia library a prerequisite for an HD pack.

| Priority / asset class                                                      | Approach                                                                                                                                                                      |
| --------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| First: frequently visible galaxy, sector, and planet artwork                | Inspect resources/palette behavior; compare scaling with procedural terrain/spheres/atmosphere/lighting, SVG, or hybrid rendering; selective AI for suitable textured regions |
| Next: portraits and unit/ship art displayed in routine gameplay             | Preserve likeness/design; consider SVG/layered 2D/simple 3D reconstruction for ships and vehicles, hybrid textures, or selective AI for static candidates                     |
| Small static pilot: menu/background artwork                                 | Useful for establishing style and validating R5 integration; avoid spending most of the art budget on menus                                                                   |
| UI borders, buttons, bitmap labels/emblems, cursors, small markers/icons    | SVG/code reconstruction or deterministic/key-aware scaling; live text/primitives use R3                                                                                       |
| Flat fills, gradients, sparse starfields                                    | SVG/procedural rendering, simple resize, or existing primitives; preserve authored star positions and avoid invented texture                                                  |
| Animated sprites, briefing sequences, movies, palette-remapped/animated art | Keep original path plus R4 scaling initially; separate sequence/variant work before replacements                                                                              |
| Later: encyclopedia illustrations                                           | Optional selective enhancement after frequently visible gameplay artwork                                                                                                      |

Start at **2× source dimensions** and compare at the actual on-screen size; prepare
4× where the display scale and visible benefit justify it. Pack dimensions stay exact
integer multiples of the original. A full 640×480 background is 1280×960 at 2× or
2560×1920 at 4×, already suitable for the game's letterboxed presentation on modern
displays. There is no 8K master requirement or automatic upgrade of every resource.
Choose higher scales only for a demonstrated display need.

Local inspection found 187 `EData/EDATA.nnn` BMP files, each 400×200 and 8-bit, referenced
through `ENCYBMAP.DLL`; these would be 800×400 or 1600×800. They are useful size/style
examples, not the initial pack scope. Samples include rendered vehicles/ships and
portraits: a photographic skin/texture reconstruction prompt would change the look.

#### Preparation workflow and pack contract

1. Export an inventory with source identity (module, resource type/id/name, language
   where relevant, frame or external file), source hash, dimensions, palette/key,
   decoded masks, logical bounds/pivots, animation/atlas membership, and known palette
   dependencies. Keep unknown semantics marked unknown until audited at draw sites.
   Retain original indexed data: equal RGB colors can have different palette roles.
2. Choose a small pilot from frequently visible static gameplay art, plus menu art
   if useful for integration. Compare nearest, an appropriate smooth resize, and
   key-aware pixel-art scaling as applicable. Compare SVG/procedural or hybrid
   reconstructions where shape, materials, and lighting can be reproduced faithfully,
   including planets and ships. Keep simple scaling when it looks good.
3. Use `$flowers-upscale` to select the approach per asset or region. For code-driven
   rendering, preserve editable source, fitted parameters, fixed seeds, renderer
   version, and source textures; render to the exact PNG pack dimensions. For raster
   AI candidates, start with one whole-image contextual edit. Use padded registered tiles only if
   the native generation budget or a failed whole-image attempt warrants them;
   add targeted passes for observed defects rather than mandatory 2/3-pass batches.
   Every edit sees the unchanged original and target crop. Preserve geometry, style,
   text/emblems, alpha masks, and quiet areas; generated detail is reconstructed.
4. Review baseline/enhanced comparisons at game display size and 100% focal/overlap
   crops. Reject invented structures, likeness changes, seams, doubling, or alpha
   fringes. At most two targeted retries per defect; keep prior pixels on failure.
   Verify exact export dimensions and PNG decoding. Preserve native generated outputs
   and record their actual resolution separately from resized pack exports.
5. Keep source exports, baselines, intermediates, PNGs, previews, and preparation
   manifests in ignored `work/hd/` or `extracted/`. Commit only tooling/instructions.
   Record route, scale, rendering source/parameters/seeds/version, prompts/settings,
   crop boxes/transforms/masks, and accepted or
   rejected edits. The preparation manifest is provenance; the versioned binary contract above
   is what the runtime loads.

R5 must preserve original hit-testing, frame alignment, atlas regions, and logical
sizes, and identify replacements even when the original is copied into an off-screen
DIB before presentation. Indexed companions apply the same palette remaps as logical pixels. Custom type-302 animation resources are palette-index delta frames, so they
must be decoded in sequence; independent AI edits would introduce flicker and drift.
Missing, rejected, unsupported, or disabled replacements retain the original path.
Validate accepted replacements in real game scenes with the pack enabled.

Procedural reconstruction is not limited to UI. A planet can use a shaded sphere,
terrain/cloud fields, atmosphere, and rings fitted to the original; a ship can use
SVG paths, layered 2D shapes, or simple 3D geometry with shared repeated components.
Preserve distinctive land/cloud features, terminator and light direction, or ship
silhouette, perspective, component count, and insignia. Use retained/enhanced source
textures where code alone cannot match the artwork. Judge the rendered composite
against the source and baseline; neither cleaner geometry nor richer texture excuses
a changed design. This is offline asset preparation, not a requirement for a runtime
vector/3D renderer. Derived SVGs, meshes, textures, and per-asset reconstruction code
remain outside git with the pack; reusable generic tooling can live in the repository.

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

## Milestones and remaining work

| Milestone | State / acceptance |
| --- | --- |
| Display fitting | Implemented: sharp, nearest, integer, fullscreen, shared mouse viewport, density-aware cursors |
| Indexed device surfaces | Implemented: logical readback, clipping, palette mapping, raster operations, bounded sidecars |
| Sharp text and line geometry | Implemented: direct and offscreen text at 2×/4×, logical metrics, high-resolution diagonals and XOR restore |
| Regression tools | Implemented: F12 rendered screenshots; native presentation, GDI and text tests; host smoke |
| Asset inventory | Implemented: local PE bitmap export with hashes, language identity, retained indices and comparison sheets |
| Alternative bitmap scalers | Deferred: evaluate by asset class; a whole-frame pixel-art scaler can distort text and painted imagery |
| Replacement pack loader and pilot | Implemented: indexed 2×/4× loader and composition; local 68-resource STRATEGY pilot (4 cockpit/galaxy assets and 64 console states); four large assets accepted in gameplay and 64 button states reviewed offline |
| Widescreen galaxy layout | Implemented: adaptive gameplay canvas with faction controls around the centered map and expanded panel workspace; classic mode remains configurable; combined desktop visual acceptance is in progress |
| Native Windows backend | Build entrypoint and platform separation added; Windows host compilation/play testing remains unverified |

Run `cmake --build build/native` and `ctest --test-dir build/native --output-on-failure`.
The 18 tests exercise fractional and integer mouse edges, original subpixel data
surviving presentation, screenshot readback, scaled clipping, palette-index drawing,
XOR restoration, offscreen text and direct-write invalidation, HD asset format, and
HD composition. The offline HD preparation checks are
`uv run --with pillow --with pefile python -m unittest discover -s tests/tools -p 'test_build_hd_pack.py'`.
The asset-dependent UI/save smoke remains `REB_UI_SMOKE=1 tools/modern_smoke.py ...`.

The combined desktop scene pass should cover the menu, saved-game list, galaxy and
sector panels, dialog text, dragging, fullscreen, and F12. Do not claim Windows/Linux
play verification from a macOS run. Performance and visual quality at unusually
large outputs remain hardware-dependent; the renderer caps internal GDI resolution
at 4× and the gameplay canvas at 1280×480.
