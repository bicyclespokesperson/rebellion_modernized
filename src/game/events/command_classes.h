#pragma once
// The concrete event/command classes at 0x562c70-0x5638e0 (names by kind; their purpose is read from what they call):
//   LoyaltyEvent373 (VT 0x6632a8)  shifts a system's loyalty by a stored delta toward side 1 (System::AdjustLoyalty)
//   SystemCommand225 (0x6632d8)    command on a system: checks are System slots 111/112, execution is the helper 0x58c1e0
//   SystemCommand224 (0x663300)    same with slots 113/114 and the helper 0x58d800
// plus the helper objects they build (rules/loyalty_rule.h, 0x58b960-0x58e570).
#include <cstdint>

#include "core/layout.h"
#include "events/event_base.h"
#include "gameobj/game_hooks.h"
#include "gameobj/status_code.h"

class LoyaltyEvent373 : public PlainEvent {
public:
    LoyaltyEvent373();                                 // 0x562c70
    LoyaltyEvent373(const LoyaltyEvent373& other);     // 0x562cc0
    virtual ~LoyaltyEvent373();                        // 0x562cb0

    virtual LoyaltyEvent373* Clone() override;         // 1: 0x562cf0
    virtual void Read(istream* in) override;            // 2: 0x562de0
    virtual void Write(ostream* out) override;          // 3: 0x562e00
    virtual void Checksum(Crc32* crc) override;         // 4: 0x562e20
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x562e50
    virtual int CheckValid(StatusCode* status) override;    // 7: 0x562f00
    virtual int Execute() override;                    // 8: 0x562fb0
    virtual int GetKind() override;                    // 9: 0x562d60, 0x373
    virtual int GetPriority() override { return 0x100; }  // 10: 0x48bc60

    int m_delta;  // +0x40
};

class SystemCommand225 : public Command {
public:
    SystemCommand225();                                  // 0x563090
    SystemCommand225(const SystemCommand225& other);     // 0x5630e0
    virtual ~SystemCommand225();                         // 0x5630d0

    virtual SystemCommand225* Clone() override;          // 1: 0x563100
    virtual void Read(istream* in) override;              // 2: 0x5635f0
    virtual void Write(ostream* out) override;            // 3: 0x563600
    virtual void Checksum(Crc32* crc) override;           // 4: 0x563610
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x5631f0
    virtual int CheckValid(StatusCode* status) override;    // 7: 0x5632c0
    virtual int Execute() override;                      // 8: 0x563390
    virtual int GetKind() override;                      // 9: 0x563170, 0x225
};

class SystemCommand224 : public Command {
public:
    SystemCommand224();                                  // 0x563490
    SystemCommand224(const SystemCommand224& other);     // 0x5634e0
    virtual ~SystemCommand224();                         // 0x5634d0

    virtual SystemCommand224* Clone() override;          // 1: 0x563500
    virtual void Read(istream* in) override;              // 2: 0x5635f0 (shared with 0x225)
    virtual void Write(ostream* out) override;            // 3: 0x563600
    virtual void Checksum(Crc32* crc) override;           // 4: 0x563610
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x563620
    virtual int CheckValid(StatusCode* status) override;    // 7: 0x5636f0
    virtual int Execute() override;                      // 8: 0x5637c0
    virtual int GetKind() override;                      // 9: 0x563570, 0x224
};

// Factory functions registered in the event factory table (0x51ef80).
void* CreateLoyaltyEvent373();   // 0x562d70
void* CreateSystemCommand225();  // 0x563180
void* CreateSystemCommand224();  // 0x563580

// The next object after `object` in the character families 0x30-0x3c (0x562a50, a thiscall on the object).
GameObjNotifier* __thiscall NextCharacterSibling(GameObj* object);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(LoyaltyEvent373, 0x44);
REB_ASSERT_OFFSET(LoyaltyEvent373, m_delta, 0x40);
REB_ASSERT_SIZE(SystemCommand225, 0x50);
REB_ASSERT_SIZE(SystemCommand224, 0x50);
#pragma clang diagnostic pop
