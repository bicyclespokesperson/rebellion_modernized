#pragma once
// STAND-IN declaration of the command class (base 0x54eee0 / 0x54f2c0, events subsystem), to be replaced when that
// module is reconstructed. Commands are the executable side of an order: an order builds one per team member, checks
// it and runs it. Only the part the order code needs is declared.
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "gameobj/game_obj.h"
#include "gameobj/status_code.h"

class Command : public AvlNode {
public:
    virtual ~Command();
    // Copies the request into the command (+0x40).
    void SetRequest(const ActivityRequest* request);  // 0x54f3e0
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
};
