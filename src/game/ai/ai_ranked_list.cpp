#include "ai/ai_ranked_list.h"

#include "data/random.h"
#include "core/checked_new.h"

namespace {

constexpr int kTieBreakRange = 10;
constexpr int kTieBreakThreshold = 5;

}  // namespace

// ---- AiRankedItem -------------------------------------------------------------------------------------------------

AiRankedItem::AiRankedItem() : LinkNode(), m_key(), m_score(0), m_rank(0) {}

AiRankedItem::AiRankedItem(const ObjectKey& key, int score) : LinkNode(), m_key(key), m_score(score), m_rank(0) {}

AiRankedItem::AiRankedItem(AiRankedItem* other) : LinkNode(), m_key() {
    ObjectKey copy;
    m_key = *other->GetKey(&copy);
    m_score = other->m_score;
    m_rank = other->m_rank;
}

AiRankedItem::~AiRankedItem() = default;

void AiRankedItem::Read(istream* in) {
    m_key.Read(in);
    ReadValue32(in, &m_score);
    ReadValue32(in, &m_rank);
}

void AiRankedItem::Write(ostream* out) {
    m_key.Write(out);
    WriteValue32(out, &m_score);
    WriteValue32(out, &m_rank);
}

AiRankedItem* AiRankedItem::Clone() { return NewOrNull<AiRankedItem>(this); }

ObjectKey* AiRankedItem::GetKey(ObjectKey* out) {
    out->m_value = m_key.m_value;
    return out;
}

// ---- AiRankedList -------------------------------------------------------------------------------------------------

AiRankedList::AiRankedList() : SortedLinkList(), IdLinkNode(), m_descending(0) {}

AiRankedList::AiRankedList(int descending) : SortedLinkList(), IdLinkNode(), m_descending(descending) {}

AiRankedList::~AiRankedList() {
    SortedLinkList::Clear();

}

void AiRankedList::Clear() { SortedLinkList::Clear(); }

int AiRankedList::IsLess(LinkNode* a, LinkNode* b) {
    if (!a || !b) return 0;
    const AiRankedItem* left = static_cast<AiRankedItem*>(a);
    const AiRankedItem* right = static_cast<AiRankedItem*>(b);
    if (left->m_score < right->m_score) return m_descending == 1;
    if (right->m_score < left->m_score) return m_descending != 1;
    return GameplayRandomNext(kTieBreakRange) < kTieBreakThreshold;
}

void AiRankedList::Read(istream* in) {
    IdLinkNode::Read(in);
    ReadValue32(in, &m_descending);
    uint32_t count;
    ReadValue32(in, &count);
    for (uint32_t i = 0; i < count; ++i) {
        AiRankedItem* item = NewOrNull<AiRankedItem>();
        if (item) {
            item->Read(in);
            AddItem(item);
        }
    }
}

void AiRankedList::Write(ostream* out) {
    IdLinkNode::Write(out);
    WriteValue32(out, &m_descending);
    int count = m_count;
    WriteValue32(out, &count);
    for (AiRankedItem* item = First(); item; item = static_cast<AiRankedItem*>(item->m_next)) item->Write(out);
}

void AiRankedListRead(IdLinkNode* base, istream* in) { static_cast<AiRankedList*>(base)->AiRankedList::Read(in); }

void AiRankedListWrite(IdLinkNode* base, ostream* out) {
    static_cast<AiRankedList*>(base)->AiRankedList::Write(out);
}

uint32_t AiRankedList::AddItem(AiRankedItem* item) {
    ObjectKey key;
    if (item) {
        ObjectKey copy;
        key = *item->GetKey(&copy);
        if (key.Id() != 0 && key.Family() != 0) Insert(item);
    }
    return key.Id();
}

AiRankedItem* AiRankedList::RemoveKey(const ObjectKey* key) {
    AiRankedItem* item = FindKey(key);
    if (item) Remove(item);
    return item;
}

namespace {

__attribute__((noinline)) ObjectKey ReadItemKey(AiRankedItem* item) {
    ObjectKey copy;
    return *item->GetKey(&copy);
}

}  // namespace

AiRankedItem* AiRankedList::FindKey(const ObjectKey* key) {
    AiRankedItem* item = First();
    while (item) {
        if (key->m_value == ReadItemKey(item).m_value) return item;
        item = static_cast<AiRankedItem*>(item->m_next);
    }
    return nullptr;
}

AiRankedItem* AiRankedList::First() const { return static_cast<AiRankedItem*>(m_head); }

void AiRankedList::Number() {
    int rank = 1;
    for (AiRankedItem* item = First(); item; item = static_cast<AiRankedItem*>(item->m_next)) item->m_rank = rank++;
}
