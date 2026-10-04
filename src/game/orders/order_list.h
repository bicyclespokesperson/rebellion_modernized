#pragma once
// The tree of pending messages (0x536eb0-0x537030): an IdAvlTree of MessageBase objects keyed by message id. The game
// session keeps one for the messages waiting to be delivered (0x435b90 +0x24) and the network layer one per player
// (0x48a660 +0x4). Saved as a count followed by {kind, message body} pairs; loading creates each message through the
// order factory table (CreateMessageByKind).
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "orders/order_base.h"

class MessageTree : public IdAvlTree {
public:
    MessageTree();  // 0x536eb0
    virtual ~MessageTree();

    // Appends the messages read from the stream (kinds the factory table does not know are skipped).
    void Read(istream* in);  // 0x536f00
    void Write(ostream* out);  // 0x536f70
    // Takes ownership of `message` (any message, only its id is used): queued under its id, or deleted when it has no id
    // or the id is already queued.
    void Adopt(IdNode* message);  // 0x536fe0
    IdNode* Detach(uint32_t id);  // 0x537030

    uint32_t m_unk0c;  // +0x0c, 1
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(MessageTree, 0x10);
REB_ASSERT_OFFSET(MessageTree, m_unk0c, 0x0c);
#pragma clang diagnostic pop
