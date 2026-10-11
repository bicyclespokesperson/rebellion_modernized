#include "gameobj/status_code.h"

#include "core/checked_new.h"
#include "core/iostream_lib.h"
#include "core/stream_io.h"

namespace {

// A throwaway node used as the probe of a lookup.
class KindProbe {
public:
    explicit KindProbe(int kind) {
        m_node = new (storage) EventKindNode();
        m_node->m_key = kind;
    }
    ~KindProbe() { m_node->EventKindNode::~EventKindNode(); }
    EventKindNode* node() const { return m_node; }

private:
    alignas(8) char storage[sizeof(EventKindNode)];
    EventKindNode* m_node;
};

}  // namespace

// ---- IntPair -----------------------------------------------------------------------------------------------

IntPair::IntPair() : m_first(0), m_second(0) {}

IntPair::IntPair(const IntPair& other)  = default;

IntPair* IntPair::Set(uint32_t first, uint32_t second) {
    m_first = first;
    m_second = second;
    return this;
}

void IntPair::SetSecond(uint32_t second) { m_second = second; }

int IntPair::Write(ostream* out) {
    WriteValue32(out, &m_first);
    WriteValue32(out, &m_second);
    return 1;
}

int IntPair::Read(istream* in) {
    ReadValue32(in, &m_first);
    ReadValue32(in, &m_second);
    return 1;
}

IntPair* IntPair::Add(IntPair* out, const IntPair& other) {
    out->Set(m_first + other.m_first, m_second + other.m_second);
    return out;
}

int IntPair::Equals(const IntPair& other) { return m_first == other.m_first && m_second == other.m_second ? 1 : 0; }

int IntPair::NotEquals(const IntPair& other) { return Equals(other) == 0; }

// ---- EventKindSet ------------------------------------------------------------------------------------------

EventKindSet::EventKindSet() : IntKeyedSet() {}

EventKindSet::~EventKindSet() = default;

EventKindSet& EventKindSet::operator=(const EventKindSet& other) {
    if (this != &other) IntKeyedSet::operator=(other);
    return *this;
}

int EventKindSet::Contains(int kind) {
    KindProbe probe(kind);
    return AvlSet::Contains(probe.node());
}

EventKindNode* EventKindSet::Add(int kind, int value) {
    EventKindNode* node = nullptr;
    if (!Contains(kind)) {
        void* memory = ::operator new(sizeof(EventKindNode));
        node = memory ? new (memory) EventKindNode() : nullptr;
        if (node) {
            node->m_value = value;
            node->m_key = kind;
            AddIfAbsent(node);
        }
    }
    return node;
}

EventKindNode* EventKindSet::Find(int kind) {
    KindProbe probe(kind);
    return static_cast<EventKindNode*>(AvlSet::Find(probe.node()));
}

int EventKindSet::Read(istream* in) {
    AvlTree::Clear();
    uint32_t count = 0;
    ReadValue32(in, &count);
    if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0) return 0;
    for (uint32_t i = 0; i < count; ++i) {
        EventKindNode* node = NewOrNull<EventKindNode>();
        if (!node) {
            iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
            return 0;
        }
        node->Read(in);
        if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0) {
            delete node;
            return 0;
        }
        if (Contains(node->m_key)) {
            delete node;
            continue;
        }
        if (!Insert(node)) {
            delete node;
            iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
            return 0;
        }
    }
    return 1;
}

int EventKindSet::Write(ostream* out) {
    if (!WriteCount32(out, Count())) return 0;
    for (AvlNode* node = AvlTree::First(); node; node = node->m_successor) static_cast<EventKindNode*>(node)->Write(out);
    return 1;
}
