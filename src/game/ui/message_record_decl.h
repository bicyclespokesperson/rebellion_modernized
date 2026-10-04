#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/game_string.h"

// Stand-in declaration of the records the message index lists (they come from game_ui_facade 0x41d1b0; the owning
// class is not reconstructed yet): an id-keyed node with flags at +0x34 and the message text at +0x44. Only what
// MessageListItem uses is declared; drop this header for the real one.
class MessageRecord : public IdNode {
public:
    uint8_t m_unk1c[0x28];  // +0x1c
    String m_text;          // +0x44
};
