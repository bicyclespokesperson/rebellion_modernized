#include "gameobj/child_iterator.h"

#include "core/vptr_store.h"
#include "gameobj/object_family.h"

// GameObj::MatchesStateFilter (0x4f6b90), declared on the notifier base because children are always GameObjs.
int __thiscall FUN_004f6b90(GameObjNotifier* object, uint32_t filter);

ChildIterator::ChildIterator(GameObjNotifier* parent, const FamilyRange* range, uint32_t stateFilter)
    : m_parent(parent), m_range(*range), m_hasSideFilter(0), m_side(0), m_current(nullptr) {
    m_stateFilter = stateFilter;
}

ChildIterator::ChildIterator(GameObjNotifier* parent, uint32_t side, const FamilyRange* range, uint32_t stateFilter)
    : m_parent(parent), m_range(*range), m_hasSideFilter(1), m_side(side), m_current(nullptr) {
    m_stateFilter = stateFilter;
}

ChildIterator::~ChildIterator() { REB_KEEP_VPTR_STORE(); }

ChildIterator* ChildIterator::Advance() {
    bool atEnd;
    int accepted;
    do {
        if (m_current) m_current = static_cast<GameObjNotifier*>(m_current->m_successor);
        atEnd = m_current == nullptr;
        accepted = atEnd ? 1 : Accepts();
    } while (!accepted);
    if (atEnd) Reset();
    return this;
}

ChildIterator* ChildIterator::Begin() {
    Reset();
    m_current = m_parent ? m_parent->FirstChild() : nullptr;
    if (!m_current) {
        Reset();
        return this;
    }
    if (!Accepts()) Advance();
    return this;
}

void ChildIterator::Reset() { m_current = nullptr; }

int ChildIterator::Count() {
    int count = 0;
    Begin();
    while (m_current) {
        ++count;
        Advance();
    }
    return count;
}

bool ChildIterator::HasCurrent() {
    Begin();
    return m_current != nullptr;
}

int ChildIterator::Accepts() {
    int accepted = 1;
    if (m_current) {
        const uint32_t family = m_current->GetFamily();
        if (family < m_range.m_first || family >= m_range.m_second) accepted = 0;
        if (!FUN_004f6b90(m_current, m_stateFilter)) accepted = 0;
        if (m_hasSideFilter && m_current->ControlSide() != m_side) accepted = 0;
    }
    return accepted;
}

UnitIterator::UnitIterator(GameObjNotifier* parent, uint32_t stateFilter)
    : ChildIterator(parent, &family::kUnits, stateFilter) {}

UnitIterator::~UnitIterator() { REB_KEEP_VPTR_STORE(); }
