#pragma once
#include <cstdint>

#include "core/layout.h"

// Plain result records the tactical engine hands back to the strategic game when a battle ends (read through the
// facade in tactical_facade.h). Field meanings are inferred from how the battle flow (app/battle_results.cpp)
// applies them.

// What the battle came to (TacticalGame +0x66c, filled by the simulator at +0x10 when it finishes).
struct BattleOutcome {
    int m_sideAFinal;    // +0x00 a side-1 unit survived the final phase (used only when the first test is inconclusive)
    int m_sideAAlive;    // +0x04 a side-1 unit is alive when the battle ended
    int m_sideBFinal;    // +0x08
    int m_sideBAlive;    // +0x0c
    int m_unk10;         // +0x10
    int m_unfinished;    // +0x14 non-zero when a live battle stopped without a winner
    int m_unk18;         // +0x18
};

// One capital ship's final state (0x2c bytes). The floats are integer values held as floats by the engine.
struct CapitalShipResult {
    int m_unk00;
    int m_unk04;
    int m_unk08;
    int m_unk0c;
    float m_hull;                 // +0x10 hull points left
    int m_speedCH;                // +0x14
    int m_shieldRechargeCH;       // +0x18
    int m_hyperdriveDamage;       // +0x1c total hyperdrive critical hits, split over the primary and backup drive
    int m_tractorBeamCH;          // +0x20
    int m_weaponRechargeCH;       // +0x24
    int m_unk28;                  // +0x28
};

// One fighter squadron's final state (0x20 bytes).
struct FightSquadResult {
    int m_unk00;
    int m_unk04;
    int m_unk08;
    int m_unk0c;
    int m_unk10;
    int m_unk14;
    int m_squadSize;  // +0x18 squadron size left
    int m_unk1c;
};

// A Death Star's final state (0x14 bytes).
struct DeathStarResult {
    int m_unk00;
    int m_unk04;
    int m_destroyed;  // +0x08 zero when the Death Star survived
    float m_hull;     // +0x0c hull points left
    int m_unk10;
};

// A character's final state: just the handle it was registered under (0x20 bytes, the rest is zero). The battle flow
// builds it and then ignores it.
struct CharacterResult {
    int m_handle;
    int m_unk04[7];
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(BattleOutcome, 0x1c);
REB_ASSERT_SIZE(CapitalShipResult, 0x2c);
REB_ASSERT_SIZE(FightSquadResult, 0x20);
REB_ASSERT_SIZE(DeathStarResult, 0x14);
REB_ASSERT_SIZE(CharacterResult, 0x20);
REB_ASSERT_OFFSET(CapitalShipResult, m_hull, 0x10);
REB_ASSERT_OFFSET(CapitalShipResult, m_hyperdriveDamage, 0x1c);
REB_ASSERT_OFFSET(FightSquadResult, m_squadSize, 0x18);
REB_ASSERT_OFFSET(DeathStarResult, m_hull, 0x0c);
REB_ASSERT_OFFSET(DeathStarResult, m_destroyed, 0x08);
#pragma clang diagnostic pop
