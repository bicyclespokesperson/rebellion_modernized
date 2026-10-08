#pragma once
// MessageCodec (0x51c300-0x51c740, VT 0x65ecd8, 0x1c bytes): turns a MessageBase into bytes and back. The bytes are
// the message kind (32 bit) followed by the message's own Write output; CommSendMessage ships them to the other
// machine, where the receiving player controller (RemotePlayer, the lobby window) creates the message from its kind
// through the order factory table and lets it read itself. It owns one buffer with an istrstream and an ostrstream
// over it; the buffer doubles when a message fills it.
#include <cstdint>

#include "core/layout.h"
#include "core/strstream_io.h"

class MessageBase;

class MessageCodec {
public:
    MessageCodec();           // 0x51c300
    virtual ~MessageCodec();  // 0x51c350 (deleting 0x51c330)

    // Reads the kind at the start of the buffer, creates the message and lets it read itself. *out is the new
    // message or null; true if there is one.
    int Deserialize(MessageBase** out);  // 0x51c360
    // Writes the message into the buffer (growing it as needed); m_size is the byte count afterwards.
    int Serialize(MessageBase* message);  // 0x51c410
    // Copies received bytes into the buffer for Deserialize.
    void SetData(const void* data, uint32_t size);  // 0x51c510
    // Frees the streams and the buffer; always true.
    int Release();  // 0x51c560
    // Makes the buffer at least `capacity` bytes (and new streams over it), keeping the contents if asked.
    int Reserve(int capacity, int keepContents);  // 0x51c5c0

    int m_valid;           // +0x04 the buffer and the streams exist
    IStrStream* m_input;   // +0x08
    OStrStream* m_output;  // +0x0c
    int m_capacity;        // +0x10
    uint32_t m_size;       // +0x14 bytes written by the last Serialize / given to SetData
    char* m_buffer;        // +0x18
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(MessageCodec, 0x1c);
REB_ASSERT_OFFSET(MessageCodec, m_valid, 0x04);
REB_ASSERT_OFFSET(MessageCodec, m_buffer, 0x18);
#pragma clang diagnostic pop
