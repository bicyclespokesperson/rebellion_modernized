#include "data/int_pair_node.h"

#include "core/checked_new.h"
#include "core/utility_classes.h"
#include "core/vptr_store.h"

IntPairNode::IntPairNode() : m_value(0) {}

IntPairNode::IntPairNode(const int& key, const int& value) : IntNode(key), m_value(value) {}

IntPairNode::IntPairNode(const IntPairNode& other) : IntNode(other), m_value(other.m_value) {}

IntPairNode::~IntPairNode() { REB_KEEP_VPTR_STORE(); }

IntNode* IntPairNode::Clone() { return NewOrNull<IntPairNode>(*this); }

int IntPairNode::Read(istream* in) {
    IntNode::Read(in);
    StreamRead32(in, &m_value);
    return 1;
}

int IntPairNode::Write(ostream* out) {
    IntNode::Write(out);
    StreamWrite32(out, &m_value);
    return 1;
}

int IntPairNode::Checksum(Crc32* crc) {
    int ok = IntNode::Checksum(crc);
    if (ok) {
        crc->AddInt(m_value);
        ok = 1;
    }
    return ok;
}
