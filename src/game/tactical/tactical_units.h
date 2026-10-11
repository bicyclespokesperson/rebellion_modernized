#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/game_string.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"

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
    ~TacticalUnit() override;  // 0x5a6280 (scalar deleting destructor 0x5a6260)
    virtual int Virtual01() = 0;  // the unit's kind: 1 ship, 3 fighter group, 4 capital-ship group, 5 Death Star, 7 character / special force

    void AssignHandle(int side);    // 0x5a6660 takes the next unit handle of the side
    void AssignGroupId(int side);   // 0x5a6680 takes the next group id of the side
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
    TacticalCharacterUnit() {  // built by TacticalCharacterTree::Read before it reads the fields
        m_channelA.m_data = nullptr;
        m_channelB.m_data = nullptr;
    }
    ~TacticalCharacterUnit() override;  // 0x596d80 (scalar deleting destructor 0x596d60)
    int Virtual01() override;  // 0x5bd880: 7

    void Checksum(Crc32* crc);  // 0x5bd7d0
    void Read(istream* in);     // 0x5bd890
    void Write(ostream* out);   // 0x5bd9a0

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
    TacticalSpecialForceUnit() {  // built by TacticalSpecialForceTree::Read before it reads the fields
        m_channelA.m_data = nullptr;
        m_channelB.m_data = nullptr;
    }
    ~TacticalSpecialForceUnit() override;  // 0x596ec0 (scalar deleting destructor 0x596ea0)
    int Virtual01() override { return 7; }  // 0x5bd880 (shared with TacticalCharacterUnit)

    void Checksum(Crc32* crc);  // 0x5bdd40
    void Read(istream* in);     // 0x5bddb0
    void Write(ostream* out);   // 0x5bde70

    TurnSyncChannelA m_channelA;  // +0x1c
    TurnSyncChannelB m_channelB;  // +0x24
    uint32_t m_objectFamily;        // +0x2c
    uint32_t m_objectId;            // +0x30
    uint16_t m_spriteId;          // +0x34
    String m_name;                // +0x38
    int m_notAlliance;            // +0x44
    int m_shipHandle;             // +0x48
};

// The simulator's trees of characters (VT 0x66c380) and of special forces and troops (VT 0x66c370) by handle: an IdAvlTree
// with an accessor for the first node.
class TacticalCharacterTree : public IdAvlTree {
public:
    TacticalCharacterTree() {}
    ~TacticalCharacterTree() override = default;  // 0x5bdaa0 (scalar deleting destructor 0x5a8860)
    virtual IdNode* FirstItem();                                 // 0x5f6590 (shared stub; the special-force tree has an inline copy)

    int CountItems();           // 0x5bdf20 (shared with TacticalSpecialForceTree)
    void Read(istream* in);     // 0x5bdab0 replaces the contents with the records of the stream
    void Write(ostream* out);   // 0x5bdba0
    void Checksum(Crc32* crc);  // 0x5bdbf0
};

class TacticalSpecialForceTree : public IdAvlTree {
public:
    TacticalSpecialForceTree() {}
    ~TacticalSpecialForceTree() override = default;  // 0x5bdf10 (scalar deleting destructor 0x5a8880)
    virtual IdNode* FirstItem() { return static_cast<IdNode*>(First()); }  // 0x5f6590 (shared stub)

    int CountItems() {          // 0x5bdf20 (the code of TacticalCharacterTree::CountItems)
        int count = 0;
        for (IdNode* node = FirstItem(); node != nullptr; node = static_cast<IdNode*>(node->m_successor)) ++count;
        return count;
    }
    void Read(istream* in);     // 0x5bdf40
    void Write(ostream* out);   // 0x5be030
    void Checksum(Crc32* crc);  // 0x5be080
};

// What the object manager hands an event handler: the payload is an array of 32-bit words.
struct TacticalEvent {
    void* m_unk00;
    int* m_args;  // +0x04
};

// Event handler of the 3D battle (cdecl; registered in the object manager's event table): a character's ship and
// injury state. args: 2 character handle, 6 ship handle, 8 / 9 injury flags.
void TacticalCharacterStatusEvent(TacticalEvent* event);  // 0x5bdc30
