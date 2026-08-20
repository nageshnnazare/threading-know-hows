# 5.5 — The ABA Problem

Lock-free algorithms built on **compare-and-swap (CAS)** (Part 3.2) assume: "if
the value is still what I read, nothing changed." The **ABA problem** proves that
assumption false: the value can **match** while the **structure** changed — and a
CAS "success" can corrupt a linked data structure. It is the subtlest failure mode
of naive lock-free code.

---

## 5.5.1 The scenario: lock-free stack

Consider a lock-free stack with head pointer `top`:

```
   initial:   top → A → B → C → nullptr
```

Thread **T1** reads `head = A`, saves `next = A->next` (which is B), and is
preempted before CAS.

Thread **T2** runs:

```
   pop A          top → B → C
   pop B          top → C
   (A and B freed or moved to free list)
   push new node  reuses address of old A
                  top → A' → C     (A' at same address as old A)
```

Thread **T1** resumes and executes:

```
   CAS(head, A, B)   // "expected" A matches current head — SUCCESS
```

But **B was already popped and may be freed**. Head now points to **stale memory**.
The list is corrupt — use-after-free, double-free, or silent data loss.

![T1's CAS succeeds because head looks like A again, but B is stale](figures/aba-problem.svg)

```
   time ──▶

   T1:  read head=A, next=B
   T2:              pop A, pop B, free A, push X (addr == old A)
   T1:  CAS(head, A, B) ✓  →  head → B (FREED — disaster)
```

> **Pitfall ▸** "CAS succeeded" means **the bit pattern matched**, not "the world
> is unchanged since my load." Any algorithm that CASes a **pointer alone** on a
> structure with **reuse** is ABA-vulnerable.

---

## 5.5.2 Why it happens: memory reuse

ABA requires **address reuse**:

1. Node **A** is freed to an allocator pool.
2. A new allocation returns the **same address** for a different object.
3. The pointer value **A** appears again at `head`.

Without reuse (unique addresses forever), ABA is theoretically avoided — impractical
for general allocators. Real systems recycle heap blocks aggressively.

> **Under the hood ▸** Even **GC'd languages** can ABA if the GC moves/reuses
> identity in ways visible to native CAS (less common in Java `AtomicReference`,
> but relevant in C/C++ and in **epoch reclamation** bugs).

---

## 5.5.3 Fix 1: tagged / versioned pointers

Pack a **monotonic counter (tag)** with the pointer. Increment the tag on **every**
head change. CAS the **(pointer, tag)** pair — double-width CAS (128-bit on x86-64
with `-mcx16`, 16-byte aligned on AArch64):

```cpp
// g++ -std=c++17 -pthread aba_tagged.cpp -o aba_tagged
#include <atomic>
#include <cstdint>
#include <iostream>

struct Node { int value; Node* next; };

struct TaggedPtr {
    Node*     ptr;
    uintptr_t tag;
    bool operator==(const TaggedPtr& o) const {
        return ptr == o.ptr && tag == o.tag;
    }
};

class TaggedStack {
    std::atomic<TaggedPtr> head_{ TaggedPtr{nullptr, 0} };
public:
    void push(int v) {
        Node* n = new Node{v, nullptr};
        TaggedPtr cur = head_.load(std::memory_order_relaxed);
        for (;;) {
            n->next = cur.ptr;
            TaggedPtr next{n, cur.tag + 1};
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
            TaggedPtr next{cur.ptr->next, cur.tag + 1};
            if (head_.compare_exchange_weak(cur, next,
                    std::memory_order_acquire,
                    std::memory_order_relaxed)) {
                out = cur.ptr->value;
                delete cur.ptr;   // still unsafe without deferred free — see below
                return true;
            }
        }
    }
};
```

When T2 puts A back, the **tag** has incremented twice (pop, pop, push). T1's
stale `(A, tag_old)` fails CAS — ✓ safe from ABA on the head field.

**Trade-offs ▸** Tag width is finite — **tag wraparound** reintroduces ABA after
2^k operations (64-bit tag: never in practice). `std::atomic<TaggedPtr>` may fall
back to a hidden lock if not lock-free — check `is_lock_free()`.

---

## 5.5.4 Fix 2: hazard pointers

**Hazard pointers** (Michael, 2004): each thread publishes "I am reading node X"
in a global hazard list. **Reclamation is deferred** until no hazard pointer
references X:

```
   T1: hazard[0] = B     (announces: don't free B)
   T2: wants to free B   → scans hazards → B still protected → defer
   T1: CAS fails or completes, clears hazard[0]
   T2: retries reclamation → B safe to free
```

Used in production lock-free maps (Folly, ConcurrencyKit). Correct but fiddly —
reference counting or epoch schemes trade complexity for throughput.

---

## 5.5.5 Fix 3: RCU / epoch-based reclamation

**Read-Copy-Update (RCU)** (Linux kernel) and **epoch-based reclamation (EBR)**:

```
   readers enter epoch E
   writer removes node, queues it for epoch E+1
   when all readers leave epoch E, batch-free queued nodes
```

Readers incur **zero atomic RMW** on the hot path — excellent read throughput.
Writers pay deferred free batching. C++ users: `liburcu`, Folly's `rcu`, or
custom epoch counters (Part 3.2 lock-free stack example pairs with EBR).

> **Rule ▸** In C++, **never `delete` a node immediately after a lock-free pop**
> unless a hazard pointer, RCU, or epoch scheme proves no other thread still
> reads it. ABA-safe CAS on the pointer is necessary but **not sufficient** for
> safe memory reclamation.

---

## 5.5.6 Relation to the memory model

ABA is orthogonal to **memory_order** (Part 3.4): even `memory_order_seq_cst`
CAS does not detect structural change — it only compares bits. Tagging and
reclamation are **lifetime** problems, not ordering problems. Acquire/release on
pop still required so `cur.ptr->next` is visible before dereference.

---

## Summary

- **ABA**: CAS sees expected value **A** again, but the structure changed (A was
  removed, B freed, A reused) → successful CAS corrupts the list.
- "Value matched" ≠ "nothing changed" — pointer-only CAS on reused memory is
  unsafe.
- **Fixes**: **tagged pointers** (double-width CAS), **hazard pointers**,
  **RCU/epoch reclamation** for safe freeing.
- Tagging stops stale CAS; reclamation stops use-after-free — you need both for a
  production lock-free stack (Part 3.2).

Next: [5.6 — Lost wakeups](06-lost-wakeups.md)
