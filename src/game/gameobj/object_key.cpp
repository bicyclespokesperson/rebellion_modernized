#include "gameobj/object_key.h"

#include <limits>

#include "core/checked_new.h"
#include "core/utility_classes.h"

// ---- ObjectKey ---------------------------------------------------------------------------------------------

ObjectKey ObjectKey::Make(uint32_t family, uint32_t id)
{
  ObjectKey key;
  key.m_value = (family << 24) | (id & 0xffffff);
  return key;
}

ObjectKey::ObjectKey() : m_value(kUnsetValue)
{
}

ObjectKey::ObjectKey(const ObjectKey& other) = default;

void ObjectKey::Reset()
{
  m_value = kUnsetValue;
}

ObjectKey* ObjectKey::OrNull()
{
  if (m_value == kUnsetValue)
    return nullptr;
  return this;
}

int ObjectKey::IsSet() const
{
  return m_value != kUnsetValue;
}

int ObjectKey::Read(istream* in)
{
  StreamRead32(in, this);
  return 1;
}

int ObjectKey::Write(ostream* out)
{
  StreamWrite32(out, this);
  return 1;
}

int ObjectKey::Checksum(Crc32* crc)
{
  crc->AddInt(static_cast<int>(m_value));
  return 1;
}

// ---- NullableKey -----------------------------------------------------------------------------------------------

NullableKey* NullableKey::OrNull()
{
  if (m_value == 0)
    return nullptr;
  return this;
}

int NullableKey::IsSet() const
{
  return m_value != 0;
}

// ---- ObjectKeyNode -----------------------------------------------------------------------------------------

ObjectKeyNode::ObjectKeyNode() : AvlNode()
{
}

ObjectKeyNode::ObjectKeyNode(const ObjectKey& key) : AvlNode(), m_key(key)
{
}

ObjectKeyNode::ObjectKeyNode(const ObjectKeyNode& other) : AvlNode(), m_key(other.m_key)
{
}

ObjectKeyNode::~ObjectKeyNode() = default;

ObjectKeyNode* ObjectKeyNode::Clone()
{
  return NewOrNull<ObjectKeyNode>(*this);
}

void ObjectKeyNode::Load(istream* in)
{
  Read(in);
}

void ObjectKeyNode::Read(istream* in)
{
  m_key.Read(in);
}

void ObjectKeyNode::Save(ostream* out)
{
  Write(out);
}

void ObjectKeyNode::Write(ostream* out)
{
  m_key.Write(out);
}

int ObjectKeyNode::Checksum(Crc32* crc)
{
  return m_key.Checksum(crc);
}

int ObjectKeyNode::HasSameKey(const ObjectKeyNode& other) const
{
  return m_key.m_value == other.m_key.m_value;
}

// ---- ObjectKeyValueNode ------------------------------------------------------------------------------------

ObjectKeyValueNode::ObjectKeyValueNode() : ObjectKeyNode(), m_value(0)
{
}

ObjectKeyValueNode::ObjectKeyValueNode(const ObjectKey& key, int value) : ObjectKeyNode(key), m_value(value)
{
}

ObjectKeyValueNode::ObjectKeyValueNode(const ObjectKeyValueNode& other) = default;

ObjectKeyValueNode::~ObjectKeyValueNode() = default;

ObjectKeyNode* ObjectKeyValueNode::Clone()
{
  return NewOrNull<ObjectKeyValueNode>(*this);
}

void ObjectKeyValueNode::Read(istream* in)
{
  ObjectKeyNode::Read(in);
  ReadValue32(in, &m_value);
}

void ObjectKeyValueNode::Write(ostream* out)
{
  ObjectKeyNode::Write(out);
  WriteValue32(out, &m_value);
}

int ObjectKeyValueNode::Checksum(Crc32* crc)
{
  int ok = ObjectKeyNode::Checksum(crc);
  if (ok)
    crc->AddInt(m_value);
  return ok;
}

int ObjectKeyValueNode::HasSameKeyAndValue(const ObjectKeyValueNode& other) const
{
  return HasSameKey(other) && m_value == other.m_value;
}

// ---- ObjectKeySet ------------------------------------------------------------------------------------------

ObjectKeySet::ObjectKeySet() = default;

ObjectKeySet::ObjectKeySet(const ObjectKeySet& other)
{
  for (const auto& entry : other.Entries())
  {
    if (ObjectKeyNode* copy = static_cast<ObjectKeyNode*>(entry.get())->Clone())
      Add(copy);
  }
}

ObjectKeySet::~ObjectKeySet() = default;

void ObjectKeySet::Clear()
{
  decltype(m_keys) detached;
  m_keys.swap(detached);
}

void ObjectKeySet::Remove(ObjectKeyNode* node)
{
  if (!node)
    return;
  const auto [first, last] = m_keys.equal_range(node->m_key.m_value);
  for (auto position = first; position != last; ++position)
  {
    if ((*position).get() == node)
    {
      m_keys.extract(position).value().release();
      return;
    }
  }
}

ObjectKeyNode* ObjectKeySet::NewNode()
{
  return NewOrNull<ObjectKeyNode>();
}

void ObjectKeySet::Load(istream* in)
{
  Read(in);
}

void ObjectKeySet::Read(istream* in)
{
  Clear();
  uint32_t count = 0;
  ReadValue32(in, &count);
  if (iostream_access::State(in) != 0)
    return;
  for (uint32_t i = 0; i < count; ++i)
  {
    auto node = std::unique_ptr<ObjectKeyNode>(NewNode());
    if (!node)
    {
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return;
    }
    node->Read(in);
    if (iostream_access::State(in) != 0 || !Add(node.get()))
    {
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return;
    }
    node.release();
  }
}

void ObjectKeySet::Save(ostream* out)
{
  Write(out);
}

void ObjectKeySet::Write(ostream* out)
{
  if (Count() > std::numeric_limits<uint32_t>::max())
  {
    iostream_access::SetState(iostream_access::IosOf(out), iostream_flags::kFail);
    return;
  }
  auto count = static_cast<uint32_t>(Count());
  WriteValue32(out, &count);
  for (const auto& entry : Entries())
    static_cast<ObjectKeyNode*>(entry.get())->Write(out);
}

int ObjectKeySet::Checksum(Crc32* crc)
{
  int ok = 1;
  crc->AddInt(static_cast<uint32_t>(Count()));
  for (const auto& entry : Entries())
  {
    auto* node = static_cast<ObjectKeyNode*>(entry.get());
    ok = node->Checksum(crc);
    if (!ok)
      break;
  }
  return ok;
}

ObjectKeySet& ObjectKeySet::operator=(const ObjectKeySet& other)
{
  if (this != &other)
  {
    Clear();
    for (const auto& entry : other.Entries())
    {
      if (ObjectKeyNode* copy = static_cast<ObjectKeyNode*>(entry.get())->Clone())
        Add(copy);
    }
  }
  return *this;
}

uint32_t ObjectKeySet::Add(ObjectKeyNode* node)
{
  if (!node || !node->m_key.IsComplete())
    return 0;
  const auto [first, last] = m_keys.equal_range(node->m_key.m_value);
  for (auto position = first; position != last; ++position)
    if ((*position).get() == node)
      return 0;
  std::unique_ptr<ObjectKeyNode> pending(node);
  try
  {
    m_keys.insert(std::move(pending));
  }
  catch (...)
  {
    pending.release();
    throw;
  }
  return node->m_key.Id();
}

int ObjectKeySet::AddKey(const ObjectKey* key)
{
  auto* node = NewOrNull<ObjectKeyNode>(*key);
  if (!node)
    return 0;
  if (Add(node))
    return 1;
  delete node;
  return 0;
}

void ObjectKeySet::RemoveKey(const ObjectKey* key)
{
  ObjectKeyNode* node = Find(key);
  if (node)
  {
    Remove(node);
    delete node;
  }
}

ObjectKeyNode* ObjectKeySet::Detach(const ObjectKey* key)
{
  ObjectKeyNode* node = Find(key);
  if (node)
    Remove(node);
  return node;
}

ObjectKeyNode* ObjectKeySet::Find(const ObjectKey* key)
{
  const auto found = m_keys.find(key->m_value);
  return found == m_keys.end() ? nullptr : (*found).get();
}

// ---- ObjectKeyValueSet -------------------------------------------------------------------------------------

ObjectKeyValueSet::ObjectKeyValueSet() : ObjectKeySet()
{
}

ObjectKeyValueSet::ObjectKeyValueSet(const ObjectKeyValueSet& other) = default;

ObjectKeyValueSet::~ObjectKeyValueSet() = default;

ObjectKeyNode* ObjectKeyValueSet::NewNode()
{
  return NewOrNull<ObjectKeyValueNode>();
}

void ObjectKeyValueSet::Read(istream* in)
{
  ObjectKeySet::Read(in);
}

void ObjectKeyValueSet::Write(ostream* out)
{
  ObjectKeySet::Write(out);
}

int ObjectKeyValueSet::Checksum(Crc32* crc)
{
  return ObjectKeySet::Checksum(crc);
}

ObjectKeyValueSet& ObjectKeyValueSet::operator=(const ObjectKeyValueSet& other)
{
  if (this != &other)
    ObjectKeySet::operator=(other);
  return *this;
}

int ObjectKeyValueSet::operator==(const ObjectKeyValueSet& other) const
{
  bool same = true;
  auto aPosition = Entries().begin();
  auto bPosition = other.Entries().begin();
  while (aPosition != Entries().end() && bPosition != other.Entries().end())
  {
    if (!same)
      return false;
    auto* a = static_cast<const ObjectKeyValueNode*>((*aPosition).get());
    auto* b = static_cast<const ObjectKeyValueNode*>((*bPosition).get());
    same = a->HasSameKeyAndValue(*b);
    ++aPosition;
    ++bPosition;
  }
  return same && aPosition == Entries().end() && bPosition == other.Entries().end();
}

int ObjectKeyValueSet::operator!=(const ObjectKeyValueSet& other) const
{
  return !(*this == other);
}

int ObjectKeyValueSet::AddKeyValue(const ObjectKey* key, int value)
{
  auto* node = NewOrNull<ObjectKeyValueNode>(*key, value);
  if (!node)
    return 0;
  if (Add(node))
    return 1;
  delete node;
  return 0;
}

// ---- ObjectRefList -----------------------------------------------------------------------------------------

ObjectRefList::ObjectRefList() : m_ok(1)
{
}

ObjectRefList::~ObjectRefList()
{
  Clear();
}

ObjectRefList& ObjectRefList::operator=(const ObjectRefList& other)
{
  bool copied = true;
  if (this != &other)
  {
    Clear();
    for (const auto& entry : other.Entries())
    {
      if (!copied)
        break;
      std::unique_ptr<ObjectRefNode> copy(NewOrNull<ObjectRefNode>(*entry));
      copied = copy && Add(copy.get());
      if (copied)
        copy.release();
    }
  }
  m_ok = copied;
  return *this;
}

int ObjectRefList::Read(istream* in)
{
  Clear();
  ReadValue32(in, &m_ok);
  uint32_t count = 0;
  ReadValue32(in, &count);
  if (iostream_access::State(in) != 0)
    return 0;
  for (uint32_t i = 0; i < count; ++i)
  {
    std::unique_ptr<ObjectRefNode> node(NewOrNull<ObjectRefNode>());
    if (!node || !node->Read(in) || iostream_access::State(in) != 0 || !Add(node.get()))
    {
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return 0;
    }
    node.release();
  }
  return 1;
}

int ObjectRefList::Write(ostream* out)
{
  int ok = 1;
  WriteValue32(out, &m_ok);
  if (!WriteCount32(out, Count()))
    return 0;
  for (const auto& entry : Entries())
  {
    ok = entry->Write(out);
  }
  return ok;
}

int ObjectRefList::Checksum(Crc32* crc)
{
  int ok = 1;
  crc->AddInt(m_ok);
  crc->AddInt(Size());
  for (const auto& entry : Entries())
    ok = entry->Checksum(crc);
  return ok;
}

int ObjectRefList::Add(ObjectRefNode* node)
{
  return AddWithId(node) != 0;
}

size_t ObjectRefList::Size() const
{
  return Count();
}

ObjectRefNode* ObjectRefList::AddKey(ObjectKey key)
{
  std::unique_ptr<ObjectRefNode> node(NewOrNull<ObjectRefNode>());
  if (!node)
    return nullptr;
  node->Set(key);
  return Add(node.get()) ? node.release() : nullptr;
}

ObjectRefNode::ObjectRefNode() : IdNode(), m_flag(1), m_count(0)
{
  m_key.m_value = 0;
}

// The copy constructor clears m_flag instead of copying it.
ObjectRefNode::ObjectRefNode(const ObjectRefNode& other)
: IdNode(),
  m_flag(0),
  m_count(other.m_count),
  m_key(other.m_key)
{
}

ObjectRefNode::~ObjectRefNode() = default;

int ObjectRefNode::Read(istream* in)
{
  ReadValue32(in, &m_flag);
  ReadValue32(in, &m_count);
  return m_key.Read(in);
}

int ObjectRefNode::Write(ostream* out)
{
  WriteValue32(out, &m_flag);
  WriteValue32(out, &m_count);
  return m_key.Write(out);
}

int ObjectRefNode::Checksum(Crc32* crc)
{
  crc->AddInt(m_flag);
  crc->AddInt(m_count);
  m_key.Checksum(crc);
  return 1;
}

int ObjectRefNode::Set(const ObjectKey& key)
{
  m_key = key;
  m_count = 0;
  return 1;
}

ObjectKey* ObjectRefNode::GetKey(ObjectKey* out) const
{
  out->m_value = m_key.m_value;
  return out;
}
