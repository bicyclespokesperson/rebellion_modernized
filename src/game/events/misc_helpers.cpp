#include "events/misc_helpers.h"

#include "core/checked_new.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"

namespace {
constexpr FamilyRange kCharacterRange(0x30, 0x38);
}

CharacterRangeIterator::CharacterRangeIterator(GameObjNotifier* parent, uint32_t stateFilter)
    : ChildIterator(parent, &kCharacterRange, stateFilter) {}

CharacterRangeIterator::~CharacterRangeIterator() = default;

ObjectKeyPairNode::ObjectKeyPairNode() : ObjectKeyNode(), m_second() {}

ObjectKeyPairNode::ObjectKeyPairNode(const ObjectKey& first, const ObjectKey& second)
    : ObjectKeyNode(first), m_second(second) {}

ObjectKeyPairNode::ObjectKeyPairNode(const ObjectKeyPairNode& other)  = default;

ObjectKeyPairNode::~ObjectKeyPairNode() = default;

ObjectKeyNode* ObjectKeyPairNode::Clone() { return NewOrNull<ObjectKeyPairNode>(*this); }

void ObjectKeyPairNode::Read(istream* in) {
    ObjectKeyNode::Read(in);
    StreamRead32(in, &m_second.m_value);
}

void ObjectKeyPairNode::Write(ostream* out) {
    ObjectKeyNode::Write(out);
    StreamWrite32(out, &m_second.m_value);
}

int ObjectKeyPairNode::Checksum(Crc32* crc) {
    int ok = ObjectKeyNode::Checksum(crc);
    if (ok) {
        crc->AddInt(m_second.m_value);
        ok = 1;
    }
    return ok;
}
