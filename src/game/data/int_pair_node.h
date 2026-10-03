#pragma once
#include <cstdint>

#include "core/layout.h"
#include "core/stream_io.h"
#include "gameobj/object_key.h"

class Crc32;

// ObjectKeyNode with a second persistent 4-byte value (+0x1c). The object-model agent believes the value is an
// ObjectKey too ({ObjectKey, ObjectKey} node); it is kept as a plain int until a caller shows otherwise.
class IntPairNode : public ObjectKeyNode {
public:
    IntPairNode();
    IntPairNode(const ObjectKey& key, const int& value);
    IntPairNode(const IntPairNode& other);
    virtual ~IntPairNode();
    virtual ObjectKeyNode* Clone() override;
    virtual void Read(istream* in) override;
    virtual void Write(ostream* out) override;
    virtual int Checksum(Crc32* crc) override;

    int m_value;  // +0x1c
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(IntPairNode, 0x20);
REB_ASSERT_OFFSET(IntPairNode, m_value, 0x1c);
#pragma clang diagnostic pop
