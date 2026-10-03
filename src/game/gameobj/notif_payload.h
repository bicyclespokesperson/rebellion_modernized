#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "core/stream_io.h"

// Base of the small notification payload classes (VT 0x65ecf8; the leaf classes' vtables are 0x661300-0x661380, the
// ObjectKey-carrying subclass built at 0x583fe0 adds a key at +0x20): a LinkNode followed by five ints that the leaf
// classes' Read/Write call ReadFields/WriteFields for. Slots 3, 6 and 7 are pure and not understood yet.
class NotifPayload : public LinkNode {
public:
    NotifPayload();
    virtual ~NotifPayload();
    virtual int Virtual03() = 0;
    virtual int Virtual04() { return 1; }
    virtual void Virtual05() {}
    virtual int Virtual06() = 0;
    virtual int Virtual07() = 0;

    // LinkNode::Read (a no-op), then the five ints; always 1.
    int ReadFields(istream* in);
    int WriteFields(ostream* out);

    int m_value0c;  // +0x0c
    int m_value10;  // +0x10
    int m_value14;  // +0x14
    int m_value18;  // +0x18
    int m_value1c;  // +0x1c
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(NotifPayload, 0x20);
REB_ASSERT_OFFSET(NotifPayload, m_value0c, 0x0c);
REB_ASSERT_OFFSET(NotifPayload, m_value1c, 0x1c);
#pragma clang diagnostic pop
