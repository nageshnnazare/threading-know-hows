# 3.2 — Lock-Free Programming & CAS

**Lock-free** means the system as a whole makes progress even if individual threads
stall: at least one operation completes in a finite number of steps. **Wait-free**
is stronger — every thread finishes in bounded steps. Most production "lock-free"
code is lock-free but not wait-free, built from one primitive: **compare-and-swap**
(CAS) in a retry loop.

This chapter walks the CAS loop pattern, a Treiber stack, and why lock-free is
harder than it looks (ABA, reclamation).

---

## 3.2.1 compare_exchange_weak vs strong

> **The API ▸**
> ```cpp
> bool compare_exchange_weak(T& expected, T desired,
>                            memory_order success, memory_order failure);
> bool compare_exchange_strong(T& expected, T desired, ...);
> ```
> On failure, both update `expected` to the current value.

| | weak | strong |
|---|------|--------|
| Spurious failure | yes — may fail when value == expected | no |
| Use in loop | preferred — may be faster on ARM | OK but no benefit |
| Single shot | risky | use when not looping |

```
   CAS loop (weak):
   do {
       old = head.load();
       new_head = { val, old };
   } while (!head.compare_exchange_weak(old, new_head));
```

> **Rule ▸** In a **retry loop**, use `compare_exchange_weak`. Use **strong** when
> you attempt once and handle failure explicitly.

Spurious failure on weak CAS mirrors how LL/SC (load-linked/store-conditional)
works on weak ISAs — the CPU may fail if another core touched the cache line.

---

## 3.2.2 The CAS retry loop pattern

Lock-free updates follow a template:

```
   1. READ  current state (relaxed or acquire)
   2. COMPUTE new state from current
   3. CAS(current → new)
         success → done
         failure → someone else won; expected updated → goto 1
```

You never hold a lock, but you may **retry many times** under contention — lock-free
≠ wait-free, and high contention can burn CPU (Part 5.3 starvation).

---

## 3.2.3 Treiber stack — hello, lock-free

![Lock-free stack push/pop via CAS on head](figures/lockfree-stack.svg)

```
   head ──▶ [node3] ──▶ [node2] ──▶ [node1] ──▶ nullptr

   push(x):  new->next = head;  CAS(head, old, new)
   pop():    old = head;        CAS(head, old, old->next)
```

> **Under the hood ▸** Each successful push/pop is one winning CAS on `head`. Losers
> retry with fresh `old`. Progress: some thread succeeds whenever the stack isn't
> in infinite retry limbo — but one thread can theoretically starve (not wait-free).

```cpp
// g++ -std=c++17 -pthread lockfree_stack.cpp -o lockfree_stack
#include <atomic>
#include <optional>
#include <thread>
#include <vector>
#include <iostream>

template<class T>
class TreiberStack {
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
        while (old && !head_.compare_exchange_weak(
                   old, old->next,
                   std::memory_order_acquire,
                   std::memory_order_relaxed))
            ;
        if (!old) return std::nullopt;
        T v = std::move(old->value);
        // leak node — see reclamation below
        return v;
    }
};

int main() {
    TreiberStack<int> s;
    std::vector<std::thread> ts;
    for (int p = 0; p < 4; ++p)
        ts.emplace_back([&,p]{ for (int i = 0; i < 1000; ++i) s.push(p*1000+i); });
    for (auto& t : ts) t.join();
    int n = 0;
    while (s.pop()) ++n;
    std::cout << "popped " << n << " (expected 4000)\n";
}
```

`release` on successful push publishes the new node; `acquire` on pop sees it
(Part 3.3).

---

## 3.2.4 Lock-free vs wait-free vs obstruction-free

```
   obstruction-free : a thread runs alone → finishes in bounded steps
   lock-free        : system-wide progress; one thread completes
   wait-free        : every thread completes in bounded steps
        wait-free ⊂ lock-free ⊂ obstruction-free
```

Real stacks/queues are usually lock-free. Wait-free structures exist but are rare
and often slower in average case.

**Trade-offs ▸** Lock-free avoids mutex priority inversion and kernel waits, but
adds complexity, retry storms under load, and **does not** solve memory reclamation.

---

## 3.2.5 Why lock-free is HARD

**ABA problem** — `head` looks unchanged but the world changed:

```
   thread A: read head = X
   thread A: pop delayed...
   thread B: pop X, pop Y, push X back   → head == X again
   thread A: CAS(head, X, X->next) succeeds on WRONG structure
```

Fixes: tagged pointers (version counter in low bits), hazard pointers, epoch
reclamation — Part 5.5.

**Memory reclamation** — after `pop`, when is `delete node` safe? Another thread
may still be reading it. Production code uses **hazard pointers**, **RCU**, or
**epoch-based** schemes. The example above **leaks** nodes deliberately.

> **Pitfall ▸** Copying a textbook Treiber stack into production without ABA
> protection or safe reclamation. It will break under real concurrency.

> **Rule ▸** Lock-free is an algorithm **plus** a reclamation strategy. One without
> the other is incomplete.

---

## Summary

- Use `compare_exchange_weak` in loops; `strong` for single attempts.
- CAS retry: read → compute → CAS until success — the lock-free template.
- Treiber stack: one atomic head; push/pop via CAS — canonical introduction.
- Lock-free guarantees system progress, not per-thread bounded time.
- ABA and memory reclamation are the hard parts — see Part 5.5 before shipping.
- Match memory orders on publish/consume edges (Part 3.3–3.4).

Next: [3.3 — The C++ memory model](03-the-memory-model.md)
