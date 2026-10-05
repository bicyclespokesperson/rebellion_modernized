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
- `orders/command_decl.h` now only includes events/event_base.h and events/event_queue.h (the command base classes and
  queues are reconstructed). `move_orders_decl.h` and `order_route_decl.h` still declare the concrete commands
  0x578ab0-0x57f230 (and `RenameCommand` and the manufacture/destination payload at +0x50 through casts and `fieldAt`),
  plus stand-ins for `MissionOrder` (the 0x241 order, mission module) and the manufacturing-manager helpers 0x528040,
  0x528820, 0x52e580, 0x52e970, 0x555f00, 0x555f90, 0x555d30.
- `OrderBase::Read/Write` and `TextMessage::Read/Write` are declared pure but have bodies, so every concrete order
  writes a forwarder (the originals do too: the vtable slots are `_purecall`).
- 0x553c80 (`AppendObjectDescription`), 0x520120/0x520170 and 0x5204f0/0x520520 (order/text Read/Write) are not reached
  by any scenario.

## Events (src/game/events)
- `events/events_decl.h` holds stand-ins (`PostedEvent`, `PostedEventQueue`, `TickTable`, `LegacySaveBlock`, by-address
  declarations of save/load helpers at 0x568xxx and battle functions at 0x5167xx-0x518460); drop them as owners land.
- Not reconstructed: the jmp thunks 0x41e420, 0x41e4c0, 0x51dde0, 0x541fa0/0x541fb0, 0x5409c0; the empty destructor 0x541ea0;
  static initializer 0x54f7c0-0x54f7f0 (array of nine `IntPair`s at 0x6bb2f0, rebuilt by `GetCapacityBucketRange` anyway).
- `LoyaltyEvent373`, `SystemCommand224/225`, `SearchSpec` (command_classes.h) are named by kind/shape; rename when the
  owners of their callers (economy 0x55d900 etc., 0x58c1e0/0x58d800 helper classes) know the purpose.
- 0x4f5d10-0x4f5de0 (events/key_property_helpers.h) are callbacks of the table at 0x65d234; its owner is not identified.
- Event vtable slot 10 is `GetPriority` (TypeQueue ordering) on PlainEvent-derived classes; slot 11 of KeyedEvent-family
  vtables is 0 or an unrelated function (0x4f6030) and is not declared.
