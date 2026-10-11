#include "gameobj/object_key.h"

#include "core/checked_new.h"
#include "core/utility_classes.h"

// ---- ObjectKey ---------------------------------------------------------------------------------------------

ObjectKey ObjectKey::Make(uint32_t family, uint32_t id) {
    ObjectKey key;
    key.m_value = (family << 24) | (id & 0xffffff);
    return key;
}

ObjectKey::ObjectKey() : m_value(kUnsetValue) {}

ObjectKey::ObjectKey(const ObjectKey& other)  = default;

void ObjectKey::Reset() { m_value = kUnsetValue; }

ObjectKey* ObjectKey::OrNull() {
    if (m_value == kUnsetValue) return nullptr;
    return this;
}

int ObjectKey::IsSet() const { return m_value != kUnsetValue; }

int ObjectKey::Read(istream* in) {
    StreamRead32(in, this);
    return 1;
}

int ObjectKey::Write(ostream* out) {
    StreamWrite32(out, this);
    return 1;
}

int ObjectKey::Checksum(Crc32* crc) {
    crc->AddInt(static_cast<int>(m_value));
    return 1;
}

// ---- NullableKey -----------------------------------------------------------------------------------------------

NullableKey* NullableKey::OrNull() {
    if (m_value == 0) return nullptr;
    return this;
}

int NullableKey::IsSet() const { return m_value != 0; }

// ---- ObjectKeyNode -----------------------------------------------------------------------------------------

ObjectKeyNode::ObjectKeyNode() : AvlNode() {}

ObjectKeyNode::ObjectKeyNode(const ObjectKey& key) : AvlNode(), m_key(key) {}

ObjectKeyNode::ObjectKeyNode(const ObjectKeyNode& other) : AvlNode(), m_key(other.m_key) {}

ObjectKeyNode::~ObjectKeyNode() = default;

ObjectKeyNode* ObjectKeyNode::Clone() { return NewOrNull<ObjectKeyNode>(*this); }

void ObjectKeyNode::Load(istream* in) { Read(in); }

void ObjectKeyNode::Read(istream* in) { m_key.Read(in); }

void ObjectKeyNode::Save(ostream* out) { Write(out); }

void ObjectKeyNode::Write(ostream* out) { m_key.Write(out); }

int ObjectKeyNode::Checksum(Crc32* crc) { return m_key.Checksum(crc); }

int ObjectKeyNode::HasSameKey(const ObjectKeyNode& other) const { return m_key.m_value == other.m_key.m_value; }

// ---- ObjectKeyValueNode ------------------------------------------------------------------------------------

ObjectKeyValueNode::ObjectKeyValueNode() : ObjectKeyNode(), m_value(0) {}

ObjectKeyValueNode::ObjectKeyValueNode(const ObjectKey& key, int value) : ObjectKeyNode(key), m_value(value) {}

ObjectKeyValueNode::ObjectKeyValueNode(const ObjectKeyValueNode& other)  = default;

ObjectKeyValueNode::~ObjectKeyValueNode() = default;

ObjectKeyNode* ObjectKeyValueNode::Clone() { return NewOrNull<ObjectKeyValueNode>(*this); }

void ObjectKeyValueNode::Read(istream* in) {
    ObjectKeyNode::Read(in);
    ReadValue32(in, &m_value);
}

void ObjectKeyValueNode::Write(ostream* out) {
    ObjectKeyNode::Write(out);
    WriteValue32(out, &m_value);
}

int ObjectKeyValueNode::Checksum(Crc32* crc) {
    int ok = ObjectKeyNode::Checksum(crc);
    if (ok) crc->AddInt(m_value);
    return ok;
}

int ObjectKeyValueNode::HasSameKeyAndValue(const ObjectKeyValueNode& other) const {
    return HasSameKey(other) && m_value == other.m_value;
}

// ---- ObjectKeySet ------------------------------------------------------------------------------------------

ObjectKeySet::ObjectKeySet() : AvlTree() {}

ObjectKeySet::ObjectKeySet(const ObjectKeySet& other) : AvlTree() {
    for (AvlNode* node = other.AvlTree::First(); node; node = node->m_successor) {
        if (ObjectKeyNode* copy = static_cast<ObjectKeyNode*>(node)->Clone()) Add(copy);
    }
}

ObjectKeySet::~ObjectKeySet() {
    AvlTree::Clear();

}

int ObjectKeySet::IsLess(AvlNode* a, AvlNode* b) {
    if (!a || !b) return 0;
    ObjectKey keyA;
    ObjectKey keyB;
    static_cast<ObjectKeyNode*>(a)->GetKey(&keyA);
    static_cast<ObjectKeyNode*>(b)->GetKey(&keyB);
    return keyA.m_value < keyB.m_value;
}

ObjectKeyNode* ObjectKeySet::NewNode() { return NewOrNull<ObjectKeyNode>(); }

void ObjectKeySet::Load(istream* in) { Read(in); }

void ObjectKeySet::Read(istream* in) {
    Clear();
    uint32_t count;
    ReadValue32(in, &count);
    for (uint32_t i = 0; i < count; ++i) {
        ObjectKeyNode* node = NewNode();
        // BUG(original): a failed allocation is not checked before the node is used.
        node->Read(in);
        Add(node);
    }
}

void ObjectKeySet::Save(ostream* out) { Write(out); }

void ObjectKeySet::Write(ostream* out) {
    uint32_t count = static_cast<uint16_t>(Count());
    WriteValue32(out, &count);
    for (ObjectKeyNode* node = First(); node; node = static_cast<ObjectKeyNode*>(node->m_successor)) node->Write(out);
}

int ObjectKeySet::Checksum(Crc32* crc) {
    int ok = 1;
    crc->AddInt(static_cast<uint16_t>(Count()));
    for (ObjectKeyNode* node = First(); node; node = static_cast<ObjectKeyNode*>(node->m_successor)) {
        ok = node->Checksum(crc);
        if (!ok) break;
    }
    return ok;
}

ObjectKeySet& ObjectKeySet::operator=(const ObjectKeySet& other) {
    if (this != &other) {
        Clear();
        for (AvlNode* node = other.AvlTree::First(); node; node = node->m_successor) {
            if (ObjectKeyNode* copy = static_cast<ObjectKeyNode*>(node)->Clone()) Add(copy);
        }
    }
    return *this;
}

uint32_t ObjectKeySet::Add(ObjectKeyNode* node) {
    ObjectKey key;
    if (node) {
        node->GetKey(&key);
        if (key.IsComplete()) Insert(node);
    }
    return key.Id();
}

int ObjectKeySet::AddKey(const ObjectKey* key) {
    ObjectKeyNode* node = NewOrNull<ObjectKeyNode>(*key);
    return node && Add(node) != 0;
}

void ObjectKeySet::RemoveKey(const ObjectKey* key) {
    ObjectKeyNode* node = Find(key);
    if (node) {
        AvlTree::Remove(node);
        delete node;
    }
}

ObjectKeyNode* ObjectKeySet::Detach(const ObjectKey* key) {
    ObjectKeyNode* node = Find(key);
    if (node) AvlTree::Remove(node);
    return node;
}

ObjectKeyNode* ObjectKeySet::Find(const ObjectKey* key) {
    ObjectKeyNode* node = static_cast<ObjectKeyNode*>(m_root);
    while (node) {
        ObjectKey nodeKey;
        node->GetKey(&nodeKey);
        if (key->m_value == nodeKey.m_value) break;
        node = static_cast<ObjectKeyNode*>(nodeKey.m_value < key->m_value ? node->m_right : node->m_left);
    }
    return node;
}

// ---- ObjectKeyValueSet -------------------------------------------------------------------------------------

ObjectKeyValueSet::ObjectKeyValueSet() : ObjectKeySet() {}

ObjectKeyValueSet::ObjectKeyValueSet(const ObjectKeyValueSet& other)  = default;

ObjectKeyValueSet::~ObjectKeyValueSet() = default;

ObjectKeyNode* ObjectKeyValueSet::NewNode() { return NewOrNull<ObjectKeyValueNode>(); }

void ObjectKeyValueSet::Read(istream* in) { ObjectKeySet::Read(in); }

void ObjectKeyValueSet::Write(ostream* out) { ObjectKeySet::Write(out); }

int ObjectKeyValueSet::Checksum(Crc32* crc) { return ObjectKeySet::Checksum(crc); }

ObjectKeyValueSet& ObjectKeyValueSet::operator=(const ObjectKeyValueSet& other) {
    if (this != &other) ObjectKeySet::operator=(other);
    return *this;
}

int ObjectKeyValueSet::operator==(const ObjectKeyValueSet& other) const {
    bool same = true;
    const ObjectKeyValueNode* a = static_cast<const ObjectKeyValueNode*>(AvlTree::First());
    const ObjectKeyValueNode* b = static_cast<const ObjectKeyValueNode*>(other.AvlTree::First());
    while (a && b) {
        if (!same) return false;
        same = a->HasSameKeyAndValue(*b);
        a = static_cast<const ObjectKeyValueNode*>(a->m_successor);
        b = static_cast<const ObjectKeyValueNode*>(b->m_successor);
    }
    return same && !a && !b;
}

int ObjectKeyValueSet::operator!=(const ObjectKeyValueSet& other) const { return !(*this == other); }

int ObjectKeyValueSet::AddKeyValue(const ObjectKey* key, int value) {
    ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(*key, value);
    return node && Add(node) != 0;
}

// ---- ObjectRefList -----------------------------------------------------------------------------------------

ObjectRefList::ObjectRefList() : IdAvlTree(), m_ok(1) {}

ObjectRefList::~ObjectRefList() {
    IdAvlTree::Clear();

}

ObjectRefList& ObjectRefList::operator=(const ObjectRefList& other) {
    bool copied = true;
    if (this != &other) {
        IdAvlTree::Clear();
        for (const AvlNode* node = other.AvlTree::First(); node && copied; node = node->m_successor) {
            ObjectRefNode* copy = NewOrNull<ObjectRefNode>(*static_cast<const ObjectRefNode*>(node));
            copied = copy != nullptr;
            if (copied) Add(copy);
        }
    }
    m_ok = copied;
    return *this;
}

int ObjectRefList::Read(istream* in) {
    int ok = 1;
    IdAvlTree::Clear();
    ReadValue32(in, &m_ok);
    int count;
    ReadValue32(in, &count);
    for (int i = 0; i < count && ok; ++i) {
        ObjectRefNode* node = NewOrNull<ObjectRefNode>();
        ok = 0;
        if (node) {
            ok = node->Read(in);
            if (ok) ok = Add(node);
        }
    }
    return ok;
}

int ObjectRefList::Write(ostream* out) {
    int ok = 1;
    WriteValue32(out, &m_ok);
    uint32_t count = Size();
    WriteValue32(out, &count);
    for (AvlNode* node = AvlTree::First(); node; node = node->m_successor) {
        ok = static_cast<ObjectRefNode*>(node)->Write(out);
    }
    return ok;
}

int ObjectRefList::Checksum(Crc32* crc) {
    int ok = 1;
    crc->AddInt(m_ok);
    crc->AddInt(Size());
    for (AvlNode* node = AvlTree::First(); node; node = node->m_successor) ok = static_cast<ObjectRefNode*>(node)->Checksum(crc);
    return ok;
}

int ObjectRefList::Add(ObjectRefNode* node) {
    if (node) AddWithId(node);
    return node != nullptr;
}

uint16_t ObjectRefList::Size() const { return static_cast<uint16_t>(Count()); }

ObjectRefNode* ObjectRefList::AddKey(ObjectKey key) {
    ObjectRefNode* node = NewOrNull<ObjectRefNode>();
    if (node) {
        node->Set(key);
        Add(node);
    }
    return node;
}

ObjectRefNode::ObjectRefNode() : IdNode(), m_flag(1), m_count(0) {
    m_key.m_value = 0;
}

// The copy constructor clears m_flag instead of copying it.
ObjectRefNode::ObjectRefNode(const ObjectRefNode& other) : IdNode(), m_flag(0), m_count(other.m_count), m_key(other.m_key) {}

ObjectRefNode::~ObjectRefNode() = default;

int ObjectRefNode::Read(istream* in) {
    ReadValue32(in, &m_flag);
    ReadValue32(in, &m_count);
    return m_key.Read(in);
}

int ObjectRefNode::Write(ostream* out) {
    WriteValue32(out, &m_flag);
    WriteValue32(out, &m_count);
    return m_key.Write(out);
}

int ObjectRefNode::Checksum(Crc32* crc) {
    crc->AddInt(m_flag);
    crc->AddInt(m_count);
    m_key.Checksum(crc);
    return 1;
}

int ObjectRefNode::Set(const ObjectKey& key) {
    m_key = key;
    m_count = 0;
    return 1;
}

ObjectKey* ObjectRefNode::GetKey(ObjectKey* out) const {
    out->m_value = m_key.m_value;
    return out;
}
