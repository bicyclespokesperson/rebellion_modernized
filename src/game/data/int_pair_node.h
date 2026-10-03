#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "core/stream_io.h"

class Crc32;

// Tree node holding one persistent int (+0x18). Owned by another module (0x4ecc00); declared here with just the
// virtuals its derived classes in this module need. Load and Save forward to the virtual Read and Write.
class IntNode : public AvlNode {
public:
    IntNode();
    explicit IntNode(const int& key);
    IntNode(const IntNode& other);
    virtual ~IntNode();
    virtual IntNode* Clone();
    virtual void Load(istream* in);
    virtual int Read(istream* in);
    virtual void Save(ostream* out);
    virtual int Write(ostream* out);
    virtual int Checksum(Crc32* crc);

    int m_key;  // +0x18
};

// IntNode with a second persistent int.
class IntPairNode : public IntNode {
public:
    IntPairNode();
    IntPairNode(const int& key, const int& value);
    IntPairNode(const IntPairNode& other);
    virtual ~IntPairNode();
    virtual IntNode* Clone() override;
    virtual int Read(istream* in) override;
    virtual int Write(ostream* out) override;
    virtual int Checksum(Crc32* crc) override;

    int m_value;  // +0x1c
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(IntNode, 0x1c);
REB_ASSERT_OFFSET(IntNode, m_key, 0x18);
REB_ASSERT_SIZE(IntPairNode, 0x20);
REB_ASSERT_OFFSET(IntPairNode, m_value, 0x1c);
#pragma clang diagnostic pop
