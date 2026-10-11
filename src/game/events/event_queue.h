#pragma once
// The event queues (0x54ea70-0x54eed0): ordered trees of EventBase nodes. The game manager keeps three of them
// (+0xa8 a TimerQueue, +0xac a CommandQueue-like queue, +0xb0 a TypeQueue) and serves them in order; orders keep a
// CommandQueue of the commands that carry them out.
#include <cstdint>

#include "core/containers.h"
#include "core/stream_io.h"
#include "events/event_base.h"

class Crc32;

// Abstract base (VT 0x661fb8). Slot 2 (IsLess, from OrderedTree) decides where Add puts a node; slot 7 returns the
// node that is next to run.
class EventQueue : public OrderedTree {
public:
    EventQueue();           // 0x54ea70
    virtual ~EventQueue();  // 0x54eab0

    virtual int Read(istream* in);                  // 3: 0x54eb00 clears, then reads count + (kind, event) pairs
    virtual int Write(ostream* out);                // 4: 0x54eb80
    virtual int Checksum(Crc32* crc);               // 5: 0x54ebf0
    virtual int Validate();                         // 6: 0x54ec30 walks the tree (the result is dropped)
    virtual EventBase* GetNext() = 0;               // 7: the event to run next, or null

    // Pops GetNext() if there is one and runs it.
    int RunNext();  // 0x54ec70
    // Pops the first event and runs it; if its Execute returns non-zero it goes back in the queue, otherwise it is
    // deleted. Returns 1 if there was an event.
    int RunFirst();  // 0x54ec90
    void Add(EventBase* event);  // 0x54ece0
    // Number of nodes in the subtree below `node`, itself included.
    static int CountSubtree(AvlNode* node);  // 0x54ed00

    int m_flag0c;  // +0x0c, 1
};

// Ordered by due day (TimedEvent +0x40); GetNext returns the first event only if it is due today (0x54edc0).
class TimerQueue : public EventQueue {
public:
    TimerQueue();           // 0x54ed40
    virtual ~TimerQueue();  // 0x54ed80
    virtual int IsLess(AvlNode* a, AvlNode* b) override;  // 0x54ed90, true if either is null
    virtual EventBase* GetNext() override;                // 0x54edc0
};

// An order's commands: first in, first out (IsLess is always false); GetNext is the first one.
class CommandQueue : public EventQueue {
public:
    CommandQueue();           // 0x54ede0
    virtual ~CommandQueue();  // 0x54ee20
    virtual int IsLess(AvlNode* a, AvlNode* b) override;  // 0x5c61f0
    virtual EventBase* GetNext() override;                // 0x54eed0

    // Forwards to Add (0x54ee30).
    void AddCommand(EventBase* event);

    Command* FirstCommand() const { return static_cast<Command*>(m_first); }
};

// Ordered by the events' priority (slot 10, 0x10 for the events found so far); GetNext is the first one.
class TypeQueue : public EventQueue {
public:
    TypeQueue();           // 0x54ee40
    virtual ~TypeQueue();  // 0x54ee80
    virtual int IsLess(AvlNode* a, AvlNode* b) override;  // 0x54ee90, true if either is null
    virtual EventBase* GetNext() override;                // 0x54eed0
};
