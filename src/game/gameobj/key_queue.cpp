#include "gameobj/key_queue.h"

#include "core/checked_new.h"
#include "core/utility_classes.h"

// ---- KeyQueueNode ------------------------------------------------------------------------------------------

KeyQueueNode::KeyQueueNode() : LinkNode(), m_flag(1), m_value(0)
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
  LinkNode::Read(in);
  ReadValue32(in, &m_flag);
  m_key.Read(in);
  ReadValue32(in, &m_value);
}

void KeyQueueNode::WriteFields(ostream* out)
{
  Write(out);
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

KeyQueue::KeyQueue() : NodeQueue(), m_ok(1)
{
}

KeyQueue::~KeyQueue()
{
  Clear();
}

int KeyQueue::CountMatching(KeyQueueNode* probe)
{
  int matches = 0;
  for (LinkNode* node = Head(); node; node = node->m_next)
  {
    KeyQueueNode* entry = static_cast<KeyQueueNode*>(node);
    ObjectKey probeKey;
    ObjectKey entryKey;
    probe->GetKey(&probeKey);
    entry->GetKey(&entryKey);
    if (entryKey.m_value == probeKey.m_value && entry->m_value == probe->m_value)
      ++matches;
  }
  return matches;
}

int KeyQueue::Read(istream* in)
{
  int ok = 1;
  Clear();
  ReadValue32(in, &m_ok);
  int count;
  ReadValue32(in, &count);
  for (int i = 0; i < count && ok; ++i)
  {
    KeyQueueNode* node = NewOrNull<KeyQueueNode>();
    ok = 0;
    if (node)
    {
      ok = node->m_flag;
      if (ok)
      {
        node->Read(in);
        Add(node);
      }
    }
  }
  return ok;
}

int KeyQueue::Write(ostream* out)
{
  WriteValue32(out, &m_ok);
  int count = m_count;
  WriteValue32(out, &count);
  for (LinkNode* node = Head(); node; node = node->m_next)
    static_cast<KeyQueueNode*>(node)->WriteFields(out);
  return 1;
}

int KeyQueue::Checksum(Crc32* crc)
{
  crc->AddInt(m_ok);
  crc->AddInt(m_count);
  for (LinkNode* node = Head(); node; node = node->m_next)
    static_cast<KeyQueueNode*>(node)->Checksum(crc);
  return 1;
}

LinkNode* KeyQueue::Head() const
{
  return m_head;
}

int KeyQueue::Add(LinkNode* node)
{
  Enqueue(node);
  return 1;
}

// ---- ObjectKeyPairSet --------------------------------------------------------------------------------------

ObjectKeyPairSet::ObjectKeyPairSet() : ObjectKeySet()
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
  ObjectKeyPairNode* node = NewOrNull<ObjectKeyPairNode>(*key, *second);
  return node && Add(node) != 0;
}
