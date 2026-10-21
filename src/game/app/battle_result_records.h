#pragma once
#include <cstdint>

// Strategic hand-back records used by BattleResults. The field layout preserves the existing strategic mapping.

// Side survival and withdrawal flags used to decide who holds the system after battle.
struct BattleOutcome {
  int m_sideAFinal; // +0x00 a side-1 unit survived the final phase (used only when the first test is inconclusive)
  int m_sideAAlive; // +0x04 a side-1 unit is alive when the battle ended
  int m_sideBFinal; // +0x08
  int m_sideBAlive; // +0x0c
  int m_unk10;      // +0x10
  int m_unfinished; // reserved; battles always finish in the simulator
  int m_unk18;      // +0x18
};

// One capital ship's final state. Hull values are integers represented as floats.
struct CapitalShipResult {
  int m_unk00;
  int m_unk04;
  int m_unk08;
  int m_unk0c;
  float m_hull;           // +0x10 hull points left
  int m_speedCH;          // +0x14
  int m_shieldRechargeCH; // +0x18
  int m_hyperdriveDamage; // +0x1c total hyperdrive critical hits, split over the primary and backup drive
  int m_tractorBeamCH;    // +0x20
  int m_weaponRechargeCH; // +0x24
  int m_unk28;            // +0x28
};

// One fighter squadron's final state.
struct FightSquadResult {
  int m_unk00;
  int m_unk04;
  int m_unk08;
  int m_unk0c;
  int m_unk10;
  int m_unk14;
  int m_squadSize; // +0x18 squadron size left
  int m_unk1c;
};

// A Death Star's final state.
struct DeathStarResult {
  int m_unk00;
  int m_unk04;
  int m_alive;  // +0x08 zero when the Death Star has no hull left
  float m_hull; // +0x0c hull points left
  int m_unk10;
};

