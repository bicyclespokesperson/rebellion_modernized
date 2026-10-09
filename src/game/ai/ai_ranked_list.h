#pragma once
#include "core/reb_abi.h"
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "gameobj/object_key.h"

// Ranked candidate lists used by the AI's selectors (0x41be00-0x41c590).
//
//   AiRankedItem   a key and a score, VT 0x658a78 (0x18 bytes); the rank (1 = best) is filled in by Number()
//   AiRankedList   SortedLinkList of AiRankedItems ordered by score, VT 0x658a58 (0x20 bytes). The IdLinkNode base at
//                  +0xc has its own vtable (0x658a68) that carries the list's Read/Write: it is how the list is saved.
//                  m_descending (+0x1c) selects the order: 1 sorts the highest score first. Equal scores are ordered by
//                  a draw from the session random generator.

class AiRankedItem : public LinkNode {
public:
    AiRankedItem();                                  // 0x41c2e0
    AiRankedItem(const ObjectKey& key, int score);   // 0x41c360
    explicit AiRankedItem(AiRankedItem* other);      // 0x41c3d0
    virtual ~AiRankedItem();                         // 0: 0x41c340 (deleting), 0x41c460
    virtual void Read(istream* in) override;         // 1: 0x41c510
    virtual void Write(ostream* out) override;       // 2: 0x41c540
    virtual AiRankedItem* Clone();                   // 3: 0x41c4b0

    ObjectKey* GetKey(ObjectKey* out);  // 0x41c2c0

    ObjectKey m_key;  // +0x0c
    int m_score;      // +0x10
    int m_rank;       // +0x14
};

class AiRankedList : public SortedLinkList, public IdLinkNode {
public:
    AiRankedList();                         // 0x41be00
    explicit AiRankedList(int descending);  // 0x41be80
    virtual ~AiRankedList();                // 0: 0x41be60 (deleting), 0x41bee0
    virtual int IsLess(LinkNode* a, LinkNode* b) override;  // 1: 0x41c250
    virtual void Clear();                   // 2: 0x41c220 (jumps to SortedLinkList::Clear)

    // IdLinkNode vtable slots 1 and 2: the original functions take the IdLinkNode subobject.
    virtual void Read(istream* in) override;    // 0x41bf50
    virtual void Write(ostream* out) override;  // 0x41c010

    uint32_t AddItem(AiRankedItem* item);           // 0x41c070: key id part, or 0 if the key is incomplete
    AiRankedItem* RemoveKey(const ObjectKey* key);  // 0x41c120
    AiRankedItem* FindKey(const ObjectKey* key);    // 0x41c150
    AiRankedItem* First() const;                    // 0x41c210
    void Number();                                  // 0x41c230

    int m_descending;  // +0x1c
};

void REB_THISCALL AiRankedListRead(IdLinkNode* base, istream* in);    // 0x41bf50
void REB_THISCALL AiRankedListWrite(IdLinkNode* base, ostream* out);  // 0x41c010

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(AiRankedItem, 0x18);
REB_ASSERT_OFFSET(AiRankedItem, m_key, 0x0c);
REB_ASSERT_OFFSET(AiRankedItem, m_score, 0x10);
REB_ASSERT_OFFSET(AiRankedItem, m_rank, 0x14);
REB_ASSERT_SIZE(AiRankedList, 0x20);
REB_ASSERT_OFFSET(AiRankedList, m_descending, 0x1c);
#pragma clang diagnostic pop
