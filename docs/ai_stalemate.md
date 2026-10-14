# Classic AI territory stalemate

The headless HQ-only match with seed 3, small galaxy, and difficulty 1 reached the same
77 Alliance / 12 Empire / 11 neutral split at ticks 20,001, 25,001, and 30,001 in a
single uninterrupted run. Coruscant remained Imperial and the Alliance HQ remained
intact. The saved game at tick 30,001 reports day 15,001. A continuation from the
tick-20,001 checkpoint remained at the same split through tick 70,001. The plateau
therefore begins without a save/load restart.

## What the AI is doing

Temporary order tracing from the tick-25,001 checkpoint showed both AIs advancing and
submitting valid orders. In 200 ticks, the Alliance sent movement and character mission
orders. Some character missions targeted Coruscant, but no Alliance movement order in
that sample targeted it. The strategic model still had 13 Alliance fleet records.

`AiPosture::CanTakeSystem` (`src/game/ai/ai_posture.cpp`) judges one fleet at a time. It
requires positive assessed enemy strength and a fleet strength strictly above 120% of
that strength, followed by ship and defense checks. The original `REBEXE.EXE` routine at
`0x4AE150` has the same positive-strength branch and the same 12/10 comparison. This
threshold is original game logic, not a change made for AI-vs-AI support.

At tick 25,001, the Alliance AI assessed Coruscant's enemy strength as 14,099. Its
strongest individual fleet was 16,125, below the required 16,918. None of its 13 fleets
passed `CanTakeSystem` for Coruscant, although their summed strength exceeded 100,000.
Two fleets passed the predicate at tick 20,001, when Coruscant's assessed strength
was 12,899; neither was at Coruscant then. By tick 25,001, the Imperial assessment had
risen enough to exclude both. This prevents fresh combat assignments to Coruscant at
the later checkpoint. It does not explain the earlier failed opportunity by itself.

A checkpoint at tick 20,001 shows the Alliance's sector-24 posture assigned fleet
`08004346` to Coruscant (`90000109`), although the fleet was still at `90000105`.
At tick 25,001 that fleet remained at `90000105`, still marked for Coruscant; another
fleet was assigned to a different system in the sector. `FindIdleFleet` skips assigned
fleets, and `OrderAssignedFleets` processes a combat assignment only once the fleet's
current system equals its assigned target. A replay from tick 15,001 to 20,001 recorded
six valid proposed move orders to Coruscant for *other* fleets. The AI therefore did
try to move toward the HQ earlier. `CheckValid` alone does not establish whether those
orders completed, why fleet `08004346` remained away, or whether a later battle or
interdiction intervened. The 200-tick sample beginning at 20,001 had no *new* move
orders to Coruscant.

Of the 12 remaining Imperial systems at tick 25,001, nine had zero assessed enemy
strength and were excluded by `CanTakeSystem`. Two other systems (`90000108` and
`9000010c`) passed that predicate for several Alliance fleets. Their sector-24
assessments had the enemy-control bit `4` but lacked the enemy-fleet bit `0x200`.
`AiPostureKind6::AssignTakeFleet` calls the free `ai_posture::AssignTakeFleet` helper, whose candidate query requires
`0x200`. These two systems never reach the predicate through that combat-target route.
The original `AiPostureKind6::AssignTakeFleet` at `0x4B6340` also passes the required
`0x200` mask to its sector query; this specific exclusion is inherited from the
original AI. Other routes, such as diplomacy or troop moves, have not been ruled out.

## Implication

The game is progressing through turns and battles; the HQ-only victory predicate is
also consistent with the saved state. The verified obstacles are the classic AI's
per-fleet takeover rule and its combat selector's requirement for an enemy fleet at
the target. Several fleets were ordered toward Coruscant earlier, but the saved state
shows at least one combat assignment remained away from its target while the strength
threshold subsequently closed. A future competing AI can be evaluated against this baseline. A potential
classic-AI improvement would explicitly gather fleets for the HQ objective and choose
targets with a separate rule for undefended enemy systems, but either change would
intentionally depart from the original strategy and needs gameplay testing.
