#include "ai/ai_selector.h"

#include "core/checked_new.h"
#include "core/vptr_store.h"

namespace {

AiRankedList* ListOf(LinkNode* node) {
    return node ? static_cast<AiRankedList*>(static_cast<IdLinkNode*>(node)) : nullptr;
}

AiRankedList* NextList(AiRankedList* list) { return ListOf(static_cast<IdLinkNode*>(list)->m_next); }

}  // namespace

AiRankedSelector::AiRankedSelector() : IdSortedLinkList() {}

AiRankedSelector::~AiRankedSelector() {
    Reset();
    REB_KEEP_VPTR_STORE();
}

void AiRankedSelector::Read(istream* in) {
    uint32_t nextId;
    ReadValue32(in, &nextId);
    m_nextId = nextId;
    uint32_t count;
    ReadValue32(in, &count);
    for (uint32_t i = 0; i < count; ++i) {
        AiRankedList* list = NewOrNull<AiRankedList>();
        if (list) {
            static_cast<IdLinkNode*>(list)->Read(in);
            Add(list);
        }
    }
}

void AiRankedSelector::Write(ostream* out) {
    uint32_t nextId = m_nextId;
    WriteValue32(out, &nextId);
    int count = m_count;
    WriteValue32(out, &count);
    for (AiRankedList* list = ListOf(m_head); list; list = NextList(list)) static_cast<IdLinkNode*>(list)->Write(out);
}

void AiRankedSelector::Add(AiRankedList* list) {
    if (list)
        AddWithId(list);
    else
        AddWithId(nullptr);
}

int AiRankedSelector::AddRankOf(const ObjectKey* key, int* total, AiRankedList* list) {
    int found = 0;
    AiRankedItem* item = list->First();
    while (item && !found) {
        ObjectKey itemKey;
        if (item->GetKey(&itemKey)->m_value == key->m_value) {
            found = 1;
            *total += item->m_rank;
        } else {
            item = static_cast<AiRankedItem*>(item->m_next);
        }
    }
    return found;
}

ObjectKey* AiRankedSelector::PickBest(ObjectKey* out) {
    ObjectKey best;
    ObjectKey current;
    int bestScore = 0;
    int score = 0;
    AiRankedList* first = ListOf(m_head);
    if (first) {
        AiRankedItem* item = first->First();
        while (item) {
            int inAll = 1;
            ObjectKey itemKey;
            current = *item->GetKey(&itemKey);
            score = item->m_rank;
            for (AiRankedList* other = NextList(first); other; other = NextList(other)) {
                if (!inAll) break;
                inAll = AddRankOf(&current, &score, other);
            }
            if (!inAll) {
                item = static_cast<AiRankedItem*>(item->m_next);
                AiRankedItem* removed = first->RemoveKey(&current);
                delete removed;
            } else {
                if (bestScore < score) {
                    bestScore = score;
                    best = current;
                }
                item = static_cast<AiRankedItem*>(item->m_next);
            }
        }
    }
    if (best.Family() != 0 && best.Family() <= 0xfe) {
        AiRankedItem* removed = first->RemoveKey(&best);
        delete removed;
    }
    *out = best;
    return out;
}
