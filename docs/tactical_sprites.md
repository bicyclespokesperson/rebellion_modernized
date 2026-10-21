# Tactical battle sprites

The space battle view draws ships from a local sprite pack rendered offline in Blender (5.x, on `PATH`, in
`/Applications`, or via `--blender`/`$BLENDER`). Build it from the repository root:

```sh
uv run --locked --python 3.12 tools/build_tactical_sprites.py \
  --install-dir "orig_game_files/Star Wars - Rebellion"
```

Without a pack the battle view draws procedural silhouettes. The game finds the pack through config key
`tactical_sprite_dir` (relative to the config file) or `REB_TACTICAL_SPRITE_DIR`; `launch_game.sh` exports the latter
for `work/hd/tactical/pack` when it exists, and `--no-hd` turns it off.

## Sources

- The 29 LOD0 capital-ship meshes in `TACTICAL.DLL` (type 301, binary `.x`, id `2010 + 10 i` for Alliance type index
  `i` and `2510 + 10 (i - 15)` for Empire; LOD1/LOD2 are the next ids), with their type-303 textures (count/index RLE,
  a count of 0 meaning 256, coloured by the palette of `RT_BITMAP` 1000). The meshes have about 60 vertices and a
  128 x 128 texture: the right outline, no detail.
- Fighters have no meshes. The type-303 families 3020-3627 are explosion, ion and shield effects and 4000-4139 are
  single-view HUD icons, so fighters and the Death Star have procedural fallbacks in `tools/tactical_model_recipes/`
  (`fighter_proxies.py`, `death_star.py`).
- Remodels: every class has a committed Blender recipe in `tools/tactical_model_recipes/` (status below). A built
  remodel, `work/hd/tactical/models/<typeIndex>_<slug>.blend` with an object named `ship`, replaces its class's mesh
  or procedural fallback by presence (`--models-dir`; `--no-remastered` ignores them). Pack its textures and avoid
  linked libraries: the render cache fingerprints only the .blend itself.

The model convention (metres, +X bow, +Z up, compressed class lengths) and the view conventions are in the header of
`tools/tactical_sprites_bpy.py`; the manifest records each class's origin and any rescale.

## Outputs and pack format

Everything stays in ignored `work/hd/tactical/` and is never committed: `extract/` (meshes and textures), `renders/`
(frames, cached per class by a fingerprint of sources, recipes, scripts and Blender version; `--force` re-renders,
`--only` re-renders a few classes), `pack/`, and `review/` contact sheets (both sizes on dark and light backgrounds,
plus a same-scale line-up).

The pack is `manifest.txt` (version line, then `setting`, `source` hashes, `atlas`, `class`, `view` and `frame`
records; comments explain the fields) and QOI atlases per class and size.
`src/port/native/space_battle/sprite_pack.{h,cpp}` loads it; the header documents the heading, pivot and projection
conventions. The `native_space_battle_sprite_pack` test decodes a fixture from the Python encoder, so change both
encoders together. To see what a pack uses: `grep '^class' work/hd/tactical/pack/manifest.txt` (remodelled classes
say `remastered`).

## Remodel status

All 38 classes are remodelled. IoU is the silhouette fit against the encyclopedia reference, from `report.json`.

| Type | Class | Side | IoU | Family | Tactical size (m) | Reference |
|---|---|---|---|---|---|---|
| 0 | Mon Calamari Cruiser | Alliance | 0.921 | Organic shell | 1750 | EDATA.042 |
| 1 | Bulk Cruiser | Alliance | 0.824 | Frigates and cruisers | 1258 | EDATA.043 |
| 2 | Assault Frigate | Alliance | 0.722 | Dreadnaught hull | 1138 | EDATA.044 |
| 3 | Nebulon-B Frigate | Alliance | 0.796 | Frigates and cruisers | 805 | EDATA.045 |
| 4 | Alliance Escort Carrier | Alliance | 0.926 | Escort carriers | 1414 | EDATA.046 |
| 5 | Corellian Corvette | Alliance | 0.859 | Corellian | 796 | EDATA.047 |
| 6 | Medium Transport | Alliance | 0.928 | Transports | 1193 | EDATA.048 |
| 7 | Bulk Transport | Alliance | 0.902 | Transports | 1047 | EDATA.049 |
| 8 | Corellian Gunship | Alliance | 0.905 | Corellian | 746 | EDATA.050 |
| 9 | Alliance Dreadnaught | Alliance | 0.943 | Dreadnaught hull | 1056 | EDATA.051 |
| 10 | CC-7700 Frigate | Alliance | 0.922 | Frigates and cruisers | 996 | EDATA.052 |
| 11 | CC-9600 Frigate | Alliance | 0.887 | Frigates and cruisers | 862 | EDATA.055 |
| 12 | Bulwark Battlecruiser | Alliance | 0.871 | Alliance capitals | 1559 | EDATA.053 |
| 13 | Liberator Cruiser | Alliance | 0.976 | Alliance capitals | 907 | EDATA.054 |
| 14 | Dauntless Cruiser | Alliance | 0.896 | Alliance capitals | 1600 | EDATA.056 |
| 15 | Strike Cruiser | Empire | 0.915 | Frigates and cruisers | 889 | EDATA.057 |
| 16 | Lancer Frigate | Empire | 0.899 | Frigates and cruisers | 807 | EDATA.058 |
| 17 | Interdictor Cruiser | Empire | 0.858 | Star Destroyer wedge | 1024 | EDATA.059 |
| 18 | Carrack Light Cruiser | Empire | 0.964 | Frigates and cruisers | 903 | EDATA.060 |
| 19 | Victory Destroyer | Empire | 0.902 | Star Destroyer wedge | 1469 | EDATA.061 |
| 20 | Imperial Star Destroyer | Empire | 0.918 | Star Destroyer wedge | 1602 | EDATA.062 |
| 21 | Super Star Destroyer | Empire | 0.916 | Star Destroyer wedge | 4795 | EDATA.063 |
| 22 | Assault Transport | Empire | 0.901 | Transports | 535 | EDATA.064 |
| 23 | Galleon | Empire | 0.945 | Transports | 819 | EDATA.066 |
| 24 | Victory II Star Destroyer | Empire | 0.887 | Star Destroyer wedge | 1469 | EDATA.067 |
| 25 | Imperial II Star Destroyer | Empire | 0.915 | Star Destroyer wedge | 1602 | EDATA.068 |
| 26 | Star Galleon | Empire | 0.961 | Transports | 927 | EDATA.069 |
| 27 | Imperial Escort Carrier | Empire | 0.963 | Escort carriers | 1171 | EDATA.070 |
| 28 | Imperial Dreadnaught | Empire | 0.943 | Dreadnaught hull | 1056 | EDATA.071 |
| 29 | X-wing | Alliance | 0.760 | Alliance fighters | 100 | EDATA.036 |
| 30 | Y-wing | Alliance | 0.780 | Alliance fighters | 128 | EDATA.037 |
| 31 | A-wing | Alliance | 0.908 | Alliance fighters | 76.8 | EDATA.034 |
| 32 | B-wing | Alliance | 0.686 | Alliance fighters | 135.2 | EDATA.035 |
| 33 | TIE Fighter | Empire | 0.912 | TIE family | 60 | EDATA.038 |
| 34 | TIE Interceptor | Empire | 0.779 | TIE family | 76.8 | EDATA.039 |
| 35 | TIE Bomber | Empire | 0.732 | TIE family | 62.4 | EDATA.040 |
| 36 | TIE Defender | Empire | 0.616 | TIE family | 73.6 | EDATA.041 |
| 37 | Death Star | Empire | 0.992 | Unique | 9600 | EDATA.065 |

Type indices match `src/game/space_battle/unit_names.cpp`. Tactical sizes use bow-axis length for capitals and
largest dimension for fighters and the Death Star. Values are the original meshes × 800 m (ships) and
8× the canonical largest dimension (fighters, so they stay visible); the sizes in `tools/build_tactical_sprites.py`
are authoritative. References are the
encyclopedia renders that `tools/build_encyclopedia_icons.py` extracts to `work/hd/encyclopedia/EDATA.0nn/`.
Use the guarded mask corrections in `tools/encyclopedia_icon_recipes.json` before fitting: the Bulwark's three
visible pods and dark intake, the TIE family's opaque solar-panel interiors, the Victory bow and the Bulk Transport
nose are otherwise omitted by automatic segmentation. Old cached masks can also include the planet behind the Super
Star Destroyer; regenerate them before fitting.

### Fit exceptions

- **Below the original mesh's CPU baseline:** Liberator (0.9759 vs 0.9806) and Bulk Cruiser (0.8236 vs 0.8267) beat
  the original rendered through the same Blender pipeline (0.9721, 0.8220); `tools/check_tactical_model_baseline.py
  <type>` reproduces this. The Bulk Cruiser and Assault Frigate references have corrupted pixels that the recipes do
  not reproduce.
- **Fighters below 0.85** (X-wing 0.761, Y-wing 0.780, TIE Interceptor 0.779, TIE Bomber 0.732, B-wing 0.686) have
  no original mesh baseline; they were accepted on coherent geometry and all-heading visual review. The TIE Defender
  (0.616) is the clearest limitation: its six panel caps flare outward, away from the cockpit, and sweep toward the
  bow (+30 recipe units) more than the stern (−14); pose and contour still differ from the reference.
- **Death Star** (0.992) verifies the outline only; its camera is also constrained to align dish and trench, and the
  surface was reviewed in heroes, all headings and native close-ups.
- **Deliberate deviations:** Imperial II keeps the Imperial Star Destroyer's square stern; CC-9600's solid aft case
  follows the encyclopedia rather than the mesh's stern notch; the Dreadnaughts mirror the six visible flank
  blisters onto the hidden side; the Bulwark's reference is viewed from below the battle plane.
- Victory II and Assault Transport never appear in the calibration corpus, so their native reviews use synthetic
  snapshots. `REB_SPACE_BATTLE_SIDE=empire` focuses the close-up on an Empire remodel.

## How to remodel a class

### Once per machine

- Blender 5.2 or later (`brew install --cask blender`; the tools find it on `PATH`, in `/Applications`, or via
  `$BLENDER`), and `uv`.
- The encyclopedia references: `uv run --locked --python 3.12 tools/build_encyclopedia_icons.py --install-dir
  "orig_game_files/Star Wars - Rebellion" --no-overrides` (writes `work/hd/encyclopedia/`).
- The original meshes, used for comparison: run the sprite pack build once (below); it extracts them to
  `work/hd/tactical/extract/`.

### Per class

1. **Write the recipe** `tools/tactical_model_recipes/<slug>.py`. A class recipe declares module constants
   `TYPE_INDEX`, `NAME`, `ENCYCLOPEDIA` (e.g. `"EDATA.061"`), `LENGTH` (from the table above), and `VIEW`
   (yaw and pitch bounds of the encyclopedia camera, see Pitfalls; optional `roll_seeds` angles in degrees
   help asymmetric ships such as B-wing), and a `build(palette, length)` function that
   builds the parts from the primitives in `_kit.py` (lofts, superellipse sections, prisms, boxes, cylinders,
   explicit spheres, bevels, voxel-remesh unions, greeble boxes; plated, organic and emissive materials) and returns
   `kit.finalize(parts, length)`. Copy the closest existing recipe: `imperial_star_destroyer.py` for hard-surface
   hulls, `mon_calamari_cruiser.py` for organic ones. Conventions: one object named `ship`, +X forward (bow), +Y port,
   +Z up, origin at the bounding-box centre, 1 unit = 1 m, the X extent equal to the class length.
2. **Build and validate**: `uv run --locked --script tools/build_tactical_models.py <slug>` (or the type index;
   `--quick` skips the 64-heading frames while iterating; `--list` shows the recipes it found). Each run samples hull
   colours from the reference, builds `work/hd/tactical/models/<type>_<slug>.blend`, fits a camera to the
   encyclopedia view, and writes review images and `report.json` to `work/hd/tactical/models/review/<type>_<slug>/`.
3. **Review**, by looking at the images, not only at the numbers:
   - `fit_sheet.png`: reference and model at the fitted camera; silhouette overlap (red: model only, blue: reference
     only); model edges over the reference; the original mesh under the same fit; and the plan view against the
     original mesh.
   - `heroes.png` (front and rear three-quarter views), `headings_200px.png` and `headings_50px.png` (how it will
     look in battle, zoomed in and zoomed out), `top.png`.
4. **Rebuild the pack**: `uv run --locked --python 3.12 tools/build_tactical_sprites.py --install-dir
   "orig_game_files/Star Wars - Rebellion"` (only changed classes re-render). Check
   `work/hd/tactical/review/scale_lineup.png` and the `sheet_*.png` contact sheets: the remodel must sit in scale and
   in tone with its neighbours.
5. **Look at it in a battle**: `REB_SPACE_BATTLE_DEMO=<a snapshot containing the class>
   REB_TACTICAL_SPRITE_DIR=$PWD/work/hd/tactical/pack REB_SPACE_BATTLE_REVIEW_DIR=<dir>` on the game executable
   writes review captures in both views (top-down and angled) without showing a window; or play with
   `launch_game.sh`. Real snapshots come from `REB_BATTLE_CORPUS` (see `docs/ai_matches.md`); `grep -l
   'type-index=<N> ' <corpus>/*/*.snapshot` finds battles with the class.
6. **Commit the recipe only** (never `.blend` files, renders or anything derived from the game's pixels: they stay in
   ignored `work/`), and update this document's status row with the IoU from `report.json`.

### Fighters and the Death Star

All classes accept `work/hd/tactical/models/<type>_*.blend` overrides. Without one, fighters (29–36) and
Death Star (37) retain their procedural fallback; `--no-remastered` selects those fallbacks explicitly.

Fighter and Death Star recipes must declare `FIT = "size"`, set `LENGTH` to the largest tactical dimension in
the table, and call `kit.finalize(parts, length, FIT)`. Capitals default to `FIT = "length"`, measured along X.
The model builder checks the resulting extent, and the sprite renderer uses the same fit so a tall TIE or B-wing
is not enlarged a second time. Fighters use canonical size × 8; Death Star uses diameter 9600 m.
These classes have no original mesh baseline: review their reference silhouette and all gameplay views directly.
Native simulator review also captures the first deployed friendly squadron at close zoom in angled and top views
(`04c_fighter_angled_close` and `04d_fighter_top_close`), so use a snapshot containing the target fighter.
The fabricated Death Star review captures the station at close zoom in both views
(`hud_deathstar_03a_station_top_close` and `hud_deathstar_03b_station_angled_close`).

Run `uv run tools/test_tactical_model_pipeline.py` to check override/fallback selection and both sizing modes
through Blender and the actual sprite normalizer using synthetic geometry.

### Acceptance

- Silhouette IoU at least the original mesh's under the same fit (the tool reports both). The original meshes
  already score about 0.91, so a fixed threshold like 0.85 means nothing; beating the original is the bar.
  The default original baseline uses CPU polygon coverage, while remodel IoU uses rendered alpha. For a close
  result, `uv run tools/check_tactical_model_baseline.py <type>` renders the original through the same Blender
  pipeline. Record both baseline values when the rasterization difference changes acceptance.
- Plan-view IoU against the original mesh about 0.85 or better, unless a deliberate deviation (like the Imperial Star
  Destroyer's square stern) is noted in the recipe. The encyclopedia never shows the top-down view the game uses most,
  so this is the check that matters for gameplay.
- It reads at 200 px (detail) and at 50 px (silhouette and tone).
- Deterministic: two builds give the same geometry hash in `report.json`.
- Colours look like the reference, not tinted by the 1998 renderer's blue-violet shadows.

## Pitfalls (learned on the first two classes)

- **One three-quarter view barely constrains proportions.** IoU stays flat within ~0.006 across about ±5 degrees of
  yaw and ±10% of length. Take proportions from the original mesh (plan view) and the reference's details, and use
  IoU to catch gross errors.
- **Silhouettes are mirror-ambiguous**: a view from behind and below can fit as well as the true one. Every recipe
  declares `VIEW` bounds; the encyclopedia renders so far look at the port bow from nearly level (pitch about 4
  degrees; yaw about 38 for the Star Destroyer, 54 for the Mon Calamari).
- **Place features by triangulation**, not by eye: fit the hull first, then put towers, domes and engines where the
  fitted camera says the reference shows them (the Star Destroyer's bridge was 90 m off when eyeballed).
- **Keep builds deterministic**: `bmesh.ops.create_uvsphere` changes face order between runs; use the kit's explicit
  spheres. Seed every random placement.
- **Colour** is sampled from the lit half of the fitted view (k-means tints for multi-coloured hulls); an all-pixel
  mean turned the Star Destroyer lavender.
- **Lights**: previews use EEVEE and the light rig imported from `tools/tactical_sprites_bpy.py`
  (`KEY_LIGHT`, `FILL_LIGHT`, `AMBIENT`, `sun`), so what you calibrate is what the pack shows. Renaming those breaks
  this tool.
- **The recipe folder is shared**: `fighter_proxies.py` and `death_star.py` belong to the sprite pipeline. Only modules
  that declare `TYPE_INDEX` and `ENCYCLOPEDIA` are class recipes. A remodel replaces its original mesh
  or procedural fallback automatically.
- Heavy meshes are fine (the Mon Calamari is 320,000 triangles); everything is offline.


## Handing a class to an agent

A brief that has worked:

> Remodel the <class> (tactical type index <N>, encyclopedia <EDATA.0nn>) as a Blender recipe, following
> `docs/tactical_sprites.md` exactly: start from `<closest recipe>.py`, iterate with
> `tools/build_tactical_models.py <slug> --quick` and look at every review image yourself, meet the acceptance
> criteria, rebuild the sprite pack, check the class in a battle review capture in both views, commit only the
> recipe, and update the status table. Report the IoU numbers and include the fit sheet and hero images.
