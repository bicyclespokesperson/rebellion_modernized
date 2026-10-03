# UI navigation notes (640x480, headless Wine)

How to drive the original game with `tools/run_game.sh --orig tests/scenarios/<name>.txt` (scenario syntax:
`tools/container/run_in_container.sh`). All coordinates are screen pixels in the 640x480 desktop. Everything
here was verified against the original game; the screenshots land in `work/run/<scenario>/`.

## Scenarios

| Scenario | Reaches | Runtime (incl. +10 s teardown) |
|---|---|---|
| `smoke.txt` | main menu | ~37 s |
| `load_game.txt` | loads save "RebelsFastStart" (Alliance), galaxy screen, runs ~60 s at Fast | ~120 s |
| `new_game_rebel.txt` | new Alliance game, galaxy screen after the agent-droid intro, ~60 s at Fast | ~310 s |
| `new_game_empire.txt` | same as Empire | ~310 s |
| `ui_tour.txt` | loaded game, opens finders / sector / system / fleet / personnel / encyclopedia / objectives / overview / message index / GID menu / options screen with a shot of each | ~175 s |
| `save_game_slot.txt` | loads "RebelsFastStart", opens the in-game Saved Games screen (monitor at 15,372) and saves into the first empty row (left icon at 50,258); creates `work/game/SaveGame/SAVEGAME.005` (delete it before re-running: a second save asks to overwrite) | ~65 s |
| `space_battle_autoresolve.txt` | loaded game; the "Battle at Ando" window pops up; chooses *Simulate Results* (auto-resolve) | ~130 s |

Each was run at least twice (loads/battle/tour in parallel containers) with identical results. The load-based
scenarios are far more deterministic than the new-game ones (the new-game galaxy is randomised).
Prefer `load_game.txt` as the base for anything that needs a game in progress: it is 3x shorter.

## Timing rules

- Intro movies: Escape at 12, 16, 20 s; the main menu is up at ~26 s. Extra Escapes are harmless.
- Screenshots of Smacker movies are black (the video path is not captured); a black frame just means a movie is playing.
- Clicks right after a pointer *teleport* are unreliable. Always `move` to the target (ideally two moves, the
  last at the exact spot, 1 s apart) and then `click` 1 s later. Every scenario here does this.
- Allow 2-4 s after any screen change before the next click. A screen load (save game) takes ~3 s.
- Game time is wall-clock driven: the same scenario hits events (e.g. the Ando battle) at the same
  wall-clock second to within ~1-2 s. Loaded games start **paused**.
- Window double-clicks: send two `click`s 0.15 s apart (scenario times accept decimals, e.g. `46.0 click ...` /
  `46.15 click ...`). Two clicks one second apart are two single clicks.
- Escape-spam trick: pressing Escape repeatedly is the generic "close the top window" and is safe on the galaxy screen.

## Main menu (shuttle cockpit, up at ~26 s)

| Control | Click at | Effect |
|---|---|---|
| Empire monitor (bottom-left, green) | 185,335 | new game as the Empire |
| Alliance monitor (bottom-right, red; right edge ~498, so stay at x<495) | 465,335 | new game as the Alliance |
| Load Game monitor (right of centre) | 430,248 | Saved Games / options screen |

Clicking a faction monitor starts a game immediately: there is no setup dialog (default settings, no galaxy
size choice). The screen goes black (a movie plays, not captured in screenshots); Escape at ~31 s skips it.
Other monitors were not explored.

## New game: agent-droid introduction (the big pitfall)

After the shuttle movie the galaxy screen appears but a scripted advice-droid introduction runs for ~190 s
(until ~205-222 s after launch). During it the GID overlay cycles through its views, the day counter/clock
controls are inactive, and mouse + hotkeys do nothing. The intro cannot be skipped by Escape/space/Return/click
(tried). It ends by opening the Message Index with the advice list ("Victory Conditions" ...); Escape closes it.
Only after that does the speed menu work. This is why `new_game_*` take ~300 s; use `load_game.txt` when you
do not need a fresh game.

## Game speed (works only when no intro / modal is running)

Right-click the clock box in the top bar, then pick an entry (Pause / Very Slow / Slow / Medium / Fast, 20 px apart):

| Skin | Clock box (right-click) | Entries (x, y) |
|---|---|---|
| Alliance | 135,24 | Pause 170,36 / Very Slow 170,56 / Slow 170,76 / Medium 170,96 / Fast 170,116 |
| Empire | 535,22 | menu is clamped to the right edge: Fast at 575,114 (others 20 px steps above) |

The readme lists `alt+plus`/`alt+minus` (speed) and `alt+p` (pause); these were not verified, use the menu.
The counter at the top (day number + bar) shows the speed. Fast ~0.75 game days per real second; Very Slow is
nearly stationary over a minute, which makes it handy for tours that must not be interrupted by events.

## Galaxy screen, Alliance skin (loaded save "RebelsFastStart")

Hotkeys (verified; they are skin-independent): F2 System Finder, F3 Fleet Finder, F4 Troop Finder, F5 Personnel
Finder, F6 Message Index, F7 Galactic Encyclopedia (F1 does nothing). `alt+h` Objectives, `alt+o` Galaxy Overview.
`alt+w` closes all windows except encyclopedia articles (use Escape twice for those). Other readme hotkeys
(`alt+i`, `alt+m`, `alt+s`, `alt+b`...) were only tried during the new-game intro, where nothing works, so they are
unverified. Escape closes the topmost finder/dialog window but **not** the Game Controls (options) screen or the GID menu.

Bottom consoles (click = same as the F-key; hover shows a tooltip):

| Control | x,y |
|---|---|
| System Finder / Fleet Finder / Troop Finder / Personnel Finder | 120,415 / 170,411 / 222,411 / 268,411 |
| Encyclopedia (right console) | 405,411 |
| Galactic Information Display (GID) | 460,411 -> overlay menu (Loyalty, Fleets, Personnel, Resources, Manufacturing, Defense, Display Off at ~545,431) |
| R2-D2 / C-3PO (cosmetic, tooltips) | 338,432 / 575,400 |
| Game Controls (small monitor, lower left) | 15,372 |

Left toolbar message-category buttons (click = Message Index on that tab; x=15): Popular Support y=118, Fleets 142,
Missions 166, Resources 190, Manufacturing 220, Defense 245, Conflict 270, Advice 293, Chat 318.
Top bar: raw materials / refined materials counters at ~280,24 / ~380,24 (tooltips only).

Galaxy map (inside the main viewport): single-click a star cluster -> Sector window (e.g. 281,268 opens "Fakir";
the window fills the left half of the viewport, close with the X at ~285,45 or alt+w). Double-click a planet in
a sector window (e.g. 210,157 "Berchest") -> system window whose first tab lists Ship Construction / Troops in
Training / Facilities Under Construction (the manufacturing summary). Right-click a planet -> menu
(Encyclopedia, Status). Right-click a character icon (personnel tab) -> Move / Confirmed Move / Mission / Command /
Encyclopedia / Status / Retire (Mission and Command were greyed for the sample character).

## Finder windows (Alliance skin; these all follow "list + side buttons")

- System Finder (F2): first row of the list at ~(130,215) when "All Systems" header is at y~176; double-click a
  row -> that system's Sector window. Right-hand buttons: close (498,90), display/open (498,157).
- Fleet Finder (F3): double-click "Fleet 1" at ~(105,200) -> Fleet window ("Yavin") plus its Sector window; a second
  double-click on the fleet icon in that window expands the fleet's ship list.
- Troop Finder (F4): table of troops per location (read-only).
- Personnel Finder (F5): double-click first row (135,210) -> personnel window for that character's system.
- Message Index (F6): tabs across the top, list below; rows are mission/diplomacy reports etc.
- Encyclopedia (F7): list of entries, first "A-wing" at ~(130,197); double-click -> article (arrows at the top
  navigate; Escape closes the article and then the finder).

## Saved Games / options screen

Reached from the main menu (Load Game monitor, 430,248) or in-game via the lower-left monitor (15,372).

| Control | x,y | Effect |
|---|---|---|
| Rows (name, y) | JAS1 90 (Empire), Rebels 132 (Alliance), RebelsFastStart 173 (Alliance), JAS2 216 (Empire), 2 empty rows 258/300 | |
| Load icon at the right of a row | 305,y | load that game (confirm dialog if a game is running: "Load without saving?" check mark at ~280,300, X at ~370,300) |
| Icon at the left end of a row | 50,y | not explored (presumably Save; no tooltip appeared) |
| Right-end icon tooltip | 305,y | shows "Load Game" |
| Bottom icons | left 95,400 = "Restart the Game" (tooltip), middle 183,400 = return to the galaxy, right 270,400 = "Exit" (tooltip). Restart/Exit were not clicked | |
| Music / sound sliders, tactical display toggles | right half | |

When loading from the main menu there is no confirm dialog and the galaxy screen appears ~3 s later.
**Escape does not leave this screen in game; click the middle icon (183,400).**
Saving creates `work/game/SaveGame/SAVEGAME.00x` files; only `save_game_slot.txt` saves (the left icon of an empty row, then a "Selected Game Saved Successfully" dialog).

## Space battle (auto-resolve)

`load_game.txt` + Fast speed -> ~40 s of game time later (about 85-90 s after launch) a "Battle at Ando" window
opens over the galaxy (the Imperial fleet attacks the Alliance defenders at Ando). Game time pauses on the popup.
The three buttons under the picture:

| Button | Click at | Tooltip |
|---|---|---|
| left | 115,356 | Retreat |
| middle | 275,356 | **Simulate Results** (auto-resolve) |
| right | 410,356 | Take Command (3D tactical mode: avoid) |

Right-hand icons: Battle Summary (493,88), Alliance Forces (493,152), Imperial Forces, Go-to-system.
Simulate Results shows the outcome immediately and deterministically ("The Alliance fleet is defeated. Ando is now
under blockade by Imperial forces. The Alliance fleet has withdrawn to Delaya."). The result window closes with
the top-right button (498,90) and the clock resumes. See `space_battle_autoresolve.txt`.

## Pitfalls summary

- Black screenshots = movies; Escape skips them.
- New games spend ~190 s in the uninterruptible advice-droid intro; loaded games do not.
- Loaded games start paused; set the speed via the right-click clock menu.
- Teleported clicks get lost: always `move` first (twice) then click.
- Escape does not close the Game Controls screen or the GID overlay menu.
- Back-to-back runs share `work/game`; parallel runs of non-saving scenarios are fine (we ran 4 at once).
- New-game maps are random: do not rely on coordinates of galaxy objects in `new_game_*`; the loaded saves are fixed.
