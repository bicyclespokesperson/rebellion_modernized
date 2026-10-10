---
name: flowers-upscale
description: Prepare faithful HD replacement artwork for Rebellion using deterministic scaling, SVG or procedural reconstruction, hybrid rendering, or contextual AI enhancement with registered crop blending. Use for static game-art and pack preparation, not runtime filters, UI layout changes, or independent animation-frame enhancement.
---

# Rebellion asset upscaling

Adapted from the flowers-upscale workflow supplied by the user, credited to
@flowersslop. Preserve its original-context edits, registration, overlap blending,
and defect checks; replace its photographic/8K defaults with game-specific routing.

Read [the upscaling plan](../../../docs/design/high_resolution.md).
Tier 3 is proposed: do not assume an exporter, replacement loader, or manifest format
has already been implemented. This skill prepares assets; it does not implement the
renderer. Keep derived artwork local under ignored `work/hd/` or `extracted/`.

## Establish the source and target

- Use the user's chosen assets, or inspect the local licensed installation under
  `orig_game_files/`. Do not ask for an upload when the source is already available.
  Ask only if the source or requested asset cannot be identified.
- Inspect the decoded asset and its actual use: source dimensions, palette,
  transparency, atlas regions, animation membership, logical bounds, and maximum
  intended display size. A screenshot is context, not a substitute for the asset.
- Keep the original bytes, indexed pixels, palette, and decoder metadata unchanged.
  Record a source hash and resource identity: module, resource type/id/name,
  language where applicable, frame, or external file path. `EData/EDATA.nnn` files
  are BMPs despite their extension; `ENCYBMAP.DLL` refers to them.
- Default to **2× each source dimension**, preserving aspect ratio. Use 4× when
  the intended display size or comparison shows a benefit. A 400×200 encyclopedia
  image becomes 800×400 or 1600×800; a 640×480 background becomes 1280×960 or
  2560×1920. Choose an exact integer pack scale, not an arbitrary longest edge.
  Do not default to 8K. Follow an explicitly requested size, noting any pack
  compatibility issue before generating.
- Discover available vector/procedural rendering, AI editing, and raster processing
  tools before selecting a workflow. Code-driven rendering does not require an
  image-generation model. If AI editing or reliable
  registration is unavailable, report the limitation and produce the applicable
  deterministic baseline; do not claim an AI or tiled result was completed.

## Route per asset, before spending generation calls

| Asset | Preferred treatment |
|---|---|
| Live text, lines, rectangles, ordinary UI geometry | Tier-2 device-resolution rendering |
| Bitmap labels, logos, emblems, buttons, borders, tiny map markers, cursors | Exact SVG/code reconstruction where shapes are clear, or nearest/key-aware pixel-art scaling |
| Gradients, flat fills, sparse stars, simple backgrounds | SVG/procedural rendering or deterministic resize; preserve authored positions and colors |
| Planets and terrain | Consider procedural sphere/terrain, atmosphere, shading, and texture; match original features, palette, and lighting |
| Static ships, vehicles, stations | Consider SVG, layered 2D rendering, or simple 3D geometry for clear silhouettes/components; preserve perspective and design |
| Static portraits and textured menu/galaxy/encyclopedia artwork | Deterministic baseline, then code-driven, hybrid, or contextual AI treatment according to structure and texture |
| Palette-animated/remapped art or unknown palette behavior | Keep original indexed path until semantics are audited; optional variants need explicit per-item rules |
| Delta/RLE sprite sequences, movies, briefing animations | Deterministic scaling by default; any AI work needs a separate temporally consistent sequence workflow |

Assess palette dependence at the call site, not from the file's bit depth alone.
`Sprite::m_drawMode` and remap tables can change how the same bitmap is used. Custom
type-302 `DeltaFrame` resources contain palette-index differences: decode a complete
sequence before reviewing it; never enhance the encoded delta or independent frames.

For each static candidate, compare nearest-neighbour and a suitable smooth resize
(e.g. bicubic/Lanczos for continuous-tone art, checking overshoot) at intended display
size. Pixel-art scalers are another candidate, not a universal choice. Keep the
cheapest result that meets visual quality. Tiny, indistinct sources do not become
reliable structural references just because they are tiled.

## SVG, procedural, and hybrid reconstruction

LLM-authored rendering code is a first-class option, including for planets and
ships. Choose by how well geometry, materials, and lighting can be constrained from
the reference, not by a rule that only icons can be procedural. Reconstruct the
existing art rather than designing a generic planet or a different ship.

- Use SVG paths/gradients for clear contours and layered shapes; 2D raster/vector
  code for terrain, textures, stars, and shading; simple 3D rendering where it helps
  match ship perspective, occlusion, and mechanical geometry. Use established
  repo-native tooling where practical; no runtime SVG/3D support is implied.
- For planets, preserve disc size/position, limb, terminator, light direction,
  atmosphere, rings, and recognizable land/cloud features. Fit procedural parameters
  to the source. Use fixed seeds and constrain texture to the authored features;
  unrestricted noise can change the planet's identity.
- For ships, preserve silhouette, camera angle, proportions, engine/wing/weapon
  count, panel layout, insignia, and highlights. Use shared geometry for repeated
  parts. Ambiguous structure is a reason to retain source pixels or use a hybrid,
  not permission to invent components.
- Combine approaches region by region: procedural geometry/shading with retained
  or enhanced texture, SVG lettering/emblems over raster artwork, or code-rendered
  backgrounds behind reconstructed objects. Review the composite as one image.
- Retain editable SVG/code, parameters, seeds, renderer/version, and any source
  textures alongside local outputs. Avoid automatic tracing that merely preserves
  the source's jagged pixels. Render at target dimensions and compare against the
  original at logical size and the baseline at intended display size; check
  silhouettes, landmarks, colors, and alpha on light/dark backgrounds.
- Rasterize accepted results to the same PNG pack contract. Keep logical bounds,
  source mask/hit-test semantics, and palette rules intact. Code reconstruction of
  baked labels must preserve their wording, placement, and metrics. Derived SVGs,
  meshes, textures, and per-asset reconstruction code stay local with the pack;
  reusable generic tooling may be committed.

## Contextual AI enhancement

Prioritize frequently visible gameplay art: galaxy/sector/planet views and portraits
or unit artwork used in normal play. Small icons can be frequent yet still be best
handled by SVG/code reconstruction or deterministic scaling. Menu art is a convenient static pilot. Encyclopedia images
are low priority because they are rarely viewed; do not start with their full library.

Start with a representative pilot, not a whole library. Use **one whole-image edit**
when the asset fits the tool's native output budget; stop if it beats the baseline
and passes fidelity checks. Add a selective second pass only for visible defects or
useful detail. A third pass needs a specific unresolved defect and a reason that
another edit can fix it. There is no mandatory pass-count question or nine-call grid.

Every AI edit receives **both the unchanged original as context and the target crop**,
even in later passes. For a whole-image edit these can be the same image. Inspect
local images before editing and identify each input's role explicitly. Use the
available image-editing tool and its applicable skill instructions. Record the exact
prompt, tool/model/settings when exposed, returned dimensions, and output immediately.

Use this prompt foundation, adding only relevant material constraints:

> Input 1 is the original Rebellion artwork and the authority for identity, design,
> composition, color, lighting, silhouette, and style. Input 2 is the target: return
> that exact crop, framing, and geometry. Improve clarity and reconstruct restrained
> detail consistent with this late-1990s rendered/painted artwork. Preserve pose,
> expression, proportions, object count, focus, and original imperfections. Preserve
> faction emblems, lettering, ship components, panel spacing, terrain boundaries,
> and star positions. Do not redesign or reinterpret ambiguous shapes. Repair
> compression/dithering artifacts only where the underlying form is supported.
> Avoid beautification, photorealistic restyling, relighting, invented markings or
> structures, extra stars, halos, ringing, embossed/crosshatched/repeated texture,
> synthetic grain, and blanket sharpening. Keep low-information regions quiet.

For portraits preserve likeness, facial proportions, costume, and painterly/rendered
skin; do not request pores or detailed irises absent from the source. For ships and
vehicles preserve hard edges, repeated components, perspective, and mechanical
symmetry; prefer leaving an ambiguous detail alone to inventing a weapon or panel.
Protect baked text, logos, UI borders, and other exact regions with masks and
deterministically scaled originals or reviewed vector/code reconstructions; do not
rely on prompt compliance alone.

For large textured artwork or a failed whole-image edit, read
[the crop workflow](references/crop-workflow.md). Use it only when its added calls
and registration can produce a useful improvement at the target size.

## Transparency and composition

Determine whether the asset is opaque or keyed from its use. Do not infer that every
black pixel is transparent, or treat every bitmap's first pixel as a key. For keyed
art, derive the mask from the actual palette index and decoded blitter semantics
before conversion to RGB. Keep the original mask as the authority; scale it without
introducing opaque background pixels. Preserve pivots, logical extents, hit-test
behavior, atlas boundaries, and frame alignment. AI output cannot redefine them.

Keep color processing separate from alpha, use premultiplied-alpha filtering/blending
for soft edges, and inspect on light and dark backgrounds. Do not send a packed atlas
for unconstrained editing: isolate regions with padding and reassemble their original
layout. Do not replace palette-dependent resources with static RGBA just to get a
sharper image.

## Review and export

- Inspect original/baseline/enhanced comparisons at intended in-game size, the full
  image, and 100% focal/edge crops. Check identity, rigid geometry, lettering,
  silhouettes, colors, alpha fringes, seams, doubling, and invented texture.
- Reject changes that improve apparent detail while changing design. Keep the prior
  pixels or the deterministic baseline. Allow at most two targeted retries per
  defect; disclose rejected regions and unresolved defects. Stop when further work
  is not visibly useful at display size.
- Export lossless PNG at the exact selected dimensions; decode it again and verify
  dimensions, mode/alpha, and source-to-output alignment. Keep native generated
  crops and intermediates separately from the pack-sized output.
- Store a local preparation manifest beside the output: source identity/hash/path,
  dimensions, palette/key/mask and semantic flags (including unknowns), pack scale,
  routing decision, baseline method, rendering source/parameters/seeds/version where
  used, prompts/settings and actual native output sizes,
  crop boxes/transforms/masks when used, accepted/rejected edits, review results,
  and output paths. This is provenance, not an implemented loader contract.
- Deliver output paths and previews plus prompts/settings and limitations. Describe
  generated detail as **reconstructed**, and distinguish native generated resolution
  from final exported resolution. Generated art remains outside git.
