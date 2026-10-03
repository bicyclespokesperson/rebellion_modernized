#pragma once
#include <cstddef>
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"

// Object keys: how the game names a game object. The top byte of a key is the object's *family* (which class of
// object it is, see object_family.h), the low 24 bits are the id within the family. Everything that refers to a
// game object persistently (save files, orders, notification payloads) stores an ObjectKey, and resolves it with
// ObjectDatabase lookups (object_database.h).
//
//   ObjectKey            4-byte key value; a default-constructed key is the "unset" value 2
//   NullableKey          4-byte key where 0 means empty
//   ObjectKeyNode        AVL node holding a key (VT 0x65ca50)
//   ObjectKeyValueNode   ObjectKeyNode + an int payload (VT 0x65d1f0)
//   ObjectKeySet         tree of ObjectKeyNodes ordered by key (VT 0x65d1c8)
//   ObjectKeyValueSet    ObjectKeySet of ObjectKeyValueNodes (VT 0x65d040)
//   ObjectRefNode/List   IdAvlTree of {id, int, int, ObjectKey} records (VT 0x661aec / 0x65d030)

class Crc32;

class ObjectKey {
public:
    static constexpr uint32_t kUnsetValue = 2;

    // The family/id halves of a key are 0 only in the unset value.
    static ObjectKey Make(uint32_t family, uint32_t id);

    ObjectKey();
    ObjectKey(const ObjectKey& other);

    void Reset();
    // `this`, or null if the key is unset.
    ObjectKey* OrNull();
    int IsSet() const;
    uint32_t Family() const { return m_value >> 24; }
    uint32_t Id() const { return m_value & 0xffffff; }
    // Family and id both non-zero: the only keys that name a real object.
    bool IsComplete() const { return Family() != 0 && Id() != 0; }

    int Read(istream* in);
    int Write(ostream* out);
    int Checksum(Crc32* crc);

    uint32_t m_value;  // +0x00
};

// A 4-byte key where 0 means empty (as opposed to ObjectKey, whose empty value is 2); the data-table record keys
// (SeedKey::m_key) are of this kind.
class NullableKey {
public:
    NullableKey* OrNull();
    int IsSet() const;

    uint32_t m_value;  // +0x00
};

class ObjectKeyNode : public AvlNode {
public:
    ObjectKeyNode();
    explicit ObjectKeyNode(const ObjectKey& key);
    ObjectKeyNode(const ObjectKeyNode& other);
    virtual ~ObjectKeyNode();
    virtual ObjectKeyNode* Clone();
    // Load/Save are the virtual entry points the containers call; they forward to Read/Write.
    virtual void Load(istream* in);
    virtual void Read(istream* in);
    virtual void Save(ostream* out);
    virtual void Write(ostream* out);
    virtual int Checksum(Crc32* crc);

    int HasSameKey(const ObjectKeyNode& other) const;
    ObjectKey* GetKey(ObjectKey* out) const {
        out->m_value = m_key.m_value;
        return out;
    }

    ObjectKey m_key;  // +0x18
};

class ObjectKeyValueNode : public ObjectKeyNode {
public:
    ObjectKeyValueNode();
    ObjectKeyValueNode(const ObjectKey& key, int value);
    ObjectKeyValueNode(const ObjectKeyValueNode& other);
    virtual ~ObjectKeyValueNode();
    virtual ObjectKeyNode* Clone() override;
    virtual void Read(istream* in) override;
    virtual void Write(ostream* out) override;
    virtual int Checksum(Crc32* crc) override;

    int HasSameKeyAndValue(const ObjectKeyValueNode& other) const;

    int m_value;  // +0x1c
};

class ObjectKeySet : public AvlTree {
public:
    ObjectKeySet();
    // Copies the other set's nodes (through Clone).
    explicit ObjectKeySet(const ObjectKeySet& other);
    virtual ~ObjectKeySet();
    virtual int IsLess(AvlNode* a, AvlNode* b) override;
    virtual ObjectKeyNode* NewNode();
    virtual void Load(istream* in);
    virtual void Read(istream* in);
    virtual void Save(ostream* out);
    virtual void Write(ostream* out);
    virtual int Checksum(Crc32* crc);

    ObjectKeySet& operator=(const ObjectKeySet& other);

    // Inserts the node unless its key is incomplete (then the node is leaked). Returns the id part of the node's key
    // (of the unset key, 2, for a null node), so a non-zero result does not mean the node was inserted.
    uint32_t Add(ObjectKeyNode* node);
    int AddKey(const ObjectKey* key);
    void RemoveKey(const ObjectKey* key);
    ObjectKeyNode* Detach(const ObjectKey* key);
    ObjectKeyNode* Find(const ObjectKey* key);

    ObjectKeyNode* First() const { return static_cast<ObjectKeyNode*>(AvlTree::First()); }
};

class ObjectKeyValueSet : public ObjectKeySet {
public:
    ObjectKeyValueSet();
    explicit ObjectKeyValueSet(const ObjectKeyValueSet& other);
    virtual ~ObjectKeyValueSet();
    virtual ObjectKeyNode* NewNode() override;
    virtual void Read(istream* in) override;
    virtual void Write(ostream* out) override;
    virtual int Checksum(Crc32* crc) override;

    ObjectKeyValueSet& operator=(const ObjectKeyValueSet& other);
    int operator==(const ObjectKeyValueSet& other) const;
    int operator!=(const ObjectKeyValueSet& other) const;
    int AddKeyValue(const ObjectKey* key, int value);
};

// {id, int, int, key} record: m_flag defaults to 1, m_count to 0.
// Its virtual table (0x661aec, after the destructor) is owned by the module that defines the class (0x540750+); only
// the non-virtual serialization helpers and the destructor are used here.
class ObjectRefNode : public IdNode {
public:
    ObjectRefNode();
    ObjectRefNode(const ObjectRefNode& other);
    virtual ~ObjectRefNode();
    int Read(istream* in);
    int Write(ostream* out);
    int Checksum(Crc32* crc);

    int Set(const ObjectKey& key);  // copies the key, zeroes m_count, returns 1
    ObjectKey* GetKey(ObjectKey* out) const {
        out->m_value = m_key.m_value;
        return out;
    }

    int m_flag;       // +0x1c
    int m_count;      // +0x20
    ObjectKey m_key;  // +0x24
};

// IdAvlTree of ObjectRefNodes; m_ok records whether the last Copy/Read/Add succeeded.
class ObjectRefList : public IdAvlTree {
public:
    ObjectRefList();
    virtual ~ObjectRefList();

    ObjectRefList& operator=(const ObjectRefList& other);
    int Read(istream* in);
    int Write(ostream* out);
    int Checksum(Crc32* crc);
    int Add(ObjectRefNode* node);
    uint16_t Size() const;
    // Appends a new node for `key`; returns it, or null if allocation or insertion failed.
    ObjectRefNode* AddKey(ObjectKey key);

    int m_ok;  // +0x0c
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(ObjectKey, 0x04);
REB_ASSERT_SIZE(NullableKey, 0x04);
REB_ASSERT_SIZE(ObjectKeyNode, 0x1c);
REB_ASSERT_OFFSET(ObjectKeyNode, m_key, 0x18);
REB_ASSERT_SIZE(ObjectKeyValueNode, 0x20);
REB_ASSERT_OFFSET(ObjectKeyValueNode, m_value, 0x1c);
REB_ASSERT_SIZE(ObjectKeySet, 0x08);
REB_ASSERT_SIZE(ObjectKeyValueSet, 0x08);
REB_ASSERT_SIZE(ObjectRefNode, 0x28);
REB_ASSERT_OFFSET(ObjectRefNode, m_key, 0x24);
REB_ASSERT_SIZE(ObjectRefList, 0x10);
REB_ASSERT_OFFSET(ObjectRefList, m_ok, 0x0c);
#pragma clang diagnostic pop
