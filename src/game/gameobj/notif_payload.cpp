#include "gameobj/notif_payload.h"

#include "core/vptr_store.h"

NotifPayload::NotifPayload() : LinkNode(), m_value0c(0), m_value10(0), m_value14(0), m_value18(0), m_value1c(0) {}

NotifPayload::~NotifPayload() { REB_KEEP_VPTR_STORE(); }

int NotifPayload::ReadFields(istream* in) {
    LinkNode::Read(in);
    ReadValue32(in, &m_value0c);
    ReadValue32(in, &m_value10);
    ReadValue32(in, &m_value14);
    ReadValue32(in, &m_value18);
    ReadValue32(in, &m_value1c);
    return 1;
}

int NotifPayload::WriteFields(ostream* out) {
    LinkNode::Write(out);
    WriteValue32(out, &m_value0c);
    WriteValue32(out, &m_value10);
    WriteValue32(out, &m_value14);
    WriteValue32(out, &m_value18);
    WriteValue32(out, &m_value1c);
    return 1;
}

KeyedNotifPayload::KeyedNotifPayload() : NotifPayload(), m_key() {}

KeyedNotifPayload::~KeyedNotifPayload() { REB_KEEP_VPTR_STORE(); }

int KeyedNotifPayload::ReadKeyed(istream* in) {
    int result = NotifPayload::ReadFields(in);
    if (result) m_key.Read(in);
    return result;
}

int KeyedNotifPayload::WriteKeyed(ostream* out) {
    int result = NotifPayload::WriteFields(out);
    if (result) m_key.Write(out);
    return result;
}
