#include "gameobj/status_code.h"

#include "core/checked_new.h"
#include "core/iostream_lib.h"
#include "core/stream_io.h"

// ---- IntPair -----------------------------------------------------------------------------------------------

IntPair::IntPair()
    : m_first(0)
    , m_second(0)
{
}

IntPair::IntPair(const IntPair& other) = default;

IntPair* IntPair::Set(uint32_t first, uint32_t second)
{
  m_first = first;
  m_second = second;
  return this;
}

void IntPair::SetSecond(uint32_t second)
{
  m_second = second;
}

int IntPair::Write(ostream* out)
{
  WriteValue32(out, &m_first);
  WriteValue32(out, &m_second);
  return 1;
}

int IntPair::Read(istream* in)
{
  ReadValue32(in, &m_first);
  ReadValue32(in, &m_second);
  return 1;
}

IntPair* IntPair::Add(IntPair* out, const IntPair& other)
{
  out->Set(m_first + other.m_first, m_second + other.m_second);
  return out;
}

int IntPair::Equals(const IntPair& other)
{
  return m_first == other.m_first && m_second == other.m_second ? 1 : 0;
}

int IntPair::NotEquals(const IntPair& other)
{
  return Equals(other) == 0;
}

// ---- EventKindSet ------------------------------------------------------------------------------------------

EventKindSet::EventKindSet() = default;
EventKindSet::~EventKindSet() = default;
EventKindSet& EventKindSet::operator=(const EventKindSet& other) = default;

void EventKindSet::Clear()
{
  m_kinds.clear();
}

size_t EventKindSet::Count() const
{
  return m_kinds.size();
}

EventKindSet& EventKindSet::Intersect(const EventKindSet& other)
{
  std::erase_if(m_kinds, [&other](const auto& entry) {
    return !other.m_kinds.contains(entry.first);
  });
  return *this;
}

int EventKindSet::Contains(int kind)
{
  return m_kinds.contains(kind);
}

EventKindNode* EventKindSet::Add(int kind, int value)
{
  auto [position, inserted] = m_kinds.try_emplace(kind);
  if (!inserted) {
    return nullptr;
  }
  position->second.m_key = kind;
  position->second.m_value = value;
  return &position->second;
}

EventKindNode* EventKindSet::Find(int kind)
{
  const auto found = m_kinds.find(kind);
  return found == m_kinds.end() ? nullptr : &found->second;
}

int EventKindSet::Read(istream* in)
{
  Clear();
  uint32_t count = 0;
  ReadValue32(in, &count);
  if (iostream_access::State(in) != 0) {
    return 0;
  }
  for (uint32_t i = 0; i < count; ++i) {
    EventKindNode node;
    node.Read(in);
    if (iostream_access::State(in) != 0) {
      return 0;
    }
    m_kinds.try_emplace(node.m_key, node);
  }
  return 1;
}

int EventKindSet::Write(ostream* out)
{
  if (!WriteCount32(out, Count())) {
    return 0;
  }
  for (auto& [kind, node] : m_kinds) {
    node.Write(out);
  }
  return iostream_access::State(out) == 0;
}

// ---- EventKindNode ----------------------------------------------------------------------------------------

EventKindNode::EventKindNode()
    : m_value(1)
    , m_unknown(0)
{
}

EventKindNode::EventKindNode(const EventKindNode& other) = default;

EventKindNode::~EventKindNode() = default;

int EventKindNode::Read(istream* in)
{
  ReadValue32(in, &m_key);
  ReadValue32(in, &m_value);
  ReadValue32(in, &m_unknown);
  return iostream_access::State(in) == 0;
}

int EventKindNode::Write(ostream* out)
{
  WriteValue32(out, &m_key);
  WriteValue32(out, &m_value);
  WriteValue32(out, &m_unknown);
  return iostream_access::State(out) == 0;
}
