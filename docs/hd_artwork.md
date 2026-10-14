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
