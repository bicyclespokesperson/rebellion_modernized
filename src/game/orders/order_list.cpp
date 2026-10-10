#include "orders/order_list.h"

#include "core/vptr_store.h"

MessageTree::MessageTree() : IdAvlTree() { m_unk0c = 1; }

MessageTree::~MessageTree() { REB_KEEP_VPTR_STORE(); }

void MessageTree::Read(istream* in) {
    uint32_t count;
    ReadValue32(in, &count);
    while (count != 0) {
        --count;
        int kind;
        ReadValue32(in, &kind);
        MessageBase* message = static_cast<MessageBase*>(CreateMessageByKind(kind));
        if (message) {
            message->Read(in);
            Adopt(message);
        }
    }
}

void MessageTree::Write(ostream* out) {
    uint32_t count = static_cast<uint16_t>(Count());
    WriteValue32(out, &count);
    for (AvlNode* node = First(); node; node = node->m_successor) {
        MessageBase* message = static_cast<MessageBase*>(node);
        int kind = message->GetKind();
        WriteValue32(out, &kind);
        message->Write(out);
    }
}

void MessageTree::Adopt(IdNode* message) {
    if (!message) return;
    if (message->m_id == 0 || FindById(message->m_id)) {
        delete message;
        return;
    }
    AddWithId(message);
}

IdNode* MessageTree::Detach(uint32_t id) { return DetachById(id); }
