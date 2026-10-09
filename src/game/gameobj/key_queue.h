#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "core/stream_io.h"
#include "events/misc_helpers.h"
#include "gameobj/object_key.h"

// Small serializable containers of object keys used by the side/role code (0x52e-0x53b).

class Crc32;

// {flag, key, value} entry of a KeyQueue. m_flag is 1 for every entry the game creates.
class KeyQueueNode : public LinkNode {
public:
    KeyQueueNode();
    virtual ~KeyQueueNode();
    virtual void Read(istream* in) override;

    // Calls the virtual Write (a no-op in LinkNode), then writes flag, key and value.
    void WriteFields(ostream* out);
    void Checksum(Crc32* crc);
    ObjectKey* GetKey(ObjectKey* out) const;  // 0x52fd10

    int m_flag;       // +0x0c
    ObjectKey m_key;  // +0x10
    int m_value;      // +0x14
};

// FIFO of KeyQueueNodes. m_ok is saved with the queue; the persistence functions return 0 if an entry cannot be built.
class KeyQueue : public NodeQueue {
public:
    KeyQueue();
    virtual ~KeyQueue();

    // Number of queued entries with the same key and value as `probe`.
    int CountMatching(KeyQueueNode* probe);
    int Read(istream* in);
    int Write(ostream* out);
    int Checksum(Crc32* crc);
    LinkNode* Head() const;
    int Add(LinkNode* node);

    int m_ok;  // +0x10
};

// ObjectKeySet whose entries carry a second key.
class ObjectKeyPairSet : public ObjectKeySet {
public:
    ObjectKeyPairSet();
    explicit ObjectKeyPairSet(const ObjectKeyPairSet& other);
    virtual ~ObjectKeyPairSet();
    virtual ObjectKeyNode* NewNode() override;
    virtual void Read(istream* in) override;
    virtual void Write(ostream* out) override;
    virtual int Checksum(Crc32* crc) override;

    int AddPair(const ObjectKey* key, const ObjectKey* second);
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(KeyQueueNode, 0x18);
REB_ASSERT_OFFSET(KeyQueueNode, m_key, 0x10);
REB_ASSERT_SIZE(KeyQueue, 0x14);
REB_ASSERT_OFFSET(KeyQueue, m_ok, 0x10);
REB_ASSERT_SIZE(ObjectKeyPairSet, 0x08);
#pragma clang diagnostic pop
