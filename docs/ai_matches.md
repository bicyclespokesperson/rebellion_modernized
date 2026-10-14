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

Press **3** in either the shuttle cockpit or the strategic screen to choose **Classic (game
default)** or **Codex Strategist** independently for Alliance and Empire. Choices persist as
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
controllers are `classic` (kind 3, the game's existing `AiPlayer`) and `codex` (kind 4).
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
