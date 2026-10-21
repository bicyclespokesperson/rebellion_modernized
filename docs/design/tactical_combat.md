# Tactical space combat: a new design

Status: implemented with live and headless battles; legacy retirement completed 2026-10-19 (section 8.5).
The original 3D tactical mode was deliberately left out (decision of 2026-10-09); its scaffolding and the legacy auto-resolve have now been removed. Historical analysis below records the design's
starting point; section 15 lists what was built and where it departs from this text, and current calibration
results are in docs/ai_matches.md.

## Summary

The player is the admiral, not a pilot. Battles are real-time with pause and are fought on a **2D battle plane**,
seen from above or at an angle, with ships drawn as 3D-looking models. Capital-ship combat in the original already
lived near a plane, and the games this design borrows most from (*Star Wars: Armada*, *Battlefleet Gothic: Armada*,
*Starsector*) are 2D for good reason: attacks from above and below are hard to read and hard to command. The
player's main tool is **doctrine**: per group (or per ship) target priorities, subsystem focus, engagement range,
approach, power routing and a withdrawal threshold. Ships then fight on their own, and every ship can explain what
it is doing and why. Live orders (focus fire, tractor, withdraw, launch a strike) are there for the decisive
moments, and an optional auto-pause cadence turns the same rules into a simultaneous-turns game.

Ship identity comes from the data the strategic game already has. Each capital ship has a 4 x 3 weapon grid (four
arcs, three weapon kinds), shields, hull, recharge rates, tractor beams, gravity wells, hangars and damage control;
each fighter has a squadron size, torpedoes and torpedo range. The new mode turns those numbers into behaviour.
An Imperial Star Destroyer has twice the firepower in its bow that it has in either broadside, so it points at its
target; a Mon Calamari cruiser has nearly twice as much in each broadside as in its bow, so it turns side-on. A
laser-only Lancer or Corellian Gunship becomes an anti-fighter umbrella for the ships around it, and an Interdictor
becomes the thing the enemy must kill before it can escape.

The payoff for playing a battle instead of auto-resolving it is mostly **strategic**. You can bring damaged
capital ships home, pin down an enemy fleet so it cannot escape, kill the carrier so its fighters are stranded, sink
the transports before the invasion lands, and keep a commander's flagship alive. Winning more often is secondary,
and the new mode is calibrated so that it rewards skill without trivialising the campaign.

The legacy auto-resolve had defects (section 8.4), so rather than fix it, the new simulator runs every battle,
watched or simulated, under one set of rules.

## 1. Goals and non-goals

Goals:

- A battle worth taking command of: three to eight minutes at normal speed, readable at a glance, with decisions
  whose consequences carry back into the campaign.
- Control through doctrine and a few well-chosen orders, per group or per ship. Pause is always available in a
  single-player game; speed of hand never decides a battle.
- Ships that feel distinct, derived from the existing ship records (`CAPSHPSD.DAT`, `FIGHTSD.DAT`) instead of a
  parallel balance sheet.
- The same strategic seam as auto-resolve: the units that `BattleSetup` registers go in, the result records that
  `BattleResults` applies come out.
- One simulator for every battle: run live when a human takes command, headless for "Simulate Results" and for
  AI-against-AI battles (section 8.5).
- A deterministic simulation, so battles can be tested, replayed, saved mid-battle and later played over LAN.
- Art that looks good and that we can produce. Licensed art stays local, as with the HD icon pack; anything
  committed is code or recipes.

Non-goals for the first version:

- Flying individual fighters. Fighters are commanded as squadrons and flights, as in the original.
- Ground combat, bombardment and planet destruction. These stay strategic.
- True 3D movement. Height is cosmetic: fighters are drawn above the plane, but nothing attacks from above or below.
- Rebalancing the strategic game. Replacing the resolver will move outcomes, so campaign balance is measured with
  AI tournaments (section 8.3) rather than assumed.
- Two-human tactical battles. The architecture is designed for them (section 11.5), but they ship later.

## 2. What the original did, and what to keep

The original 3D mode (manual chapter 4, TEXTTACT.DLL strings) had a reasonable command vocabulary. Its problems
were mostly presentation and opacity.

| Original feature | What went wrong | This design |
|---|---|---|
| A wireframe "holocube", a keypad-driven camera, and four fixed sets of navigation pyramids as the only move targets | Hard to read depth; moving a task force meant hunting for the right pyramid | A 2D battle plane under an angled camera, a tactical overlay, click-to-move on the plane, and mostly relative orders (approach, keep range, escort) that need no waypoint |
| Maneuvers (Left Hook, Right Hook, Hammer, Anvil, Stop) and tactics (Stand Off, Surround) | The names were evocative but their effect was hard to see | Kept as the **approach** and **engagement range** settings of a doctrine, with visible consequences: arcs and weapon ranges (section 6.3). Hammer and Anvil (over the top, from below) have no meaning on a plane; Pincer takes their place |
| Missions: Attack Capital Ships ("the most dangerous"), Attack Fighters, Escort, Recover, Attack Death Star | "Most dangerous" was opaque and fixed | Explicit, reorderable target priorities; each unit shows its current intent and the reason for it (section 5.3) |
| Order latency, reduced by assigning a Fleet Admiral | Felt like input lag, not command | Orders are immediate; ships still take time to turn and to reroute power. The admiral's skill buys command signals and faster transitions (section 5.6) |
| Fighters drawn as coloured dots, ordered only as Red/Blue/Green/Gold groups | No sense of a strike; fighters were background noise | Squadrons are visible craft flying attack runs, rearming at carriers, with escort and intercept missions |
| "Observe Battle" handed control to an AI that never withdrew | Watching was not a real option | The AI uses the same doctrine, including withdrawal thresholds, so "watch at 4x" is a legitimate way to play |
| Death Star superlaser (charge, then fire at a capital ship) and the Alliance fighter run | Remembered as the highlights | Kept and expanded into a set piece (section 6.12) |
| Planetary defenses had no part in the 3D battle | | The planet's LNR and KDY-150 batteries fire on enemy capital ships inside a ring around it (section 6.13) |
| The battle opened paused | Good: the player could plan | Kept as a short briefing and deployment phase |

## 3. Borrowed from other games

| Game | Mechanic | How it lands here |
|---|---|---|
| *Star Wars: Armada* (tabletop) | Hull zones with their own firing arcs; command dials (navigate, repair, concentrate fire, squadron); squadrons that escort or engage; capital ships that activate nearby squadrons | Rebellion's data already has four arcs per ship, so facing is the heart of capital-ship play. Power routing and commander signals play the part of the command dials. Squadrons near a ship with a commander aboard fight better |
| *Homeworld* | Aggressive, neutral and evasive tactics; formations; a sensors view; carrier strike cycles. (Also true 3D movement, which this design deliberately does not take) | The zoomed-out tactical overlay and fighter rearm cycles |
| *Nexus: The Jupiter Incident* | Subsystem targeting (engines, weapons, shields, jump drive); slow, heavy capital ships; fighter wings | Subsystem focus maps onto Rebellion's existing critical systems: engines, shield recharge, weapon recharge, hyperdrive, tractor beam |
| *Gratuitous Space Battles* | Battle decided by pre-battle orders: target-size priorities, engagement range, formation; then you watch | The doctrine card as the main input, and watching at speed as a valid way to play. GSB's weakness, opaque behaviour, is answered by inspectable intent |
| *Starsector* | Officer personalities; fleet-level objectives; retreat to preserve ships that cost a campaign to replace | Withdrawal thresholds and retreat as the main skill payoff; commanders with temperaments |
| *Battlefleet Gothic: Armada* | Broadside ships against prow-gunned ships; special orders with cooldowns (brace, all ahead full, lock on) | The Alliance-broadside against Imperial-bow asymmetry; commander signals with cooldowns |
| *X-Wing* / *TIE Fighter* (1993-94), *FTL* | Routing power between weapons, shields and engines; disabling specific systems | Power routing per group, and ion weapons as the disabling tool |
| *Empire at War* | Heroes with abilities; hardpoints; planetary ion cannons | Commander signals; planetary turbolaser batteries and ion cannons (section 6.13) |
| *Frozen Synapse*, *Combat Mission*, *Battlestar Galactica Deadlock* | Simultaneous planning, then execution (WEGO) | Optional auto-pause every N seconds, which is also the natural model for two-human battles |

Three things are deliberately not borrowed: true 3D movement, line-of-fire obstruction by friendly hulls (an Armada
staple, but it is fiddly and frustrating in real time) and individual fighter piloting.

## 4. The shape of a battle

1. **Battle Alert** (exists): Simulate Results, Retreat or Take Command. Nothing changes for players who never
   take command.
2. **Briefing and deployment** (paused). Enemy composition is shown only as far as the strategic game would show
   it on the Battle Alert window. Capital ships are pre-grouped into task forces by archetype (section 7.2) and
   fighters into flights by type. Each group starts with its archetype's default doctrine. The player can regroup,
   edit doctrines, apply saved presets and drag groups around their deployment zone, or just press Engage.
   Capital hulls deploy inside a circular zone, clear of the planet and battle-plane edges. Existing layouts that
   fit stay unchanged; crowded lines wrap into ranks behind, keeping front archetypes ahead of support ships and
   task forces together where the chord permits. If wrapping cannot fit, the circle grows rearward only, preserving
   its forward edge and the gap between sides. Bounds are derived deterministically from the snapshot on restore.
   A fleet too large for the battle plane keeps the old unwrapped lines rather than searching forever.
   Dragging translates or rotates the formation rigidly and stops at the first hull boundary, including the planet.
3. **Engagement**: real time with pause at 0.5x to 8x speed. Optional auto-pause on events (section 5.7).
4. **Resolution**: the battle ends when one side has nothing left in the battlespace, or at a stalemate or time
   limit, which leaves the system with whoever held it (section 6.11).
5. **Battle Results** (exists): the strategic game applies the results as it does today.

The battlespace is the orbital zone of the system. The defender deploys between the planet and the arrival
vector, with fighters stationed on the planet launching from the planet's limb. The attacker drops out of
hyperspace along the bearing of the system it came from on the galaxy map, which gives each approach a direction
for free. Everything moves on one plane. Fighters are drawn a little above it with a shadow, so they visibly
swarm over capital ships, but that height is cosmetic: a fighter over a ship is simply close to it.

## 5. Command model

This is the heart of the design. The player's three layers are doctrine (set once, adjusted occasionally), live
orders (decisive moments) and commander signals (scarce, powerful).

### 5.1 Units and groups

- **Ships** and **squadrons** are the units. A squadron is up to 12 craft (the record's squadron size) that fly,
  fight and die individually but take orders together.
- **Task forces** group capital ships (number keys 1-9, as the original's 1-8). **Flights** group squadrons. A
  task force moves at the speed of its slowest ship unless its doctrine says "break formation".
- Doctrine belongs to a group. Any field can be overridden for one ship, and the override shows as a badge on the
  ship card. Dragging a ship to another group adopts that group's doctrine, minus its overrides.

### 5.2 The doctrine card

| Setting | Options | Default |
|---|---|---|
| Role | Line, Strike, Flank, Screen (escort a named ship or group), Carrier (stand off and launch), Picket, Reserve (hold until committed) | From the hull's archetype (section 7.2) |
| Engagement range | Close (all weapons, including lasers), Optimal (computed from the battery mix), Stand-off (outside the enemy's best range; needs a speed advantage) | Optimal |
| Approach | Direct, Left Hook, Right Hook (swing around that flank of the target group), Pincer (split and take both flanks), Hold, Under the guns (engage without leaving the side's planetary battery cover, section 6.13) | Direct; Under the guns for a defender's battle line with batteries |
| Facing | Auto, Bow-on, Broadside, Present strongest arc to the strongest threat | Auto |
| Target priorities | An ordered list of up to four target tags, plus "finish damaged targets" (section 5.3) | From role |
| Fire allocation | Concentrate (one target for the group), Spread (each ship picks its own), Auto (overkill-aware split) | Auto |
| Subsystem focus | Hull, Shields, Engines, Weapons, Hyperdrive, Tractor beam | Hull |
| Power routing | Balanced, Weapons, Shields, Engines | Balanced |
| Withdraw at | Never, hull below 25% / 50% / 75%, when shields collapse, only on order | Hull below 25% |
| Formation | Line abreast, Column, Wedge, Ring (around an escorted ship), Loose | From role |

Fighters have their own card:

| Setting | Options |
|---|---|
| Mission | Intercept (hunt fighters and bombers in a zone), Escort (a ship or another flight), Strike (attack capital ships by priority), Patrol (zone), Hold in hangar |
| Target priorities | As for capital ships, restricted to what the flight can hurt |
| Dogfight | Engage fighters freely, or avoid them unless attacked and press on to the strike target |
| Return to rearm | When torpedoes are spent, at 50% losses, never |
| Launch | Immediately, when enemy fighters commit, on order |

The card is compact on screen: one line of icons per setting, expanded on click. Presets ("Hammer", "Screen",
"Strike package", "Net", "Hold the line") are named, saved per profile and applied in one click.

### 5.3 Target priorities, explained

Each candidate target carries tags computed from its data, so priority lists work on any ship without a hand-made
table:

- **Size**: fighter, light (corvettes, frigates), medium (cruisers, Victory), heavy (Star Destroyers, Mon
  Calamari), super (Super Star Destroyer, Bulwark), Death Star.
- **Function**: carrier (has hangar capacity), transport (carries troops and little else), interdictor (gravity
  well), tractor (has a tractor beam), picket (laser-heavy), bomber (fighter with ion cannons: Y-wing, B-wing,
  TIE Bomber),
  flagship (a commander aboard).
- **State**: shields down, damaged (hull under 50%), withdrawing (spooling hyperdrive), tractored, threatening (is
  shooting at a ship this group escorts).

Each group's list is a ranked set of tags ("Carriers, Interdictors, Damaged, Nearest"). A candidate's score is
the weight of the best-ranked tag it matches (8, 4, 2, 1), multiplied by how efficiently this ship can hurt it
(weapon suitability, range, which arc it is in, time to turn) and by how close it is to dying. "Finish damaged
targets" boosts targets that the group can kill within a few seconds.

Allocation then avoids overkill. Every second, staggered across groups, the group allocator assigns its ships
(strongest first) to the best target whose already-committed damage per second is below its remaining effective
hull plus shields; anything above that spills to the next target. Concentrate removes the cap for the top target;
Spread skips the allocator.

Weapons split by suitability. Each battery (an arc and weapon kind) picks its own target from the same list,
filtered to targets it can hurt. While the turbolasers of a Star Destroyer hammer the Mon Calamari cruiser, its
laser-armed escorts' point defence covers the incoming X-wings without any order. "All batteries on target"
overrides this when the target's shields are down and every gun counts.

**Every unit can explain itself.** Hovering a ship shows its intent line, for example: "Targeting Nebulon-B
*Redemption*: priority #1 Pickets; in fore arc; about 22 s to destroy; 2 other ships assigned." A group with nothing
to shoot says why: "No target in range: holding Stand-off at 9 km". This answers the main complaint about
doctrine-driven games, which is that you cannot tell whether the ships are obeying you.

### 5.4 Live orders

Orders apply to the selection and replace or (with Shift) queue after the current one:

- **Attack** a target: focus fire overriding priorities; Ctrl-click adds further targets in order, as in the
  original.
- **Move / approach**: right-click on the plane to move, or right-drag to set the final facing as well; or
  approach a target with a chosen vector.
- **Hold**, **Escort**, **Screen**.
- **Tractor** a ship (tractor-equipped ships only; section 6.9).
- **Launch**, **Recall**, **Strike** (flights), **Recover** (dock for rearm or before a jump).
- **Withdraw**: spool hyperdrive and jump when able (section 6.10); fleet-wide withdrawal asks for confirmation, as
  the original did.
- **Get Immediate Result**: stop commanding; the same simulator finishes the battle headless from its current
  state, with the AI commanding both sides (section 11.1).
- **Power routing** and **doctrine** changes, which are orders like any other.

### 5.5 Power routing

Each group routes power to Balanced, Weapons, Shields or Engines. Switching takes about five seconds, during which
both old and new settings are half-applied, so it is a commitment rather than a toggle to spam. Initial tuning:

| Routing | Weapon recharge | Shield recharge | Speed and turn rate |
|---|---|---|---|
| Balanced | 1.0 | 1.0 | 1.0 |
| Weapons | 1.3 | 0.5 | 0.9 |
| Shields | 0.7 | 1.8 | 0.9 |
| Engines | 0.8 | 0.8 | 1.4 |

The existing ship records carry separate weapon and shield recharge rates, so routing scales numbers the game
already has. Ion hits and critical hits to the weapon or shield recharge systems interact with it naturally.

### 5.6 Commanders

The manual's rules are kept and made visible:

- The **fleet admiral** (the best admiral present, as before) sets the fleet's **command rating**. It shortens
  power-routing and formation transitions, improves capital-ship speed and turn rate (section 6.2), and grants
  **command signals**, whose number of charges grows with leadership and which recharge slowly. Signals are
  fleet-wide moments:
  - *Concentrate fire*: every ship in range shifts to the named target for 20 s, with an accuracy bonus.
  - *Hold the line*: no automatic withdrawal and doubled damage-control rate for 30 s.
  - *Scramble*: all hangars launch at three times the rate.
  - *Coordinated jump*: every ship withdrawing within 15 s spools 40% faster.
- A **commander** (the manual's fighter-boosting rank) aboard a ship improves the accuracy and evasion of
  squadrons within a command radius of that ship. Fighters therefore want to work near the ship that leads them,
  which is Armada's squadron-command idea.
- **Generals** do nothing in space, as before.
- Characters' existing skill values drive these numbers. A Jedi or a named character gives no special
  abilities in v1; the system can host them later.

The flagship carrying the admiral is tagged as a flagship. Losing it loses the signals. Characters aboard a destroyed
ship are set adrift by the existing strategic rules (`Character::HandleDestroyedParent`), and capture by the enemy
comes with an injury roll that can be fatal.

### 5.7 Pacing and pause

- Pause any time in single player; speeds 0.5x, 1x, 2x, 4x and 8x.
- Auto-pause, each individually toggleable: enemy reinforcements arrive, a gravity well activates, a ship drops
  below its withdrawal threshold, a carrier is lost, the Death Star superlaser is charged.
- **Command phases** (optional): the game pauses every 10, 20 or 30 s of battle time. This is the WEGO mode, and
  it is the default for two-human battles.

## 6. Combat model

The retired legacy auto-resolve (formerly `src/game/tactical/`) was the starting point: the new model keeps its data, its
weapon-against-target matrix, its critical systems and its withdrawal contract. Where the original is too abstract
for a real-time battle, or simply wrong (section 8.4), the departure is called out.

### 6.1 Scale and pacing

- **Distance** comes from the data's weapon ranges: one range point is 100 m. Turbolasers reach 3.5-7.5 km (the
  CC-9600's 75 is the longest), ion cannons 3.5-6 km, capital-ship lasers 1.7-2.5 km, fighter guns 1.2-2 km (the
  B-wing reaches furthest), torpedoes 0.7-1 km, tractor beams 2 km. The battle plane is about 40 km across.
- **Speed** keeps the original's ratio between fighters and capital ships (sub-light x 0.4 against x 0.24).
  Starting scale: an Imperial Star Destroyer (sub-light 4) cruises at 240 m/s, a Corellian Gunship (10) at
  600 m/s, an X-wing (8) at 800 m/s and an A-wing (11) at 1,100 m/s. (The original also added the admiral term,
  at least 1, to every capital ship's speed, which swamped the differences between slow classes; here the admiral
  adds a modest percentage instead.)
- **Turn rate** follows the original's structure: a capital ship's turn points are its maneuverability plus the
  admiral term (1 without an admiral, as in the original, rising to 3 with leadership), at 10 degrees per second per
  point; fighters use maneuverability + 1. An unled Imperial Star Destroyer (1 + 1) takes 9 s to turn around, a
  Lancer (3 + 1) 4.5 s.
- **Pacing targets**: a capital ship under the focused fire of an equal group lasts 45-90 s; fighter attack runs
  take 10-20 s; a typical battle lasts 3-8 minutes at normal speed. One global damage scale is tuned to hit these
  and then held fixed for calibration.

### 6.2 Movement

- Capital ships move along their heading with modest acceleration and no drift, as in *Homeworld*; the challenge
  is turning, not inertia. A task force keeps formation at its slowest member's speed.
- Fighters fly attack runs: approach, a firing window, break off, extend, come round again. In a dogfight the more
  agile craft gets more firing windows. A squadron is a loose formation of up to 12 craft.
- The fleet admiral improves capital-ship speed and turn rate. (In the original, both 3D and auto-resolve, the
  admiral term is `clamp(9 - leadership / 10, 1, 9)`, so a better admiral gives a smaller bonus. The arithmetic is
  in the original executable; the new simulator makes the bonus grow with leadership.)

### 6.3 Arcs and facing

Each capital ship has up to twelve **batteries**: four arcs (fore, aft, port, starboard) times three weapon kinds,
taken directly from the record's 4 x 3 grid. Each arc spans 100 degrees, centred on its side of the ship, so
adjacent arcs overlap by 10 degrees at the corners, where both can fire.

- **Output.** A battery fires a salvo whenever it is charged; its damage is its points times the weapon factor
  against that target (6.4). The starting scale is set so that a ship firing only its best arc deals what the
  legacy auto-resolve credits it with (the weapon recharge rate times that arc's weapon mix). **Each additional arc
  that bears adds its own points.** This is the one deliberate change to the original's arithmetic: there the weapon
  recharge rate was a power budget shared by all arcs, so total output hardly depended on how many guns faced the
  enemy. Here a Mon Calamari cruiser sailing between two targets fires both broadsides, and a Star Destroyer that
  lets an enemy reach its stern has only its weak aft arc to answer with.
- **Auto facing** samples headings and turns toward the one that brings the most effective firepower to bear on
  the current targets, limited by turn rate. Bow-heavy ships therefore point at their target, broadside ships turn
  side-on, and omni-armed pickets just keep station.

### 6.4 Weapon kinds

| Kind | vs capital-ship shields | vs capital-ship hull | vs fighters | Notes |
|---|---|---|---|---|
| Turbolaser | 1.0 | 1.0 | 0.5, and poor tracking against agile craft | The killer |
| Ion cannon | 1.0 | none; disrupts instead (6.6) | none | The disabler, as in the original 3D mode |
| Laser cannon | 1/6 | 1/6 | 1.0 | Point defence |
| Torpedo (fighters) | 0.25 | 1.0 | none | Limited per craft; rearm at a carrier |

The turbolaser and laser factors are the original's (`tactical_weapon_profile.cpp`). The ion row follows the
original 3D mode's ion hit handler (shields only) rather than the weapon profile, which rates ion like a
turbolaser. Against fighters, laser and turbolaser hits are scaled by `max(0.1, attacker turn rate / target turn
rate)`, the original's evasion term, so an A-wing or TIE Interceptor is hard for a slow ship to hit. Calibration
scales capital batteries against craft to 25% and fighter guns against craft to 50% of their starting output;
anti-capital damage stays unchanged. A torpedo
against a ship that still has shields does a quarter of its damage, to the shields only; against an unshielded
ship it does full damage to the hull. (In the legacy auto-resolve torpedoes count only once shields are gone.) The
combined-arms rhythm is the same either way: guns and ion strip the shields, then the bombers go in.

**Point defence covers neighbours.** Laser batteries engage any enemy fighter within their range, including
fighters attacking other ships. A Lancer or Corellian Corvette alongside a Star Destroyer protects it simply by
being there, and the Screen role keeps laser-armed ships between the threat and the escorted ship.

### 6.5 Shields, hull and critical systems

- **Shields** are one pool per ship with the record's maximum and recharge rate. They absorb everything, torpedoes
  included (6.4). Recharge is also a damage threshold: fire that cannot outpace it never breaks through, which is
  why a lone picket cannot hurt a Bulwark (shield recharge 60).
- **Hull** takes damage only once shields are down. A damaged ship fights worse: weapon and shield recharge scale
  with remaining hull, as in the original. It is the anchor for calibration, and it gives withdrawal its point: a
  ship at 30% hull is a liability worth bringing home.
- **Critical systems** are the five the strategic game already stores: shield recharge, weapon recharge, tractor
  beam, engines and hyperdrive. Each hit costs 25% of the system and four disable it; the hyperdrive takes one hit
  per drive (primary and backup). They come in from the strategic game and go back out in the results.
- **Subsystem focus** biases which system a hull hit damages. It costs 30% of the hull damage and triples the
  chance of a critical hit on the chosen system. Hyperdrive focus pins a fleet (the Corellian Corvette and every
  Alliance transport have no backup drive, so one hit is enough); Engines focus slows a ship so the line can
  outmaneuver it; Weapons focus blunts a ship you cannot kill in time.
- **Damage control**: on a fixed interval each ship rolls its damage-control rating as a percentage chance to repair
  one critical hit, as the original 3D mode did (every 50 s; the interval is a tuning value). The Mon Calamari
  cruiser (50) recovers from critical hits far better than the Super Star Destroyer (20).

### 6.6 Ion and disruption

Ion fire hits shields only, at full value. Once a capital ship's shields are down, further ion fire **disrupts**
it, as in the original's ion hit handler: engines go offline for 30-50 s, a weapon arc is discharged, and in this
design also any tractor lock or gravity well drops. Charged hyperdrive progress is retained; an engine outage
must expire before another ion hit can disable the engines again, giving a charged ship an escape window. Disruption is temporary and does
not carry back to the strategic game. Ion weapons therefore have a clear job that turbolasers cannot do: stopping a
ship without killing it, freeing a ship from a tractor beam, or opening a gravity well long enough to escape.
Y-wings, B-wings and TIE Bombers carry ion cannons, and so do most heavy capital ships (the Imperial Star Destroyer
puts half its bow firepower into ion).

### 6.7 Fighters

- A squadron is up to 12 craft with 8 hull each (as in the original), per-craft shields from the record, and guns
  in the fore arc only. Losses remove craft; the result reports the squadron's remaining size.
- **Torpedoes** are ammunition: per craft from the record (X-wing 4, Y-wing and TIE Bomber 8, TIE Defender 10,
  B-wing 12), with the record's torpedo range. A flight that has spent them returns to rearm if its doctrine says
  so.
- **Hyperdrive.** Every Alliance fighter and the TIE Defender can jump on its own. TIE Fighters, Interceptors and
  Bombers cannot: when the Empire withdraws they must be docked aboard a carrier or they are lost, which is the
  original rule. Imperial carriers are therefore something the Empire must protect, and something the Alliance
  should hunt.
- **Commander.** A commander aboard a ship improves accuracy and evasion for friendly squadrons within a command
  radius of that ship (section 5.6).

### 6.8 Carriers and hangars

- Each carrier launches one squadron every 5 s plus a random 0-7 s, as in the original; the Scramble signal
  triples the rate. Launch timing (immediately, when enemy fighters commit, or on order) is part of the flight's
  doctrine.
- **Recover** docks a flight in its own carrier or any friendly ship with free hangar slots. Docked craft rearm
  torpedoes and recharge shields; lost craft are not replaced in battle.
- **Docking is safe only under cover.** A flight may dock to rearm or shelter while another unit of its side is in
  space. Docking is never a way to outlast the enemy (§6.11). The last squadron in space does not return to rearm,
  whether its doctrine or an order calls for it; it fights on instead.
- A carrier that is destroyed takes its docked squadrons with it (original rule). Its launched squadrons look for
  another carrier with space; any left over return to the strategic game through the existing carrier-change and
  hangar-overflow rules.

### 6.9 Tractor beams

A ship with a tractor beam can lock one enemy capital ship within its tractor range (20 points, 2 km). A target
held by tractors with combined power at least equal to its sub-light rating is stopped: it barely turns and
**cannot jump**. Weaker locks slow it in proportion. Power comes from the record, scaled by the tractor ship's hull
and critical hits: Super Star Destroyer and Bulwark 6, Imperial Star Destroyer, Imperial II and Strike Cruiser 3,
most others 1-2. A lone Imperial Star Destroyer (3) cannot stop a Mon Calamari cruiser (sub-light 4); add a Victory
(2) and it can. The fast pickets are hard to hold: the Corellian Gunship (10) needs a fleet's worth of tractors.
Locks break when the target leaves range, when the tractor system is knocked out, or under ion disruption.

This is a new rule. The original subtracted tractor power from a speed of `admiral term + 0.24 x sub-light`, with
the admiral term at least 1. Every class then had a speed between about 1.2 and 3.4, so a single Imperial Star
Destroyer could stop almost anything, which made tractor power and target speed nearly irrelevant.

### 6.10 Gravity wells and withdrawal

- **Gravity wells.** Only the Interdictor Cruiser and the CC-7700 frigate carry projectors. In the original a
  working projector anywhere in the enemy fleet blocks all withdrawal. Here it projects a large circle on the plane
  (initially 15 km radius in design units, 22.5 km on the plane after the world-scale change of section 15;
  shown on the overlay). No enemy ship inside can jump, so escape becomes a question of
  killing the Interdictor, disrupting it with ion fire, or running out of its reach. The Battle Alert's Retreat
  stays disabled while the enemy has a working projector, as now.
- **Withdrawal** is a per-ship, per-group or fleet-wide order, or a doctrine threshold, or the side's strategic
  threshold (section 8.2). A ship can jump if its side can retreat at all (`QueryRetreat`), it has a working
  primary or backup hyperdrive, its speed is above zero (engines working, not held by tractors), and it is outside
  every active enemy well.
- **Spool time** comes from the side's retreat time (the original passed it as seconds to the 3D mode) and the
  ship's hyperdrive rating, where lower is faster: the Assault Transport (50) gets out first, the Medium
  Transport (100) last. Calibration uses `clamp(30 + retreatDays * 0.4, 8, 60)` seconds before multiplying by
  the working drive's rating / 100; this gives pursuit time to matter without changing strategic retreat thresholds.
  A Death Star in the battle greatly lengthens Imperial spool times, as the original README
  describes. A spooling ship is tagged "withdrawing", which enemy doctrines can prioritise.
- When a side withdraws, ships that cannot jump yet (inside a well, held by tractors, hyperdrive knocked out) keep
  spooling and fighting until they can, or until they are destroyed. Fighters without a hyperdrive dock with a
  departing carrier or stay behind. The original instead destroyed every unit that could not leave the moment its
  side withdrew; together with the automatic fleet-wide threshold that would kill trapped ships outright, which is
  the kind of result section 8.4 calls a defect. Ships that jumped report as withdrawn with their remaining hull.

### 6.11 Ending

- A side is finished when it has no active units, whether destroyed or withdrawn. Squadrons in a hangar hold no space.
  A squadron that has flown and docked again does not keep its side in the battle. Only one still waiting for its
  first launch does. When a side has nothing left in space and the enemy does, the battle ends against it, and its
  docked squadrons are lost: they cannot come out again. This applies at a stalemate or the time limit too. A planet's
  garrison kept in its hangar concedes the system rather than winning by default.
- If neither side has lost strength for a long time (the original uses 1,200 steps), the battle ends with both
  sides present. The engine reports both sides alive and never "unfinished"; `BattleResults` resolves that
  combination with `contestedWinner`, in favour of the side recorded as holding the system first, and the strategic
  game routes the other side away. (The legacy simulator cleared that combination, so its results never reach this
  path.) The original's rule that a stalemate loser unable to withdraw is destroyed outright is not carried over.
- Calibration requires a loss of at least one craft's hull (8 points, which may be spread over several craft) or 1%
  of a side's last recorded hull total, whichever is greater, within 120 seconds to count as progress. Negligible
  chip damage cannot keep a stalled battle open. Point defence wearing a squadron down can.
- A time limit (initially 15 minutes of battle time) ends the battle the same way.

Live battles wind down for a few seconds after they are decided, with movement and effects continuing under a
ceasefire that cannot change the results; Continue ends the wind-down early.

### 6.12 Death Star

The Death Star is the original's best-remembered set piece, and it stays one.

- **Superlaser.** The battle starts with it charged, as in the original; a recharge takes five minutes of battle
  time. The Imperial player chooses the target, which must be a capital ship, and the AI picks its top priority. The
  target is destroyed outright about a second after firing. The charge meter is on the HUD, and the charged moment
  is an auto-pause event.
- **The station** cannot be damaged by ordinary weapons (original rule). Its surface batteries (the record has
  1,400 turbolaser points per arc, unused by the original's battle code) can optionally defend its approaches; that
  is a balance question, since the original ignored them.
- **The run.** As in the original: the Death Star must be complete, no Death Star Shield facility may be in the
  system, the Alliance must have more fighters in play than the Empire, and one flight at a time may attack. The run
  is shown, not rolled: the flight dives into the trench, each craft survives the run by its maneuverability and the
  commander's skill (the A-wing's agility shines), and at the port the shot succeeds with a chance from the
  flight's torpedoes and the commander's combat skill, starting from the original formula. Success destroys the
  station and everything still in its hangars.

### 6.13 Planetary defenses

The system's KDY-150 ion cannons and LNR Series I and II turbolaser batteries (defense facilities of families 0x22 and
0x23) fight in the battle; GenCore and Death Star shields stay strategic. They are not units: they cannot be destroyed,
ordered or selected, and they report no results, so taking the system still means winning the fleet battle and then
bombarding the planet.

- **Cover.** A side's batteries fire from emplacements on the planet's limb at enemy capital ships whose hulls come
  within 10 km (design units; 15 km on the plane) of the surface. The ring spans the defender's deployment zone and the
  approach to it, out to about the battle's centre, and is drawn around the planet in the owner's colour with inward
  ticks; ships inside it carry an "Under planetary guns" flag. Fighters, the Death Star and friendly ships are never
  targets.
- **Targets.** Every battery fires on the intruder nearest the planet, or on the side's Concentrate Fire target when
  it is in reach. An ion cannon moves on from a ship it has already stopped (shields down and engines out).
- **Damage.** The records' attack strengths (LNR I 800, KDY-150 2,000, LNR II 5,000) are on the bombardment scale, where
  one hit cripples a ship. In space a battery's damage per second is 0.35 x the square root of its attack strength:
  LNR I 9.9, KDY-150 15.7, LNR II 24.7, fired every 3 s as heavy bolts. Accuracy falls with distance from the surface,
  as for a ship of detection 50. LNR fire hits shields and then hulls like a turbolaser. KDY-150 fire is ion: it hits
  shields only, and its disruption always takes the engines when they are running (30-50 s, the usual ion outage),
  otherwise a weapon arc. A ship it stops stays under the guns and cannot jump.
- **Under the guns.** A new approach keeps a group engaging whatever comes into the ring without leaving it; explicit
  attack orders still leave it. The defender's battle line starts with it when its side has batteries, and the battle
  AI keeps it until it is clearly the stronger (1.25 / 1.5 / 2 times, aggressive / balanced / cautious). Stand-off
  doctrines keep out of an enemy ring as they keep out of enemy ships' reach.
- **Strength.** Batteries add their firepower (ion at half) to their side's strength estimate but no durability, so the
  battle AI weighs them in its withdrawal decisions without counting a fleetless planet as able to hold.
- **Knowledge.** The briefing and the map show enemy batteries that the side's intelligence has seen; others are
  revealed when they open fire.

Calibration (2026-10-20). In AI campaigns 23% of battles happen at a defended planet, and 97% of those have a single
battery, usually an LNR II; real attacks are mostly lopsided, so a battery rarely changes who wins. On the 1,674
corpus battles with capital ships on both sides, giving the defender one LNR II raised its win rate from 41.4% to
44.6% and the attacker's hull lost from 6.5% to 8.9%; one extra Dreadnaught raised them to 44.9% and 7.9%. One LNR
II is worth about one Dreadnaught: it deals a little more damage, since it cannot be killed, but soaks up none. An LNR
I adds 2.3 points of defender wins, a KDY-150 1.7, and two LNR IIs with a KDY-150 4.5.

### 6.14 Starting tuning values

Collected in one place for phase 1. All of them are tuned by calibration.

| Value | Start |
|---|---|
| Range point | 100 m |
| Capital-ship speed per sub-light point | 60 m/s |
| Fighter speed per sub-light point | 100 m/s |
| Turn rate per turn point (maneuverability + admiral term 1-3; fighters maneuverability + 1) | 10 degrees/s |
| Admiral speed bonus | up to +15% |
| Arc width (adjacent arcs overlap at the corners) | 100 degrees |
| Tractor stop rule | combined power at least the target's sub-light rating |
| Squadron launch interval | 5 s + 0-7 s |
| Damage-control interval | 50 s |
| Ion disruption of engines | 30-50 s |
| Gravity-well radius | 15 km |
| Precision (subsystem) fire | 70% hull damage, 3x critical chance on the chosen system |
| Power-routing switch time | 5 s |
| Battle time limit | 15 min |
| Planetary battery cover | 10 km beyond the planet's surface |
| Planetary battery damage | 0.35 x sqrt(attack strength) per second, a salvo every 3 s |

## 7. Making ships feel different

### 7.1 From data to behaviour

Ship identity is derived, not authored. A small committed tuning table holds only what the records lack (model,
silhouette scale, vertical coverage exceptions); everything else comes from `CAPSHPSD.DAT` and `FIGHTSD.DAT`.

| Record data | Behaviour it drives |
|---|---|
| Weapon grid per arc | Default facing (bow, broadside, omni), which arcs bear, which approach suits the ship |
| Weapon kind mix | Default priorities (laser-heavy ships hunt fighters, turbolaser ships hunt capitals, ion ships disable) |
| Weapon ranges | "Optimal" range: the CC-9600 (75) outranges everything, the Bulk Cruiser (35) has to close |
| Sub-light and maneuverability | Speed, turn rate, how quickly a ship can swing a new arc onto a target, whether it can stand off |
| Shield, hull, recharge rates | Shield-heavy ships (Assault Frigate, CC-9600, Bulwark) recover between exchanges; hull-heavy ships (Victory, Imperial Star Destroyer) wear down steadily and lose output as they do |
| Damage control | In-battle repair of critical hits |
| Hangars | Carrier behaviour and the Carrier role |
| Troop capacity | The transport tag: defenders' favourite target |
| Tractor power and range | Pinning, measured against the target's speed |
| Gravity-well projector | The interdictor tag and the escape-denial circle |
| Hyperdrive and backup | Spool time, and how many hits strand the ship |
| Detection | Accuracy at long range and point-defence reaction time (it has no other battle use) |

### 7.2 Archetypes

Classes fall into archetypes that set default doctrine and the tactical symbol used when zoomed out. The facing
column is computed from the grid (anti-capital firepower, with lasers at 1/6, in the fore arc against the stronger
broadside).

| Archetype | Classes | Default doctrine | How it plays |
|---|---|---|---|
| Bow-gunned line | Imperial Star Destroyer, Imperial II, Victory II, Strike Cruiser, Super Star Destroyer; the Alliance's Bulwark | Line, Bow-on, Direct, capitals first | Point it at what you want dead. It turns slowly, so a flank or stern attack is the real danger |
| Broadside line | Mon Calamari Cruiser, Liberator, Dauntless, Assault Frigate, CC-9600, both Dreadnaughts, Bulk Cruiser; Victory (either way) | Line, Broadside, Left or Right Hook, capitals first | Sail past and rake; engage two targets at once; never let anything sit astern of the Assault Frigate or Victory, which have no aft guns |
| Picket | Corellian Corvette, Corellian Gunship, Lancer, Carrack | Screen the line, bombers then fighters | Fast, omni-armed with lasers, fragile; keeps fighters off the big ships and dies quickly to turbolasers |
| Escort frigate | Nebulon-B | Screen, Bow-on toward threats | Bow turbolasers plus lasers all round, two squadrons, detection 30 |
| Carrier | Alliance and Imperial Escort Carriers | Carrier, launch immediately | Six squadrons and only lasers to defend itself; losing it strands fighters |
| Interdictor | Interdictor Cruiser, CC-7700 | Picket on the enemy's escape side, fighters first | The net: weak guns, a gravity well, and the first thing the enemy wants dead |
| Transport | Medium and Bulk Transport, Galleon, Star Galleon, Assault Transport | Reserve, withdraw at 50% | The invasion's weak point; the Assault Transport is fast enough to punch through |

### 7.3 Two fleets, two styles

The data gives the factions different toolkits without any extra rules.

- **The Empire pins and pounds.** Its line ships put 1.3 to 2.25 times as much firepower in the bow as in either
  broadside, carry more hull than shield (an Imperial Star Destroyer has 2,750 hull to 300 shield), have stronger
  tractors on average (1.4 against 1.0), and carry fighters inside the line ships themselves. Its TIE Fighters,
  Interceptors and Bombers are cheap, unshielded and cannot jump on their own. The Imperial game is to hold the
  enemy in place (Interdictor, tractors, hyperdrive focus) and close bow-on, while keeping the carriers its TIEs
  depend on alive.
- **The Alliance strikes and leaves.** Its cruisers are broadside ships (69% of Alliance anti-capital firepower is
  in the broadsides, against 56% for the Empire), with better damage control (22 against 17 on average) and
  maneuverability. All of its fighters have shields and hyperdrives. The Alliance game is to hook across the enemy's
  line, disable with ion, finish with bombers, and jump before the Empire can close the net.
- **Matched pairs** give mirrored tools with different handling: Mon Calamari Cruiser against Imperial Star
  Destroyer, CC-7700 against Interdictor, Bulwark against Super Star Destroyer, the two Escort Carriers.

### 7.4 Every class in one line

| Class | Identity |
|---|---|
| Mon Calamari Cruiser | Broadside cruiser with ion in every arc and the best damage control in the game (50); rakes with both sides and shrugs off critical hits |
| Bulk Cruiser | Cheap, short-ranged (35) turbolaser hull; must close, and soaks fire while it does |
| Assault Frigate | Quick (5/2) shield brawler (600, recharge 30) with no aft guns: hook in, rake, keep moving |
| Nebulon-B Frigate | The escort that leads with its nose: bow turbolasers, lasers all round, two squadrons |
| Alliance Escort Carrier | Six squadrons, lasers only, slow (2/1): the ship the enemy is hunting |
| Corellian Corvette | Fast omni-laser picket with no backup hyperdrive: one critical hit traps it |
| Medium Transport | Two regiments and the slowest hyperdrive (100) |
| Bulk Transport | Six regiments, unarmed, sub-light 1: escort it or keep it out of the fight |
| Corellian Gunship | The fastest ship in the game (10/4) with long-reach lasers (25); hull 300 |
| Alliance Dreadnaught | Old turbolaser broadside ship with one squadron; dependable line filler |
| CC-7700 Frigate | The Alliance's gravity well, behind a laser screen; keep it alive and un-ionized |
| Bulwark Battlecruiser | The Alliance's Imperial-style bow ship: 600 bow turbolasers, 1,100 shields, ten squadrons, tractor 6 |
| Liberator Cruiser | Broadside carrier-cruiser: six squadrons and heavy ion broadsides |
| CC-9600 Frigate | The sniper: the longest turbolaser reach (75) and quick for its size (6/2) |
| Dauntless Cruiser | The heaviest Alliance broadsides (400 per side, half ion) with damage control 45; slow to turn |
| Strike Cruiser | A small Star Destroyer whose ion range matches its turbolasers (60); tractor 3 |
| Lancer Frigate | 150 lasers in every arc: the purest anti-fighter screen, useless against capital ships |
| Interdictor Cruiser | Gravity well, 600 shields, a laser screen; the anchor of an ambush |
| Carrack Light Cruiser | Fast (6/2) laser cruiser with token turbolasers and good damage control (30) |
| Victory Destroyer | Turbolasers only, bow and broadsides almost equal, no aft guns, thin shields (200) |
| Imperial Star Destroyer | The hammer: 200 bow points (half ion), six squadrons, tractor 3, slow to turn |
| Super Star Destroyer | 700 bow points, twelve squadrons, nine regiments, tractor 6; crawls (2/1) and repairs poorly (20) |
| Assault Transport | The fastest hyperdrive (50), sub-light 8, 150 bow lasers: delivers its regiment through a screen |
| Galleon | Unarmed two-regiment transport |
| Victory II Star Destroyer | A Victory with triple the shields (600), ion cannons and aft guns: sturdy at any angle |
| Imperial II Star Destroyer | The heaviest regular hammer: 400 bow points, 800 shields |
| Star Galleon | Three regiments behind light lasers |
| Imperial Escort Carrier | Six squadrons: the TIEs' only way home |
| Imperial Dreadnaught | Identical to the Alliance Dreadnaught in every figure |

| Fighter | Identity |
|---|---|
| A-wing | Interceptor: the fastest (11) and most agile (8) craft, weak guns, no torpedoes; the best trench-run survivor |
| X-wing | Multirole: 8 lasers, 4 torpedoes, shields, hyperdrive |
| Y-wing | Bomber: ion and 8 torpedoes; strips shields and finishes the job |
| B-wing | Heavy bomber: 6 ion, 8 lasers, 12 torpedoes, the toughest shields (9); slow and clumsy (7/3), needs an escort |
| TIE Fighter | Cheap swarm: no shields, no hyperdrive |
| TIE Interceptor | Dogfighter: fast and agile (10/8) |
| TIE Bomber | Bomber: ion and 8 torpedoes, no shields, slow |
| TIE Defender | Elite: 10 lasers, 10 torpedoes with the longest torpedo range (10), shields and hyperdrive |

### 7.5 Identity on screen and in sound

Silhouette and size come first: a Star Destroyer's wedge, a Mon Calamari's rounded hull and a Nebulon-B's split
profile should read when zoomed out. Faction bolt colours and engine glows follow the original. Heavy batteries
sound slow and deep, lasers chatter, ion fire crackles. The original Alliance and Empire voice packs give each side
its own radio chatter.

## 8. Strategic stakes and auto-resolve

### 8.1 What a battle changes in the campaign

The result records are narrow (section 11.1), and that is enough. Everything that makes a battle matter already
exists in the strategic game:

| Outcome | Strategic consequence (existing rules) | Tactical lever |
|---|---|---|
| A capital ship is destroyed | Months of production lost; popular support drops in the sector | Withdraw thresholds, damage control, screening |
| A ship survives damaged | Hull and critical systems carry over; repairs are slow and need a shipyard | Pull damaged ships back early; target subsystems you want the enemy to carry home |
| Hyperdrive hits | With every drive hit the ship cannot withdraw this battle; afterwards a ship that lost its primary drive travels on its slower backup until repaired | Subsystem focus Hyperdrive to pin a fleet before killing it |
| A carrier is destroyed | Its squadrons overflow to the system and lose their transport | Strike carriers first; recover fighters before jumping |
| A transport is destroyed | Troops aboard are lost, so the invasion fails | Escort transports; defenders prioritise them |
| A ship carrying characters is destroyed | They are set adrift and may be captured, with an injury roll that can be fatal | Protect flagships; hunt the enemy admiral |
| One side withdraws | The strategic game routes its survivors to a retreat system | Interdiction and tractors decide whether withdrawal is possible |
| The Death Star is destroyed | The Empire's largest investment is gone | The fighter run (section 6.12) |

### 8.2 What the tactical layer must honour

- **Each side's withdraw threshold.** The vote's "time" field is a strength ratio: a side withdraws when its
  strength falls below that fraction of the enemy's (`SideParams::withdrawThreshold`, checked against
  `SideStrength` in `DefaultCommander::Update`). The Claude AI votes 0.35, or 0 (fight to the end) at its own
  capital. A human's Simulate Results and Take Command votes store 0, and Retreat stores 10000 (leave at once). The
  battle commander uses the threshold as its fleet-wide withdrawal trigger, so strategic personalities carry into
  battle unchanged. Because a human's vote carries 0, a human side commanded by the AI (Simulate Results, Observe
  Battle, Get Immediate Result) uses `BattleOptions::humanSideAiWithdrawThreshold` (0.33) instead.
- **Each side's retreat time and can-retreat flag** from `QueryRetreat`. In the original 3D mode the retreat time
  was the hyperdrive spool time; the new mode uses it the same way (section 6.10). A side with no reachable retreat
  system cannot withdraw at all, as on the Battle Alert window.
- **Commanders**: the best admiral present, first and second commanders, and character skill values, exactly as
  `BattleSetup` provides them.

### 8.3 Calibration

Taking command should reward skill mostly by preserving ships and making kills decisive, not by turning hopeless
battles into victories. The legacy auto-resolve is a reference, not ground truth: it has defects of its own
(section 8.4), and its abstraction (no movement, best arc only) makes it a weak target for a positional simulator
even if fixed. The real gates are plausibility and campaign balance. Targets for the calibration harness (section
11.7), to be tuned in phase 1:

- **Plausible outcomes.** With default doctrines on both sides, the winner tracks fleet strength: the side with at
  least twice the simulator's own strength estimate (firepower and durability, fighters included) wins at least 90%
  of battles. No side is wiped out by a stalemate, and the Death Star behaves as section 6.12 describes.
- **Skill shows as ships saved.** A scripted "expert" doctrine (focus fire, sensible priorities, timely
  withdrawal) preserves 20-40% more capital-ship value than the default, but flips fewer than 5% of lopsided
  battles (strength ratio below 0.5).
- **The campaign stays balanced.** AI-against-AI tournaments (`tools/run_ai_tournament.py`) keep side win rates
  and campaign lengths within their current spread.

Results (2026-10-19): plausibility and campaign pacing are met; the legacy-agreement and expert-doctrine targets
were revised, since both proved wrong rather than unmet (section 15).

### 8.4 Defects in the legacy auto-resolve

The legacy auto-resolve wiped out a stalemated side that could not withdraw, fired the Death Star exactly once, let the
Empire always shoot first, and had several smaller scaling and seeding errors; the new simulator avoids all of them,
and the vote-flow defects were fixed in place (phase 0). The full table is in this file's git history before ea47c15.

### 8.5 One simulator, and retiring the legacy one

The new simulator runs every battle: live when a human takes command, and headless for Simulate Results, for
AI-against-AI battles, and to finish a battle abandoned with Get Immediate Result. A headless battle is the same
simulation stepped as fast as possible, with the AI commanding both sides. Watched and simulated battles then
follow one set of rules.

**Done (2026-10-19, ea47c15).** The legacy simulator, its tables and the 3D stubs were deleted, and
`SpaceBattleEngine` is the sole engine.

## 9. The opponent

The tactical AI uses the same command interface as the player: doctrines, orders and signals, and nothing else.
It gets no stat bonuses at any difficulty.

- **Composition reading.** At deployment it tags both fleets and chooses each group's role and priorities from
  matchup rules. If the enemy has carriers and it has bombers, it sends a strike against the carriers. If the enemy
  has bombers, it screens with laser-heavy ships. Defending against an invasion, it prioritises transports. If it is
  outmatched and an enemy Interdictor holds a gravity well, it focuses the Interdictor.
- **Running estimate.** It keeps a Lanchester-style estimate of both sides' strength from firepower and durability
  (the legacy simulator summed attack ratings only), reassesses every few seconds and moves groups between Line,
  Reserve and Screen.
- **Withdrawal.** The fleet withdraws by the strategic threshold (section 8.2). Personalities that value their
  ships also withdraw single badly damaged capital ships early.
- **Difficulty** changes how often it reassesses (every 5 s, 2 s or 1 s), whether it uses commander signals, and
  how good its fire allocation is (Spread at the lowest level, overkill-aware at the higher ones).
- **Personalities.** The strategic AI's personality picks a tactical temperament: aggressive personalities favour
  Close range and Concentrate fire; cautious ones favour Stand-off and early withdrawal.
- **Observe Battle.** The player's side can be handed to the same AI at any time, starting from the player's
  doctrines. Unlike the original, it withdraws: by the doctrines' thresholds per ship, and by the fleet-withdrawal
  default for the side (section 8.2).
- **Headless.** The same AI commands both sides when a battle is simulated (section 8.5), so its quality sets the
  quality of every auto-resolved battle in the campaign.

## 10. Presentation

### 10.1 Camera

- The battle plane is seen from above or at an angle. Two views are planned:
  - **Top-down**, the default while commanding, and the first to be built: maximum readability, and each ship is
    a single sprite rotated to its heading.
  - **Angled**, at one fixed pitch (around 50 degrees), for a more cinematic look. Ships are drawn from frames
    pre-rendered at many headings (section 10.5).
- Pan with WASD, edge scroll or a middle-drag; zoom with the wheel; rotate the view in 45-degree steps (Q/E).
  Double-click a ship to centre it, F (or the top bar's Follow button) to follow it, Home to frame the whole
  battle. Following shows a banner naming the ship; the wheel zooms on it, edge scrolling leaves it alone, and
  F with another ship selected switches to that ship. Camera bookmarks replace the original's memorized position.
- **Semantic zoom**: zoomed out, ships become faction-coloured tactical symbols by archetype (section 7.2), and
  flights become one marker with a craft count. Zoomed in, the ship art takes over.
- **Tactical overlay** (Tab): firing arcs of the selection, weapon range rings, target lines (solid when firing,
  dashed when assigned but not yet in range or arc), gravity-well circles, tractor links, fighter strike paths and
  projected positions a few seconds ahead. Off by default when zoomed in, on by default when paused.

### 10.2 Screen layout

```
+------------------------------------------------------------------------------------+
| || 1x 2x 4x 8x | 03:42 | ALLIANCE ######...  71%    EMPIRE ####.....  52% | Signals 2/3 |
+--------------+---------------------------------------------------+-----------------+
| TASK FORCES  |                                                   | EVENTS          |
| 1 Line  [][] |                                                   | 03:40 Liberty:  |
| 2 Screen [][]|                   battle view                     |   shields down  |
| 3 Carrier [] |                                                   | 03:31 Red Sqn:  |
| FLIGHTS      |                                                   |   strike away   |
| Red   Strike |                                                   |                 |
| Blue  Escort |                                                   |                 |
+--------------+---------------------------------------------------+-----------------+
| TF1 Line - Mon Calamari Cruiser x2, Assault Frigate                                 |
| Range Optimal | Approach Left Hook | Facing Broadside | Power Weapons | Withdraw <25%  |
| Targets 1 Interdictors 2 Carriers 3 Damaged 4 Nearest | Alloc Auto | Subsystem Hyperdrive |
| [Attack] [Move] [Hold] [Escort] [Withdraw]                                          |
| Intent: Liberty -> Interdictor "Hammerhead": priority #1, port arc, ~35 s to disable |
+------------------------------------------------------------------------------------+
```

Ship cards in the roster show hull, shields and the five critical systems as small icons, with the same
remaining / maximum convention the strategic ship status already uses.

### 10.3 Visual language

- Bolts follow the original's colour code: red for the Alliance, green for the Empire, blue for ion fire. They
  are visible, travelling projectiles, but the hit is decided when the shot is fired (section 11.3), so the
  visual only flies to an outcome the simulation already knows.
- Shield hits flash an arc of the shield bubble at the impact point; once shields are down, hits scorch and spark
  the hull, and critical hits show (engine glow sputtering, a dark turret section, venting).
- Fighters are drawn slightly above the plane with soft shadows, so strikes visibly sweep over their targets.
- Tractor beams are a blue cone; a gravity well is a faint shimmering circle on the plane; a ship spooling its
  hyperdrive shows a brightening engine glow and a countdown ring.
- Explosions scale with hull size and leave debris; a destroyed capital ship ejects escape pods, a nod to the
  characters aboard being set adrift (section 5.6).

### 10.4 Two looks

- **Tactical** (zoomed out, and whenever the tactical overlay is on): clean silhouettes tinted by faction over a
  faint grid, the briefing-room hologram look, which is the look the player commands in. It needs good silhouettes
  but little surface detail.
- **Detailed** (zoomed in, follow camera, spectating): lit ship art with engine glow over a starfield, nebulae and
  the system's planet.

### 10.5 Art: where the ships come from

Ships are sprites rendered offline in Blender: by default from the original TACTICAL.DLL meshes, which are crude
(about 60 vertices) but keep the original's compressed relative scale, and for a few classes from scripted remodels
(`tools/build_tactical_models.py`, recipes in `tools/tactical_model_recipes/`) checked against the encyclopedia
renders. Sources, the build, the pack format, remodel status and how to remodel a class are in
`docs/tactical_sprites.md`. Rendered and extracted art is never committed: it stays in ignored `work/hd/tactical/`.
When the pack is missing or lacks a class, the game draws procedural silhouettes per archetype
(`src/game/battle_ui/silhouettes.h`), so the mode always runs. Starfield and nebulae are procedural.

### 10.6 Sound

TACTICAL.DLL embeds 66 sound effects, and VOICEFXA.DLL and VOICEFXE.DLL hold 153 Alliance and 132 Empire voice
clips (task-force and squadron reports and order acknowledgements). Like other original assets, they are read
from the player's installation at runtime. Which clip belongs to which event is decoded in `docs/battle_audio.md`
and `src/game/battle_ui/battle_sounds.{h,cpp}`; the original plays no sound at the muzzle, only at the target, and
drops a voice line while another is speaking. The original tactical music (`MDATA.401`-`MDATA.406`) is missing
from this installation, so battles use the strategic music tracks until it can be found in another edition.

## 11. Engineering

### 11.1 Where it plugs in

The seam is the battle flow, snapshot builder and results adapter (`src/game/app/battle_flow*.cpp`,
`battle_handoff.cpp`, `battle_results.cpp`, `battle_engine_space.*`):

- **Choosing the mode.** The battle-type vote already distinguishes Take Command (vote state 2) from Simulate
  Results and Retreat (state 1). A live battle starts only when a local human voted Take Command and a tactical
  presenter is available (a real video driver, not the dummy one the smokes use). Otherwise vote 2 is treated as 1 and the battle runs headless. AI
  controllers always vote 1, so `REB_SIM`, the AI tournament, `tools/modern_smoke.py` and the LAN smokes run
  headless battles.
- **Keep the strategic game alive.** The original destroyed the strategic game for the battle and reloaded it
  from a pre-battle autosave file in the install's `SaveGame` directory. The
  new mode does not copy this: the strategic game stays in memory, held (`m_hold = 1`, as in state 3), and the
  pre-battle file is never written. The sim harness, the LAN host and checkpoints all assume the step driver
  exists.
- **Inputs.** `BattleSetup::Convert` builds a `BattleSnapshot`: battle parameters, each capital ship's figures,
  4 x 3 weapon grid and weapon ranges, squadrons with carriers, torpedoes and torpedo range, characters with leadership and combat skill
  and commander flags, passengers, and the Death Star. Tactical handles are still stored in each strategic object
  (`SetDataField04`). Two battle-level inputs matter in particular: each side's withdraw threshold (the vote "time",
  see section 8.2) and each side's retreat time and can-retreat flag (`QueryRetreat`).
- **Outputs.** The engine fills strategic result records (`src/game/app/battle_result_records.h`), which
  `BattleResults::Apply` already applies: the outcome, each capital ship's hull and its five critical-system values,
  the Death Star's hull and survival, each squadron's size and carrier, and hangar overflow. Characters, troops and
  special forces get no results, as now; their fate follows the existing strategic rules once their ship is
  destroyed or withdrawn. Retreat destinations are also strategic (`GameRoot::PrepareBattle` auto-routes the
  loser). A stalemate or time limit reports both sides alive (section 6.11).
- **Get Immediate Result** finishes the battle headless in the same simulator, from its live state, with the AI
  commanding both sides. It does not rebuild units from strategic objects: ships that jumped remain gone, and
  shields, torpedoes and the Death Star charge retain their current values. The engine never reports "unfinished".

### 11.2 Code layout

- `src/game/space_battle/` (SDL-free simulator, namespace `reb::battle`): `snapshot.*` (inputs) and `results.h`
  (outputs); `ship_profile.*` and `tuning.h` (per-class profiles, section 7.1); `battle.*`, `battle_state.*` and the
  `sim_*.cpp` files (the fixed-step world); `doctrine.*` and `commands.h` (the command model); `commander.*` (the
  opponent and headless commander, section 9); `battle_save.*` (save blocks and replays); `view_model.h` and
  `battle_view_model.cpp` (what the renderer and HUD read, so they never touch simulation internals).
- `src/game/battle_ui/` (SDL-free): camera, HUD, input and audio mapping, producing a draw list.
- `src/port/native/space_battle/` (SDL): the presenter, draw-list executor, sprite pack, text atlas, input and audio
  playback. Each directory has its own CMake glob in `cmake/native.cmake`. Section 15 lists the rest.

### 11.3 Simulation rules that keep it testable

- A fixed 20 Hz tick, counted as an integer. Rendering interpolates between ticks. Nothing reads the wall-clock
  `Stopwatch` the original 3D branch used.
- One gameplay RNG (a small PCG generator, not `std::` distributions, whose output differs between standard
  libraries), separate from cosmetic randomness, saved with the battle.
- Hits are decided when a battery fires; the projectile on screen only flies to that known outcome. Simulation cost
  then does not depend on how many bolts are drawn.
- Stable iteration by unit ID, explicit tie-breaks, and the build's existing `-ffp-contract=off -fno-fast-math`.
  Same-platform determinism is required; matching bytes across platforms is not (LAN is host-authoritative,
  11.5).
- Squadrons are units with per-craft positions for flight and visuals, and with spatial hashing for fighter
  searches. Budget: 2 ms per tick at 60 capital ships and 100 squadrons (about 1,200 craft) on the host.

### 11.4 Saving mid-battle and replays

The original state-0x14 save path was removed with the old 3D scaffolding. The strategic game stays alive, so a
mid-battle save is the normal strategic save plus a versioned tactical block containing:

- the snapshot (battle parameters and units, with the handle-to-object mapping);
- a full state checkpoint (every unit's transform, velocity, orders, group, hull, shields, critical systems,
  battery cooldowns, hangar and torpedo state, withdrawal timers; the Death Star charge; the RNG; the tick);
- the order log since the checkpoint.

The order log doubles as the replay format: the snapshot plus every order is enough to re-simulate a battle on the
same build. Replays make bug reports reproducible and allow golden tests.

### 11.5 Two-human battles (later phase)

LAN play is host-authoritative: the host runs the strategic game and the battle simulator, and the guest sends
orders and receives snapshots (docs/multiplayer.md). Battles follow suit. The host simulates; the guest sends
commands and receives tactical state at 10-20 Hz, which it interpolates for its own view model. This needs new
message kinds (`TacticalBegin`, `TacticalCommand`, `TacticalState`, `TacticalEnd`) and a protocol bump; the build
ID check already guarantees identical builds. Command phases (section 5.7) are the default, so neither player
pauses the other. On a disconnect the host finishes the battle headless, as Get Immediate Result does.

### 11.6 Rendering stack

The renderer uses SDL, as the rest of the game does, but on a 2D plane it does not need SDL's GPU API:

- **`SDL_Renderer` with sprites** (recommended). The game already presents through `SDL_Renderer` (Metal on macOS).
  While a battle is live, the tactical presenter takes over that renderer and draws at full output resolution and
  aspect instead of the 640 x 480 palette canvas: sprites rotated or picked by heading (section 10.5), additive
  blending for bolts, glows and shields, `SDL_RenderGeometry` for arcs, rings and beams, and text from a FreeType
  glyph atlas (FreeType is already vendored). It needs no shaders, no new SDL subsystem and no new toolchain, and
  it runs on every renderer the game supports.
- **`SDL_GPU`** (later, only if wanted). SDL 3.4's GPU API (Metal, D3D12, Vulkan) would allow a free 3D camera with
  real-time lighting from small mesh files instead of sprite atlases. The vendored SDL has it but compiles it out
  (`SDL_GPU OFF` in `cmake/native.cmake`), and it needs per-backend shaders (MSL, SPIR-V, DXIL, for example through
  SDL_shadercross). Nothing in this design requires it; the view model keeps the option open.
- The tactical HUD is drawn in true colour by the port layer rather than as palette-canvas `CoolWindow`s: at modern
  resolutions over the battle view the 8-bit UI would look out of place and would need its layout rebuilt anyway.
  The game side provides the HUD model, so the HUD stays testable headless.
- Sound uses the existing software mixer, with pan and volume computed per voice from the camera position, since
  the emulated `IDirectSound3DListener` is a stub.

### 11.7 Testing

- **Unit tests** (new `native_space_battle_*` CTest executables): arcs and facing, damage and shields, ion and
  critical systems, hangars and rearming, tractors and wells, withdrawal, allocator overkill rules, outcome
  encoding, save round-trips.
- **Scenario tests**: scripted battles with expected outcomes or outcome bands. For example: "a Mon Calamari
  cruiser on Auto facing deals more damage per minute than the same ship held Bow-on"; "a fleet inside an active
  gravity well cannot jump"; "a carrier's flight recovered before the jump survives, one left out is reported
  stranded"; "a stalemate leaves both fleets intact".
- **Golden replays**: a recorded order log must re-simulate to the same final state hash on the same platform.
- **Calibration**: `REB_SIM` AI matches dump the snapshot of every battle they fight. A harness runs each snapshot
  through the simulator headless over many seeds and checks the section 8.3 plausibility targets.
- **AI tournaments** (`tools/run_ai_tournament.py`) for campaign balance.
- **Renderer smoke**: a `REB_UI_SMOKE`-style check enters a scripted battle, renders a few frames with a real video
  driver and saves review captures (tactical and detailed looks, overlay), as the HD review captures do.

## 12. Plan

Phases 0-2, 4 and 5 are done (vote-flow fixes, headless simulator, playable top-down battles, legacy retirement),
as are planetary defenses (section 6.13). Phase 4, angled view and polish, delivered heading frames, view rotation,
the Death Star set piece, presets, options and accessibility (colour-blind-safe faction palettes, scalable HUD).

| Phase | Scope | Exit criteria |
|---|---|---|
| 3. Art and sound — sound done; art uses the original meshes, two classes remodelled | Model recipes for the remaining classes (`docs/tactical_sprites.md`); effects; backdrop planet | Every class at IoU of at least 0.85 or reviewed; review captures checked |
| 6. Two-human battles | Host-authoritative streaming, guest view model, command phases, disconnect handling | `tools/lan_smoke.py --battle` extended to a live battle |
| Later | Named-character abilities; a free 3D camera through `SDL_GPU` if wanted | |

## 13. Risks

The pre-implementation risk table is retired now that the design is built; see section 15 and git history.

## 14. Decisions

Settled (2026-10-19):

1. **A 2D battle plane**, seen top-down or at an angle, rather than true 3D movement.
2. **Retire the legacy auto-resolve** rather than fix it; the new simulator runs every battle (section 8.5). Only
   the vote-flow defects are fixed in place (section 8.4).
3. **Pursue the design**; implementation starts with phase 0.
4. **Rendering**: `SDL_Renderer` with pre-rendered sprites.
5. **Art route**: Blender, rendering the original TACTICAL.DLL meshes by default, with recipe remodels for a few
   classes as a proof of concept (the Mon Calamari cruiser and the Imperial Star Destroyer); the rest are tracked
   in `docs/tactical_sprites.md`. The image-to-3D experiment was not run.
6. **Default time model**: real time with pause.
7. **Two-human battles**: out of the first release; LAN battles resolve headless.

## 15. Implementation status (2026-10-19)

The design is implemented: every battle now runs on the new simulator, live when a human takes command and headless
otherwise, and the legacy auto-resolve is retired.

| Area | Where | Notes |
|---|---|---|
| Simulator | `src/game/space_battle/` (`reb::battle`) | Pure C++, 20 Hz fixed step; deterministic per platform; saves, replays and state hashes. `reb_space_battle_log` prints a battle |
| Battle AI | `src/game/space_battle/commander.*` | Matchup planning, reassessment, temperaments from the strategic AI (Classic aggressive, Codex and human balanced, Claude cautious), signals, pursuit, withdrawal |
| Strategic seam | `src/game/app/battle_engine*`, `battle_handoff.cpp`, `battle_results.cpp` | `BattleSnapshot` (v2, with class and display names) in, `BattleResultSet` out |
| Live battles | `src/game/app/battle_live.*`, `battle_flow*.cpp` | Take Command plays the battle in the running campaign; Observe, Get Immediate Result, a pause menu, mid-battle saves; LAN battles resolve headless |
| Deployment | `src/game/space_battle/sim_setup.cpp`, `deployment_geometry.h` | Hull-aware ranks; per-side circles grow rearward only when needed; rigid drag shared with HUD |
| Battle screen | `src/game/battle_ui/`, `src/port/native/space_battle/` | SDL-free UI producing a draw list; SDL executor taking over the existing `SDL_Renderer`; top-down and angled views; command HUD; sound |
| Art | `tools/build_tactical_sprites.py`, `tools/tactical_model_recipes/` | Original meshes rendered to sprites in Blender; two classes remodelled; roadmap in `docs/tactical_sprites.md` |
| Sound | `src/game/battle_ui/battle_audio.*`, `docs/battle_audio.md` | The original effects and voices from TACTICAL.DLL and VOICEFX*.DLL, mapped by disassembly and transcription |
| Calibration | `docs/ai_matches.md` | Corpus of 1,937 real campaign battles, runner, calibration scripts, paired tournaments |

Deliberate departures from the text above, each explained where it is implemented:

- **World scale**: one range point is 150 m, not 100 m, and speeds and the battle plane scale with it, so that weapons
  reach more than two to five hull lengths of the original game's compressed ship sizes (§6.1).
- **Facing**: Auto facing counts a target fully only once it is 15 degrees inside an arc; otherwise the 10-degree arc
  corners made every ship angle 45 degrees off its target (§6.3).
- **Critical hits**: the chance is per point of hull damage, since the legacy per-hit chance was unusable with
  per-salvo hits (§6.5).
- **Ion**: disruption delays a jump but no longer resets the hyperdrive spool, and cannot refresh an engine outage
  indefinitely; resets made escape from an ion-heavy fleet impossible (§6.6).
- **Withdrawal**: the base spool is 30 s and the winning side pursues spooling ships (and tractor ships close on
  them). Without this, 86.5% of escaping ships began spooling at tick 0 and the loser lost 21% of its capital ships
  in contested battles against the legacy engine's 46%; it is now 39% (§6.10, §8.3).
- **Fighters and point defence**: fighter gun damage x0.5 and capital-ship point defence x0.25, so strikes last long
  enough to matter (§6.4, §6.7).
- **Stalemates** require meaningful accumulated damage to keep a battle going (§6.11). Hull worn off craft counts
  before any craft dies.
- **Docked squadrons** hold no space (2026-10-20, §6.8, §6.11). A garrison that flew home to rearm used to keep its
  side present indefinitely. A lone Y-wing squadron could not hurt a Carrack and docked whenever its torpedoes ran out,
  so that battle ended as a stalemate in the garrison's favour. In the held-first corpus (2,382 snapshots, 2 seeds),
  stalemates fall from 6.8% to 5.2% of runs and 98.4% of winners are unchanged; most of the flips are such garrisons.
- **Death Star**: the superlaser needs a completed station; a Death Star Shield facility blocks the trench run but not
  firing; surface batteries are off (§6.12).
- **Calibration targets** (§8.3): plausibility holds (the stronger side wins about 97% of battles where it is twice
  as strong; outmatched flips stay under 2%) and campaign pacing is within the legacy spread. Two targets proved
  wrong rather than unmet: agreement with the legacy winner settles around 84-85% because the legacy engine had the
  defects of §8.4, and the "expert doctrine preserves 20-40% more" target has no headroom because the battle AI
  already preserves about 95% of capital value; expert play gains about 1% on the contested subset.
- **Held first**: the side that held the system before the last arrival deploys beside its planet. Before 2026-10-20
  the snapshot named the side that arrived last, so most battles put the attacker beside the defender's planet and the
  planet's fighter garrison out in space; correcting it exposed, and fixed, garrison deadlocks and an attacker that
  never closed on a docked garrison (§4, §6.11).
- **Not built**: two-human live battles (§11.5; LAN battles resolve headless), drawn
  fighter strike paths (the view model has targets and velocities but no planned routes).
