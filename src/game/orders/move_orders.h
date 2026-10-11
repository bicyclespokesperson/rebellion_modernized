#pragma once
// The move, rename, scrap and ready-for-delete orders (0x53c040-0x53e0a0).
//
// Move (0x201) and Confirmed Move (0x202) are "group" orders over the move base 0x53c040: they own a list of
// sub-orders (a MessageTree at +0x44) and a status copy (+0x48). Whenever one of the checks runs, the group first
// rebuilds the list from the team: one 0x241 mission order for the characters at one location, then one 0x204
// per-object move for everything else. The checks and the execution are then forwarded to the sub-orders.
#include <cstdint>

#include "data/int_pair_node.h"
#include "gameobj/key_int_set.h"
#include "gameobj/object_key.h"
#include "orders/command_decl.h"
#include "orders/move_orders_decl.h"
#include "orders/order_base.h"
#include "orders/order_list.h"

class ObjectMoveOrder;

// 0x53c040, VT 0x6617f8, 0x4c bytes.
class MoveOrderBase : public OrderBase {
public:
    MoveOrderBase();           // 0x53c040
    virtual ~MoveOrderBase();  // 0x53c0b0, scalar deleting 0x53c090
    virtual int CheckEnabled(StatusCode* status) override;         // 6: 0x53c100
    virtual int CheckValid(StatusCode* status) override;           // 7: 0x53c1a0
    virtual int Execute(StatusCode* status) override;              // 8: 0x53c240
    virtual Command* CreateCommand() override { return nullptr; }  // 18: 0x6158b0

    // Rebuilds the sub-orders from the team, leaves the result in *status and in *m_status. Returns 0 on failure.
    int Expand(StatusCode* status);  // 0x53c4b0
    // After validation: every sub-order's travel time per team member, merged into `out` (key -> days).
    int CollectTransitDays(StatusCode* status, ObjectKeyValueSet* out);  // 0x53c2e0
    int CreateLists();   // 0x53c710
    int DestroyLists();  // 0x53c7d0
    // Moves the characters of `team` that sit at one location into a new mission order (*out, null if none).
    int BuildMissionOrder(ObjectKeyValueSet* team, MissionOrder** out, StatusCode* status);  // 0x53c810
    ObjectKey* GetTargetInto(ObjectKey* out);                                                // 0x53ca30
    // A per-object move for what is left of `team` (*out, null if the team is empty).
    int BuildMoveOrder(ObjectKeyValueSet* team, ObjectMoveOrder** out, StatusCode* status);  // 0x53ca50

    MessageTree* m_subOrders;  // +0x44
    IntPair* m_status;         // +0x48
};

// 0x53ce70, VT 0x661850.
class MoveOrder : public MoveOrderBase {
public:
    MoveOrder();           // 0x53ce70
    virtual ~MoveOrder();  // 0x53ceb0, scalar deleting 0x53ce90
    virtual int Read(istream* in) override { return OrderBase::Read(in); }  // 0x580fb0
    virtual int Write(ostream* out) override;                               // 0x53d010
    virtual int GetKind() override;                                         // 0x53cf30
};

// 0x53cf40, VT 0x6618a8.
class ConfirmedMoveOrder : public MoveOrderBase {
public:
    ConfirmedMoveOrder();           // 0x53cf40
    virtual ~ConfirmedMoveOrder();  // 0x53cf80, scalar deleting 0x53cf60
    virtual int Read(istream* in) override { return OrderBase::Read(in); }      // 0x580fb0
    virtual int Write(ostream* out) override { return OrderBase::Write(out); }  // 0x53d010
    virtual int GetKind() override;                                             // 0x53d000
};

// 0x53d020, VT 0x661900, 0x44 bytes: the move of the objects of one team (kind 0x204 has no menu record; the group
// orders create it).
class ObjectMoveOrder : public OrderBase {
public:
    ObjectMoveOrder();           // 0x53d020
    virtual ~ObjectMoveOrder();  // 0x53d060, scalar deleting 0x53d040
    virtual int Read(istream* in) override;                      // 0x53d0f0
    virtual int Write(ostream* out) override;                    // 0x53dd90
    virtual int GetKind() override;                              // 0x53d0e0
    virtual int IsListed() override { return 0; }                // 4: 0x6158b0
    virtual int CheckValid(StatusCode* status) override;         // 7: 0x53d100
    virtual Command* CreateCommand() override;                   // 18: 0x53d3c0
    virtual int CheckDestination(StatusCode* status) override;   // 20: 0x53d430

    // Validates, then fills the value of every member of the team's dependency set with the days it needs.
    int CollectTransitDays(StatusCode* status, ObjectKeyValueSet* out);  // 0x53d200
};

// 0x53d9b0, VT 0x661958, 0x50 bytes: the new name is the text.
class RenameOrder : public TextMessage {
public:
    RenameOrder();           // 0x53d9b0
    virtual ~RenameOrder();  // 0x53d9f0, scalar deleting 0x53d9d0
    virtual int Read(istream* in) override;                     // 0x53da80
    virtual int Write(ostream* out) override;                   // 0x53da90
    virtual int GetKind() override;                             // 0x53da70
    virtual int CheckEnabled(StatusCode* status) override;      // 6: 0x53daa0
    virtual Command* CreateCommand() override;                  // 18: 0x53db60
    virtual int CheckDestination(StatusCode* status) override;  // 20: 0x53dbd0
};

// 0x53dcd0, VT 0x6619b0, 0x44 bytes.
class ScrapOrder : public OrderBase {
public:
    ScrapOrder();           // 0x53dcd0
    virtual ~ScrapOrder();  // 0x53dd10, scalar deleting 0x53dcf0
    virtual int Read(istream* in) override { return OrderBase::Read(in); }      // 0x53d0f0
    virtual int Write(ostream* out) override { return OrderBase::Write(out); }  // 0x53dd90
    virtual int GetKind() override { return 0x200; }                            // 0x48b450
    virtual Command* CreateCommand() override;                                  // 18: 0x53dda0
};

// 0x53de10, VT 0x661a08, 0x44 bytes: asks for a destroyed object to be deleted (its command sets ReadyForDelete).
class ReadyForDeleteOrder : public OrderBase {
public:
    ReadyForDeleteOrder();           // 0x53de10
    virtual ~ReadyForDeleteOrder();  // 0x53de50, scalar deleting 0x53de30
    virtual int Read(istream* in) override { return OrderBase::Read(in); }      // 0x53d0f0
    virtual int Write(ostream* out) override { return OrderBase::Write(out); }  // 0x53dd90
    virtual int GetKind() override;                                             // 0x53ded0
    virtual Command* CreateCommand() override;                                  // 18: 0x53dee0
};

void* CreateMoveOrder();            // 0x53cec0, factory of kind 0x201
void* CreateConfirmedMoveOrder();   // 0x53cf90, kind 0x202
void* CreateObjectMoveOrder();      // 0x53d070, kind 0x204
void* CreateRenameOrder();          // 0x53da00, kind 0x203
void* CreateScrapOrder();           // 0x53dd20, kind 0x200
void* CreateReadyForDeleteOrder();  // 0x53de60, kind 0x205

// Moves the characters of `team` that are at the same location (the first one's) into `out`; the others stay.
int TakeCharactersAtOneLocation(ObjectKeyValueSet* team, ObjectKeyValueSet* out, StatusCode* status);  // 0x53cbc0
