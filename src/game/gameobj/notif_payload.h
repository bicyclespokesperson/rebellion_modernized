#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "core/stream_io.h"
#include "gameobj/object_key.h"

// Base of the small notification payload classes (VT 0x65ecf8; the leaf classes' vtables are 0x661300-0x661380, the
// ObjectKey-carrying subclass built at 0x583fe0 adds a key at +0x20): a LinkNode followed by five ints that the leaf
// classes' Read/Write call ReadFields/WriteFields for. Slots 3, 6 and 7 are pure; the UI notification handlers implement them.
class PlayerController;

class NotifPayload : public LinkNode {
public:
    NotifPayload();
    virtual ~NotifPayload();
    virtual int GetKind() = 0;
    virtual int Virtual04() { return 1; }  // the delivery mode of the notification (1 unless a handler overrides it)
    virtual void Virtual05() {}
    virtual void Dispatch(PlayerController* controller) = 0;  // the UI handlers (events/ui_notifs.h) tell a controller
    virtual void DispatchToSink(void* sink) = 0;              // dead code in the original, see events/ui_notif_helpers.h

    // LinkNode::Read (a no-op), then the five ints; always 1.
    int ReadFields(istream* in);
    int WriteFields(ostream* out);

    int m_value0c;  // +0x0c
    int m_value10;  // +0x10
    int m_value14;  // +0x14
    int m_value18;  // +0x18
    int m_value1c;  // +0x1c
};

// 0x583fe0 (VT 0x66a130): a payload that names the object it is about (the key at +0x20). Still abstract; the leaf
// classes (events/notif_event_payloads.h) call ReadKeyed/WriteKeyed from their own Read/Write.
class KeyedNotifPayload : public NotifPayload {
public:
    KeyedNotifPayload();                 // 0x583fe0
    virtual ~KeyedNotifPayload();        // 0x584030 (deleting), 0x584050

    int ReadKeyed(istream* in);          // 0x5840a0
    int WriteKeyed(ostream* out);        // 0x5840d0

    ObjectKey m_key;  // +0x20
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(NotifPayload, 0x20);
REB_ASSERT_OFFSET(NotifPayload, m_value0c, 0x0c);
REB_ASSERT_OFFSET(NotifPayload, m_value1c, 0x1c);
REB_ASSERT_SIZE(KeyedNotifPayload, 0x24);
#pragma clang diagnostic pop
