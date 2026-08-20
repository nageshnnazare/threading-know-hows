// 06_aba_problem.cpp -- The subtlest CAS bug.
//
//  In a CAS-based stack, suppose the head pointer goes A -> B -> C, and
//  thread 1 has read head=A and is about to CAS-pop. Meanwhile, thread 2
//  pops A and B (now head=C), frees A, then pushes a NEW node X whose
//  address happens to equal A's old address (because the allocator reused
//  it). Now head=A again. Thread 1's CAS(head, A, A->next=B) succeeds!
//  But B is freed and head should not point to it.
//
//      time --->
//      thread 1:  read head=A,A->next=B
//      thread 2:                       pop A, pop B,
//                                      free A,
//                                      push X (X's addr == old A)
//      thread 1:  CAS(head, A, B)   <- "succeeds": head now points to freed B
//
//  Coined ABA: the value went A -> B -> A; CAS can't tell the difference.
//
//  CURES:
//   1. Tagged pointers: pack a counter into the high bits, increment on
//      every change. CAS on (ptr, counter) tuple. The counter making each
//      ABA unique.
//   2. Hazard pointers: thieves announce "I'm reading X"; reclamation
//      defers freeing X until no hazard points there.
//   3. Epoch-based reclamation (RCU style).
//
//  This file demonstrates a tagged-pointer fix using a 128-bit DCAS
//  (double-width compare and swap) when available. Many platforms do.

#include <atomic>
#include <iostream>
#include <thread>
#include <cstdint>

struct Node { int value; Node* next; };

struct TaggedPtr {
    Node*    ptr;
    uintptr_t tag;
    bool operator==(const TaggedPtr& o) const { return ptr == o.ptr && tag == o.tag; }
};

// Most modern platforms support 128-bit atomics with -mcx16 (x86_64) or
// 16-byte aligned atomics on aarch64. Otherwise std::atomic falls back to
// a hidden lock; correctness is preserved, but it isn't lock-free.
class TaggedStack {
    std::atomic<TaggedPtr> head_{ TaggedPtr{nullptr, 0} };
public:
    void push(int v) {
        Node* n = new Node{v, nullptr};
        TaggedPtr cur = head_.load(std::memory_order_relaxed);
        for (;;) {
            n->next = cur.ptr;
            TaggedPtr next = { n, cur.tag + 1 };
            if (head_.compare_exchange_weak(cur, next,
                                            std::memory_order_release,
                                            std::memory_order_relaxed))
                return;
        }
    }
    bool pop(int& out) {
        TaggedPtr cur = head_.load(std::memory_order_acquire);
        for (;;) {
            if (!cur.ptr) return false;
            TaggedPtr next = { cur.ptr->next, cur.tag + 1 };
            if (head_.compare_exchange_weak(cur, next,
                                            std::memory_order_acquire,
                                            std::memory_order_relaxed)) {
                out = cur.ptr->value;
                // Real code would defer-free cur.ptr (hazard ptr / RCU).
                // We leak for clarity.
                return true;
            }
        }
    }
};

int main()
{
    std::cout << "is_lock_free TaggedPtr: "
              << std::atomic<TaggedPtr>{}.is_lock_free()
              << "  (1 = native, 0 = std uses hidden lock)\n";

    TaggedStack s;
    for (int i = 0; i < 5; ++i) s.push(i);
    int v;
    while (s.pop(v)) std::cout << v << " ";
    std::cout << "\n";

    std::cout << "Note: hazard pointers / RCU are needed for safe reclamation.\n";
}
