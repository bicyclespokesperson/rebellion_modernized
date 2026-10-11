#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/stream_io.h"

// A pair of 32-bit values that is copied around as a unit. The game uses it for two things:
//   * status results {category, code}: virtual "can this be done?" queries fill one in; {1, -1} means success
//     and {1, n} names the reason it failed (476 callers of the copy constructor);
//   * half-open family ranges {first, last}, as taken by the object lookups and iterators.
class IntPair
{
public:
  IntPair();
  IntPair(const IntPair& other);
  constexpr IntPair(uint32_t first, uint32_t second) : m_first(first), m_second(second)
  {
  }

  // The original is the (uint32_t, uint32_t) constructor: it returns `this`, and original callers pass that EAX on.
  IntPair* Set(uint32_t first, uint32_t second); // 0x51c900
  void SetSecond(uint32_t second); // 0x51c9b0
  // Both values as raw words (the Read/Write of the pairs the economy classes keep).
  int Write(ostream* out); // 0x51c920
  int Read(istream* in); // 0x51c950
  // out = {first + other.first, second + other.second}; returns out.
  IntPair* Add(IntPair* out, const IntPair& other); // 0x51ca00
  int Equals(const IntPair& other); // 0x51ca30
  int NotEquals(const IntPair& other); // 0x51ca60

  uint32_t m_first; // +0x00
  uint32_t m_second; // +0x04
};
using StatusCode = IntPair;
using FamilyRange = IntPair;

// A set of event kinds (notification/event ids such as 0x200), keyed by kind. Every game class keeps one static set
// describing the kinds it can raise; nodes carry an int that classes set to 1.
class EventKindNode;

class EventKindSet : public IntKeyedSet
{
public:
  EventKindSet();
  ~EventKindSet() override;
  int Read(istream* in) override;
  int Write(ostream* out) override;

  EventKindSet& operator=(const EventKindSet& other);

  int Contains(int kind);
  // Adds `kind` unless present. Returns the new node, or null if the kind was already there (or allocation failed).
  EventKindNode* Add(int kind, int value);
  EventKindNode* Find(int kind);
};

// Set node: IntKeyNode (key = kind) plus the value and a field that is only ever 0.
class EventKindNode : public IntKeyNode
{
public:
  EventKindNode(); // 0x582a00, value 1
  EventKindNode(const EventKindNode& other); // 0x582a40
  ~EventKindNode() override; // 0x582a70 (scalar deleting destructor 0x582a20)
  CloneableNode* Clone() override; // 0x582a80
  int Read(istream* in) override; // 0x582ae0
  int Write(ostream* out) override; // 0x582b20

  int m_value; // +0x1c
  int m_unknown; // +0x20
};
