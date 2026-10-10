# Design: higher resolutions and upscaling (preliminary)

Status: proposal, not started. Written Oct 2026. Companion: [modernization.md](modernization.md).

## Goal

Make the game look good on modern displays: sharp at any window size, optionally smoothed or HD artwork, and
eventually more screen space for the galaxy map, without breaking the game's logic or its look.

## How the game draws today (measured)

* **One 640×480 8-bit palettised framebuffer** (minwin's screen surface) with one global palette. The game relies on the
  palette: 79 `SelectPalette`, 9 `RealizePalette` sites, palette-changing movies, colour-index tricks (`BlitRemapped*`).
* **Self-drawn UI on Win32 windows.** Every widget is an HWND; the game positions them with absolute 640×480 coordinates
  (33 `SetWindowPos`, 9 `MoveWindow`, 49 `SetRect`, ~49 creation calls with literal coordinates, plus coordinate tables
  in code and in the resource DLLs' `RT_RCDATA`).
* **Pixels come from the game's software blitter** (`DibBitmap`: plain, colour-keyed, RLE, remapped and overlay blits) into
  8-bit DIBs, then `BitBlt`/`SetDIBitsToDevice` (31 sites) into window DCs. GDI primitives: rectangles, lines, XOR
  outlines, fills.
* **Text** goes through minwin's FreeType renderer (Arial or Liberation Sans): resolution independent at the source.
* **Assets:** 8-bit bitmaps in the resource DLLs (`STRATEGY.DLL`, `COMMON.DLL`, `ENCYBMAP.DLL`, `ALSPRITE`/`EMSPRITE`
  with ~2,000 RLE sprite frames…), Smacker movies at their authored size, the game's own cursors.
* **Presentation** (`src/port/minwin_impl/user/present.cpp`): the finished 8-bit frame is converted through the palette
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

* Cost: small, contained in `present.cpp`.
* Limitation: everything is filtered together, so text and thin UI lines get the same treatment as artwork, and pixel-art
  scalers distort text. Good enough as a first step; tier 2 fixes it.

### Tier 2 — resolution-independent minwin (the main proposal)

Render at the output resolution *inside* the GDI layer instead of upscaling the finished frame.

* The screen surface becomes **k× in device pixels while the logical coordinate space stays 640×480**: window DCs carry a
  scale; every GDI call maps logical → device coordinates.
* **Bitmaps:** each 8-bit DIB blit is scaled at blit time with the chosen scaler, in **palette-index space**, so the scaled
  surface is still 8-bit indices + one palette. Palette animation, remapped blits and palette-changing movies keep
  working unchanged. Colour-keyed and RLE sprites are scaled with key-aware variants (the key colour never bleeds).
  Scaled results are cached per (bitmap, contents generation, k).
* **Primitives** (lines, rectangles, XOR outlines, fills) are drawn at device resolution: crisp at any scale.
* **Text** is rendered by FreeType at device resolution (font size × k): sharp text at any scale, no scaler artefacts.
  Metrics stay in logical units, so line breaks don't change; `GetTextExtentPoint32` returns logical values.
* **Movies:** decoded at native size, scaled with the video scaler (or tier 3 replacements).
* **Cursors:** scaled with the bitmap scaler or replaced (tier 3).
* **Game code that reads pixels back:** none found so far (`GetPixel` is unused); any DIB the game *reads* must stay at
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

* Palette effects: replacement art is true-colour, so palette-animated or remapped items need per-item rules (keep the
  original 8-bit path for those, or provide per-palette variants). The manifest marks which items are palette-dependent.
* Movies: an optional modern codec with a permissive decoder (e.g. AV1 via dav1d, BSD) for replacement videos; Smacker
  stays the default.
* Requires the frame to become true-colour where replacement art is drawn: the scaled surface gets an RGBA overlay layer
  composited above the 8-bit layer, which keeps tier 2 intact for everything else.

### Tier 4 — larger logical resolution and widescreen (needs modernization)

More visible galaxy, larger encyclopedia, widescreen layouts. This changes the logical coordinate space and therefore
layouts and hit-testing.

* Menus and dialogs: keep 640×480 logical, centred (pillarbox), possibly scaled up independently.
* Galaxy map: the one screen that benefits most. It needs a viewport of variable size, the side panels anchored to the
  edges instead of absolute positions, and the map's scrolling/zoom logic to accept the larger viewport.
* Prerequisite: layouts expressed as data with anchors instead of literal coordinates in code (modernization M2/M6).
  Not worth starting before that.

## Milestones

| # | Milestone | Exit criterion |
|---|---|---|
| R1 | Tier 1 filters (sharp-bilinear, xBRZ, CRT) in `present.cpp`, config key, GPU path + CPU fallback | Filters selectable; 1× output unchanged |
| R2 | Scaled DC plumbing in minwin GDI: logical/device mapping, k× surface, nearest scaler only | k = 1 identical; k > 1 downsampling test exact on all scenarios |
| R3 | Text and primitives at device resolution | Sharp text at k = 2…4; line breaks unchanged (text metrics tests) |
| R4 | Index-space scalers for bitmaps (key-aware, RLE-aware), scaled-bitmap cache, movie and cursor scaling | Screenshot goldens per scaler; frame time budget met at 4K |
| R5 | Asset export tool + replacement pack loader + RGBA overlay layer | A sample pack replaces the main menu and galaxy art; fallbacks work |
| R6 | Widescreen galaxy map (after modernization) | Galaxy view uses the full window; other screens pillarboxed |

## Risks and open questions

* **Palette semantics at scale.** Index-space scaling keeps them, but some scalers blend colours; blended pixels must
  map back to palette indices (nearest index) or switch that blit to the RGBA layer. Decide per scaler.
* **Hidden pixel readback.** If game code turns out to read the screen or a window DC (screen grabs for transitions,
  XOR drag outlines), those paths need the logical-resolution copy. Audit before R2.
* **Performance.** Scaling every blit at 4K is fine on the GPU, heavier on the CPU path; caching makes static UI cheap,
  animated sprites dominate. Budget: 60 fps at 3840×2160 on a mid-range laptop.
* **Text metrics vs look.** Text drawn at k× but measured at 1× can look slightly different from the 1× glyphs (hinting);
  layout stays stable because metrics are logical.

## Decisions to make

1. Default look: pixel-perfect integer (today), sharp-bilinear to fill the screen, or a pixel-art scaler.
2. Whether HD packs are in scope (tier 3) and which art matters most (galaxy map, planets, portraits, encyclopedia).
3. Whether widescreen galaxy (tier 4) is a goal; it ties this work to the modernization plan.
