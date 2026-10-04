#pragma once
// STAND-IN declaration of the command class (base 0x54eee0 / 0x54f2c0, events subsystem), to be replaced when that
// module is reconstructed. Commands are the executable side of an order: an order builds one per team member, checks
// it and runs it (they are what the event queue 0x54ea70 schedules). Only the part the order code needs is declared.
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "gameobj/game_hooks.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_key.h"
#include "gameobj/status_code.h"

// Base 0x54eee0 (VT 0x662038) plus the request holder 0x54f2c0 (VT 0x662088). Slots 1-4 and 10 onwards are filled by
// the concrete commands (move command 0x578ab0, VT 0x669558, ...).
class Command : public AvlNode {
public:
    virtual ~Command();
    virtual Command* Clone() = 0;                      // 1: a fresh copy, 0x50 bytes
    virtual int Virtual02() = 0;                       // 2
    virtual int Virtual03() = 0;                       // 3
    virtual int Virtual04() = 0;                       // 4
    virtual int IsChecked() { return 0; }              // 5
    virtual int CheckEnabled(StatusCode* status) = 0;  // 6: the object may take part (the move command's 0x578c00)
    virtual int CheckValid(StatusCode* status) = 0;    // 7: the command is allowed (0x578d00)
    virtual int Execute() = 0;                         // 8: carries it out (0x578f30)
    virtual int GetKind() = 0;                         // 9: the order kind (0x201 for the move command)

    // Copies the request into the command (+0x40).
    void SetRequest(const ActivityRequest* request);  // 0x54f3e0
    // If the request's argument is set, records it and the command kind in the command's context (+0x20).
    void StampContext();  // 0x54f3f0

    int m_flag;                 // +0x18, 1
    int m_unk1c;                // +0x1c
    NotifyContext m_context;    // +0x20
    ObjectKey m_key;            // +0x3c, the object the command acts on
    ActivityRequest m_request;  // +0x40
};

// The ordered set of commands an order owns (0x54ede0, VT 0x661ff8, over the event queue 0x54ea70 which is an
// OrderedTree whose +0xc flag is 1). Elements are Commands linked through their successor pointer (+0x10).
class CommandQueue : public OrderedTree {
public:
    CommandQueue();  // 0x54ede0
    virtual ~CommandQueue();
    virtual int IsLess(AvlNode* a, AvlNode* b) override;
    virtual void Virtual03() override;

    int m_unk0c;  // +0x0c

    Command* FirstCommand() const { return static_cast<Command*>(m_first); }
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(Command, 0x50);
REB_ASSERT_OFFSET(Command, m_context, 0x20);
REB_ASSERT_OFFSET(Command, m_key, 0x3c);
REB_ASSERT_OFFSET(Command, m_request, 0x40);
#pragma clang diagnostic pop
