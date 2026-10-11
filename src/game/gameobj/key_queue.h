#pragma once
#include <cstdint>
#include <deque>
#include <memory>

#include "core/containers.h"
#include "core/stream_io.h"
#include "events/misc_helpers.h"
#include "gameobj/object_key.h"

// Small serializable containers of object keys used by the side/role code (0x52e-0x53b).

class Crc32;

// {flag, key, value} entry of a KeyQueue. m_flag is 1 for every entry the game creates.
class KeyQueueNode
{
public:
  KeyQueueNode();
  ~KeyQueueNode();
  void Read(istream* in);

  // Writes flag, key and value using the legacy field encoding.
  void WriteFields(ostream* out);
  void Checksum(Crc32* crc);
  ObjectKey* GetKey(ObjectKey* out) const; // 0x52fd10

  int m_flag; // +0x0c
  ObjectKey m_key; // +0x10
  int m_value; // +0x14
};

// FIFO of KeyQueueNodes. m_ok is saved with the queue; the persistence functions return 0 if an entry cannot be built.
class KeyQueue
{
public:
  KeyQueue();
  ~KeyQueue();
  KeyQueue(const KeyQueue&) = delete;
  KeyQueue& operator=(const KeyQueue&) = delete;

  // Number of queued entries with the same key and value as `probe`.
  int CountMatching(KeyQueueNode* probe);
  int Read(istream* in);
  int Write(ostream* out);
  int Checksum(Crc32* crc);
  void Clear();
  KeyQueueNode* Dequeue();
  int Add(std::unique_ptr<KeyQueueNode> node);
  const auto& Entries() const
  {
    return m_entries;
  }

  int m_ok;

private:
  std::deque<std::unique_ptr<KeyQueueNode>> m_entries;
};

// ObjectKeySet whose entries carry a second key.
class ObjectKeyPairSet : public ObjectKeySet
{
public:
  ObjectKeyPairSet();
  explicit ObjectKeyPairSet(const ObjectKeyPairSet& other);
  ~ObjectKeyPairSet() override;
  ObjectKeyNode* NewNode() override;
  void Read(istream* in) override;
  void Write(ostream* out) override;
  int Checksum(Crc32* crc) override;

  int AddPair(const ObjectKey* key, const ObjectKey* second);
};
