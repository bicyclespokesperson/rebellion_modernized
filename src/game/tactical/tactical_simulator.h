#pragma once
#include <cstdint>

#include "app/player_controller_decl.h"
#include "core/layout.h"
#include "tactical/tactical_types.h"

// The headless battle simulator (DAT_006bc4a8, 0x274 bytes): the auto-resolve engine. It runs the same capital-ship and
// fighter unit objects as the 3D battle, one fixed step per frame, without any rendering (TacticalGame::Update with the
// abstract flag set steps it with 1.0). Its constructor (0x5af5f0) and most unit handling belong to the capital-ship
// module; the part reconstructed here is the step and the end-of-battle bookkeeping (0x5ae460-0x5af4b0).

// One side's state inside the simulator (the object at +0x24c / +0x250; class and vtable live in the capital-ship module).
struct SimulatorSide {
    uint8_t m_pad00[0x24];
    GameTime m_battleTime;  // +0x24 vote time of this side's controller
    uint8_t m_pad28[0xb0 - 0x28];
    int m_unkB0;            // +0xb0
};

struct HeadlessSimulator {
    float m_elapsed;            // +0x00 simulated time
    uint8_t m_pad04[0x0c];
    BattleOutcome m_outcome;    // +0x10
    uint8_t m_pad2c[0x44 - 0x2c];
    int m_battleKind;           // +0x44
    uint8_t m_pad48[0x24c - 0x48];
    SimulatorSide* m_sides[2];  // +0x24c
    uint8_t m_pad254[0x274 - 0x254];
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(HeadlessSimulator, 0x274);
REB_ASSERT_OFFSET(HeadlessSimulator, m_outcome, 0x10);
REB_ASSERT_OFFSET(HeadlessSimulator, m_battleKind, 0x44);
REB_ASSERT_OFFSET(HeadlessSimulator, m_sides, 0x24c);
REB_ASSERT_OFFSET(SimulatorSide, m_battleTime, 0x24);
REB_ASSERT_OFFSET(SimulatorSide, m_unkB0, 0xb0);
#pragma clang diagnostic pop
