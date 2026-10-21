#include "gameobj/key_queue.h"

#include "core/checked_new.h"
#include "core/utility_classes.h"

// ---- KeyQueueNode ------------------------------------------------------------------------------------------

KeyQueueNode::KeyQueueNode()
    : m_flag(1)
    , m_value(0)
{
}

KeyQueueNode::~KeyQueueNode() = default;

ObjectKey* KeyQueueNode::GetKey(ObjectKey* out) const
{
  out->m_value = m_key.m_value;
  return out;
}

void KeyQueueNode::Read(istream* in)
{
  ReadValue32(in, &m_flag);
  m_key.Read(in);
  ReadValue32(in, &m_value);
}

void KeyQueueNode::WriteFields(ostream* out)
{
  WriteValue32(out, &m_flag);
  m_key.Write(out);
  WriteValue32(out, &m_value);
}

void KeyQueueNode::Checksum(Crc32* crc)
{
  crc->AddInt(m_flag);
  m_key.Checksum(crc);
  crc->AddInt(m_value);
}

// ---- KeyQueue ----------------------------------------------------------------------------------------------

KeyQueue::KeyQueue()
    : m_ok(1)
{
}

KeyQueue::~KeyQueue() = default;

void KeyQueue::Clear()
{
  m_entries.clear();
}

int KeyQueue::CountMatching(KeyQueueNode* probe)
{
  int matches = 0;
  for (const auto& entry : m_entries) {
    if (entry->m_key.m_value == probe->m_key.m_value && entry->m_value == probe->m_value) {
      ++matches;
    }
  }
  return matches;
}

int KeyQueue::Read(istream* in)
{
  Clear();
  ReadValue32(in, &m_ok);
  uint32_t count = 0;
  ReadValue32(in, &count);
  if (iostream_access::IosOf(in)->m_state & iostream_flags::kFail) {
    return 0;
  }
  for (uint32_t i = 0; i < count; ++i) {
    std::unique_ptr<KeyQueueNode> node(NewOrNull<KeyQueueNode>());
    if (!node) {
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return 0;
    }
    node->Read(in);
    if (iostream_access::IosOf(in)->m_state & iostream_flags::kFail) {
      return 0;
    }
    Add(std::move(node));
  }
  return 1;
}

int KeyQueue::Write(ostream* out)
{
  WriteValue32(out, &m_ok);
  if (!WriteCount32(out, m_entries.size())) {
    return 0;
  }
  for (const auto& node : m_entries) {
    node->WriteFields(out);
  }
  return (iostream_access::IosOf(out)->m_state & iostream_flags::kFail) == 0;
}

int KeyQueue::Checksum(Crc32* crc)
{
  crc->AddInt(m_ok);
  crc->AddInt(static_cast<int>(m_entries.size()));
  for (const auto& node : m_entries) {
    node->Checksum(crc);
  }
  return 1;
}

KeyQueueNode* KeyQueue::Dequeue()
{
  if (m_entries.empty()) {
    return nullptr;
  }
  auto node = std::move(m_entries.front());
  m_entries.pop_front();
  return node.release();
}

int KeyQueue::Add(std::unique_ptr<KeyQueueNode> node)
{
  if (!node) {
    return 0;
  }
  m_entries.push_back(std::move(node));
  return 1;
}

// ---- ObjectKeyPairSet --------------------------------------------------------------------------------------

ObjectKeyPairSet::ObjectKeyPairSet()
    : ObjectKeySet()
{
}

ObjectKeyPairSet::ObjectKeyPairSet(const ObjectKeyPairSet& other) = default;

ObjectKeyPairSet::~ObjectKeyPairSet() = default;

ObjectKeyNode* ObjectKeyPairSet::NewNode()
{
  return NewOrNull<ObjectKeyPairNode>();
}

void ObjectKeyPairSet::Read(istream* in)
{
  ObjectKeySet::Read(in);
}

void ObjectKeyPairSet::Write(ostream* out)
{
  ObjectKeySet::Write(out);
}

int ObjectKeyPairSet::Checksum(Crc32* crc)
{
  return ObjectKeySet::Checksum(crc);
}

int ObjectKeyPairSet::AddPair(const ObjectKey* key, const ObjectKey* second)
{
  auto* node = NewOrNull<ObjectKeyPairNode>(*key, *second);
  return node && Add(node) != 0;
}
