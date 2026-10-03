#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "core/stream_io.h"

// A pair of 32-bit values that is copied around as a unit. The game uses it for two things:
//   * status results {category, code}: virtual "can this be done?" queries fill one in; {1, -1} means success
//     and {1, n} names the reason it failed (476 callers of the copy constructor);
//   * half-open family ranges {first, last}, as taken by the object lookups and iterators.
class IntPair {
public:
    IntPair();
    IntPair(const IntPair& other);
    constexpr IntPair(uint32_t first, uint32_t second) : m_first(first), m_second(second) {}

    uint32_t m_first;   // +0x00
    uint32_t m_second;  // +0x04
};
using StatusCode = IntPair;
using FamilyRange = IntPair;

// A set of event kinds (notification/event ids such as 0x200), keyed by kind. Every game class keeps one static set
// describing the kinds it can raise; nodes carry an int that classes set to 1.
class EventKindNode;

class EventKindSet : public IntKeyedSet {
public:
    EventKindSet();
    virtual ~EventKindSet();
    virtual int Read(istream* in) override;
    virtual int Write(ostream* out) override;

    EventKindSet& operator=(const EventKindSet& other);

    int Contains(int kind);
    // Adds `kind` unless present. Returns the new node, or null if the kind was already there (or allocation failed).
    EventKindNode* Add(int kind, int value);
    EventKindNode* Find(int kind);
};

// Set node: IntKeyNode (key = kind) plus the value and a field that is only ever 0.
class EventKindNode : public IntKeyNode {
public:
    int m_value;    // +0x1c
    int m_unknown;  // +0x20
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(IntPair, 0x08);
REB_ASSERT_SIZE(EventKindSet, 0x08);
REB_ASSERT_SIZE(EventKindNode, 0x24);
REB_ASSERT_OFFSET(EventKindNode, m_value, 0x1c);
#pragma clang diagnostic pop
