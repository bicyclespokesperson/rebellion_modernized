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

- `character/role.h`: Role slots 131–134 probably take `ObjectKey*`/`NotifyContext*`, not `int` (Character code
  uses reinterpret_cast as a workaround) — verify from the callee `ret n` and fix.
- `Character::SetRecord` (0x50e1c0) returns int in the original but the base declares `void`.
- `CharacterData`/`RoleData` (character/character_data.h) are reconstructed; `RoleRecord` (role.h) is still provisional.

## Unassigned functions (code map gaps / mis-assignments)
- 0x58adb0–0x58b2e0: object-name node (0x58adb0), system-distance tables and neighbour-list builder
  (0x58af30–0x58b060), serialization wrappers (0x58b1a0, 0x58b2c0) → galaxy / gameobj.
- 0x5696b0–0x5698b0: GameObj subclass of type 0xf2 → gameobj (needs the GameObj header).

## Orders and movement (src/game/orders)
- `orders/command_decl.h`, `move_orders_decl.h`, `order_route_decl.h` are stand-ins: the command classes (base
  0x54eee0/0x54f2c0, concrete commands 0x578ab0-0x57f230, event queue 0x54ea70-0x54ede0) are not reconstructed; the
  orders reach `RenameCommand` and the manufacture/destination command payload at +0x50 through casts and `fieldAt`.
  Also stand-ins there: `MissionOrder` (the 0x241 order, mission module), the manufacturing-manager helpers 0x528040,
  0x528820, 0x52e580, 0x52e970, 0x555f00, 0x555f90, 0x555d30.
- 0x54f5b0 (`GetCapacityBucketRange`, nine family ranges, re-initialised on every call) sits between the event module and
  the route module and is only declared; reconstruct it with whichever owns 0x54f2c0-0x54f830.
- 0x4f5d10-0x4f5de0 (class VT 0x65d234 after the 0x4f5c70 message class) is unassigned.
- `OrderBase::Read/Write` and `TextMessage::Read/Write` are declared pure but have bodies, so every concrete order
  writes a forwarder (the originals do too: the vtable slots are `_purecall`).
- 0x553c80 (`AppendObjectDescription`), 0x520120/0x520170 and 0x5204f0/0x520520 (order/text Read/Write) are not reached
  by any scenario.
