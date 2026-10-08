# REBEXE.EXE code map

This map splits the game code at 0x401000–0x618a70 into 289 contiguous modules across 26 subsystems. It is meant for planning module-by-module decompilation.

The data is in `symbols/modules.csv` (`start,end,module,subsystem,confidence,evidence`; end is exclusive, and the ranges cover the whole span with no gaps). CRT/LIBCMT from 0x618a70 onward is described in `docs/library_code.md`.

Every row has a confidence value:
- **high:** a source-file string, a class-name string, or an import pins the module.
- **medium:** a vtable/constructor cluster, static-init markers, or call locality supports it.
- **low:** the label is a best guess.

The `evidence` column cites the strings, vtables (VT) and constructors behind each row. `notes:<x>` refers to `refs/open-rebellion/ghidra/notes/<x>.md`.

## Read this first: Ghidra is missing about 2,000 real functions

Ghidra finds 10,735 functions in 0x401000–0x618a70. That count leaves out code that is reached only through a pointer in a table: vtable slots, the CRT static-initializer table (`.CRT$XCU` at 0x6a6004, 1,781 entries) and EH tables. This binary has no RTTI, so nothing led Ghidra to that code. About 385 KB of `.text` is therefore undisassembled.

`symbols/missed_functions.csv` lists 3,784 such entry points:
- **1,781 static initializers.** Most are 38-byte stubs. 1,667 of them are in the tactical module, where a header defining about 131 global IDs is included by 12 .cpp files.
- **1,793 targets of data pointers.** These are mostly virtual methods, and they include real logic (e.g. `0x40dfd0`, 3.2 KB, a MultiPlayerWindow method).
- **210 heuristic starts.** These follow padding in the gaps and are lower confidence.

None of these has a `work/decomp/*.c` file.

**Recommendation:** have Ghidra create functions at the `data_ref` and `static_init` addresses before decompiling a module. This was not done here, because the Ghidra project was off-limits for writes. In the counts below, "missed" means these functions.

Two more cautions for reading call graphs:
- **Identical COMDAT folding (ICF) is on.** Trivial bodies such as `ret`, `return this->f10` and `return 1` are shared across unrelated classes and can sit in any module. For example, the empty function `0x5ae1e0` lives in tactical code but appears in about 200 strategic vtable slots. A call into a 3–16-byte function in another subsystem is therefore not a real dependency.
- **The vtable-run detector merges adjacent vtables.** A class is identified most reliably by which constructor writes which vtable.

## Overview

Functions are counted as Ghidra's functions plus the missed ones, excluding the static-init stubs. Code bytes cover both.

| Subsystem | Modules | Functions (Ghidra + missed) | Static-init stubs | Code bytes | Main ranges |
|---|---:|---:|---:|---:|---|
| ai | 35 | 1175 + 150 = 1325 | 0 | 375,616 | 0x401c70–0x403dd0, 0x4178f0–0x41c590, 0x42dc10–0x4359f0, 0x472bc0–0x484df0, 0x4856f0–0x486120, 0x49c6a0–0x49ecb0, 0x4ad770–0x4ecc00 (minus the advisor/UI islands) |
| tactical (out of scope) | 20 | 1300 + 343 = 1643 | 1667 | 350,070 | 0x595b10–0x596fa0, 0x597530–0x5f2f00 |
| ui_strategic | 36 | 732 + 152 = 884 | 5 | 348,158 | 0x421c70–0x42dc10, 0x4422a0–0x45d400, 0x460090–0x472bc0, 0x486120–0x488460, 0x49ecb0–0x4ac120, 0x4c06f0–0x4c5450 |
| events (notifications, commands, timers, player messages) | 32 | 1768 + 411 = 2179 | 1 | 190,177 | 0x41dce0–0x41ecf0, 0x43a2d0–0x440ea0, 0x488460–0x49c6a0, 0x51cd00–0x520570, 0x53f950–0x541fc0, 0x54ea70–0x54f830, 0x562a50–0x566b90, 0x5777c0–0x582a00, 0x585b20–0x5866c0 |
| mission | 26 | 792 + 176 = 968 | 23 | 112,190 | 0x5208b0–0x5264b0, 0x545180–0x549af0, 0x55bfd0–0x55cb60, 0x5698c0–0x576000 (per-mission files), 0x5866c0–0x58adb0, 0x5923a0–0x5947a0 |
| ui_framework | 15 | 479 + 76 = 555 | 4 | 85,183 | 0x5fef50–0x610b20 ("Cool" toolkit), 0x41ecf0–0x420bd0, 0x4ac120–0x4ad770 |
| galaxy (systems, sectors) | 11 | 402 + 49 = 451 | 6 | 70,910 | 0x507130–0x513040, 0x518b20–0x51c040, 0x558bb0–0x55a100 |
| gameobj (object model) | 23 | 610 + 39 = 649 | 0 | 67,266 | 0x4ecc00–0x4ecea0, 0x4f26d0–0x4fcd90, 0x504d50–0x507130, 0x513040–0x513f70, 0x539e80–0x53a670, 0x53ed20–0x53f950, 0x540360–0x540750 |
| fleet (fleets, ships, fighters, troops, orders, routes) | 11 | 500 + 122 = 622 | 6 | 65,088 | 0x4fd650–0x503ae0, 0x504150–0x504d50, 0x536eb0–0x539290, 0x53c040–0x53e0a0, 0x54f830–0x554020, 0x557410–0x558460 |
| economy (facilities, ManuMgr, Side) | 10 | 518 + 89 = 607 | 6 | 61,331 | 0x526730–0x5336b0, 0x539980–0x539e80, 0x53a670–0x53b7f0, 0x55a600–0x55bea0 |
| character | 9 | 402 + 108 = 510 | 8 | 53,581 | 0x4eceb0–0x4f26d0, 0x503ae0–0x504150, 0x5336b0–0x536eb0, 0x55ec90–0x562a50 |
| combat_strat (low confidence; see below) | 5 | 213 + 16 = 229 | 1 | 48,334 | 0x513f70–0x518b20, 0x554020–0x557410, 0x55cb60–0x55e290, 0x58b960–0x58e570 |
| story | 10 | 241 + 75 = 316 | 9 | 42,839 | 0x541fc0–0x545180, 0x549af0–0x54cd80, story characters and missions in 0x56d1b0–0x576000 |
| multiplayer | 7 | 217 + 21 = 238 | 5 | 36,042 | 0x40d7a0–0x4119e0, 0x484df0–0x4856f0, 0x596fa0–0x597530, 0x5f79b0–0x5fb850 |
| data (.DAT tables, GNPRTB, SD records) | 11 | 336 + 58 = 394 | 2 | 34,363 | 0x53b7f0–0x53c040, 0x53e0a0–0x53ed20, 0x568fe0–0x5698c0, 0x584750–0x585b20, 0x58adb0–0x58b960, 0x58e570–0x5923a0, 0x593b90–0x595b10 |
| app | 5 | 217 + 17 = 234 | 6 | 25,510 | 0x401000–0x401c70, 0x406600–0x406950, 0x412fc0–0x4173a0, 0x4359f0–0x436a40 |
| gfx | 4 | 121 + 44 = 165 | 15 | 25,367 | 0x5fbd20–0x5fef50, 0x403dd0–0x404c30, 0x4119e0–0x412fc0, 0x614000–0x6153b0 |
| util | 7 | 326 + 8 = 334 | 0 | 19,477 | 0x5f2f00–0x5f6920, 0x5fbb20–0x5fbd20, 0x612bd0–0x614000 |
| battle_flow | 1 | 41 + 1 = 42 | 3 | 13,648 | 0x40a050–0x40d7a0 |
| library (LIBCI iostream) | 1 | 101 + 17 = 118 | 0 | 13,198 | 0x6153b0–0x618a70 |
| encyclopedia | 1 | 15 + 2 = 17 | 0 | 11,113 | 0x45d400–0x460090 |
| sound | 3 | 113 + 14 = 127 | 8 | 8,916 | 0x610b20–0x612bd0, 0x4173a0–0x4178f0 |
| saveload | 2 | 63 + 5 = 68 | 5 | 6,806 | 0x409230–0x40a050, 0x568160–0x568fe0 |
| video | 2 | 37 + 5 = 42 | 0 | 6,243 | 0x421300–0x421c70, 0x5f6920–0x5f79b0 |
| unknown | 1 | 11 + 4 = 15 | 1 | 646 | 0x56ea00–0x56ece0 |
| resource | 1 | 5 + 1 = 6 | 0 | 589 | 0x60ed60–0x60f020 |
| **total** | 289 | 10735 + 2003 = 12738 | 1781 | 2,072,661 | 0x401000–0x618a70 |

The "Main ranges" column is condensed. `symbols/modules.csv` is the authority for exact boundaries.

## Subsystem notes

### Program-level layout

The link order (= address order) is roughly:
1. App shell, game options, battle flow and the multiplayer window (0x401–0x417).
2. The AI side controller and strategic windows (0x417–0x472).
3. AI planners, player controllers and player messages (0x472–0x4a0).
4. Object windows and the AI posture/task/evaluator classes (0x4a0–0x4ec).
5. The core object model and game-object classes in the order charac → GameObj → fleet → capital-ship subclasses → capshp → fightsqd → special forces → TroopReg → System (0x4ec–0x513).
6. Game root, scheduler and factories (0x513–0x520).
7. Mission, sector, facilities, ManuMgr, Side, Role, orders, notifier, ProcFacil, object DB, MissionMgr, route validation, CombatUnit and Detector (0x520–0x558).
8. GNPRTB rule files, CharacterMgr, event/timer classes, and one .cpp per mission type or story character (0x558–0x576).
9. More event/command classes, data tables and mission base classes (0x576–0x595b10).
10. Tactical (0x595b10–0x5f2f00).
11. Libraries (0x5f2f00 onward).

### app, battle_flow, saveload

- **WinMain and the app object:** WinMain is 0x4165b0. `RebellionApplication` is 0x412fc0–0x4173a0: window class `STARWARS_REBELLION`, message loop 0x413560, quit 0x414330, `DoMainChildWindow` 0x414a80.
- **Registry paths:** 0x401440–0x401760.
- **CD and version check:** 0x406600 (`data0.reb`).
- **`GameOptionsWindow`:** 0x406950–0x409230.
- **Game session singleton:** `DAT_006b14e4`, at 0x4359f0, creates the human, AI and remote side controllers (0x4861b0 / 0x4857a0 / 0x484e70).
- **Frame step:** 0x41dff0 (`game_step_driver`).
- **`battle_flow`:** state machine 0x40a050 (state names at 0x40a700) and the strategic↔tactical converters. Details are in the tactical section below.
- **`saveload`:** the session save/load driver (0x409690 save, 0x409700 load, 0x409480 restore) writes through MSVC `ofstream`/`ifstream` (0x5f4a60/0x5f4b10). Serialization itself is spread across all game classes as virtual Read/Write pairs over the stream helpers 0x5f4d90/0x5f4db0. So "save/load" is a cross-cutting interface, not a module.

### gameobj, galaxy, fleet, character, economy

- **File pattern:** each game-object .cpp follows the same shape:
  1. constructor (writes the vtable);
  2. validated setters (`"Invalid X value!"`);
  3. virtuals;
  4. `<Class><Field>Notif` functions;
  5. the INIT block for its static notification table;
  6. 2–3 child-iterator constructors that fix the family range (through the iterator base 0x513050).
- **Family codes (high byte of the object ID):**
  - fleet 0x08–0x0f
  - TroopReg 0x10–0x13
  - capital ships 0x14–0x1b (Death Star 0x18–0x1b, inferred)
  - FightSquad 0x1c–0x1f
  - facilities 0x20–0x2f
  - characters 0x30–0x3b (Leia 0x31, Luke 0x32, Han 0x33, 0x34/0x35 probably Vader/Emperor)
  - special forces 0x3c–0x3f
  - missions 0x40–0x7f
  - sectors 0x80–0x8f
  - systems 0x90–0x97
  - ManuMgr 0xa0–0xaf
  - Side 0xf3
  - MissionMgr 0xf8
  - type-0xf9 manager
  - CharacterMgr 0xfa
- **Object-model hubs:** key/null checks 0x4ece30; key copy 0x4f26d0; key lists 0x4f57b0/0x4f5940; child walks 0x513120/0x5130d0; notifier 0x539fd0/0x53a010; side check 0x5404d0; factories 0x51ebf0 (objects by family), 0x51ef80 (events/timers), 0x51f4b0 (orders).
- **Scheduler and clock:** game manager / day tick at 0x51cd00 (`DAT_006b2fcc`); event queue 0x54ea70; timer base 0x585fe0; event base 0x5863b0 (notes:timer-scheduler).

### mission, story

- **Mission base:** 0x5208b0, with setters, `Mission*Notif` and member iterators (team/decoy/captured).
- **Per-type missions:** one .cpp each, from 0x5698c0 to 0x577050 (Subdue, Sabotage, Rescue, Recruit, Recon, Research, Incite, Espionage, Diplomacy, DS Sabotage, Jedi Training, Autorouting/Assassination/Abduction…).
- **Story characters and missions:** Luke, Leia, Han, Dagobah, Palace, Pickup, Vacation, Bounty.
- **`MissionMgr` (story manager, type 0xf8):** 0x541fc0–0x54cd80 (`src/game/story/`). It owns twelve story flags (word at +0x58: Luke Dagobah/Palace, Han captured at the palace, Leia/Chewbacca Palace, Darth pickup, Darth to Luke/Emperor, final battle ready/battle, bounty hunters) and derives them from where the named characters are: Luke 0x32000242, Leia 0x31000241, Han 0x33000243, Chewbacca 0x38000343 (`Character38`), Darth Vader 0x35000281 (`MajorChar35`) and the Emperor 0x34000280 (`MajorChar34`). Pulsing a flag (set then clear) makes its `OnXChanged` create the story mission (Dagobah 0x71, Palace 0x64, Pickup 0x73). It also holds the mission-type check table (+0x6c/+0x70) and the checks and candidate lists of the mission orders 0x240/0x241.
- **Story missions and characters:** `Luke`/`Leia`/`Han` (0x56f3b0–0x5701d0, 0x572530–0x572c50) are MajorCharacters with a word of story flags at +0xb0. The ranges also hold the family iterators of Research (0x56d1b0) and Incite (0x571c20), which belong to those classes.
- **Resolution phases:** decoy/foil/traitor/force-detect at 0x587100–0x58adb0.
- **Mission formulas:** `*MSTB` at 0x55bfd0.

### ai

The computer player is the second-largest subsystem, and it is scattered.

- **Computer player:** `AiPlayer` (0x4856f0–0x486120, `src/game/ai/ai_player.h`) embeds the AI side controller `AiSideController` (0x4178f0–0x41be00). Its member sets are at 0x42dc10–0x4359f0.
- **Records and selectors:**
  - candidate records 0x401c70 and 0x472bc0–0x476140;
  - per-system record 0x476140;
  - mission-planner base 0x47aae0 / 0x4bce50, with per-mission planners at 0x47bb80–0x484180 (notes:ai-mission-planning);
  - posture base and 6 posture classes 0x4ad770–0x4bb9f0;
  - task manager and factory of 14 task classes 0x4be580–0x4e60c0;
  - 7 scoring evaluators 0x4e60c0–0x4ebae0.
- **No GDI:** the AI code calls no GDI functions.
- **Advisor droid:** two "advisor" objects, one per side (0x4c06f0–0x4c4990), plus `advisor_ai_helpers`, reuse AI machinery for the human side.

### ui_strategic, ui_framework, gfx, sound, video, encyclopedia

- **Galaxy view (main strategy window):** 0x421c70, `WndProc` 0x422ce0, with child windows by type id: sector 0x4591d0, system 0x452fc0, object status 0x442d70, fleet 0x4a2630, the command-center lists, message index 0x466350, mission dialog 0x46a750, object pop-up menu 0x4422a0/0x4aa7f0, and `CustomDialogBox` 0x4ac120.
- **UI notification handlers:** about 610 tiny functions at 0x43a2d0–0x440ea0, registered by 0x41e580.
- **Player-message classes:** about 46 classes at 0x488460–0x49c6a0. Text comes from TEXTSTRA.
- **"Cool" GUI toolkit (`ui_framework`, 0x5fef50–0x610b20):** CoolGameWindow, CoolStrobeButton, CoolDragList, CoolSelectionBox, CoolTabControl, CoolwinStatusBar, slider, CoolStringField. It is shared with Coolhand's *StarTrader* (string `"…A mouse is required to play StarTrader."`).
- **gfx:** DIB/bitmap/palette/blit at 0x5fbd20–0x5fef50.
- **sound:** `SoundManager` (DirectSound) at 0x610b20, with primary-buffer setup at 0x611330; MCI music at 0x611e90.
- **video:** `VideoManager` (Smacker, AVI, MCI) at 0x5f6920; the movie window is 0x421300.
- **encyclopedia:** 0x45d400.

### multiplayer, util, library

- **Multiplayer:**
  - `MultiPlayerWindow` (lobby) 0x40d7a0;
  - remote side controller 0x484df0;
  - `comm2` (`2Comm.cpp`, a thin wrapper) 0x596fa0;
  - `CommMgr` (DirectPlay, the only DPLAYX user) 0x5f7cc0–0x5fa720, plus packet classes.
- **util:** string class 0x5f2f50 / 0x5f2ff0 / 0x5f35b0; registry; containers and list templates 0x5f4ce0–0x5f66c0; perf timer 0x5fbb20.
- **library:** LIBCI iostream from 0x6153b0.
- **`directx_init_errors` (0x614000):** DirectDraw creation and the DirectX error-to-string function 0x614530.

### combat_strat (caution)

The reference notes' "space combat auto-resolve" chain (0x514a60 → 0x5457f0 → 0x549910, the "7-phase pipeline") is **not combat**. Two separate analyses agree on this.

- **0x514a60** handles event 0x31a (an object changing container). It is registered at 0x51ef80 with fire slot 0x57ba50.
- **0x5457f0** is gated by 0x5438a0, which matches six named-character IDs (0x31000241, 0x32000242, 0x33000243, 0x34000280, 0x35000281, 0x38000343).
- **0x549910** sets the 12 `MissionMgr` story bits. Its "phases" match the 12 `MissionMgr*Notif` strings: Luke at Dagobah, Vader pickup, final battle, and so on.

notes:space-combat, notes:COMBAT-SUMMARY and the related parts of notes:annotated-functions should be treated as wrong for these addresses.

The `combat_strat` modules hold the real strategic battle-related rules:
- pending-battle queue 0x516d50 and post-battle work 0x516ef0/0x517270;
- GNPRTB 0x14 combat/transit formulas 0x55cb60;
- ground combat 0x560d50, which sits inside the CharacterMgr file;
- the type-0xf9 manager (bombardment-style 0x555b30/0x556430);
- command 0x224/0x225 resolvers (planetary assault?).

Labels and boundaries in this subsystem are low to medium confidence.

## Tactical combat (out of scope)

### Fence

- **Ranges:** `0x595b10–0x596fa0` (façade) and `0x597530–0x5f2f00` (engine).
- **Gap between them:** `0x596fa0–0x597530` is `comm2`, multiplayer glue that only sits between the two. Keep it.
- **Size:** 20 modules; 1,300 Ghidra functions, 343 missed functions and 1,667 static-init stubs; about 350 KB of code.

### Sub-modules

**Simulation core** (about 207 KB):
- façade 0x595b10
- `TacticalGame` singleton 0x597530 (`DAT_006bc248`, 0x6b8 bytes, constructor 0x597610, per-frame 0x59a530, loads `tactical.dll`/`texttact.dll`/`VoiceFx*.dll` at 0x59a8d0)
- ship groups and orders 0x59cd60
- event dispatcher 0x5a7500 (`SHIP_ADD`…`SYSTEM_ABORT`)
- object manager 0x5a9de0
- capital ship 0x5af4e0
- fighter 0x5b8540
- Death Star 0x5ba3a0
- turn sync 0x5bbf10
- tactical AI and formations 0x5c5b30

**Presentation** (about 143 KB):
- main window 0x59ab40
- 3D view 0x5c14d0
- HUD 0x5c3560
- visual resources 0x5d2360
- D3D device 0x5d6700 (the only `Direct3DRMCreate` call, at 0x5d6930)
- D3DRM scene/camera 0x5d7040
- UI panels 0x5da950–0x5e9b60 and 0x5ef360
- formation visuals 0x5e9b60
- projectiles 0x5ee590

### How the fence was checked

Apart from the entry points below, no strategic code calls into the range. The range itself calls out only to:
- libraries (string/containers, Cool GUI, gfx, `SoundManager`, CommMgr via `comm2`, iostream/CRT);
- ICF stubs of 16 bytes or less.

The tactical singleton is referenced only inside the range.

### Shared stubs to keep or re-provide when deleting the range

These are ICF-folded functions that happen to live inside it:
- `0x5ae1e0` (empty function; about 200 strategic vtable slots)
- `0x5cd6b0`, `0x5a6730`, `0x5a66f0` (`return 1`, the human controller's type slot), `0x5a6700`, `0x5a5b60`, `0x5a5600`/`0x5a56b0`/`0x5a5920`/`0x5a5980`, `0x5c61f0`, `0x5ef070` (vtable slots)
- `0x5c7530` (list-next getter used by the core iterator 0x5130d0)
- `0x5cf920`
- `0x5ee3f0`/`0x5ee410`/`0x5ee440` (list-node helpers called from the container library 0x5f4f10/0x5f4fa0/0x5f6250 and from 0x6123b0)

### Reconstruction status (battle flow, facade, TacticalGame, simulator step)

Code lives in `src/game/app/battle_flow*.cpp|battle_handoff.cpp|battle_results.cpp` and `src/game/tactical/`.

- **Battle flow (0x40a050-0x40d780): done.** `BattleFlow::Step` (the state machine), `BattleSetup` (strategic -> tactical,
  0x40afa0-0x40c5f0) and `BattleResults` (tactical -> strategic, 0x40c730-0x40d510). The 0x409230-0x40a050 part of the same
  object (constructor, save/restore, file names) is not reconstructed. State 7 (3D) writes a pre-battle save and deletes the game
  step driver; state 0x16 re-creates the driver, restores that save, applies the results and deletes the file. The abstract
  path (8 / 0x17) keeps the live strategic game. `QueryBattle` of each side gives the fleet values/counts the engine is set up
  with; floats are scaled by 0.4 in the facade.
- **Facade (0x595b10-0x596f30): done.** The `Register*` functions are one-to-one forwards of the strategic objects' figures
  (42 words for a capital ship); the capital-ship constructor 0x5b05c0 receives all of them except `gridTotal`, plus a constant 100.
  Characters and special forces/troops are small record classes (`tactical_units.h`, 0x596c40 / 0x596df0). Results are
  struct-returning cdecl functions with a hidden out pointer (`TacticalGet*Result`).
- **comm2 (0x596fa0-0x597530): done**, thin null-checked wrappers over the library CommMgr (not exercised by single-player scenarios).
- **TacticalGame (0x597530-0x59a8d0): done** except the tactical window (0x59a8d0, presentation). The constructor builds the
  ship/sprite-state name tables and the type map (an AVL tree, `tactical_game_tables.inc`). The facade's by-value `String` is copied
  *before* the singleton is created: `SetupBattle*` are therefore free thiscall functions, otherwise heap allocation order differs.
- **Headless simulator**: step `0x5ae9c0` (strengths, stalemate after 1200 s, outcome flags) and `0x5aee90` (survivor
  bookkeeping), unit registration 0x5af420-0x5af4b0 are reconstructed and difftested. Not reconstructed: 0x5ae330/0x5ae460 (formation
  groups at battle start), 0x5aefc0 (Death Star attack/trench run), 0x5af7a0.
- **Object manager / event dispatcher (3D only)**: the base class is abstract (VT 0x66c2d0, 40 slots, concrete class in the 3D
  view; never created for auto-resolve). Only the leaf accessors, pause counter and setters are done (`tactical_objmgr.cpp`).
  0x5a7500 is the object manager's *constructor* (128 event-name strings), not a dispatcher; the rest of 0x5a7500-0x5af420 is
  unreconstructed 3D-mode code with no scenario that reaches it.
- **Unit classes (0x5af4e0-0x5bbf10, `src/game/tactical/tactical_ship*.cpp`, `tactical_capital_ship.cpp`, `tactical_fighter.cpp`,
  `tactical_death_star.cpp`, `tactical_simulator_units.cpp`): ~175 functions done.** `TacticalShipBase` (0x418, multiple
  inheritance: `TacticalShipObject` + `TacticalUnit` at +0xd4 + facets; vtable `TacticalShipObject`-primary, 57 slots),
  `TacticalCapitalShip` (0x448, + `TacticalHangar` at +0x418), `TacticalFighter` (0x418), `TacticalDeathStar` (0x7c), the
  headless simulator constructor/destructor (0x5af5f0/0x5afa50) and its unit/skill/registration functions. The registry and
  simulator pass the ship *start* pointer. Hangar-subobject virtuals are free `__thiscall` functions (`this` = hangar).
  `tactical_ship_decl.h`/`tactical_ship_ext.h` hold stand-ins for classes owned by other tactical modules (drop when merged).
  Not reconstructed: compiler thunks and static initializers; sound/presentation (0x5b00a0-0x5b0170, 0x5ba0b0-0x5ba0f0, sound
  manager 0x5ba8b0-0x5bbee0); the 3D movement/steering/hit code (0x5b2f30-0x5b8f70 big functions, incl. the weapon-hit virtuals
  0x5b5100/0x5b5750/0x5b5b60/0x5b5f50, 0x5b97a0, 0x5b9a20); small leftovers 0x5afde0, 0x5afe10, 0x5b1dc0, 0x5b2e60, 0x5ba480.
  Most unit functions are not reached by any scenario; they were verified with `difftest --entry <addr> --poke ...`, using a
  fake object manager poke (`0x6bc4a0=@[0x6bc4a8]-0x924;[0x6bc4a8]-0x924=@esp+0x300;esp+0x328=@0x461730`, stub `ret 0x10`).
  Functions that copy an uninitialised stack result (`BuildResult` 0x5b0280, 0x5b8e00, 0x5ba620, 0x5ba670) cannot match the
  original's stack garbage: XFAIL entries in `tests/difftest_known_failures.txt`.
- Calling-convention traps found: 0x5ae1e0 (shared empty stub) and 0x5bbd90 pop one argument; several stubs the decompiler shows
  as `void f(void)` take arguments. `tools/check_callee_pop.py` catches these.

### Battle lifecycle: strategic → tactical → strategic

Driven by `FUN_0040a050`; the state is at `this+8`.

1. **State 4:** 0x41e230 → 0x516d50 starts the next pending battle. Every battle goes this way.
2. **States 5/6, choosing the type:** 0x41cfc0 → 0x4362b0 polls both side controllers (vtable slot +0x3c). The battle is "abstract" only if **both** sides vote 1; otherwise it is full 3D.
   - Human: the dialog 0x450540 → 0x487830 stores 2 for tactical, 1 for auto-resolve, or 1 with +0x3c = 10000.0f (probably retreat; unverified).
   - Computer (0x485d50): always votes 1.
   - Remote: the vote arrives by network message.
3. **State 7 (3D) / state 8 (abstract): handoff.**
   - 0x40b0e0 calls façade 0x595d60 with the battle setup: sides, system, abstract flag and whether the opponent is the AI. The abstract flag is stored at tactical +0x668. Then:

   | Strategic function | What it hands over | Façade function |
   |---|---|---|
   | 0x40b8a0 | capital ships | 0x595f60, which constructs a capital ship via 0x5b05c0 |
   | 0x40bcd0 | Death Star (inferred) | 0x595ef0 |
   | 0x40be90 | fighter squadrons | 0x5964c0 |
   | 0x40c200 | characters aboard | 0x596340 |
   | 0x40c4b0/0x40c5f0 | special forces / troops aboard | 0x5961e0 |

   - Then 0x596820 creates the engine.
4. **States 0x14 (TACT) / 0x15 (TACT_ABSTRACT):** each frame calls 0x597530 / 0x59a530. With +0x668 = 1, 0x59a530 does no rendering. It steps the headless simulator `DAT_006bc4a8` (0x274 bytes, constructor 0x5af5f0) with 0x5ae9c0(1.0). That simulator runs the same capital-ship and fighter unit objects as 3D mode.
5. **States 0x16/0x17, results:** 0x40c780 applies them, and then 0x5975a0 destroys the engine.
   - winner: 0x5969d0 → system 0x50cfd0 / 0x50ceb0
   - capital ships: 0x40cfc0 ← 0x596870, applied with hull 0x501ae0/0x501b70 and the CapShip setters 0x501510–0x501820
   - Death Star: 0x40d170 ← 0x596980
   - fighter squad sizes: 0x40d260 ← 0x5968e0 (0x503370/0x503400)
   - fighter carrier changes: 0x40d320
   - hangar overflow: 0x40d510
   - characters: 0x596930's result is unused
   - an inconclusive live battle can be re-entered in abstract mode (state 8; inferred).
6. **State 9 (`STRAT_BATTLE_FINISH`):** strategic follow-up through 0x516ef0/0x517270 (bombardment/assault/ground; unverified).

### Auto-resolve is part of the tactical module

The game's only space-battle auto-resolve is `TACT_ABSTRACT`, and it runs inside the tactical engine. There is no separate strategic resolver, because the notes' "7-phase pipeline" is story logic (see `combat_strat` above).

A reimplementation that deletes 0x595b10–0x5f2f00 must therefore do one of two things:
- **(a)** port the headless simulation core (about 207 KB: facade, game, ship groups, events, object manager, capital ship, fighter, Death Star, turn sync, AI/formations), dropping only the presentation modules; or
- **(b)** write a new resolver behind the same interface. It must take the handoff data from step 3 and produce what 0x40c780 consumes: the winner, per-ship hull and the six CapShip allocation values, the Death Star result, squad sizes, and carrier changes.

Either way:
- `battle_flow` (0x40a050–0x40d7a0) is the seam to keep.
- To force auto-resolve, make state 6 always go to 8, or make the human controller vote 1.

Other strategic entry points to stub:
- the save/load branches for tactical state 0x14 (0x409480/0x4095b0 → 0x401290/0x4012b0 → 0x4160a0/0x4160c0 → 0x59cb60/0x59cb80, and 0x59a4f0);
- tactical window and DLL setup 0x415120 → 0x59a8d0, and teardown 0x596a00;
- the options and sound hooks 0x4082b0 / 0x406b80 → 0x596b70 / 0x596b90;
- volume 0x421790 → 0x59b100.

In multiplayer, 0x595d60 takes separate paths for a computer opponent (0x599c90) and a human opponent (0x59a0b0), and tactical turn sync goes through CommMgr. A replacement resolver has to be deterministic on both machines; how the original keeps abstract mode in sync has not been checked.

## Suggested decompilation order

The order runs from leaves to roots: each step depends almost only on steps before it. This follows from the subsystem call matrix (game logic → gameobj/util; UI → ui_framework/gfx/util; nothing calls up into UI except the facade/event layers).

1. **Runtime glue and util:**
   - string class, containers, registry and timer (0x5f2f00–0x5f6920, 0x5fbb20);
   - iostream: keep the original or link a replacement;
   - ICF stubs.

   These are tiny, have very high fan-in, and are needed by everything. The string and list hubs alone have 200+ callers each.
2. **Data:** GNPRTB binders and RNG (0x53e0a0), table registry (0x58adb0), table and SD record classes (0x584750, 0x58e570–0x595b10), and the table-entry registry (0x568fe0). These are pure parsers over GData/*.DAT, easy to test against the files.
3. **Object model (gameobj):** keys, key lists, iterators, notifier, object DB, root, factories, and the event/timer bases and queue (0x4ecc00–0x4fcd90, 0x504d50, 0x513040, 0x539e80, 0x53ed20–0x540750, 0x51cd00–0x520570, 0x54ea70–0x54f830, 0x585b20). Every game class sits on top of these, and they define the setter→notify→event pattern.
4. **Game-object classes, in link order:** Character/Role, GameObj, Fleet, CapShip, FightSquad, TroopReg, System, Sector, Facility/ProcFacil/ManuMgr, Side, CombatUnit/Detector. These are mostly mechanical: setters, notifications and iterators.
5. **Rules and simulation:**
   - GNPRTB rule files (0x558460–0x55ec90)
   - system rules and uprisings
   - orders and route validation
   - command/event/timer classes (0x562a50–0x566b90, 0x5777c0–0x582a00, 0x53f950–0x541fc0)
   - missions and resolution phases
   - MissionMgr / story
   - CharacterMgr
   - the strategic battle queue (0x513f70–0x518b20)

   Do this together with the save/load Read/Write virtuals of each class, so saves can round-trip.
6. **AI:** about 375 KB, the largest single subsystem. It depends only on the layers above plus util, and calls no GDI. It can be done after the rules, and verified by AI-vs-AI soak runs.
7. **Infrastructure for UI:** gfx/DIB/palette, the Cool GUI toolkit, sound, video and resource DLLs. These depend only on Win32 and util, so they could also be done early if the hybrid needs UI work in parallel.
8. **Strategic UI:** galaxy view, the various windows, player messages, UI notification handlers, encyclopedia, game options and the app shell. Most of this code is large window procedures with little game logic, and it is the most work per unit of behaviour.
9. **battle_flow plus the replacement resolver** (see the tactical section). **Multiplayer last.**

The rule behind the ordering: test seams first. Data parsing and the object model can be checked against the original by diffing object state after loading a save, so they give early verification for the riskier layers built on top.
