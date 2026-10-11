#pragma once
#include "fleet/capship.h"

// The two concrete capital-ship classes. They add no state and no events: each supplies its family, the thin slot wrappers
// over CapShip's bodies, and the "new empty object" factory of slot 57. The wrappers of the two classes are
// identical code, so the linker folded them: CapitalShip's (0x5000c0-0x500130) serve both vtables.

// Family 0x14 (VT 0x65d650, secondary ObjectBase VT 0x65d64c, 0x68 bytes).
class CapitalShip : public CapShip {
public:
    CapitalShip();           // 0x4fff80
    ~CapitalShip() override;  // 0x4fffc0 (scalar deleting destructor 0x4fffa0)

    uint32_t GetFamily() override { return 0x14; }                      // 1, 0x411dc0 (shared "return 0x14")
    int CreateViews(int controlKind, NotifyContext* ctx) override;      // 37, 0x5000c0
    GameObj* CreateViewObject() override;                               // 57, 0x500050
    int Read(istream* in) override;                                     // 5, 0x5000e0
    int Write(ostream* out) override;                                   // 6, 0x5000f0
    int Checksum(Crc32* crc) override;                                  // 7, 0x500120
    int Validate() override { return CapShip::Validate(); }             // 8, 0x500130 (a jump to ValidateState)
    int ApplyAll(NotifyContext* ctx) override;                          // 9, 0x500100
};

// Family 0x18 (VT 0x65d930, secondary ObjectBase VT 0x65d928, 0x68 bytes).
class DeathStar : public CapShip {
public:
    DeathStar();           // 0x500240
    ~DeathStar() override;  // 0x500280 (scalar deleting destructor 0x500260)

    uint32_t GetFamily() override { return 0x18; }                      // 1, 0x500380
    int CreateViews(int controlKind, NotifyContext* ctx) override;
    GameObj* CreateViewObject() override;                               // 57, 0x500310
    int Read(istream* in) override;
    int Write(ostream* out) override;
    int Checksum(Crc32* crc) override;
    int Validate() override { return CapShip::Validate(); }
    int ApplyAll(NotifyContext* ctx) override;
};

// Registry factories (families 0x14 and 0x18): a new empty object, or null if allocation fails.
GameObjNotifier* CreateCapitalShip();  // 0x4fffe0
GameObjNotifier* CreateDeathStar();    // 0x5002a0

// Iterator over family [0x14,0x18) (VT 0x65d924, destructor only).
class CapitalShipIterator : public ChildIterator {
public:
    CapitalShipIterator(GameObjNotifier* parent, uint32_t stateFilter);  // 0x5001b0
    ~CapitalShipIterator() override;                                      // 0x500230 (scalar deleting destructor 0x500190)
};

// Iterator over family [0x18,0x1c) (VT 0x65dc04, destructor only).
class DeathStarIterator : public ChildIterator {
public:
    DeathStarIterator(GameObjNotifier* parent, uint32_t stateFilter);                 // 0x5003d0
    DeathStarIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side);  // 0x500450
    ~DeathStarIterator() override;                                                     // 0x5004d0 (scalar deleting destructor 0x5003b0)
};
