# AI matches

The native game's headless simulation runner can start a game with two AI controllers or convert a loaded
single-player save. Set `REB_DETERMINISTIC=1` to make a seed reproducible. `ticks` is the maximum number of
additional game ticks; `until:win` stops at the first settled tick with an official victory flag. A tick limit
without victory exits with status 7, while a completed victory exits with status 0. Stalls, invalid references,
load/setup failures, and write failures have distinct nonzero statuses.
The result also reports the in-game `day`; ticks and days are separate counters (the tested default game
advances one day every two ticks).
The wrapper defaults to a 100,000-tick limit and saves a checkpoint every 500 ticks.

```sh
tools/run_ai_match.sh --out work/ai_match/seed42.sav --seed 42 --ticks 10000
```

The wrapper selects the native executable for macOS or Linux and configures the local game assets.
Use `--exe` to select another build, such as a sanitizer build. The underlying `REB_SIM` form is
`new:1,seed:42,difficulty:7,ticks:10000,until:win,ai1:classic,ai2:classic,every:500,save:/tmp/match.sav`.

`new` is galaxy size 1, 2, or 3. `victory:0` (the default) uses standard victory conditions;
`victory:1` uses the shorter headquarters/Coruscant conditions. `--every` saves a checkpoint at that
interval. `--difficulty` selects game rule column 0-7; the default is 7, the original game's
multiplayer rules column. For comparisons, keep galaxy size, difficulty, victory mode and seed
fixed while swapping which AI controls each side.

The terminal save is `match.sav`, the run log is `match.sav.log`, and machine-readable
results are in `match.sav.result.json` (`winner` is 1 for Alliance, 2 for Empire, or 0
when the limit is reached). The log records Alliance, Empire and neutral controlled-system counts
at the start and at each checkpoint, along with Coruscant's controller and whether the Alliance HQ has
been destroyed. The result JSON includes the final counts and objective status. A large lead in
controlled systems does not itself satisfy either victory condition.

A checkpoint can be resumed with `--load /tmp/match.sav.t501` instead of starting a new game.
A loaded human controller is converted
to the requested AI after load. The loaded game's saved random state determines the continuation;
`seed` applies only to a new game.
When resuming, omitted `--ai1` and `--ai2` preserve the saved AI kinds; a loaded human side defaults
to `classic`.
Repeating a run from the same checkpoint is deterministic. Its later save is not currently byte-identical
to an uninterrupted run; the existing human-game save/load path has the same difference.

## Adding another AI

Press **D** in either the shuttle cockpit or the strategic screen to choose **Classic (game
default)**, **Codex Strategist** or **Claude Admiral** independently for Alliance and Empire. Choices persist as
`alliance_ai` / `empire_ai` in the native config. In a running single-player game, choosing a
personality replaces that side's AI immediately while preserving queued orders; the human
side's choice applies when it is next computer-controlled. Loading a save restores its saved
controllers. The strategic menu marks the personality actually running each AI side.

`GameSession` and the simulation runner select controllers by persisted controller kind. The
AI name-to-kind lookup and controller construction live in `src/game/ai/ai_controller_factory.cpp`.
Add a distinct, stable kind and name there, and provide a `PlayerController` implementation for it.
Also add its display label to `AiControllerDefinitions()`; both menus enumerate that registry.
The controller must work for either side, implement setup and per-phase progression, handle notices,
messages and battle votes, and save and restore all of its own state. It needs a side-specific
constructor for new games and an empty constructor for loading its saved state. Keep old kind values readable
so historical match checkpoints remain resumable.

The match options name each side independently (`ai1` is Alliance, `ai2` is Empire). A tournament
driver can vary the seed, swap the AI assignments across sides, invoke the same native runner, and
collect the JSON result files. It should count tick limits and crashes separately from wins. The
controllers are `classic` (kind 3, the game's existing `AiPlayer`), `codex` (kind 4) and `claude` (kind 5).
Give a competing implementation its own new kind; do not reuse either persisted value.
An implementation can also derive from `AiPlayer` to reuse its staff. The protected
`AcceptTaskOrder(OrderBase*)` hook can veto a staff order before validation and submission;
returning false sends it through the task's normal refusal path. The hook must not delete or
submit the order. A subclass must call the base save/read methods before its own state.

## Codex Strategist

`CodexAiPlayer` adds a campaign commander to the existing AI staff. The staff continues
production, research, diplomacy, character missions and battle voting. The commander:

- Scores known enemy worlds by objectives, industry, distance and available forces, including
  planets with no defending fleet. Threats to its own known headquarters interrupt a campaign.
- Combines fleets before attacking, loads surplus garrison troops, and keeps home guards.
- Requires a margin of space superiority, checks actual bombardment strength against shields,
  and follows orbital victories with bombardment and troop assaults.
- Reserves its campaign fleet and embarked cargo so staff logistics cannot undo the offensive.
  Lost flagships are replaced, and unproductive objectives are reconsidered.

It reads the same side-specific world view as Classic and submits ordinary validated orders,
charged to the same order budget. Its target, flagship, inbound reinforcement and planning
timers are saved with the controller. Set `REB_AI_TRACE=1` for campaign decisions and orders
on stderr (the tournament wrapper captures them in each `.runner.log`).

```sh
tools/run_ai_match.sh --ai1 codex --ai2 classic --seed 1 --victory 1 \
  --ticks 16000 --out work/ai_match/codex_alliance.sav
tools/run_ai_match.sh --ai1 classic --ai2 codex --seed 1 --victory 1 \
  --ticks 16000 --out work/ai_match/codex_empire.sav
```

HQ-only games (`--victory 1`) are useful for testing military campaigns. Standard victory
(`--victory 0`, the default) additionally requires the original character objectives; do not
compare results across victory modes or treat territory advantage as a win.

Classic versus Classic HQ-only games can plateau with a large territory lead and no victory. Two original-game
rules cause this. `AiPosture::CanTakeSystem` (`src/game/ai/ai_posture.cpp`) sends a single fleet only when it is
stronger than 120% of the target's assessed defence, so a well-defended Coruscant is never attacked even when the
side's combined fleets far exceed it. The combat-target query in `ai_posture::AssignTakeFleet` also considers only
systems with a known enemy fleet (flag `0x200`), so lightly held enemy worlds are skipped.

## Claude Admiral

`ClaudeAiPlayer` (`src/game/ai/claude_ai_player.cpp`) is another `AiPlayer` subclass. Like Codex, it
leaves the Classic staff in charge of production, research, diplomacy, missions and garrisons. Its
commander only directs the military:

- **One strike group.** It claims most of the warships (the staff's orders for them are vetoed),
  rallies and merges them under a flagship, and loads spare troops and fighters. It strikes the
  best-scoring target: value (objective, industry, our popular support there) times the estimated
  win odds, divided by travel time. Combat strength is estimated as attack x hull; it attacks only
  with a 1.8x margin.
- **Sieges.** Once the group holds orbit, it bombards when its bombardment exceeds the planetary
  shields, assaults when the planet has fewer than two shields and enough troops are aboard, and
  summons troops from nearby worlds otherwise. It gives up stuck sieges.
- **Holding conquests.** The regiments that took a world stay there for 300 days, and relieving a
  fresh conquest is worth more than a new target.
- **Intel.** Systems it has not seen recently are unknown, not empty. The enemy capital is always
  assumed to hold the strongest enemy group seen so far. The Empire scouts Alliance worlds for the
  hidden HQ (and uses a Death Star on it if it has one). Scouts are not sent back where one died.
- **Defense.** A home guard stays at the capital, the capital keeps a garrison of 8, battle votes
  never retreat from the capital (elsewhere only below 35% of the enemy's strength), and the
  Alliance HQ is kept away from systems the Empire has seen (the staff's own HQ moves are vetoed;
  the staff used to walk it into Yavin, which the Empire sees from the start).

The commander spends at most 6 of the side's 10 daily orders and saves its state with the controller.
`REB_AI_TRACE=1` logs its decisions, a battle probe (forces before and after each battle), and
economy and capital summaries. For experiments, `REB_CLAUDE_OFF=scout,hq,vote,guard` switches
features off.

Several ideas were tried and removed after measurement (24-48 game batches against Codex, at 3,000
or 6,000 ticks, scored by territory margin and wins). Every economic intervention made it weaker:
building warships at idle shipyards, balancing mines and refineries, raising the staff's
capital-ship share, and reserving a yard to fortify the capital. With all of them removed the
margin went from about -28 to -1. Flag officers took the staff's best diplomats. Seizing neutral
worlds with small troop fleets handed territory to the enemy, and so did training extra regiments
for that.

Results (both sides per seed, galaxy size 1, difficulty 7, 16000-tick cap; score is the mean
system-count margin):

| Opponent | Victory mode | Games | Wins / losses | Mean margin |
|---|---|---|---|---|
| Classic | HQ + Coruscant | 32 | 6 / 3 | +16.3 |
| Codex | HQ + Coruscant | 32 | 7 / 9 | -28.8 |
| Codex | standard | 16 | 1 / 5 | -46.9 |

Over 6000 ticks it is roughly even with Codex (48 games: -4.8, 5 / 4), but Codex pulls ahead in
long games.

## Paired tournaments

`tools/run_ai_tournament.py` plays every seed twice, swapping which AI controls Alliance and
Empire. It defaults to `codex` versus `classic` and writes each match's save, native log,
wrapper log, and result JSON under `work/ai_tournament/`. It updates `summary.json` after
every match, so an interrupted tournament can continue with the same options and `--resume`.
The summary records the executable's absolute path and SHA-256, including when `--exe` is
omitted. A resume requires the same binary contents; rebuilds require a new tournament output
directory to keep results from different AI versions separate. The tool rechecks the binary
before every match and stops if it changes, leaving completed results in `summary.json`.

```sh
tools/run_ai_tournament.py --seed-start 1 --seed-count 10 --ticks 100000 \
  --out work/ai_tournament/codex_vs_classic
tools/run_ai_tournament.py --seed-start 1 --seed-count 10 --ticks 100000 \
  --out work/ai_tournament/codex_vs_classic --resume
```

Use `--ai-a`, `--ai-b`, `--galaxy`, `--difficulty`, `--victory`, `--every`, and `--exe`
to change match settings. `--timeout-seconds` optionally kills a match that exceeds a wall-clock
limit. `summary.json` records the options, each run's exit status and native result, actual wins
by AI and side, tick limits, wall-clock timeouts, crashes, other failures, and mean final
controlled-system counts by AI and side. Territory counts are diagnostic: only the native
`winner` field counts as a win. The tournament exits with status 1 if any run times out,
crashes, or fails; a game tick limit is reported separately and does not make the command fail.

## Battle corpus and runner

All battles use `SpaceBattleEngine` (`src/game/app/battle_engine.h`): `BattleSetup` describes each battle as a
`BattleSnapshot` (`src/game/space_battle/snapshot.h`) and the engine resolves it. The legacy simulator and engine
selection were removed on 2026-10-19. Both sides are AI-controlled for headless resolution, advancing 20 fixed
simulation ticks per battle-flow frame. The engine returns strategic result records, including surviving carriers
and the Death Star. Draws with both sides present use the strategic held-first rule; the engine never requests
an unfinished-battle restart.
Snapshots now write version 2 with unit display names and a separate `class-name`; version 1 remains readable.

With `REB_BATTLE_CORPUS=<dir>`, every battle writes `<dir>/<campaign seed>-<n>.snapshot` when it starts and
`<dir>/<campaign seed>-<n>.space.result` once it has resolved (`n` counts the battles of the process from 1, so
give each match its own directory or seed). Recording does not change the campaign.

`build/native/reb_battle_runner` (built with the game, not shipped) runs snapshots headless:

```sh
build/native/reb_battle_runner --seeds 4 --out work/battle_corpus/space.jsonl \
  --in work/battle_corpus/classic/*.snapshot
tools/battle_calibration.py work/battle_corpus/legacy.jsonl work/battle_corpus/space.jsonl
```

It writes one JSON line per battle and seed (seed `i` uses the snapshot's seed plus `i`): the winner as
`BattleResults` decides it, the outcome flags, and per-side capital ships destroyed, withdrawn and damaged,
hull lost and fighter craft lost. The `steps` field counts space simulation ticks; `elapsed_ms` measures engine construction,
loading and simulation with a steady clock, solely for performance reporting.
Snapshots recorded before 2026-10-20 stored the side that arrived *last* under `held-first`: the system's battle
control field is overwritten by every arriving fleet or fighter (`System::NotifyUnitUsable`), and `BattleResults`
correctly gives a contested system to that side's opponent. Those snapshots replay with the attacker beside the planet
and the defender's garrison fighters already in space, and the runner credits their stalemates to the wrong side; swap
`held-first` before calibrating with them. In 1,093 of the 1,100 corpus battles where one side is only a planet's fighter
garrison, the recorded value named the other side.
`--write-results <dir>` also writes seed 0's results in the corpus `.result` form. Previously recorded legacy
results remain available as historical calibration data in ignored `work/` directories.
`tools/battle_calibration.py` reports the `docs/design/tactical_combat.md` section 8.3 metrics from runner output; its docstring defines the strength estimate.

Matches started concurrently on one machine have been seen to diverge from the same seed. Run comparisons of
AI matches one at a time, but verify repeatability explicitly, for example by repeating the baseline binary:
sequential execution alone is not a guarantee.

### Space engine calibration (2026-10-19)

Two calibration rounds tuned the space simulator against the 1,937-snapshot campaign corpus (three seeds each)
and recorded legacy results, before the legacy simulator was retired. The resulting rules, all in
`src/game/space_battle/tuning.h` or the commander:

- Fighter gun damage is halved (`kDogfightDamageScale`) and capital point defence is quartered
  (`kPointDefenceDamageScale`); the AI's strength and DPS queries include the same factors as actual fire.
- An ion engine outage no longer resets a charged hyperdrive spool, and outages are not extended while active.
- A stalemate needs progress of at least one craft or 1% of a side's hull/craft total within 120 seconds; chip damage
  accumulates toward that but cannot reset the timer on every hit. Stalemates leave both sides' survivors in place.
- The stronger fleet prioritizes withdrawing ships, closes on them, and orders tractor ships inside locking range.
  The base hyperdrive spool was raised from 15 to 30 seconds (`kSpoolBaseSeconds`), before retreat-distance and
  drive-rating scaling.

The pursuit and spool changes addressed the main finding: most losing ships escaped by withdrawing at full health
before contact, not through per-ship preservation. With them, loser capital destruction rose from 21.3% to 39.4%
(legacy: 46.3%), and a side at least twice as strong by simulator estimate wins about 97% of battles. Two older
design checks remain unmet: majority winner agreement with legacy is 84.8% against an 85% target, and the
scripted Cautious/High "expert" commander keeps under 2% more capital value than the default against a 20–40%
target, so it is not a meaningful skill ceiling. Equal-cost Mon Calamari versus Star Destroyer tests were balanced at
six versus five ships but strongly favored the cruisers at 28 versus 23. Four-seed Classic-versus-Claude campaigns
were mostly censored by the 60,000-tick limit and do not establish side win rates. Results and campaign artifacts
are kept locally under ignored `work/calibration/`.

### Tactical commanders

The pure simulator uses `DefaultCommander` for each AI-controlled side. Set
`BattleOptions::commanders[SideIndex(side)]` to a `CommanderParams`, or supply a
`BattleOptions::makeCommander` callback. `SpaceBattleEngine` calls the reusable
`BattleOptionsFor` helper for Classic → Aggressive, Codex → Balanced, Claude → Cautious.
An omitted parameter uses Balanced at Normal difficulty.

Low, Normal and High difficulty reassess every 5, 2 and 1 seconds. Low uses Spread allocation and no signals;
Normal and High use the simulator's overkill-aware Auto allocator and situational signals. Aggressive line ships
close with weapons power (and concentrate against a lone capital); cautious line ships stand off and preserve
ships below 50% hull, balanced below 25%, aggressive below 15%. Transports retain their 50% preservation threshold.
These settings change commands only, never ship statistics or the strategic fleet-withdrawal threshold.
The stronger fleet pursues spooling capitals and uses tractors to close inside locking range. The base hyperdrive
spool is 30 seconds before retreat-distance and drive-rating scaling.
Individual hull-preservation checks run each tick, like the simulator's automatic withdrawal rules, so a slower
fleet reassessment interval cannot postpone a ship's safety response.

Composition rules send bombers against carriers, screens against fighters, defenders against invasion transports,
and outmatched fleets against active interdictors. Death Star defenders screen the station; attackers seek fighter
superiority and send the flight with the most remaining torpedoes on the run. Damaged line groups can enter reserve
when their fleet has a strength margin. Groups are reassessed from current surviving ships and squadrons using
the simulator's shared strength and damage queries.

Fleet withdrawal latches the side's vote threshold (or `humanSideAiWithdrawThreshold` for a human side). Dependent
fighters recover before carriers jump, with a 15-second recovery deadline; a capital in immediate danger leaves
earlier. Individual preservation uses the same recovery procedure. Observe Battle starts with the player's current
ship/flight doctrines and overrides, including briefing edits, while still allowing urgent interdictor/invasion
priorities, screen retargeting, recovery and fleet withdrawal. Untouched initial human sides receive composition
planning for Simulate Results. Re-enabling AI creates a fresh commander; mid-battle saves retain its temperament,
difficulty, reassessment deadline, withdrawal intent and recovery timers.

`reb_space_battle_log` also supports a batch list (one snapshot path per line) for calibration:

```sh
build/native/reb_space_battle_log --batch work/ai_calibration/corpus.txt --seeds 3 \
  --engine-label commander > work/ai_calibration/commander.jsonl
build/native/reb_space_battle_log --batch work/ai_calibration/corpus.txt --seeds 3 \
  --default-commander --engine-label default > work/ai_calibration/default.jsonl
python3 tools/battle_calibration.py work/ai_calibration/legacy.jsonl \
  work/ai_calibration/default.jsonl work/ai_calibration/commander.jsonl
python3 tools/battle_ai_summary.py work/ai_calibration/default.jsonl work/ai_calibration/commander.jsonl
```

The batch tool freezes the previous minimal commander behind `--default-commander`, writes compatible JSON lines
plus actual simulator initial strength, fighter losses at 60/120 seconds and ion-engine spool-reset counts, and
requires no game process or SDL initialization. Generated corpus data and reports belong in ignored `work/` paths.
`--ai-side alliance|empire` runs the new commander only for that side against the frozen original; add `--expert`
for High difficulty with Cautious temperament. The summary tool accepts `--baseline default --side alliance`
(or `empire`) for paired preservation and lopsided winner-flip metrics. For a comparison against the current
production commander on both sides, use `--expert-side alliance|empire` instead: it changes only that side to
Cautious/High and retains the current Balanced/Normal opponent. Its `capital_survival` telemetry records class type
bits and survival for the refined-cost benchmark above; the older aggregate capital-value fields remain hull proxies.


## Validation notes

The October 2026 implementation was checked on macOS with the native CMake build and all 18
CTest checks, plus seven tournament-tool tests. The asset-backed UI smoke covers cockpit
framing and label fit, independent persisted choices, new-game construction, and live AI
replacement without losing queued orders. Codex saves were advanced and reloaded in separate
processes; repeating a continuation from the same checkpoint produced byte-identical saves.

The final executable played seeds 1 and 2 from both sides against Classic, with small galaxy,
difficulty 7, HQ-only victory and a 16,000-tick budget. Results: **2 Codex wins, 0 Classic wins,
2 tick limits**, with no crashes, wall-clock timeouts or runner failures. This is a small sample.

| Seed | Codex side | Outcome | Final tick | Codex / Classic systems |
| --- | --- | --- | --- | --- |
| 1 | Alliance | Codex victory | 9,374 | 58 / 1 |
| 1 | Empire | Codex victory | 11,108 | 64 / 22 |
| 2 | Alliance | Tick limit | 16,001 | 49 / 28 |
| 2 | Empire | Tick limit | 16,001 | 80 / 20 |

Ticks start at 1; the 16,000-tick budget therefore ends at tick 16,001. The seed-2 territory
leads did not meet the game's victory conditions. A 1,000-tick Codex-versus-Codex run also
passed under standard victory rules. Local saves, fingerprints and results are retained under
`work/codex-release/`; the paired summaries record the executable SHA-256.

The additional sanitizer attempts were **not clean validation passes**. On this host,
AddressSanitizer deadlocked while initializing its shadow memory, before game startup. UBSan
stopped in existing reconstructed type casts at `galaxy/system_persist.cpp:93` (also reproduced
with Classic versus Classic) and `events/object_timers.cpp:138` on a loaded save. These are
separate from the ordinary native build, menu, save/reload and match checks.
