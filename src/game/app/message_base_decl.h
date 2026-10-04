#pragma once
// STAND-IN declarations, to be dropped when the `order_msg_base` module (0x51f8a0-0x520570, events subsystem)
// is reconstructed. Only what the multiplayer lobby messages (app_control_cmds, 0x420bd0) derive from.
#include <cstdint>

#include "core/containers.h"
#include "core/game_string.h"
#include "core/layout.h"
#include "core/stream_io.h"
#include "gameobj/status_code.h"

// Base of every network/event message: an id node (the message id) plus a 32-bit flag set to 1 by the
// constructor. The messages are created by kind through the factory registry 0x51f930 (kinds 1-5 and 0x10-0x15 at
// 0x41ec50; order kinds 0x2xx at 0x51f4b0). Read/Write are pure virtual in the vtable but have bodies
// (0x51f9a0/0x51f9e0) that the derived classes call for the common part: the id and the flag.
class MessageBase : public IdNode {
public:
    MessageBase();
    virtual ~MessageBase();
    virtual int Read(istream* in) = 0;
    virtual int Write(ostream* out) = 0;
    virtual int GetKind() = 0;

    uint32_t m_unk1c;  // +0x1c, 1
};

// Message with a text (0x5203a0). The real chain is MessageBase -> 0x4f5c70 class -> order base 0x51fa20 ->
// this; the middle classes are folded into padding. Virtual04-Virtual08 belong to the order base (Virtual08 is
// the "can this be done?" query that fills a StatusCode).
class TextMessage : public MessageBase {
public:
    TextMessage();
    virtual ~TextMessage();
    virtual int Read(istream* in) override;
    virtual int Write(ostream* out) override;
    virtual int Virtual04() { return 0; }
    virtual int Virtual05() { return 0; }
    virtual int Virtual06() { return 0; }
    virtual int Virtual07() { return 0; }
    virtual int Virtual08(StatusCode*) { return 0; }

    uint8_t m_unk20[0x24];  // +0x20
    String m_text;          // +0x44
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(MessageBase, 0x20);
REB_ASSERT_SIZE(TextMessage, 0x50);
REB_ASSERT_OFFSET(TextMessage, m_text, 0x44);
#pragma clang diagnostic pop
