#pragma once
#include <cstdint>

#include "gameobj/status_code.h"

// One loyalty-shift rule (0x1c bytes, no vtable): "systems of the family range that are controlled by m_controlSide
// (0 = any side), and whose loyalty toward side m_unitSide is below m_threshold, get their loyalty moved by
// m_amount (negated unless m_direction is 1)". The rules code builds one out of the GNPRTB/SDPRTB 0x14xx parameters
// whenever a system changes control, a fleet arrives or leaves, and so on; ApplyLoyaltyRuleToSector (0x518b20) then
// posts a loyalty event (class LoyaltyEvent373) for every system of the sector that meets it. A rule with m_amount 0
// does nothing. The galaxy seeding of a new game uses the same rules for its initial loyalty.
class LoyaltyRule {
public:
    LoyaltyRule();                                         // 0x562ae0
    ~LoyaltyRule();                                        // 0x562b30 (empty)
    LoyaltyRule& operator=(const LoyaltyRule& other);      // 0x562b80

    void Reset();                                          // 0x562ba0
    void CopyFrom(const LoyaltyRule& other);               // 0x562c30
    // Copies the family range into *out; returns out.
    IntPair* GetFamilyRange(IntPair* out);      // 0x51bf10
    // The original takes the range as an IntPair by value: two dwords on the stack.
    void SetFamilyRange(uint32_t first, uint32_t second);  // 0x55de00

    int m_controlSide;      // +0x00, 0 or 1-3
    IntPair m_familyRange;  // +0x04, default the systems 0x90-0x98
    int m_unitSide;         // +0x0c, 1 or 2 (the side whose loyalty is compared)
    int m_threshold;        // +0x10, default 100
    int m_direction;        // +0x14, 1-3; anything but 1 negates the amount
    int m_amount;           // +0x18, 0 disables the rule
};
