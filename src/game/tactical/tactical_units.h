#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/game_string.h"
#include "core/layout.h"

// The tactical engine's unit records.
//
// Everything the facade registers (tactical_facade.h) is a node of the registry (DAT_006bc470) keyed by the handle it
// returns to the strategic game. The capital ships and fighters (0x5b05c0 / 0x5b9c60) are in the capital-ship and
// fighter modules; the characters and special forces / troops, the two small record classes below, are built by the
// facade itself (0x596c40 / 0x596df0).

// Base of all registered units (VT 0x66bed0, 0x1c bytes): an id node whose id is the handle. Constructor and destructor
// are in the ship-group module (0x5a6210 / 0x5a6280).
class TacticalUnit : public IdNode {
public:
    TacticalUnit();           // 0x5a6210
    virtual ~TacticalUnit();  // 0x5a6280 (scalar deleting destructor 0x5a6260)
    virtual void Virtual01() = 0;  // pure
};

// The two 8-byte members of a character / special-force record. Classes of the turn-sync module (destructors 0x5bdd00 /
// 0x5bdcc0); only their layout is needed here.
class TurnSyncChannelA {
public:
    virtual ~TurnSyncChannelA();  // 0x5bdd00 (scalar deleting destructor 0x596d40)
    void* m_data;                 // +0x04
};

class TurnSyncChannelB {
public:
    virtual ~TurnSyncChannelB();  // 0x5bdcc0 (scalar deleting destructor 0x596d20)
    void* m_data;                 // +0x04
};

// A character aboard a ship (VT 0x66bac0, 0x64 bytes).
class TacticalCharacterUnit : public TacticalUnit {
public:
    TacticalCharacterUnit(uint32_t family, uint32_t id, uint16_t spriteId, String name, int notAlliance, int isMajor,
                          int isFirstCommander, int isSecondCommander, float scaleA, float scaleB,
                          int shipHandle, int injured);  // 0x596c40
    virtual ~TacticalCharacterUnit();  // 0x596d80 (scalar deleting destructor 0x596d60)
    virtual void Virtual01() override;  // 0x5bd880

    TurnSyncChannelA m_channelA;  // +0x1c
    TurnSyncChannelB m_channelB;  // +0x24
    uint32_t m_objectFamily;        // +0x2c
    uint32_t m_objectId;            // +0x30 (the strategic object)
    uint16_t m_spriteId;          // +0x34
    String m_name;                // +0x38
    int m_notAlliance;            // +0x44
    int m_isMajor;                // +0x48
    float m_scaleA;               // +0x4c leadership / 10
    float m_scaleB;               // +0x50 combat / 20
    int m_isFirstCommander;       // +0x54
    int m_isSecondCommander;      // +0x58
    int m_shipHandle;             // +0x5c the tactical handle of the ship it is on
    int m_injured;                // +0x60
};

// A special-force unit or troop regiment aboard a ship (VT 0x66bad0, 0x4c bytes).
class TacticalSpecialForceUnit : public TacticalUnit {
public:
    TacticalSpecialForceUnit(uint32_t family, uint32_t id, uint16_t spriteId, String name, int notAlliance,
                             int shipHandle);  // 0x596df0
    virtual ~TacticalSpecialForceUnit();  // 0x596ec0 (scalar deleting destructor 0x596ea0)
    virtual void Virtual01() override;    // 0x5bd880

    TurnSyncChannelA m_channelA;  // +0x1c
    TurnSyncChannelB m_channelB;  // +0x24
    uint32_t m_objectFamily;        // +0x2c
    uint32_t m_objectId;            // +0x30
    uint16_t m_spriteId;          // +0x34
    String m_name;                // +0x38
    int m_notAlliance;            // +0x44
    int m_shipHandle;             // +0x48
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(TacticalUnit, 0x1c);
REB_ASSERT_SIZE(TurnSyncChannelA, 0x08);
REB_ASSERT_SIZE(TacticalCharacterUnit, 0x64);
REB_ASSERT_OFFSET(TacticalCharacterUnit, m_name, 0x38);
REB_ASSERT_OFFSET(TacticalCharacterUnit, m_injured, 0x60);
REB_ASSERT_SIZE(TacticalSpecialForceUnit, 0x4c);
REB_ASSERT_OFFSET(TacticalSpecialForceUnit, m_shipHandle, 0x48);
#pragma clang diagnostic pop
