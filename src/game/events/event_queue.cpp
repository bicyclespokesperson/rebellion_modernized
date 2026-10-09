#include "core/reb_abi.h"
#include "events/event_queue.h"

#include "core/utility_classes.h"
#include "core/vptr_store.h"

namespace {

EventBase* asEvent(AvlNode* node) { return static_cast<EventBase*>(node); }

}  // namespace

// ---- EventQueue ------------------------------------------------------------------------------------------------

EventQueue::EventQueue() : OrderedTree(), m_flag0c(1) {}

EventQueue::~EventQueue() {
    OrderedTree::Clear();
    REB_KEEP_VPTR_STORE();
}

int EventQueue::Read(istream* in) {
    OrderedTree::Clear();
    int count;
    ReadValue32(in, &count);
    while (count != 0) {
        --count;
        int kind;
        ReadValue32(in, &kind);
        EventBase* event = static_cast<EventBase*>(CreateEventById(kind));
        if (event) {
            event->Read(in);
            Add(event);
        }
    }
    return 1;
}

int EventQueue::Write(ostream* out) {
    uint32_t count = static_cast<uint16_t>(Count());
    WriteValue32(out, &count);
    for (AvlNode* node = m_first; node; node = node->m_successor) {
        EventBase* event = asEvent(node);
        int kind = event->GetKind();
        WriteValue32(out, &kind);
        event->Write(out);
    }
    return 1;
}

int EventQueue::Checksum(Crc32* crc) {
    crc->AddInt(Count());
    for (AvlNode* node = m_first; node; node = node->m_successor) asEvent(node)->Checksum(crc);
    return 1;
}

int EventQueue::Validate() {
    if (m_root) CountSubtree(m_root);
    return 1;
}

int EventQueue::RunNext() {
    if (GetNext() == nullptr) return 0;
    return RunFirst();
}

int EventQueue::RunFirst() {
    int ran = 0;
    AvlNode* first = m_first;
    if (first) {
        Remove(first);
        ran = 1;
        EventBase* event = asEvent(first);
        if (event->Execute() != 0) {
            Add(event);
            return 1;
        }
        delete event;
    }
    return ran;
}

void EventQueue::Add(EventBase* event) {
    if (event) Insert(event);
}

int REB_STDCALL EventQueue::CountSubtree(AvlNode* node) {
    int count = 1;
    if (node->m_left) count = CountSubtree(node->m_left) + 1;
    if (node->m_right) count += CountSubtree(node->m_right);
    return count;
}

// ---- TimerQueue ------------------------------------------------------------------------------------------------

TimerQueue::TimerQueue() : EventQueue() {}

TimerQueue::~TimerQueue() { REB_KEEP_VPTR_STORE(); }

int TimerQueue::IsLess(AvlNode* a, AvlNode* b) {
    if (a == nullptr || b == nullptr) return 1;
    return static_cast<TimedEvent*>(static_cast<EventBase*>(a))->m_dueDay <
           static_cast<TimedEvent*>(static_cast<EventBase*>(b))->m_dueDay;
}

EventBase* TimerQueue::GetNext() {
    AvlNode* first = m_first;
    if (first) {
        int due = static_cast<TimedEvent*>(static_cast<EventBase*>(first))->m_dueDay;
        if (CallGameHook2B08() < due) first = nullptr;
    }
    return static_cast<EventBase*>(first);
}

// ---- CommandQueue ----------------------------------------------------------------------------------------------

CommandQueue::CommandQueue() : EventQueue() {}

CommandQueue::~CommandQueue() { REB_KEEP_VPTR_STORE(); }

int CommandQueue::IsLess(AvlNode*, AvlNode*) { return 0; }

EventBase* CommandQueue::GetNext() { return static_cast<EventBase*>(m_first); }

void CommandQueue::AddCommand(EventBase* event) { Add(event); }

// ---- TypeQueue -------------------------------------------------------------------------------------------------

TypeQueue::TypeQueue() : EventQueue() {}

TypeQueue::~TypeQueue() { REB_KEEP_VPTR_STORE(); }

int TypeQueue::IsLess(AvlNode* a, AvlNode* b) {
    if (a == nullptr || b == nullptr) return 1;
    return static_cast<PlainEvent*>(a)->GetPriority() < static_cast<PlainEvent*>(b)->GetPriority();
}

EventBase* TypeQueue::GetNext() { return static_cast<EventBase*>(m_first); }
