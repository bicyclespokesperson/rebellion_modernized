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

- `mission/mission_timers_decl.h` and `mission_encounter_decl.h`: stand-ins `TimerBase`, `NotifEventBase`,
  `MajorCharacterIterator` duplicate events' real `TimerEvent`, `SourceEvent`, `CharacterRangeIterator` (different
  member names); mission code calls `Command::Read/Write/Checksum` (events: `ReadCommand`...). They link through
  `symbols/aliases.csv` — port the mission code to the real classes, then delete those alias rows.

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

## Strategic UI (src/game/strategy_ui)
- The multiplayer vote window (window type 6, 0x44f670-0x451110 except the GID menu 0x451130-0x4511e0) is not
  reconstructed; `GalaxyWindow::CreateModalWindow` calls its constructor (`FUN_0044f670`) at the original address.
  It is not reachable from any scenario.
- Compiler-generated helpers are not hooked: the `SystemSlot` array element constructor/destructor (0x453200,
  0x453260), `MinimizedWindowSlot::MinimizedWindowSlot` (0x42dbe0) and the shared empty `ShowObjectPage` slot (0x45d3f0).
- `GalaxyWindow::WindowProc` WM_PAINT cannot be compared by difftest (uninitialised PAINTSTRUCT in the emulator); the
  hybrid scenario screenshots are the check.
- `SpriteAnimator` is still the stand-in `ui/sprite_animator_decl.h` (its constructor 0x4727e0 and the completion
  hook setter 0x472b20 are called as `FUN_` functions from `GalaxyWindow::CreateDroidAnimations`).
- `MenuWindowBase::AddTextEntry` (0x4ab1f0) and `SetEntryColors` (0x4ab1b0) are called as `FUN_` functions from
  `GidMenuWindow`; the owner of the menu base classes should name them.

## AI foundation (src/game/ai, part 1)
- Part 2a (postures 0x4ad770-0x4bb9f0, agents 0x479a40-0x47aa90, mission planners 0x47aae0-0x484180 and 0x4bce50-0x4be580) is
  reconstructed except for five planner classes that are still created through `FUN_` constructors declared in
  `ai_planner_decl.h`: Rescue (0x61, ctor 0x47e130), Diplomacy (0x51, 0x481460), the 0x54 variant for state 5 (0x47f550), the
  0x52 variant for state 6 (0x480d10) and the free 0x62 planner (0x483140). Part 2b (the 14 task types, evaluators) still
  calls its constructors as `FUN_` functions from `AiTaskManager::MakeTask`; `AiNotice` in `ai_task_base.h` is a stand-in.
- Original quirks in the planners: `AiSystemRecord::AddPlanner/RemovePlanner` write +0x70/+0x74 even for the 0x70-byte free
  planners (heap overwrite masked by allocator slack); the Step slots 16/17 of the system planners leave EAX undefined, so the
  reconstructions are `PickLeaders`/`PickMembers` (void) behind the virtual overrides; `AiDsSabotagePlanner::ChooseTarget`
  resets the target before testing it, so its keep-target branch is dead.
- Planner functions that no scenario reached (read only): sabotage 0x69, DS sabotage 0x6a, recruit 0x55, incite 0x56,
  research/subdue 0x53/0x57 steps, `AiAbductionPlanner::Read/Write`.
- Classes that sit outside the module's ranges because the module needs them: `AiTaskNode` and `AiTaskObject`
  (0x4ec920-0x4ecbc0, generic id nodes next to `ObjectKeyNode`), `AiMissionHistoryEntry` (0x4c8000, declared only; its
  constructor, Read, Write and Fill are called at the original addresses) and `AiTaskManager::Save` (0x49de40).
  `AiSideController::GetFleetRecord` (0x476110) lies between the task and system records.
- Original quirks that are reproduced and marked `BUG(original)`: `AiNoticeQueue::Load` never adds the groups it reads (a loaded
  queue is empty and leaks); `AiSystemRecord::Step` falls through from state 6 into the restart case;
  `AiSystemRecord::CheckShortfall` drops the order `ReleaseSurplus` builds; `AiTaskRecord::Step` and `AiSystemRecord::Step`
  reuse the incoming `done` slot as a flag that starts non-zero.
- `PlayerController` virtual slots 59-61, 74-79 and 92 got their real signatures in `app/player_controller_decl.h` (the AI's
  overrides need them). `AiPlayer::Virtual25/83` still pass pointers through `int` parameters, and `Virtual87` destroys the
  by-value `String` argument through a pointer to its first word; both rely on 32-bit stack layout.
- Not reached by any scenario (verified by reading only): `AiTaskRecord::Release/ReleaseShips/IsFinished/Write/
  ReinforceBySize`, `AiSystemRecord::Reset/RemovePlanner/Write/PlannerClassOf/AgentClassOf`, `AiTaskEvaluator::ChooseSector/
  ClearRecordFlags/Write`, `AiMissionHistory::*`, `AiNoticeQueue::Save`, `AiAgentBase::Write`. Every `Read` function is a
  difftest SKIP (the emulated heap is exhausted by the stream reads); `AiSideController::Load` was compared through the
  loaded-game scenarios instead.
