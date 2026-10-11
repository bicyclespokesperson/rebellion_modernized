#include "core/containers.h"

#include <cstdlib>

void ReadValue16(istream*, void*) {}
void WriteValue16(ostream*, const void*) {}
void ReadValue32(istream*, void*) {}
void WriteValue32(ostream*, const void*) {}

namespace {

void Require(bool condition) {
    if (!condition) std::abort();
}

struct CountedNode final : LinkNode {
    explicit CountedNode(int& destroyed) : destroyed(destroyed) {}
    ~CountedNode() override {
        Require(m_prev == nullptr && m_next == nullptr);
        ++destroyed;
    }

    int& destroyed;
};

struct QueueClearNode final : LinkNode {
    QueueClearNode(NodeQueue& queue, int& destroyed, int& rejectedDestroyed)
        : queue(queue), destroyed(destroyed), rejectedDestroyed(rejectedDestroyed) {}
    ~QueueClearNode() override {
        Require(queue.m_head == nullptr && queue.m_tail == nullptr && queue.m_count == 0);
        Require(m_prev == nullptr && m_next == nullptr);
        queue.Clear();
        auto* rejected = new CountedNode(rejectedDestroyed);
        Require(queue.Enqueue(rejected) == 0);
        delete rejected;
        ++destroyed;
    }

    NodeQueue& queue;
    int& destroyed;
    int& rejectedDestroyed;
};

struct StackClearNode final : LinkNode {
    StackClearNode(NodeStack& stack, int& destroyed, int& rejectedDestroyed)
        : stack(stack), destroyed(destroyed), rejectedDestroyed(rejectedDestroyed) {}
    ~StackClearNode() override {
        Require(stack.m_top == nullptr);
        Require(m_prev == nullptr && m_next == nullptr);
        stack.Clear();
        auto* rejected = new CountedNode(rejectedDestroyed);
        Require(stack.Push(rejected) == 0);
        delete rejected;
        ++destroyed;
    }

    NodeStack& stack;
    int& destroyed;
    int& rejectedDestroyed;
};

struct UnorderedList final : SortedLinkList {
    int IsLess(LinkNode*, LinkNode*) override { return 0; }
};

void VerifyQueueOwnershipAndOrder() {
    int destroyed = 0;
    NodeQueue queue;
    NodeStack stack;
    auto* first = new CountedNode(destroyed);
    auto* second = new CountedNode(destroyed);

    Require(queue.Enqueue(nullptr) == 0);
    Require(queue.Enqueue(first) == 1);
    Require(queue.Enqueue(first) == 0);
    Require(stack.Push(first) == 0);
    Require(queue.Enqueue(second) == 1);
    Require(queue.m_head == first && queue.m_tail == second && queue.m_count == 2);
    Require(first->m_next == second && second->m_prev == first);

    LinkNode* dequeued = queue.Dequeue();
    Require(dequeued == first && dequeued->m_prev == nullptr && dequeued->m_next == nullptr);
    delete dequeued;
    Require(destroyed == 1 && queue.m_head == second && queue.m_tail == second && queue.m_count == 1);
    queue.Clear();
    Require(destroyed == 2 && queue.m_head == nullptr && queue.m_tail == nullptr && queue.m_count == 0);
}

void VerifyStackOwnershipAndOrder() {
    int destroyed = 0;
    NodeQueue queue;
    NodeStack stack;
    auto* bottom = new CountedNode(destroyed);
    auto* middle = new CountedNode(destroyed);
    auto* top = new CountedNode(destroyed);
    auto* foreign = new CountedNode(destroyed);

    Require(stack.Push(nullptr) == 0);
    Require(queue.Enqueue(foreign) == 1);
    Require(stack.Push(bottom) == 1);
    Require(queue.Enqueue(bottom) == 0);
    Require(stack.Push(middle) == 1);
    Require(stack.Push(top) == 1);
    Require(stack.m_top == top && top->m_next == middle && middle->m_next == bottom);
    Require(stack.RemoveAndDelete(foreign) == 0);
    Require(stack.RemoveAndDelete(middle) == 1);
    Require(destroyed == 1 && top->m_next == bottom && bottom->m_prev == top);

    Require(stack.Pop() == top);
    delete top;
    Require(stack.Pop() == bottom);
    delete bottom;
    Require(destroyed == 3 && stack.m_top == nullptr);
    delete queue.Dequeue();
    Require(destroyed == 4);
}

void VerifyCrossContainerListOwnership() {
    int destroyed = 0;
    UnorderedList sorted;
    NodeQueue queue;
    auto* node = new CountedNode(destroyed);

    Require(sorted.Insert(node) == 1);
    Require(queue.Enqueue(node) == 0);
    Require(sorted.Remove(node) == 1);
    Require(queue.Enqueue(node) == 1);
    Require(sorted.Insert(node) == 0);
    delete queue.Dequeue();
    Require(destroyed == 1);
}

void VerifyLinkNodeCopiesStartUnlinked() {
    int destroyed = 0;
    NodeQueue source;
    NodeStack destination;
    auto* owned = new CountedNode(destroyed);
    Require(source.Enqueue(owned) == 1);

    CountedNode copy(*owned);
    Require(copy.m_prev == nullptr && copy.m_next == nullptr);
    Require(destination.Push(&copy) == 1);
    Require(destination.Pop() == &copy);
    delete source.Dequeue();
    Require(destroyed == 1);
}

void VerifyClearDetachesBeforeDestruction() {
    int queueDestroyed = 0;
    int queueRejectedDestroyed = 0;
    NodeQueue queue;
    Require(queue.Enqueue(new QueueClearNode(queue, queueDestroyed, queueRejectedDestroyed)) == 1);
    Require(queue.Enqueue(new QueueClearNode(queue, queueDestroyed, queueRejectedDestroyed)) == 1);
    queue.Clear();
    Require(queueDestroyed == 2);
    Require(queueRejectedDestroyed == 2);
    queue.Clear();
    Require(queue.m_head == nullptr && queue.m_tail == nullptr && queue.m_count == 0);
    int afterQueueClearDestroyed = 0;
    auto* afterQueueClear = new CountedNode(afterQueueClearDestroyed);
    Require(queue.Enqueue(afterQueueClear) == 1);
    delete queue.Dequeue();
    Require(afterQueueClearDestroyed == 1);

    int stackDestroyed = 0;
    int stackRejectedDestroyed = 0;
    NodeStack stack;
    Require(stack.Push(new StackClearNode(stack, stackDestroyed, stackRejectedDestroyed)) == 1);
    Require(stack.Push(new StackClearNode(stack, stackDestroyed, stackRejectedDestroyed)) == 1);
    stack.Clear();
    Require(stackDestroyed == 2);
    Require(stackRejectedDestroyed == 2);
    stack.Clear();
    Require(stack.m_top == nullptr);
    int afterStackClearDestroyed = 0;
    auto* afterStackClear = new CountedNode(afterStackClearDestroyed);
    Require(stack.Push(afterStackClear) == 1);
    delete stack.Pop();
    Require(afterStackClearDestroyed == 1);
}

}  // namespace

int main() {
    VerifyQueueOwnershipAndOrder();
    VerifyStackOwnershipAndOrder();
    VerifyCrossContainerListOwnership();
    VerifyLinkNodeCopiesStartUnlinked();
    VerifyClearDetachesBeforeDestruction();
}
