# Startup text assets, schema 1

Program state is defined by typed C++ source. `tools/extract_startup_assets.py` extracts startup text from a local
installation into `startup-assets.bin`; the modern compiler and linker do not read the EXE, symbol CSV, object symbols,
or recovered initializer tables. An asset pack is required to run, alongside the game's ordinary resources and tables.
Extracted packs are local build artifacts and must not be committed or published.

The extraction recipe in `tools/startup_assets_v1.json` describes source locations for the supported original executable.
Those locations are extraction details only. The pack and runtime contract use stable text identifiers, with no C++
symbol names, object sizes, pointer widths, or object offsets. A different supported executable can use a different
recipe while producing the same schema. Each text ends at its first null; adjacent executable bytes are excluded.
The vowel-character text preserves its original byte encoding; extraction does not transcode it.

The binary format is little-endian:

| Field | Representation |
| --- | --- |
| Signature/version | Eight bytes: `RBASSET1` |
| Entry count | Unsigned 32-bit integer |
| Each entry's identifier length | Unsigned 16-bit integer |
| Each entry's text length | Unsigned 32-bit integer |
| Identifier | ASCII bytes, no terminator |
| Text | Original encoded bytes, no terminator |

Empty text is valid. Identifiers are unique; unknown entries allow compatible extension. The loader checks lengths,
duplicate identifiers, truncation, trailing bytes, and all required identifiers before publishing pointers. Text
storage belongs to the entry-point asset owner and remains alive through game shutdown. Program constants such as
grid dimensions, geometry, resource type GUIDs, and allocation sizes belong in source rather than the asset pack.

The build scripts extract the pack before configuring CMake. CMake packages an available pack in the app's resource
directory (macOS) or beside the executable (Linux). CMake itself can compile without an installation or pack, allowing
public compile checks. To use a pack stored elsewhere, configure `REB_STARTUP_ASSET_PACK` with its path.
