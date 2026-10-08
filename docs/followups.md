# Follow-ups

Cleanup and loose ends reported by module agents. Remove an item when it is done.

## Code cleanup
- `gameobj/factory_registries.cpp` registers its 148 factories through `FUN_` declarations; 145 of the addresses have real names in
  `symbols.csv` (`CreateFleet`, `Sector::Create`, ...): declare those and take their addresses.

- `ObjectKeyNode`/`ObjectKeyValueNode`/`IntPairNode` Read/Write are declared `void`; the originals return their
  last callee's EAX (stream helper result). Check whether any caller uses the value; if so make them `int`.

- `CharacterData`/`RoleData` (character/character_data.h) are reconstructed; `RoleRecord` (role.h) is still provisional.

- `mission/mission_mgr_decl.h` declares the mission manager's functions (family 0xf8, 0x541fc0-0x54cd80) by address; the events
  subsystem calls the same manager through `events/source_events_decl.h` (`FUN_00546e30`). Replace both when the manager is
  reconstructed. The mission timers, events and iterators use the real event classes now.


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

## Events part 3 (UI notification handlers, player notices)
- The scalar deleting destructors of the 95 `UiNotif*`, 6 `NetMsg*` and 44 `Notice*` classes are compiler-generated and not
  hooked; `UiTextNotice::Virtual16` (0x4974f0) hosts the address that `UiKeyNotice::Virtual14` shares (ICF).
- `AdvisorRecordTypeResource` (0x4c5000) is `__stdcall` (it ends in `ret 4`); declared cdecl in the first version, which broke
  every original caller (found by `difftest --integration` on `Notice22`).
- `PlayerDispatch` is created in place by `HumanPlayer` (storage at +0x6c). `GameSession`/`HumanPlayer` still pass `PlayerMessage*` where the real parameter is a
  `MessageBase*` or `UiNotice*` (the stand-in `PlayerMessage` should be dropped).
- Mission families 0x56, 0x57, 0x58, 0x63 and 0x6a of `Notice22::Virtual05` (event 0x1f0) were reconstructed by reading only:
  no mission of those families exists in any scenario's game state, so no synthetic payload reached them.
- `Notice8` (the order confirmation dialog), `Notice10` (chat) and `Notice42` (character notice) are created only by the
  human player's UI paths; they were checked with synthetic calls (`--entry`), not by a scenario capture.
- Verification harness idea that worked well and could become a tool: `difftest --entry <fn> --poke ...` on a capture taken in
  a loaded game, with hand-built payload lists (vtable of the `UiNotif<N>` class, key, two ints) in the capture's stack page, and
  keys of real objects found by scanning the capture's memory for object vtables.

## Strategic UI (src/game/strategy_ui)
- The multiplayer vote window (window type 6, `VoteWindow`, 0x44f670-0x451050 plus `VoteFamilyRange` 0x4510f0) is
  reconstructed but read-only verified: no scenario reaches it (it needs a multiplayer game with an arriving fleet). The
  shared `ObjectKey` copy helper 0x451110 is not hooked.
- Compiler-generated helpers are not hooked: the `SystemSlot` array element constructor/destructor (0x453200,
  0x453260), `MinimizedWindowSlot::MinimizedWindowSlot` (0x42dbe0) and the shared empty `ShowObjectPage` slot (0x45d3f0).

## AI foundation (src/game/ai, part 1)
- Part 2a (postures 0x4ad770-0x4bb9f0, agents 0x479a40-0x47aa90, mission planners 0x47aae0-0x484180 and 0x4bce50-0x4be580) is
  reconstructed, including the last five planner classes (Rescue 0x61, Diplomacy 0x51, the 0x54 and 0x52 variants for states 5 and
  6, the free 0x62 planner). `AiNotice` in `ai_task_base.h` is a stand-in.
- Original quirks in the planners: `AiSystemRecord::AddPlanner/RemovePlanner` write +0x70/+0x74 even for the 0x70-byte free
  planners (heap overwrite masked by allocator slack); the Step slots 16/17 of the system planners leave EAX undefined, so the
  reconstructions are `PickLeaders`/`PickMembers` (void) behind the virtual overrides; `AiDsSabotagePlanner::ChooseTarget`
  resets the target before testing it, so its keep-target branch is dead; `AiRescuePlanner::ChooseTarget` tests `(flags | 0x20) == 0`
  (never true) where the free 0x62 planner tests `(flags & 0x60) == 0`, so a rescue target is never kept; `ObjectRefNode`'s copy
  constructor leaves `m_flag` uninitialised.
- Planner functions that no scenario reached (read only): sabotage 0x69, DS sabotage 0x6a, recruit 0x55, incite 0x56,
  research/subdue 0x53/0x57 steps, `AiAbductionPlanner::Read/Write`.
- Classes that sit outside the module's ranges because the module needs them: `AiTaskNode` and `AiTaskObject`
  (0x4ec920-0x4ecbc0, generic id nodes next to `ObjectKeyNode`), `AiMissionHistoryEntry` (0x4c8000) and `AiTaskManager::Save`
  (0x49de40, also the advisor task manager's Save: `AdvisorTaskManager::Save` is an unhooked copy of the folded code).
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

## AI part 2b (task types, evaluators, advisor analysis)
- The 14 task types, the seven evaluators (`ai_evaluators.cpp`), the galaxy analysis (`ai_galaxy_scan.cpp`,
  `ai_galaxy_analysis.cpp`) and the advisor tasks (`ai_advisor_tasks.cpp`) are reconstructed. The eight facility-building task
  types share one library (`ai_task_family.cpp`, parameter tables per type); the evaluators share `SectorPass` (Prepare),
  `ScoreVoteOf` and the planner/agent request functions of `AiPlanEvaluator`.
- Shared one-address functions that cannot get a second symbol row: `AdvisorTask21::Refused` is `AdvisorTask20::Refused`
  (0x4c7550), the evaluators' kind getters of plans 1, 3, 4, 5 and 6 are the shared constants 0x5a66f0, 0x5a6730, 0x5a6700,
  0x499cd0 and 0x48d640 (defined inline, not hooked), `AdvisorTaskBase::Evaluate` is `AiStagedTask::Evaluate` (0x4cc990).
- Original quirks: `AiSectorScan::Save` writes its three halfwords in a different order than `ReadRecord` reads them;
  `AiSectorScan::SetSystem`/`AiSystemScanSet` dereference a missing system record.
- Not verified by difftest: every `Read`/`Load` that streams from a save (the emulated heap or file state cannot reproduce
  it: `AiGalaxyAnalysis::Load`, `AiSectorScanSet::LoadAll` SKIP), the plan 1-5, 7 `Prepare` functions run only through
  `--entry` on plan 6's capture, `AdvisorTask23` phases 2-3 (they need the advisor droid), and the facility task types'
  `Write` functions.
