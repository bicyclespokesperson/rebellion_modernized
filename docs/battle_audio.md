# Battle audio: which original sound plays when

The installation ships the sounds of the original 3D tactical mode, now played by the native battle screen. This
document records how each recording was matched to an event, with its evidence and confidence. The result is the table
in `src/game/battle_ui/battle_sounds.{h,cpp}`. Nothing here is audio: only resource ids, code addresses and short
snippets of the transcribed speech. Everything below was found in `REBEXE.EXE` (image base 0x400000) with Capstone and
by transcribing the clips locally; there is no Ghidra project.

| Module | Holds |
|---|---|
| `TACTICAL.DLL` | 66 `WAVE` resources, ids 13000-13065: effects |
| `VOICEFXA.DLL` | 153 Alliance voice clips: ids 14001-14122 and 15133-15163 |
| `VOICEFXE.DLL` | 132 Empire voice clips: ids 15001-15132 |

Look a clip up in its module, not by id range: 15133-15163 continue the Empire numbering but live in `VOICEFXA.DLL`.

## 1. How the original plays sound

Every battle sound goes through one object, the tactical sound manager (singleton accessor 0x5ba8b0, request function
0x5ba980 `Play(manager, soundId, flag)`). All 102 direct call sites of the request function were enumerated with a
Capstone scan (section 6); no other code path plays battle sound (`REBEXE.EXE` imports only `DirectSoundCreate` and MCI,
the latter for music).

- **Sound ids.** The manager has 317 sound ids (0x00-0x13c). Its constructor (0x5bae60-0x5bbced) fills three tables,
  which `tools/battle_audio_tool.py table` replays from the executable: a variant count (1-4) per id, the WAVE ids of
  the variants (consecutive), and a category per id. A request picks one variant at random with the object manager's
  RNG.
- **Id ranges.** 0x00-0x1f are effects (`TACTICAL.DLL`, category 2). 0x20-0x99 are the Alliance voice clips in the order
  of 14001-14122, 0x9a-0x11d the Empire clips in the order of 15001-15132 (category 1), and 0x11e-0x13c the 31 extra
  Alliance clips 15133-15163 (category 3).
- **Only the human side talks.** Voice call sites choose between an Alliance and an Empire id from the player's side
  (`ObjectManager+0x9ec`), so a player never hears the enemy's reports. The same events have the same wording on both
  sides, except for the families that exist for one side only (section 3).
- **Channels.** Category 2 (effects) plays up to 5 sounds at once, category 1 (voices) one clip at a time, and
  category 3 (the Alliance Death Star trench-run chatter) has its own single channel, so it overlaps the reports. A
  request carries a flag; with it set the request is dropped when its channel is full, without it the request waits in
  a queue for 225 ms (effects), 8000 ms (voices) or 3000 ms (chatter) first. Every call site found passes the flag (a
  few through a register that holds 1), so in battle a new voice line is simply lost while another is speaking. The
  numbers are in `battle_sounds.h` (`kEffectPolyphony`, `kVoicePolyphony`, `VoiceChannel`); the reading of the update
  routine (0x5bab50) is from the disassembly, not from observation.
- **No gain, no pan.** A request carries only the id (and a "drop instead of queue" flag). The manager's volume
  constants are 0 and -30 hundredths of a dB, effectively unity. Positional audio is new in the native mode; every table
  row defaults to volume 1.0. Measured levels are within a few dB of each other (effects RMS -8 to -18 dBFS, voices -9
  to -14 dBFS, peaks at full scale), so no per-clip rebalancing is needed to start.

## 2. Effects (`TACTICAL.DLL`)

Sound id is the manager's id. "Seconds" and RMS (dBFS) are measured by `battle_audio_tool.py measure`.

| Id | WAVEs | Event (`BattleSound`) | Evidence | Seconds, RMS | Conf. |
|---|---|---|---|---|---|
| 0 | 13000-13003 | `ImpactLaserCapital` | `TacticalCapitalShip::PlayHitSoundA` 0x5b00a0 | 2.2, -14 | Medium |
| 1 | 13004-13007 | `ImpactTurbolaserCapital`, `ImpactTurbolaserFighter`, `TorpedoLaunch` | `PlayHitSoundB` 0x5b00c0, shared with fighters | 1.0-1.4, -9 | Medium (torpedo: Low) |
| 2 | 13008-13011 | `ImpactIonCapital` | `PlayHitSoundC` 0x5b00e0 | 1.0-1.7, -10 | Medium |
| 3 | 13012-13013 | `ImpactLaserFighterAlliance` | `TacticalFighter::PlayHitSoundA` 0x5ba0f0, side 0 | 1.3, -9 | Medium |
| 4 | 13014-13015 | `ImpactLaserFighterEmpire` | same function, side 1 | 1.2-1.3, -9 | Medium |
| 5 | 13016-13017 | `ImpactIonFighter` | `TacticalFighter::PlayHitSoundC` 0x5ba0b0 | 1.4-1.6, -10 | Medium |
| 6 | 13018 | `TractorTargetHeld` | `AddTractoringShip` 0x5b2570, when the first tractor source attaches | 2.6, -13 | High |
| 7 | 13019 | `TractorTargetReleased` | `RemoveTractoringShip` 0x5b25d0, when the last source detaches | 2.2, -14 | High |
| 8 | 13020 | `TractorBeamStart` | `BeginTractoring` 0x5b2520 | 1.9, -14 | High |
| 9 | 13021-13023 | `ExplosionSmall` | `PlayDestructionSound` 0x5b0170 (capital hull max < 1100) and 0x5ba0d0 (fighters) | 1.7-2.4, -14 | High |
| 10 | 13024-13026 | `ExplosionMedium` | 0x5b0170, hull max 1100-2000 | 2.4-2.9, -14 | High |
| 11 | 13027-13029 | `ExplosionLarge` | 0x5b0170, hull max above 2000 | 1.9-2.2, -13 | High |
| 12 | 13030-13032 | `ShieldHitTurbolaser` (also torpedo) | `HandleHitEvent56` 0x5b5f50 and `53` 0x5b5750, shield absorbs the damage | 1.0-1.2, -13 | Medium |
| 13 | 13033-13035 | `ShieldHitLaser` | `HandleHitEvent54` 0x5b5100, shield absorbs | 1.3-1.9, -14 | Medium |
| 14 | 13036-13038 | `ShieldHitIon` | `HandleHitEvent55` 0x5b5b60, shield absorbs | 1.3-1.4, -13 | Medium |
| 15 | 13039-13041 | `UnreferencedImpact0F` | no call site | 1.0-1.3, -14 | Low |
| 16 | 13042-13044 | `UnreferencedImpact10` | no call site | 1.0-1.7, -15 | Low |
| 17 | 13045-13047 | `HullHitTurbolaser` (also torpedo) | 0x5b5f50 and 0x5b5750, shield broken through | 2.0-2.6, -17 | Medium |
| 18 | 13048-13050 | `HullHitLaser` | 0x5b5100, shield broken through | 1.9-2.2, -15 | Medium |
| 19 | 13051-13053 | `HullHitIon` | 0x5b5b60, shield broken through | 0.8-1.2, -14 | Medium |
| 20 | 13054 | `SuperlaserImpact` | `TacticalDeathStarImpactEvent` 0x5ba520 | 4.7, -16 | High |
| 21 | 13055 | `EngineHumFighterEmpire` | `GetSoundId` of a fighter squadron, 0x5b8df0 | 2.8, -15 | Medium |
| 22 | 13056 | `EngineHumFighterAlliance` | same, side 0 | 2.3, -18 | Medium |
| 23 | 13057 | `EngineHumCapitalLarge` | `GetSoundId` of a capital ship, 0x5b0100, hull max above 2000 | 3.5, -11 | Medium |
| 24 | 13058 | `EngineHumCapitalMedium` | 0x5b0100, hull max 1100-2000 | 4.8, -11 | Medium |
| 25 | 13059 | `EngineHumCapitalSmall` | 0x5b0100, hull max below 1100 | 2.4, -17 | Medium |
| 26 | 13060 | `UnreferencedBeep` | no call site; a 0.35 s tonal beep | 0.3, -12 | Low |
| 27 | 13061 | `FighterLaunch` | 0x5a9170, called from the carrier's `LaunchNow` 0x5b8c70 | 1.7, -11, 8-bit | High |
| 28 | 13062 | `JumpInFighter` | arrival, world step 0x5a6d60 (timer 1500 ms), call at 0x5a713d | 1.8, -14, stereo 44.1 kHz | High |
| 29 | 13063 | `JumpOutFighter` | departure, per-ship step 0x5a6b40 (state 7, timer 4000 ms), call at 0x5a6bbe | 1.8, -14 | High |
| 30 | 13064 | `JumpInCapital` | same sites as 28, unit class 0 | 3.7, -15 | High |
| 31 | 13065 | `JumpOutCapital` | same sites as 29, unit class 0 | 3.2, -14 | High |

How the evidence reads:

- **Weapon identification.** Weapon kinds 0-3 are laser, turbolaser, ion cannon, torpedo. The four "shot" handlers
  0x5b1ea0 (`ImpactEventB`), 0x5b1f60 (C), 0x5b2080 (D) and 0x5b2c70 (E) are dispatched for event types 13-16 by the
  switch at 0x5a97c0 and post the hit effect `FUN_005c12d0` with kind 0, 1, 2 and 3. The four damage handlers (event
  types 17-20: 0x5b5100, 0x5b5b60, 0x5b5f50, 0x5b5750) pass kinds 0, 2, 1 and 3 to the wreck effect `FUN_005c13d0`.
  The registered event names agree with both orders: `SHIP_FIRELASERCANNON`, `SHIP_FIRETURBOLASER`,
  `SHIP_FIREIONCANNON`, `SHIP_FIRETORPEDO`, and `SHIP_TAKE_LASER_HIT`, `SHIP_TAKE_ION_HIT`, `SHIP_TAKE_TURBO_HIT`,
  `SHIP_TAKE_TORPEDO_HIT`. Two handlers are individually recognisable: 0x5b5b60 damages shields only (ion), and 0x5b5750
  is the only one without the fighter evasion divisor (torpedoes cannot be dodged). The torpedo and the kind-1 handler
  share their shield and hull sounds, which fits a heavy weapon, while laser and ion have their own. Hence Medium for the
  weapon names and High for the ids.
- **The shot sounds play on the target.** B, C and D call `PlayHitSoundA/B/C` on the ship that is hit, so ids 0-5 are what a
  ship sounds like when a laser, turbolaser or ion bolt reaches it; fighters differ by side for lasers. The original has
  no muzzle sound. The torpedo handler E plays sound B on the firing ship, so a torpedo launch sounds like a turbolaser
  hit (Low: the code is clear, the choice looks odd).
- **Shield versus hull.** In each damage handler the shield value is reduced first; if it stays at or above zero the
  "shield" id plays (12/13/14), otherwise the "hull" id (17/18/19).
- **Explosion size.** `PlayDestructionSound` 0x5b0170 compares `m_hullMax` (+0x3c8) with 1100 and 2000; every fighter
  squadron plays id 9.
- **Engine hums.** `GetSoundId` is a virtual (slot 27) whose capital-ship result is 0x17, 0x18 or 0x19 by the same hull
  thresholds and whose fighter result is 0x15 (Empire) or 0x16 (Alliance), which are sound ids 21-25. The hub plays it
  for its current unit (0x5a11e0 and 0x5a1770), keeps the sound handle and stops that sound later (0x5bab10). That is
  why they are named hums; the clips are steady noise or low drones of 2.3-4.8 s. Medium.
- **Hyperspace.** Ids 28 and 29 are one recording played forward and backward (13063 is the sample-exact time reversal of
  13062): a burst that decays, and a build-up that ends abruptly. Ids 30 and 31 are the capital-ship versions, 3.7 s and
  3.2 s. Departure is the per-ship routine that runs for ships in state 7, the jump state set by `TacticalShipJumpEvent`
  (4000 ms); arrival is the 1500 ms fly-in of every ship at the start of the battle, which is inferred from the
  world-step code. The reversed build-up for departure and the decaying burst for arrival agree with that reading.
- **Not played.** No code references ids 15, 16 and 26. They are plausible stand-ins for a torpedo impact and a UI beep, no
  more. The original also plays nothing for weapon fire at the muzzle, a shield collapsing, hyperdrive readiness,
  critical hits or ion disruption, collisions, tractor beam loops, or UI clicks.

## 3. Voices

Each family is a run of clips in the order: one generic "Ship ..." clip, then Task Force one to eight, then Red, Blue,
Green and Gold group. Alliance ids are `14001 + (soundId - 0x20)`, Empire ids `15001 + (soundId - 0x9a)`. Families
with a dash exist on one side only. Evidence is the id the original code plays (Alliance / Empire) and the transcript.

| Cue (`VoiceCue`) | Alliance WAVEs | Empire WAVEs | What it says, and when | Conf. |
|---|---|---|---|---|
| `FleetReady` | 14001 | 15001 | "The fleet is ready, Admiral." First entry of the battle-start queue (0x5adb86) | High |
| `ManeuverAck` | 14002-14014 | 15002-15014 | "Task Force three, acknowledged, beginning maneuver." Sites 0x5a2083 (ship), 0x5a30f2 (task force), 0x5a4f7e (group) | High |
| `ManeuverComplete` | 14015-14027 | 15015-15027 | "... reporting, maneuver complete, engaging targets." Sites 0x5cc769, 0x5cc738, 0x5c6a96 | High |
| `AttackAck` | 14028-14040 | 15028-15040 | "Order acknowledged, beginning attack" / "... acknowledged, engaging targets." Sites 0x5a1e4d, 0x5a27a4, 0x5a4dab | High |
| `OrdersComplete` | 14041-14053 | 15041-15053 | "Ship reporting, orders completed" / "Task Force two, requesting orders." Sites 0x5cc96b (ship) and 0x5c6c6a (group); no site found for task forces | Medium |
| `TargetDestroyed` | 14054-14066 | 15054-15066 | "Task Force one reporting, target destroyed." Played when a ship of the human side destroys an enemy (0x5a5cf0, victim in argument 1, killer in 2) | High |
| `ShipLost` | 14067-14079 | 15067-15079 | Alliance: "reporting they lost a ship" / "they're taking losses"; Empire: "ship destroyed" / "taking losses". Same function, human-side victim | High |
| `FightersLaunched` | - | 15080-15083 | "Red group reporting, all fighters launched, ready for orders." No call site; meaning from the clip | Medium |
| `FormationAck` | 14080-14087 | 15084-15091 | "Task Force one acknowledged, assuming formation." Sites 0x5a2bd0, 0x5a37ef, 0x5a38ff; task forces only | High |
| `MissionAck` | 14088-14100 | 15092-15104 | "Mission assignment acknowledged." Sites 0x5a1a9d, 0x5a2dcc, 0x5a49d1 | High |
| `CommandCircuitsFull` | 14101 | 15105 | "Command circuits are filled to capacity. No new task forces can be formed." Sites 0x5a281b, 0x5a2996, ... | High |
| `PersonnelEjecting` | 14102 | 15106 | "Personnel ejecting from fighters will have to recover them after the battle." No site found | Medium |
| `PersonnelAbandonShip` | 14103 | 15107 | "Personnel forced to abandon ship." Destroyed ship with a character aboard, 0x5a5eae | High |
| `FightersRecovered` | 14104-14107 | 15108-15111 | "Red group reporting, remaining fighters recovered." Last member of a group aboard, 0x5b8630, 0x5cfe6b | High |
| `FightersNotRecovered` | 14108-14111 | 15112-15115 | "... can't recover all their fighters, there's not enough docking space." 0x5cfbe7 | High |
| `WithdrawOrdered` | 14112 | 15116 | "All units reporting in, the fleet is preparing to make the jump into hyperspace." 0x5ad56b | High |
| `WithdrawBlockedShips` | 14118 | 15117 | "Some ships cannot make the jump into hyperspace" (left behind / scuttled), 0x5a0299 and 0x5a02ad | High |
| `WithdrawStrandsGroups` | - | 15118 | "Not all fighter groups have been recovered. A fleet withdrawal will leave them stranded." No site found | Medium |
| `WithdrawNoDockingSpace` | - | 15119 | "... shortage of docking space. A withdrawal will leave them stranded." No site found | Medium |
| `WithdrawalComplete` | 14119 | 15127 | Alliance: "The fleet is jumping into hyperspace. The withdrawal is complete." Empire: "The fleet has executed the jump into hyperspace. The withdrawal is complete." Battle-result queue, 0x5aa68b / 0x5aa6e1 | High |
| `EnemyWithdrew` | 14120 | 15128 | Alliance: "The Imperial fleet is withdrawing! We've won!" Empire: "The rebel fleet is withdrawing. The Empire is victorious." 0x5aa674 / 0x5aa6cd | High |
| `EnemyDestroyed` | 14121 | 15129 | Alliance: "All Imperial ships have been destroyed! That's it! We've won!" Empire: "The rebel fleet has been destroyed. The Empire is victorious." 0x5aa69b / 0x5aa6f1 | High |
| `FleetDestroyed` | 14122 | 15130 | Alliance: "Our fleet has been destroyed! The Empire has won the battle!" Empire: "Our fleet has been destroyed! The rebels have won the battle!" 0x5aa6af / 0x5aa705 | High |
| `DeathStarApproaching` | 14113 | - | "Stand by alert, Death Star approaching." Battle-start queue | High |
| `DeathStarFiringAtUs` | 14114 | - | "The Death Star is firing at us!" Played with `SuperlaserImpact` when the player's side is Alliance, 0x5ba588 | High |
| `DeathStarOpening` | 14115 | - | "... we have an opening to launch an attack on the Death Star." No site found | Medium |
| `DeathStarShielded` | 14116 | - | "Sensors have detected a Death Star shield generator. That means it will be impossible to attack." Battle-start queue | High |
| `DeathStarTooManyFighters` | 14117 | - | "... too many Imperial fighters to allow us to engage the Death Star." Battle-start queue | High |
| `DeathStarFireAck` | - | 15120 | "Death Star acknowledges. Target designated. Firing." 0x5a58af | High |
| `DeathStarRecharging` | - | 15121 | "Death Star reports, super laser recharging." No site found | Medium |
| `DeathStarCharged` | - | 15122 | "... super laser fully charged and ready to fire." `TacticalDeathStar::Charge` 0x5ba863, Empire player only | High |
| `DeathStarBlocked` | - | 15123 | "... line of fire is blocked by other vessels." No site found | Medium |
| `DeathStarUnderAttack` | - | 15124 | "... under attack by rebel fighters." No site found | Medium |
| `DeathStarAttackBrokenOff` | - | 15125 | "... rebel fighters have broken off their attack." No site found | Medium |
| `DeathStarDestroyed` | - | 15126 | "... it's been destroyed." No site found | Medium |
| `DeathStarUnshielded` | - | 15131 | "We do not have sufficient fighters to adequately shield the Death Star." 0x5adb55 | High |
| `DeathStarAttackContinues` | - | 15132 | "... rebel fighters are continuing to attack." 0x5d0038 | High |
| `DeathStarPilotChatter` | 15133-15147 | - | Fifteen trench-run lines ("All wings, report in", "Stay on target", "I'm hit!") | Medium |
| `DeathStarRunVoice` | 15148-15163 | - | Per group, four reports: ordered, begun, failed, succeeded; clips ordered Blue, Green, Gold, Red | High |

Notes:

- Task force clips are indexed by the group's task-force number minus one (hub function 0x5a0300), fighter groups by
  colour (0x5a02c0): Red 0, Blue 1, Green 2, Gold 3.
- The Death Star run lines are Alliance only because only Alliance fighters attack the Death Star. Per group the four
  lines are, in order: orders received, attack starting, attack failed, Death Star destroyed (for example Blue: "breaking
  off to attack Death Star", "attack on Death Star commencing", "attack failed", "target destroyed"). The original
  selects them at 0x5d0038, 0x5d00a9, 0x5d0392, 0x5d03ad and 0x5d03cd with ids 0x12d + 4 x slot + stage.
- Whisper's transcripts contained a few clear mishearings ("Census report" for the Alliance Death Star opening, "For
  Death Star" for the Empire's destruction report); the table relies on the position of the clip in its family, which
  is regular across both sides and across the code's `base + index` arithmetic, not on any single transcription.
- Not every id of a family has a call site that the disassembly could find. The families marked Medium are the ones
  where no site was found; their wording and position are unambiguous either way.

## 4. What is not covered

- Music (`MDATA.401`-`MDATA.406`) is absent from this installation; see the design, section 10.6.
- Anything the original never sounded (muzzle fire, shield collapse, critical hit) has no mapped clip. Ids 15, 16 and
  26 above are the only recordings left over.
- UI clicks: the shared button widget plays a click only when its owner calls `SetClickSound` (0x602840), and none of
  its 53 direct callers lies in the tactical code (0x590000 and up), so the original tactical window's buttons were
  silent as far as the disassembly shows. The beep at id 26 is not a click the original used.
- `ObjectManager+0x8c8` suppresses the hub's voice and hum requests and quadruples the voice patience; its meaning
  (an observer or fast-forward flag) was not determined.

## 5. Using the table

`battle_sounds.h` gives the resources; the audio comes from the installation at runtime through the existing software
mixer. A caller maps simulator events to `BattleSound` or `VoiceCue`, resolves a `SoundResource` (`EffectResource` takes a
variant roll, which the caller draws from presentation randomness, never from the simulator's generator), and loads the
WAVE from `ModuleFileName(resource.module)`. `native_battle_sounds` checks that the table assigns every clip of the
three modules exactly once.

## 6. Reproducing the mapping

`tools/battle_audio_tool.py` (uv script) prints the sound manager's id table from `REBEXE.EXE` (`table`), extracts the
WAVEs into the ignored `work/battle_audio/wav` (`extract`) and measures them (`measure`).
`tools/transcribe_battle_voices.py` transcribes the voice clips locally with faster-whisper. The play sites were found
by disassembling with Capstone: a scan of every direct `call 0x5ba980` in `.text` with the preceding instructions
gives the immediate id or the `base + index` arithmetic at each site, and the clip text in the table above gives the
meaning of each base.

## 7. Native battle playback

`battle_ui/battle_audio.*` diffs consecutive view models and accepted player commands. It emits positioned resource
cues without SDL or audio-device dependencies. The screen samples after every simulation tick as well as each rendered
view, so fast-forward does not skip a short torpedo flight. Native view models additionally expose shot fire/impact
ticks and source/target indices, effect start ticks, maximum hull, jump blockers and Death Star shield/run state.
These are presentation fields; save data, combat rules and the simulation RNG are unchanged. Variant selection uses a
presentation-only counter. Group voice indices follow group id order separately for each side's task forces and flights.

Impacts emit a weapon recording and a shield/hull recording at the target, with no ordinary muzzle sound. Torpedoes
also sound at launch. Resolved hull/shield effects supersede the shot's fire-time shield estimate; simultaneous hits
against one target can share that estimate when their individual damage outcomes are not exposed. Enemy destruction
reports use an arriving human-side shot's group when observable, and the generic ship report otherwise. Explosions use
maximum hull: below 1100 small, 1100 through 2000 medium, above 2000 large; fighters always use small. Tractor changes,
fighter launches, jump starts, superlaser impacts, human orders/losses, withdrawal, Death Star charge/trench-run changes
and battle opening/results produce their corresponding cues. Result voices trigger when the battle is decided,
before the live wind-down finishes, so the screen remains open while the report plays. Stalemates with both fleets present have no victory line.
Once the battle is decided, no other voice is generated, including the destruction report of the decisive kill and order
acknowledgements during the wind-down. The result line interrupts the voice that is speaking and clears the pending
lines, because the 2.5 to 4.5 s result clips must start at once to finish within the 5 s wind-down.

The scheduler reserves five effect slots and one voice slot, including trench-run reports in the single voice slot.
At most two instances of an effect family may play simultaneously, even when they use different WAVE variants. Voices
have two pending slots in FIFO order; repeated current/pending clips are coalesced, overflow drops the incoming line,
and pending lines expire after six real seconds,
including while paused. Effects never take voice slots. Missing resources do not occupy slots or delay later voices.
Camera projection supplies stereo pan and distance attenuation, refreshed during playback when the camera moves.

`port/native/space_battle/audio_player.*` loads the three DLLs lazily and optionally from the configured installation,
through the existing PE reader. PCM WAVEs play through `mwds::Voice` and the existing SDL software-mixer backend.
Missing DLLs, missing resources or unsupported WAVEs produce silence. Closing the presenter stops all battle voices
and clears pending lines. The existing options expose one shared `Volume` slider, with no separate voice-volume setting;
both categories respect that slider, including changes during playback. No additional dependency or required asset was
added.

Set `REB_BATTLE_AUDIO_TRACE=1` to log each started cue (module, WAVE id, category, handle, pan and gain), interrupted voices and unavailable
resources. For silent review use `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy`, `REB_SPACE_BATTLE_DEMO=<snapshot>` and
`REB_SPACE_BATTLE_REVIEW_DIR=<output directory>`; `REB_SPACE_BATTLE_DEMO_SECONDS=25` instead runs the demo in real time,
allowing clips to complete naturally. Run with the normal `REBELLION_CONFIG` setting. `native_battle_audio_cues`,
`native_battle_audio_mixing` and `native_battle_audio_resources` cover the cue sequences, channel/queue rules, camera
placement, optional resources and mixer lifecycle. The resources test accepts `--install-dir <directory>` for a local
decode-only check of every installed WAVE; its default fixtures contain only authored test data.
