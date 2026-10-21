# Local high-resolution artwork packs

`tools/build_hd_pack.py` prepares 4× indexed companions from uncompressed 8-bit
RT_BITMAP resources. It uses the source bitmap's palette at runtime and writes one
`.rhd` file per original resource fingerprint, an indexed PNG for inspection, and
a manifest with source identity and hashes. The CLI requires explicit methods and
key rules for every selected ID; it does not infer that a bitmap is keyed or that a
particular scaler suits an asset. Manifest audit status remains pending until each
candidate has been reviewed.

Install the script's declared Python dependencies through `uv`, then build a small
explicit selection. For example, this command makes the four galaxy backgrounds
and frames; the frame resources use palette index 252 as their transparent color,
while the galaxy backgrounds are opaque:

```sh
uv run tools/build_hd_pack.py \
  --module 'orig_game_files/Star Wars - Rebellion/STRATEGY.DLL' \
  --id 900 --method 900=lanczos --key 900=252 \
  --id 901 --method 901=lanczos --key 901=252 \
  --id 902 --method 902=lanczos --key 902=none \
  --id 903 --method 903=lanczos --key 903=none \
  --out work/hd/gameplay-pack
```

The 68-resource local gameplay pilot has a complete explicit recipe at
`tools/hd_gameplay_recipe.json`. Reproduce it with:

```sh
uv run tools/build_hd_pack.py --recipe tools/hd_gameplay_recipe.json
```

The recipe chooses keyed Lanczos for frames 900/901, opaque Lanczos for backdrops
902/903, and two Scale2x passes for the 64 small console-button states. The tool
merges new records into an existing compatible manifest by pack filename, so a
partial follow-up build retains earlier assets. Generated images and licensed source
data belong under ignored `work/hd/`, never in a commit.

To enable a pack, set `hd_asset_dir` in the game's configuration file to the pack
directory, for example:

```ini
hd_asset_dir=work/hd/gameplay-pack
```

`launch_game.sh` automatically builds a missing `work/hd/gameplay-pack/` from the
selected config's `install_dir` using the committed recipe and `uv`, then supplies
it as a fallback through `REB_HD_ASSET_DIR`. If the source DLL or `uv` is unavailable,
the game uses original artwork. Failed generation is retried on the next launch.
Explicit config
values take precedence, including an empty `hd_asset_dir` to disable packs.
Setting `REB_HD_ASSET_DIR` yourself overrides this launcher default; an empty
environment value disables generation and discovery without affecting an explicit config value.

Relative paths resolve from the configuration file's directory; absolute paths also
work. `REB_HD_TRACE=1` logs successful runtime matches. The loader silently falls
back to source pixels when a pack file is missing or fails validation.

The loader matches the full original DIB bytes. A changed bitmap resource stops
matching its companion; unchanged resources remain valid even if another part of
the source DLL changes. The loader accepts 2× or 4× companions, while the current
builder emits only 4× packs. Payloads are capped at 64 MiB each. They preserve logical
bounds, source palette, and keyed-cell masks; they do not reconstruct vector art,
change animation frames, or improve bitmap-font glyphs. Smooth scaling can soften
small icon detail, while Scale2x can exaggerate dither and one-pixel texture. Review
the comparison image for every selected candidate before treating it as accepted.

Run the offline builder regressions with:

```sh
uv run --with pillow --with pefile python -m unittest discover -s tests/tools -p 'test_build_hd_pack.py'
```

## Encyclopedia artwork as gameplay icons

Build the full icon pack in one offline step:

```sh
uv run --locked --python 3.12 tools/build_encyclopedia_icons.py \
  --install-dir 'orig_game_files/Star Wars - Rebellion' --no-overrides
```

On a new machine, install `uv`, clone the repository, install the game, and run the
command with that installation's path. No extracted images or mask files need to
be copied from another machine. `uv` provisions Python and the script dependencies
from `tools/build_encyclopedia_icons.py.lock`; the first run downloads the 170 MiB
rembg `isnet-general-use` background-removal model and verifies its SHA-256. Internet access is needed for
these initial downloads. Subsequent runs reuse cached masks unless their source,
model, or dependency fingerprint changes, or `--force` is specified. Nothing is sent
to an image service. The model provides alpha only; RGB pixels come from the game.
The script merges into `work/hd/gameplay-pack`, preserving existing cockpit and
console assets. `launch_game.sh` runs this step itself after building the cockpit/console pack: an interactive
launch asks before downloading the model, and a "no" is remembered in `work/hd/.icons-declined`. Set
`REB_HD_ICONS=yes` to build without asking (or `no` to skip without recording a decline). Non-interactive
launches never download and print a one-line notice. The step is skipped once the pack manifest contains
`encyclopedia_extraction`.

The audited English installation contains 187 encyclopedia pictures, producing 439
resource variants: ships, fighters, troops, facilities, portraits, mission icons,
and all 26 sector planets. Shared pictures are processed once and reused for their
resource aliases. Jedi Luke (`EDATA.192`) has no encyclopedia string alias, so it
explicitly maps to GOKRES `0x0850` and `0x4850`, selected by Luke's post-Dagobah
record bits and the object-picture path. The two fleet encyclopedia pictures
remain extracted for review without mapped resources.
This reuses the source's available detail (usually 400×200 or 180×90); a 4× output
does not imply newly invented detail at every output pixel.

This includes the facilities window's separate STRATEGY artwork: 24 building-row
variants (eight buildings with completed and two faction construction
states) and 15 category-tab variants. These use explicit mappings (`FACILITY_ROWS`: bases `0x2329`, `0x232e`,
`0x2332`, `0x2336`, `0x233a`, `0x233e`, `0x2342`, `0x2346`, each with three states;
`FACILITY_TABS`: `0x2850`–`0x285e`) audited against `SystemObjectPicture` and
`SystemWindow::OnCreate1`, rather than encyclopedia resource-number arithmetic;
unrelated STRATEGY bitmap IDs are not encyclopedia string IDs. Rows fit the cutout inside a 63×31 logical-pixel area;
construction grids are reconstructed. Tabs replace only the picture inset,
preserving the original frame pixels and using a dim grayscale subject for empty
categories. Unworked mineral deposits (`0x232d`) remain unchanged.

Set `REB_FACILITY_REVIEW_DIR` during the native UI smoke to capture the actual five
facility pages. For every row and tab state, filter the pack's `targets.tsv` to
these 39 STRATEGY resources and use `REB_HD_REVIEW_DIR` for the native asset gallery.

Planetary defense and fleet tabs also use explicit STRATEGY mappings in
`DEFENSE_TABS` and `FLEET_TABS`: 24 defense variants and 22 fleet variants cover
both factions and their active, selected and dimmed states. Personnel uses Page
(`EDATA.094`) for the Alliance and Needa (`EDATA.116`) for the Empire. Troop tabs
use Alliance Fleet Regiment and Stormtrooper Regiment artwork; ships and fighters
use the corresponding faction's encyclopedia cutouts. Defense weapon/shield tabs
use LNR Series I and GenCore Level I. Picture insets are cleared before composition,
including the two extra rows in selected defense tabs. Fleet faction outlines are
retained and closed where the old ship artwork overlapped them. Unknown-faction
empty tabs (`0x2942`, `0x2949`, `0x2950`) remain original. Set
`REB_FORCES_TAB_REVIEW_DIR` during native UI smoke to capture the actual windows
for both factions; `REB_HD_REVIEW_DIR` can review every generated resource state.

The same rebuild command also generates two character transit backdrops from SVG:
STRATEGY `0x2ced` (61×25 logical pixels) and `0x2d00` (122×50). The drawing preserves
the original silver bulkhead/window arrangement with smooth bevels and blue
hyperspace trails. Both use the existing 4× indexed companion format, source
palette, and transparent cells; portraits are still composited separately by
`CreateObjectPicture`. Character-specific transit illustrations remain unchanged.

Three more SVG companions provide blue-white star trails for facility transit:
STRATEGY `0x2cf1` (61×25), `0x2cf2` (122×50), and `0x2cf5` (70×40). Defense-window
pictures already compose these behind their foreground. The facilities window
now composes the completed building over `0x2cf5` when both HD companions exist,
falling back to its original baked transit image otherwise. The rebuild removes
the eight obsolete baked transit companions. New streak silhouettes come from
SVG alpha, while palette indices and source logical resources remain intact.
Set `REB_FACILITY_TRANSIT_REVIEW_DIR` during native UI smoke for facility, defense,
and shield comparisons and opaque-foreground preservation checks.

`tools/build_transit_backdrops.py` runs in its own locked uv environment with the
portable `resvg-py` renderer; no system SVG converter or fonts are needed. To rebuild
only these backdrops:

```sh
uv run --locked tools/build_transit_backdrops.py \
  --install-dir "orig_game_files/Star Wars - Rebellion"
```

Editable `transit.svg`, original/indexed previews, and source/SVG hashes are written
alongside the local pack. The procedural drawing code and lockfile are committed;
generated artwork remains local. Set `REB_CHARACTER_TRANSIT_REVIEW_DIR` during the
native UI smoke for character-row captures and character composite checks.

The Alliance headquarters is reconstructed separately by
`tools/build_headquarters_icons.py`, also invoked by the main rebuild. Its larger
GOKRES `0x0340` (122×50) provides the same gold saucer-and-mast reference as the
compact `0x4340` (61×25) and the separate STRATEGY `0x0388` (37×37) sector
overlay; there is no matching encyclopedia image. All three receive 4×
SVG companions with restrained metal shading, preserving their framing, palette,
and original logical data. This is reconstructed detail, not recovered source
resolution. Run the helper with `--install-dir` and optional `--out` to regenerate
only these icons; local `headquarters.svg` files and provenance accompany the pack.

Cutouts are fitted inside the original occupied icon bounds and quantized to the
original palette with the transparent index excluded from opaque colors. `REBHD02`
allows their finer silhouettes while keeping logical data and hit tests intact.
Original `REBHD01` packs continue using their logical pixel masks.

Round planet masks receive an additional matte cleanup: fit an ellipse, require
at least 94% intersection-over-union with the mask and a minor/major axis ratio
of at least 0.85, then inset by 7.5% of the smaller radius with a one-pixel coverage
edge. This removes the dark background rim without thresholding away the shaded
hemisphere. Ringed and irregular bodies fail the ellipse checks and retain their
original masks. RGB artwork remains unchanged.

Facilities receive a conditional 10% brightness lift before resizing and palette
quantization. The cutoff is mean visible-pixel luma below 80 on a 0–255 scale,
measured after any crop with alpha weighting and transparent padding excluded.
The luma weights are 0.2126 R + 0.7152 G + 0.0722 B on the encoded RGB values.
All icon sizes and window variants of a facility share the same decision. Eight of the fourteen audited
facilities qualify, including both GenCore levels; the brighter six retain their
source RGB. Alpha is unchanged. Each rebuild starts from original source RGB,
and the inventory/manifest records the measured mean, cutoff, and applied gain.

Portable corrections live in `tools/encyclopedia_icon_recipes.json`: source-sized
polygon operations (`add`, `subtract`, `intersect`, `replace`) applied after the
automatic mask cleanup. These repair ship silhouettes, fighter panels, walker gaps,
facility outlines, and portrait clothing or attached faction-emblem fragments.
An optional source-pixel `crop` rectangle overrides automatic portrait framing;
Vader uses this to retain both flared helmet edges. Local crop overrides take
precedence over recipe crops. Recipes contain editing instructions and source
hashes, not the original artwork or raster masks. Each recipe checks the source
SHA-256 and dimensions before applying; a different game edition may require
reviewing and updating its recipe or supplying a local mask override. Editing a
recipe invalidates its processed mask while reusing the cached model output.

Recipes currently cover the portraits listed in that file (restored clothing or
shoulders, removed emblem fragments, Vader's and Jedi Luke's crops) and a set of
ships, fighters, walkers and facilities. Victory Destroyer (`EDATA.061`) and
Victory II (`EDATA.067`) restore the forward hull, which automatic segmentation
can drop, leaving only the bridge: check the whole silhouette against the source,
not just that the mask is nonempty. Bulk Cruiser and Assault Frigate use polygons
that reproduce cutouts approved from an external editor, intentionally keeping the
star-speckled underside and streaks; the exact approved masks stay local under
`work/hd/encyclopedia/approved-ships/` and are used only without `--no-overrides`.
A few imperfect cutouts are follow-up work, not a reason to hold back the pack.

Review `work/hd/encyclopedia/contact-*.png` and `icons/contact-*.png`. The manifest
records each source, recipe, mask hash, resource, key, model checksum, dependency
versions, lockfile hash, and processing revision. The CPU model and dependencies
are pinned, but bit-for-bit inference equality across CPU architectures is not
guaranteed. Validate the contact sheets on a new platform.

For local experiments, omit `--no-overrides`. Local
`work/hd/encyclopedia/overrides.json` is automatically applied when present;
`--overrides` selects a different file. For example:

```json
{"EDATA.062": {"mask": "overrides/star-destroyer-mask.png"}}
```

Masks are grayscale PNGs at original encyclopedia dimensions; paths are relative
to the JSON. Optional `crop: [left, top, right, bottom]` adjusts framing. `skip: true`
removes an earlier encyclopedia companion for that source, restoring the original
icon. A local mask takes precedence over the repository recipe for that source.
Once accepted, translate corrections into repository recipe operations so other
machines can reproduce them. Thin edges or minor scenery remnants can be revisited
per image without rerunning background removal for the whole library.

To verify regeneration without existing extracted assets, use new directories:

```sh
uv run --locked --python 3.12 tools/build_encyclopedia_icons.py \
  --install-dir '/path/to/Star Wars - Rebellion' --no-overrides \
  --cache-dir work/hd/clean-icons/cache --out work/hd/clean-icons/pack
```

This builds the encyclopedia icon pack. The separate cockpit/console recipe above
can be built into the same output directory; neither builder requires existing art
from the other pack.

Run offline regressions with:

```sh
uv run --with pillow --with pefile --with numpy --with opencv-python-headless \
  python -m unittest discover -s tests/tools -p 'test_build_*.py'
```

For native review, copy the pack's `targets.tsv` into a review directory and run
the normal `tools/modern_smoke.py` command with `REB_UI_SMOKE=1`,
`REB_HD_REVIEW_DIR=/absolute/review/directory`, and
`REB_HD_ASSET_DIR=/absolute/path/to/work/hd/gameplay-pack`. This asserts that each
4× companion loads and logical pixels remain unchanged, and writes category-based
Original/HD comparison pages from the native software/GDI drawing path. The output
BMPs use actual 4× device pixels, independent of the desktop window size.

## Animated gameplay droids

The gameplay droids use a runtime 4×, palette-aware bilinear filter in `sharp` mode
above 1×. It runs after decoding a complete pose, excludes transparent neighbors,
and retains the original logical transparency mask. A cached palette lookup keeps
the cost small; unchanged frames reuse their detail. No additional pack is needed.
This is deterministic smoothing of the original art, not reconstructed new detail.

`tools/export_droid_sequences.py` decodes whole delta sequences for offline review
and reproduces the runtime filter. For example:

```sh
uv run tools/export_droid_sequences.py \
  --module 'orig_game_files/Star Wars - Rebellion/ALSPRITE.DLL' \
  --base-id 2001 --out work/hd/droid-sequences
```

The reviewed sequences are ALSPRITE 2001/3331 and EMSPRITE 2001/3001: 71 decoded
frames in total. Output includes original and filtered indexed PNGs, animated PNG
previews, comparison sheets, source hashes, and palette/transparency checks.
Sequence files remain under ignored `work/hd/`. Other droid poses use the same
runtime treatment; these four sequences are the representative visual audit.
