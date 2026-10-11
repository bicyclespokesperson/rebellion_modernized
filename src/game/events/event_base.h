#pragma once
// The event/command class family (0x54eee0-0x54f5b0, 0x594a30-0x594c60, 0x585fe0-0x5866c0): everything the event
// queues (event_queue.h) schedule and the factory table at DAT_006b92f0 builds by id.
//
//   EventBase (VT 0x662038)         AvlNode + context + object key; slots 1-9 are filled by the concrete classes
//    +- TimedEvent (0x662060)       + due day (+0x40); the TimerQueue is ordered by it
//    |   +- TimerEvent (0x66a230)   recurring timers ids 0x380-0x396 (timer_event.h)
//    +- Command (0x662088)          + ActivityRequest (+0x40): what an order's commands are
//    +- PlainEvent (0x6620b0)       nothing added
//        +- KeyedEvent (0x66ba18)   + two object keys and an int
//            +- SourceEvent (0x66a268) + two ints          (events 0x300-0x320, UI-facing events)
//            +- TwoKeyEvent (0x66a298) + two object keys
#include <cstdint>

#include "core/containers.h"
#include "core/stream_io.h"
#include "gameobj/game_hooks.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_key.h"
#include "gameobj/status_code.h"

class Crc32;

class EventBase : public AvlNode {
public:
    EventBase();                         // 0x54eee0
    EventBase(const EventBase& other);   // 0x54ef60
    virtual ~EventBase();                // 0x54efe0

    virtual EventBase* Clone() = 0;                    // 1
    virtual void Read(istream* in) = 0;                 // 2: body ReadBase (0x54f040)
    virtual void Write(ostream* out) = 0;               // 3: body WriteBase (0x54f080)
    virtual void Checksum(Crc32* crc) = 0;              // 4: body ChecksumBase (0x54f0c0)
    virtual int IsChecked() { return 0; }              // 5: 0x6158b0
    virtual int CheckEnabled(StatusCode* status) = 0;  // 6
    virtual int CheckValid(StatusCode* status) = 0;    // 7
    virtual int Execute() = 0;                         // 8: queued events return non-zero to be queued again
    virtual int GetKind() = 0;                         // 9: the id the factory table knows the class by

    // The fields common to every event: flag, +0x1c, the object key and the context.
    void ReadBase(istream* in);       // 0x54f040
    void WriteBase(ostream* out);     // 0x54f080
    void ChecksumBase(Crc32* crc);    // 0x54f0c0

    int m_flag;                // +0x18, 1
    int m_unk1c;               // +0x1c
    NotifyContext m_context;   // +0x20
    ObjectKey m_key;           // +0x3c, the object the event acts on
};

class TimedEvent : public EventBase {
public:
    TimedEvent();                          // 0x54f1b0
    TimedEvent(const TimedEvent& other);   // 0x54f200
    virtual ~TimedEvent();                 // 0x54f1f0

    virtual void Read(istream* in) override = 0;         // body 0x54f230
    virtual void Write(ostream* out) override = 0;       // body 0x54f260
    virtual void Checksum(Crc32* crc) override = 0;      // body 0x54f290

    int ReadTimed(istream* in);      // 0x54f230
    int WriteTimed(ostream* out);    // 0x54f260
    int ChecksumTimed(Crc32* crc);   // 0x54f290

    int m_dueDay;  // +0x40, the game day the event is due
};

// 0x54f2c0: an event that carries an ActivityRequest. The order commands (events/command classes 0x200-0x242) derive
// from it.
class Command : public EventBase {
public:
    Command();                       // 0x54f2c0
    Command(const Command& other);   // 0x54f380
    virtual ~Command();              // 0x54f330

    virtual Command* Clone() override = 0;              // 1: a fresh copy, 0x50 bytes

    virtual void Read(istream* in) override = 0;         // body 0x54f470
    virtual void Write(ostream* out) override = 0;       // body 0x54f490
    virtual void Checksum(Crc32* crc) override = 0;      // body 0x54f4b0

    void ReadCommand(istream* in);      // 0x54f470
    void WriteCommand(ostream* out);    // 0x54f490
    int ChecksumCommand(Crc32* crc);   // 0x54f4b0

    // Copies the request into the command (+0x40).
    void SetRequest(const ActivityRequest* request);  // 0x54f3e0
    // If the request's argument is set, records it and the command kind in the command's context (+0x20).
    void StampContext();                              // 0x54f3f0

    ActivityRequest m_request;  // +0x40
};

class PlainEvent : public EventBase {
public:
    PlainEvent();                          // 0x54f4e0
    PlainEvent(const PlainEvent& other);   // 0x54f530
    virtual ~PlainEvent();                 // 0x54f520

    virtual void Read(istream* in) override = 0;         // body 0x54f550
    virtual void Write(ostream* out) override = 0;       // body 0x54f570
    virtual void Checksum(Crc32* crc) override = 0;      // body 0x54f590

    // Slot 10, introduced by the concrete classes (PlainEvent's own vtable stops at slot 9): the sort key of the
    // TypeQueue. 0x48af30 returns 0x10 (KeyedEvent and its subclasses), 0x48bc60 returns 0x100.
    virtual int GetPriority() { return 0x10; }

    int ReadPlain(istream* in);      // 0x54f550
    int WritePlain(ostream* out);    // 0x54f570
    int ChecksumPlain(Crc32* crc);   // 0x54f590
};

class KeyedEvent : public PlainEvent {
public:
    KeyedEvent();                          // 0x594a30
    KeyedEvent(const KeyedEvent& other);   // 0x594b10
    virtual ~KeyedEvent();                 // 0x594ab0

    virtual void Read(istream* in) override = 0;         // body 0x594b80
    virtual void Write(ostream* out) override = 0;       // body 0x594bd0
    virtual void Checksum(Crc32* crc) override = 0;      // body 0x594c20

    int ReadKeyed(istream* in);      // 0x594b80
    int WriteKeyed(ostream* out);    // 0x594bd0
    int ChecksumKeyed(Crc32* crc);   // 0x594c20

    ObjectKey m_keyA;  // +0x40
    ObjectKey m_keyB;  // +0x44
    int m_value;       // +0x48
};

class SourceEvent : public KeyedEvent {
public:
    SourceEvent();                           // 0x5863b0
    SourceEvent(const SourceEvent& other);   // 0x586400
    virtual ~SourceEvent();                  // 0x5863f0

    virtual void Read(istream* in) override = 0;         // body 0x586430
    virtual void Write(ostream* out) override = 0;       // body 0x586470
    virtual void Checksum(Crc32* crc) override = 0;      // body 0x5864b0

    int ReadSource(istream* in);      // 0x586430
    int WriteSource(ostream* out);    // 0x586470
    int ChecksumSource(Crc32* crc);   // 0x5864b0

    int m_valueA;  // +0x4c
    int m_valueB;  // +0x50
};

class TwoKeyEvent : public KeyedEvent {
public:
    TwoKeyEvent();                           // 0x5864e0
    TwoKeyEvent(const TwoKeyEvent& other);   // 0x5865c0
    virtual ~TwoKeyEvent();                  // 0x586560

    virtual void Read(istream* in) override = 0;         // body 0x586630
    virtual void Write(ostream* out) override = 0;       // body 0x586660
    virtual void Checksum(Crc32* crc) override = 0;      // body 0x586690

    void ReadTwoKey(istream* in);      // 0x586630
    void WriteTwoKey(ostream* out);    // 0x586660
    int ChecksumTwoKey(Crc32* crc);   // 0x586690

    ObjectKey m_keyC;  // +0x4c
    ObjectKey m_keyD;  // +0x50
};

// Factory table (DAT_006b92f0): id -> function that builds an empty event of that class.
int RegisterEventFactory(int kind, void* (*factory)());   // 0x54f140
// The factories return the class they build; the table stores plain pointers.
template <class T>
int RegisterEventFactory(int kind, T* (*factory)()) {
    return RegisterEventFactory(kind, reinterpret_cast<void* (*)()>(factory));
}
void* CreateEventById(int kind);                          // 0x54f100
