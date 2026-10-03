#include "gameobj/status_code.h"

#include "core/checked_new.h"
#include "core/vptr_store.h"

// EventKindNode's constructor/destructor (0x582a00/0x582a70) live in another module.
EventKindNode* __thiscall FUN_00582a00(void* storage);
void __thiscall FUN_00582a70(EventKindNode* node);

namespace {

// A throwaway node used as the probe of a lookup.
class KindProbe {
public:
    explicit KindProbe(int kind) {
        m_node = FUN_00582a00(storage);
        m_node->m_key = kind;
    }
    ~KindProbe() { FUN_00582a70(m_node); }
    EventKindNode* node() const { return m_node; }

private:
    alignas(8) char storage[sizeof(EventKindNode)];
    EventKindNode* m_node;
};

}  // namespace

// ---- IntPair -----------------------------------------------------------------------------------------------

IntPair::IntPair() : m_first(0), m_second(0) {}

IntPair::IntPair(const IntPair& other) : m_first(other.m_first), m_second(other.m_second) {}

// ---- EventKindSet ------------------------------------------------------------------------------------------

EventKindSet::EventKindSet() : IntKeyedSet() {}

EventKindSet::~EventKindSet() { REB_KEEP_VPTR_STORE(); }

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
        node = memory ? FUN_00582a00(memory) : nullptr;
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
    int ok = 1;
    int count;
    ReadValue32(in, &count);
    for (int i = 0; i < count; ++i) {
        int kind;
        ReadValue32(in, &kind);
        void* memory = ::operator new(sizeof(EventKindNode));
        EventKindNode* node = memory ? FUN_00582a00(memory) : nullptr;
        if (node) {
            node->m_key = kind;
            AddIfAbsent(node);
        }
    }
    return ok;
}

int EventKindSet::Write(ostream* out) {
    int ok = 1;
    uint32_t count = static_cast<uint16_t>(Count());
    WriteValue32(out, &count);
    for (AvlNode* node = AvlTree::First(); node; node = node->m_successor) static_cast<EventKindNode*>(node)->Write(out);
    return ok;
}
