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

`GameSession` and the simulation runner select controllers by persisted controller kind. The
AI name-to-kind lookup and controller construction live in `src/game/ai/ai_controller_factory.cpp`.
Add a distinct, stable kind and name there, and provide a `PlayerController` implementation for it.
The controller must work for either side, implement setup and per-phase progression, handle notices,
messages and battle votes, and save and restore all of its own state. It needs a side-specific
constructor for new games and an empty constructor for loading its saved state. Keep old kind values readable
so historical match checkpoints remain resumable.

The match options name each side independently (`ai1` is Alliance, `ai2` is Empire). A tournament
driver can vary the seed, swap the AI assignments across sides, invoke the same native runner, and
collect the JSON result files. It should count tick limits and crashes separately from wins. The
first available controller is `classic`, the game's existing `AiPlayer`.
