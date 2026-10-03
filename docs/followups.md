# Follow-ups

Cleanup and loose ends reported by module agents. Remove an item when it is done.

## Code cleanup
- `core/string_map.cpp`, `core/stream_io.cpp`: call String/StringCursor methods by `FUN_` names (written before the
  String module merged) — switch to the real methods.
- `data/`: factories and the "Table" class are registered through `FUN_` original addresses (e.g. `FUN_005950e0`) to
  keep pointer identity; since patch_vtables now rewrites taken function addresses, use `&Function` directly.
- `data/probability_table.h`: `ParamTableRecord` is a stand-in struct; replace with the real record class.
- `data/`: `SideParamTableEntry::ParseText`/`Checksum` duplicate their 1-D helpers.

- `ObjectKeyNode`/`ObjectKeyValueNode`/`IntPairNode` Read/Write are declared `void`; the originals return their
  last callee's EAX (stream helper result). Check whether any caller uses the value; if so make them `int`.

## Unassigned functions (code map gaps / mis-assignments)
- 0x58adb0–0x58b2e0: object-name node (0x58adb0), system-distance tables and neighbour-list builder
  (0x58af30–0x58b060), serialization wrappers (0x58b1a0, 0x58b2c0) → galaxy / gameobj.
- 0x5696b0–0x5698b0: GameObj subclass of type 0xf2 → gameobj (needs the GameObj header).
