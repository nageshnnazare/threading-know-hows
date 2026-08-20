// 05_lockfree_stack.cpp -- Treiber stack: the "hello world" of lock-free DS.
//
//  Idea: each push/pop is one CAS on the head pointer.
//
//      head ---> [a] ---> [b] ---> [c] ---> nullptr
//
//  push(x):
//      new_node->next = head.load();
//      while (!head.CAS(new_node->next, new_node)) /* retry */;
//
//  pop():
//      old_head = head.load();
//      while (old_head && !head.CAS(old_head, old_head->next)) ;
//      if (old_head) value = old_head->data;
//
//  Diagram of a successful push under contention:
//
//      time -->
//      thread A: load h=[a]    CAS(h,[X]->a) succeeds
//                                           ^ head now [X]->a
//      thread B: load h=[a]    CAS fails    load h=[X]->a   CAS([X]->a,[Y]->X) ok
//
//  ATTENTION: This stack is NOT safe for `pop()` due to the ABA problem
//  (see 04_pitfalls/06_aba_problem.cpp). It's also unsafe for memory
//  reclamation -- when do we free a popped node? Production lock-free
//  containers use hazard pointers, RCU, or epoch-based reclamation.
//  Here we LEAK popped nodes for clarity. For real use, see folly,
//  TBB, or libcds.

#include <atomic>
#include <iostream>
#include <thread>
#include <vector>
#include <optional>

template <class T>
class LockFreeStack {
    struct Node { T value; Node* next; };
    std::atomic<Node*> head_{nullptr};
public:
    void push(T v) {
        Node* n = new Node{std::move(v), head_.load(std::memory_order_relaxed)};
        while (!head_.compare_exchange_weak(
                   n->next, n,
                   std::memory_order_release,
                   std::memory_order_relaxed))
            ;
    }
    std::optional<T> pop() {
        Node* old = head_.load(std::memory_order_acquire);
        while (old &&
               !head_.compare_exchange_weak(
                   old, old->next,
                   std::memory_order_acquire,
                   std::memory_order_relaxed))
            ;
        if (!old) return std::nullopt;
        T v = std::move(old->value);
        // We deliberately LEAK old to avoid the ABA / use-after-free problem.
        // A real implementation would use hazard pointers or epoch-based
        // reclamation.
        return v;
    }
};

int main()
{
    LockFreeStack<int> s;

    std::vector<std::thread> ts;
    for (int p = 0; p < 4; ++p)
        ts.emplace_back([&,p]{ for (int i = 0; i < 1000; ++i) s.push(p*1000+i); });
    for (auto& t : ts) t.join();

    int popped = 0;
    while (s.pop()) ++popped;
    std::cout << "popped " << popped << " (expected 4000)\n";
}
