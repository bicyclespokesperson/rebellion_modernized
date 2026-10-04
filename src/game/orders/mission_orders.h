#pragma once
// The mission orders (0x4f4560-0x4f5510): orders that start a mission with the characters of a team, and the order
// 0x242 that is a bare OrderBase. Order kinds 0x240-0x242 come from the factory table (0x51f4b0); the three factories
// of this file are registered there.
//
//   0x242  Order242        OrderBase without fields, VT 0x65d068; its command (ctor 0x540c40) is in another module
//   ----   MissionOrder    abstract base (VT 0x65d0c0, 0x60 bytes): the team of characters, the mission and the
//                          validation shared by the two leaves; its command is the MissionOrderCommand (kind 0x250)
//   0x240  MissionOrder240 VT 0x65d118, two non-virtual queries 0x4f52c0 / 0x4f5380 on the candidates
//   0x241  MissionOrder241 VT 0x65d170, the order the move orders build for characters and special forces
#include <cstdint>

#include "core/layout.h"
#include "gameobj/object_key.h"
#include "mission/mission_events.h"
#include "orders/order_base.h"

class Order242 : public OrderBase {
public:
    Order242();           // 0x4f4560
    virtual ~Order242();  // 0x4f45a0 (deleting 0x4f4580)

    virtual int Read(istream* in) override { return OrderBase::Read(in); }      // 1, shared 0x53d0f0
    virtual int Write(ostream* out) override { return OrderBase::Write(out); }  // 2, shared 0x53dd90
    virtual int GetKind() override { return 0x242; }                            // 3, shared 0x57db20
    virtual Command* CreateCommand() override;                                  // 18, 0x4f4620 (a 0x50-byte command)
};

class MissionOrder : public OrderBase {
public:
    MissionOrder();           // 0x4f4690
    virtual ~MissionOrder();  // 0x4f4720 (deleting 0x4f4700)

    virtual int Read(istream* in) override;                       // 1, 0x4f4790
    virtual int Write(ostream* out) override;                     // 2, 0x4f47f0
    virtual Command* CreateCommand() override;                    // 18, 0x4f4990: a MissionOrderCommand
    // Queues the command: replaces the order's commands with one command carrying the team, the characters and the
    // request. Fails with {1, 1} if no command can be made.
    virtual int CheckCommand(StatusCode* status) override;        // 19, 0x4f4a00
    // Checks the team's characters, the mission and the capacity of the mission's location, and writes the findings
    // (side, mission key, destination keys) into the queued command.
    virtual int CheckDestination(StatusCode* status) override;    // 20, 0x4f4b60

    // Validates the order, then lets the mission manager list the characters that can take part into *out.
    int CollectCandidates(StatusCode* status, ObjectKeyValueSet* out);  // 0x4f4850

    int m_unk44;                     // +0x44
    int m_unk48;                     // +0x48
    NullableKey m_missionKey;        // +0x4c, 0 for none
    ObjectKeyValueSet m_unusedSet;   // +0x50, never read or written
    ObjectKeyValueSet m_characters;  // +0x58
};

class MissionOrder240 : public MissionOrder {
public:
    MissionOrder240();           // 0x4f5200
    virtual ~MissionOrder240();  // 0x4f5240 (deleting 0x4f5220)

    virtual int GetKind() override { return 0x240; }  // 3, shared 0x57c4a0

    // Ask the mission manager for the characters of the team and of the mission that qualify (the two differ in the
    // manager function used, 0x5420d0 / 0x5422f0). Both clear *out first and return 0 without a mission manager.
    int CollectCandidatesA(ObjectKeyValueSet* out);  // 0x4f52c0
    int CollectCandidatesB(ObjectKeyValueSet* out);  // 0x4f5380
};

class MissionOrder241 : public MissionOrder {
public:
    MissionOrder241();           // 0x4f5440
    virtual ~MissionOrder241();  // 0x4f5480 (deleting 0x4f5460, a second copy at 0x4f5454)

    virtual int GetKind() override { return 0x241; }  // 3, 0x4f5500
};

Order242* CreateOrder242();              // 0x4f45b0
MissionOrder240* CreateMissionOrder240();  // 0x4f5250
MissionOrder241* CreateMissionOrder241();  // 0x4f5490

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(Order242, 0x44);
REB_ASSERT_SIZE(MissionOrder, 0x60);
REB_ASSERT_OFFSET(MissionOrder, m_missionKey, 0x4c);
REB_ASSERT_OFFSET(MissionOrder, m_characters, 0x58);
REB_ASSERT_SIZE(MissionOrder240, 0x60);
REB_ASSERT_SIZE(MissionOrder241, 0x60);
#pragma clang diagnostic pop
