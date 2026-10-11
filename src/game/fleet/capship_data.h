#pragma once
#include "fleet/unit_data_decl.h"
#include "gameobj/child_iterator.h"

// Data block of the capital ship (VT 0x662738, 0x2c bytes, ctor 0x5586a0): the repair stat of CombatUnitData plus three
// saved words. Its address range (0x558460-0x5587e0) sits after Detector's; the capship module (0x5004e0) uses it from
// CapShip::CreateData; word20-word28 are accessed through CapShip.
class CapShipData : public CombatUnitData {
public:
    CapShipData();           // 0x5586a0
    virtual ~CapShipData();  // 0x5586c0 (scalar deleting destructor), 0x5586e0
    virtual int Read(istream* in) override;      // 0x5586f0
    virtual int Write(ostream* out) override;    // 0x558740
    virtual int Checksum(Crc32* crc) override;   // 0x558790
    // Slot 4 (IsValid) is 0x5587d0, a jump to the shared "return 1" stub.

    // Written by the tactical battle simulation after a fight (CapShip::SetDataWord20/24/28, 0x58d540/0x58dc10); a nonzero
    // word20 makes the master view count the whole bombardment modifier as lost (CapShip::GetBombardmentModifierLoss).
    int m_word20;  // +0x20
    int m_word24;  // +0x24
    int m_word28;  // +0x28, set to shield strength minus its lost share minus the shield damage of the fight
};

// Iterator over the troop, capital-ship and fighter families 0x10-0x1f (VT 0x662730, ctor 0x558480, scalar deleting
// destructor 0x558460, destructor 0x558500).
class CombatUnitIterator : public ChildIterator {
public:
    CombatUnitIterator(GameObjNotifier* parent, uint32_t stateFilter);  // 0x558480
    virtual ~CombatUnitIterator();                                      // 0x558500
};

// GNPRTB 0x800-0x805 (the "rules 8" block; 0x804/0x805 are the repair levels CapShip and FightSquad pick by FastRepair).
// 0x800 is the largest critical-hit (CH) value a capital-ship subsystem can take; 0x801-0x803 drive RollCriticalHitType:
// a random bonus range added to the damage percentage, the threshold it must reach, and the range of the roll that
// picks the hit type from table 5.
extern int g_gnprtb0800;  // 0x6bb388
extern int g_gnprtb0801;  // 0x6bb378
extern int g_gnprtb0802;  // 0x6bb380
extern int g_gnprtb0803;  // 0x6bb384
extern int g_gnprtb0804;  // 0x6bb374
extern int g_gnprtb0805;  // 0x6bb37c

// Binds the six parameters for the current difficulty; every binder runs even after a failure. 1 if all succeed.
int BindGnprtb08();  // 0x558510

// Rolls PercentChance(percent) into *roll; always returns 1.
int RollPercentChance(int percent, int* roll);  // 0x558680
