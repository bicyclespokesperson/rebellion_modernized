# Design: higher resolutions and upscaling (preliminary)

Status: renderer/pack support proposed, not started; asset preparation skill available.
Written Oct 2026. Companion: [aggressive modernization](modernization_aggressive.md).

## Goal

Make the game look good on modern displays: sharp at any window size, optionally smoothed or HD artwork, and
eventually more screen space for the galaxy map, without breaking the game's logic or its look.

## How the game draws today (measured)

- **One 640×480 8-bit palettised framebuffer** (minwin's screen surface) with one global palette. The game relies on the
  palette: 79 `SelectPalette`, 9 `RealizePalette` sites, palette-changing movies, colour-index tricks (`BlitRemapped*`).
- **Self-drawn UI on Win32 windows.** Every widget is an HWND; the game positions them with absolute 640×480 coordinates
  (33 `SetWindowPos`, 9 `MoveWindow`, 49 `SetRect`, ~49 creation calls with literal coordinates, plus coordinate tables
  in code and in the resource DLLs' `RT_RCDATA`).
- **Pixels come from the game's software blitter** (`DibBitmap`: plain, colour-keyed, RLE, remapped and overlay blits) into
  8-bit DIBs, then `BitBlt`/`SetDIBitsToDevice` (31 sites) into window DCs. GDI primitives: rectangles, lines, XOR
  outlines, fills.
- **Text** goes through minwin's FreeType renderer (Arial or Liberation Sans): resolution independent at the source.
- **Assets:** 8-bit bitmaps in the resource DLLs (`STRATEGY.DLL`, `COMMON.DLL`, `ENCYBMAP.DLL`, `ALSPRITE`/`EMSPRITE`
  with ~2,000 RLE sprite frames…), Smacker movies at their authored size, the game's own cursors.
- **Presentation** (`src/port/minwin_impl/user/present.cpp`): the finished 8-bit frame is converted through the palette
  and shown at an integer scale with nearest-neighbour, letterboxed (window or fullscreen).

## Tiers

Each tier is independently useful; later tiers build on earlier ones. Logical coordinates stay 640×480 in tiers 0–3,
so game logic, hit-testing and layouts are untouched.

### Tier 0 — integer scaling (done)

Nearest-neighbour ×k, letterboxed, exact mouse mapping. Pixel-perfect, never blurry, wastes some screen.

### Tier 1 — post-process filters at presentation

Apply a filter to the final RGB frame on the GPU (SDL3 GPU/renderer with a shader, or a CPU fallback): sharp-bilinear
(fills the screen at non-integer scales without shimmer), pixel-art scalers (xBRZ, ScaleFX, hqx), optional CRT/scanline
look. Config key `upscale_filter`.

- Cost: small, contained in `present.cpp`.
- Limitation: everything is filtered together, so text and thin UI lines get the same treatment as artwork, and pixel-art
  scalers distort text. Good enough as a first step; tier 2 fixes it.

### Tier 2 — resolution-independent minwin (the main proposal)

Render at the output resolution _inside_ the GDI layer instead of upscaling the finished frame.

- The screen surface becomes **k× in device pixels while the logical coordinate space stays 640×480**: window DCs carry a
  scale; every GDI call maps logical → device coordinates.
- **Bitmaps:** each 8-bit DIB blit is scaled at blit time with the chosen scaler, in **palette-index space**, so the scaled
  surface is still 8-bit indices + one palette. Palette animation, remapped blits and palette-changing movies keep
  working unchanged. Colour-keyed and RLE sprites are scaled with key-aware variants (the key colour never bleeds).
  Scaled results are cached per (bitmap, contents generation, k).
- **Primitives** (lines, rectangles, XOR outlines, fills) are drawn at device resolution: crisp at any scale.
- **Text** is rendered by FreeType at device resolution (font size × k): sharp text at any scale, no scaler artefacts.
  Metrics stay in logical units, so line breaks don't change; `GetTextExtentPoint32` returns logical values.
- **Movies:** decoded at native size, scaled with the video scaler (or tier 3 replacements).
- **Cursors:** scaled with the bitmap scaler or replaced (tier 3).
- **Game code that reads pixels back:** none found so far (`GetPixel` is unused); any DIB the game _reads_ must stay at
  logical resolution, so scaling happens only on the path to the screen. A source bitmap the game draws into (an
  off-screen DIB composed in game code, e.g. the battle banner) is scaled when it is blitted to a window.

Verification: at k = 1 the output must be byte-identical to today (current screenshot parity stays the test). At k > 1
with the nearest scaler, box-downsampling the k× frame must reproduce the 1× frame exactly (automated test on every
scripted scenario). Other scalers are checked with screenshot goldens.

### Tier 3 — HD replacement assets (optional packs)

Let a user-supplied pack directory replace artwork: bitmaps keyed by (resource DLL, resource id[, frame]), sprites, cursors,
and optionally movies, at any integer multiple of the original size, with alpha instead of a colour key. Missing items
fall back to tier-2 scaling. Packs are never part of this repository (they derive from the game's art); a small tool
exports the originals to PNG with a manifest so anyone can produce a pack (by hand or with an upscaling model).

Use the repository skill [flowers-upscale](../../.agents/skills/flowers-upscale/SKILL.md)
(`$flowers-upscale`) to prepare and review selected replacements. It adapts the
@flowersslop contextual crop workflow to this game's rendered/painted art, with
deterministic baselines, SVG/procedural reconstruction, hybrid rendering, and
contextual AI where each improves the result. The skill is
available now; the exporter, loader, and overlay remain R5 implementation work.

- Palette effects: replacement art is true-colour, so palette-animated or remapped items need per-item rules (keep the
  original 8-bit path for those, or provide per-palette variants). The manifest marks which items are palette-dependent.
- Movies: an optional modern codec with a permissive decoder (e.g. AV1 via dav1d, BSD) for replacement videos; Smacker
  stays the default.
- Requires the frame to become true-colour where replacement art is drawn: the scaled surface gets an RGBA overlay layer
  composited above the 8-bit layer, which keeps tier 2 intact for everything else.

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
   rejected edits. The preparation manifest is provenance; define/version the actual
   loader schema during R5 rather than assuming the skill's records are loadable.

R5 must preserve original hit-testing, frame alignment, atlas regions, and logical
sizes, and identify replacements even when the original is copied into an off-screen
DIB before presentation. A static RGBA replacement cannot reproduce arbitrary palette
remaps. Custom type-302 animation resources are palette-index delta frames, so they
must be decoded in sequence; independent AI edits would introduce flicker and drift.
Missing, rejected, unsupported, or disabled replacements retain the original path.
Validate accepted replacements in real game scenes once the loader is available.

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

### Tier 4 — larger logical resolution and widescreen (needs modernization)

More visible galaxy, larger encyclopedia, widescreen layouts. This changes the logical coordinate space and therefore
layouts and hit-testing.

- Menus and dialogs: keep 640×480 logical, centred (pillarbox), possibly scaled up independently.
- Galaxy map: the one screen that benefits most. It needs a viewport of variable size, the side panels anchored to the
  edges instead of absolute positions, and the map's scrolling/zoom logic to accept the larger viewport.
- Prerequisite: layouts expressed as data with anchors instead of literal coordinates in code (modernization M2/M6).
  Not worth starting before that.

## Milestones

| #   | Milestone                                                                                                                     | Exit criterion                                                                                                                                                                  |
| --- | ----------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| R1  | Tier 1 filters (sharp-bilinear, xBRZ, CRT) in `present.cpp`, config key, GPU path + CPU fallback                              | Filters selectable; 1× output unchanged                                                                                                                                         |
| R2  | Scaled DC plumbing in minwin GDI: logical/device mapping, k× surface, nearest scaler only                                     | k = 1 identical; k > 1 downsampling test exact on all scenarios                                                                                                                 |
| R3  | Text and primitives at device resolution                                                                                      | Sharp text at k = 2…4; line breaks unchanged (text metrics tests)                                                                                                               |
| R4  | Index-space scalers for bitmaps (key-aware, RLE-aware), scaled-bitmap cache, movie and cursor scaling                         | Screenshot goldens per scaler; frame time budget met at 4K                                                                                                                      |
| R5  | Asset export/inventory tool + replacement pack loader + RGBA overlay layer; prepare a selective pilot with `$flowers-upscale` | Frequently visible static gameplay art (and menu art if useful) improves at 2×/4×; geometry/alpha/palette rules and per-item fallbacks work; encyclopedia is not a prerequisite |
| R6  | Widescreen galaxy map (after modernization)                                                                                   | Galaxy view uses the full window; other screens pillarboxed                                                                                                                     |

## Risks and open questions

- **Palette semantics at scale.** Index-space scaling keeps them, but some scalers blend colours; blended pixels must
  map back to palette indices (nearest index) or switch that blit to the RGBA layer. Decide per scaler.
- **Hidden pixel readback.** If game code turns out to read the screen or a window DC (screen grabs for transitions,
  XOR drag outlines), those paths need the logical-resolution copy. Audit before R2.
- **Performance.** Scaling every blit at 4K is fine on the GPU, heavier on the CPU path; caching makes static UI cheap,
  animated sprites dominate. Budget: 60 fps at 3840×2160 on a mid-range laptop.
- **Text metrics vs look.** Text drawn at k× but measured at 1× can look slightly different from the 1× glyphs (hinting);
  layout stays stable because metrics are logical.

## Decisions to make

1. Default look: pixel-perfect integer (today), sharp-bilinear to fill the screen, or a pixel-art scaler.
2. Whether to implement HD pack support (tier 3). Preparation uses `$flowers-upscale`;
   prioritize galaxy/sector/planet and routine gameplay portrait/unit art, with
   encyclopedia images later. Select exact pilot resource IDs after the inventory
   and palette audit.
3. Whether widescreen galaxy (tier 4) is a goal; it ties this work to the modernization plan.
