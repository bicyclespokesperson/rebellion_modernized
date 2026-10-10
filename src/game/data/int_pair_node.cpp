#include "data/int_pair_node.h"

#include "core/checked_new.h"
#include "core/utility_classes.h"
#include "core/vptr_store.h"

IntPairNode::IntPairNode() : m_value(0) {}

IntPairNode::IntPairNode(const ObjectKey& key, const int& value) : ObjectKeyNode(key), m_value(value) {}

IntPairNode::IntPairNode(const IntPairNode& other) : ObjectKeyNode(other), m_value(other.m_value) {}

IntPairNode::~IntPairNode() { REB_KEEP_VPTR_STORE(); }

ObjectKeyNode* IntPairNode::Clone() { return NewOrNull<IntPairNode>(*this); }

void IntPairNode::Read(istream* in) {
    ObjectKeyNode::Read(in);
    StreamRead32(in, &m_value);
}

void IntPairNode::Write(ostream* out) {
    ObjectKeyNode::Write(out);
    StreamWrite32(out, &m_value);
}

int IntPairNode::Checksum(Crc32* crc) {
    int ok = ObjectKeyNode::Checksum(crc);
    if (ok) {
        crc->AddInt(m_value);
        ok = 1;
    }
    return ok;
}
