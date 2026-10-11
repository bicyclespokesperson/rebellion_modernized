#include "events/game_manager_state.h"

#include <cstring>
#include <new>

#include "core/checked_new.h"
#include "core/iostream_lib.h"
#include "core/utility_classes.h"
#include "events/events_decl.h"


extern const char *g_emptyText;  // 0x6b120c
void* CreateKeyedObject(int key);  // 0x51f730

// ---- legacy option blocks ---------------------------------------------------------------------------------------

void LegacyBlockA::Reset() {
    m_value[0] = 0;
    m_value[1] = 0;
    m_value[2] = 1;
    for (int i = 3; i < 8; ++i) m_value[i] = 0;
    m_value[8] = 1;
    m_value[9] = 1;
    m_value[10] = 0;
    m_value[11] = 0;
    m_value[12] = 0;
    m_value[13] = 10;
    m_value[14] = 10;
    m_value[15] = 0x78;
    for (int i = 16; i < 20; ++i) m_value[i] = 0;
}

int LegacyBlockA::Read(istream* in) {
    for (int32_t& value : m_value) StreamRead32(in, &value);
    return 1;
}

int LegacyBlockA::Write(ostream* out) {
    for (int32_t& value : m_value) StreamWrite32(out, &value);
    return 1;
}

LegacyBlockB::LegacyBlockB() {
    for (int32_t& value : m_value) value = -1;
}

int LegacyBlockB::Read(istream* in) {
    for (int32_t& value : m_value) StreamRead32(in, &value);
    return 1;
}

int LegacyBlockB::Write(ostream* out) {
    for (int32_t& value : m_value) StreamWrite32(out, &value);
    return 1;
}

int ReadLegacyBlocks(istream* in) {
    g_legacyBlockA.Read(in);
    g_legacyBlockB[0].Read(in);
    g_legacyBlockB[1].Read(in);
    g_legacyBlockB[2].Read(in);
    return 1;
}

int WriteLegacyBlocks(ostream* out) {
    g_legacyBlockA.Write(out);
    g_legacyBlockB[0].Write(out);
    g_legacyBlockB[1].Write(out);
    g_legacyBlockB[2].Write(out);
    return 1;
}

// ---- legacy text slots ------------------------------------------------------------------------------------------

void ResetLegacyTexts() {
    g_legacyTextCount = 0;
    g_legacyTextExtra = 0;
    for (String& text : g_legacyTexts) text.Clear();
}

void ReadLegacyTexts(istream* in) {
    ReadValue32(in, &g_legacyTextCount);
    ReadValue32(in, &g_legacyTextExtra);
    // BUG(original): the saved count is not checked against the ten slots.
    String* text = g_legacyTexts;
    for (int i = 0; i < g_legacyTextCount; ++i, ++text) text->Read(in);
}

void WriteLegacyTexts(ostream* out) {
    WriteValue32(out, &g_legacyTextCount);
    WriteValue32(out, &g_legacyTextExtra);
    String* text = g_legacyTexts;
    for (int i = 0; i < g_legacyTextCount; ++i, ++text) text->Write(out);
}



LegacyTextTable::LegacyTextTable() {
    SetDefaults();
    AllocateSlots();
}

LegacyTextTable::~LegacyTextTable() {
    Reset();

}

int LegacyTextTable::ReadFrom(istream* in) {
    Reset();
    StreamRead32(in, &m_count);
    StreamRead32(in, &m_width);
    StreamRead32(in, &m_unk0c);
    int hasSlots = 0;
    StreamRead32(in, &hasSlots);
    if (hasSlots != 0) {
        AllocateSlots();
        if (m_slots) istream_read(in, m_slots, m_width * m_count);
    }
    return 1;
}

int LegacyTextTable::Read(istream* in) { return ReadFrom(in); }

void LegacyTextTable::SetDefaults() {
    m_count = 100;
    m_width = 200;
    m_unk0c = 0;
    m_slots = nullptr;
}

void LegacyTextTable::Reset() {
    m_count = 0;
    m_width = 0;
    m_unk0c = 0;
    if (m_slots) {
        operator delete(m_slots);
        m_slots = nullptr;
    }
}

void LegacyTextTable::AllocateSlots() {
    if (m_count == 0 || m_width == 0 || m_slots != nullptr) return;
    m_slots = static_cast<char*>(operator new(m_width * m_count));
    if (!m_slots) return;
    for (int i = 0; i < m_count; ++i) memcpy(m_slots + i * m_width, g_emptyText, strlen(g_emptyText) + 1);
}

// ---- posted-event queues ----------------------------------------------------------------------------------------

PostedEventQueue::PostedEventQueue() : NodeQueue(), m_flag(1) {}

PostedEventQueue::~PostedEventQueue() {
    NodeQueue::Clear();

}

int PostedEventQueue::Read(istream* in) {
    int ok = 1;
    NodeQueue::Clear();
    ReadValue32(in, &m_flag);
    int count;
    ReadValue32(in, &count);
    for (int i = 0; ok && i < count; ++i) {
        int kind;
        ReadValue32(in, &kind);
        NotifPayload* payload = static_cast<NotifPayload*>(CreateKeyedObject(kind));
        ok = payload != nullptr;
        if (ok) {
            payload->Read(in);
            Enqueue(payload);
        }
    }
    return ok;
}

int PostedEventQueue::Write(ostream* out) {
    WriteValue32(out, &m_flag);
    int count = m_count;
    WriteValue32(out, &count);
    for (LinkNode* node = m_head; node; node = node->m_next) {
        NotifPayload* payload = static_cast<NotifPayload*>(node);
        int kind = payload->GetKind();
        WriteValue32(out, &kind);
        payload->Write(out);
    }
    return 1;
}

// ---- tick table -------------------------------------------------------------------------------------------------

TickTable::TickTable() : AvlTree(), m_ready(1) {}

TickTable::~TickTable() {
    AvlTree::Clear();

}

int TickTable::IsLess(AvlNode* a, AvlNode* b) {
    int less = 0;
    if (a && b) {
        SeedKey first = static_cast<SeedKeyCountNode*>(a)->m_key;
        SeedKey second = static_cast<SeedKeyCountNode*>(b)->m_key;
        less = first < second;
    }
    return less;
}

int TickTable::Read(istream* in) { return ReadNodes(in); }

int TickTable::ReadNodes(istream* in) {
    Clear();
    ReadValue32(in, &m_ready);
    uint32_t count;
    ReadValue32(in, &count);
    for (uint32_t i = 0; i < count; ++i) {
        SeedKeyCountNode* node = NewOrNull<SeedKeyCountNode>();
        node->Read(in);
        AddNode(node);
    }
    return 1;
}

int TickTable::Write(ostream* out) { return WriteNodes(out); }

int TickTable::WriteNodes(ostream* out) {
    WriteValue32(out, &m_ready);
    uint32_t count = static_cast<uint16_t>(Count());
    WriteValue32(out, &count);
    for (AvlNode* node = First(); node; node = node->m_successor) static_cast<SeedKeyCountNode*>(node)->Write(out);
    return 1;
}

int TickTable::Checksum(Crc32* crc) {
    int result = 1;
    crc->AddInt(m_ready);
    crc->AddInt(static_cast<uint16_t>(Count()));
    for (AvlNode* node = First(); node; node = node->m_successor) result = static_cast<SeedKeyCountNode*>(node)->Checksum(crc);
    return result;
}

SeedKeyCountNode* TickTable::Find(SeedKey* key) {
    AvlNode* cursor = m_root;
    SeedKeyCountNode* found = nullptr;
    while (cursor != nullptr && found == nullptr) {
        SeedKeyCountNode* node = static_cast<SeedKeyCountNode*>(cursor);
        SeedKey current;
        node->GetKey(&current);
        if (*key < current || *key == current) {
            if (*key < current) {
                cursor = node->m_left;
            } else {
                found = node;
            }
        } else {
            cursor = node->m_right;
        }
    }
    return found;
}

SeedKeyCountNode* TickTable::Create(SeedKey* key) {
    SeedKeyCountNode* node = NewOrNull<SeedKeyCountNode>(*key);
    AddNode(node);
    return node;
}

void TickTable::AddNode(SeedKeyCountNode* node) {
    if (node) Insert(node);
}
