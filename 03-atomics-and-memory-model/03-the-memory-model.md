# 3.3 — The C++ Memory Model

Atomicity stops torn reads on **one word**. It does **not** tell you when writes
to **other** memory become visible. The C++ **memory model** is the contract between
your code, the compiler, and the CPU about which reorderings are allowed and which
**happens-before** edges make concurrent programs well-defined.

Without it, "I stored the flag after the data" is meaningless — hardware and the
optimizer can reorder anyway (Part 3.5).

---

## 3.3.1 Why a memory model exists

Two adversaries reorder your program:

```
   COMPILER                    CPU
   ────────                    ───
   hoist loads early           store buffer delays stores
   sink stores late            out-of-order execution
   eliminate "redundant" ops    LoadLoad, StoreStore, LoadStore, StoreLoad
```

Single-threaded semantics are preserved — the machine pretends to run your abstract
machine. **Multi-threaded** programs need explicit **synchronizes-with** edges so
threads agree on which writes they see (Part 0.4's data races become defined behavior
when synchronized).

---

## 3.3.2 sequenced-before, happens-before, synchronizes-with

![Happens-before relationships between threads](figures/happens-before.svg)

```
   within one thread:
   A; B;  →  A sequenced-before B

   across threads (via atomics):
   release store on atomic X  ──synchronizes-with──▶  acquire load of X
                              implies
   writes before release      ──happens-before──▶     reads after acquire
```

> **Definitions ▸**
> - **sequenced-before**: program order in one thread (same expression evaluation
>   order where required).
> - **synchronizes-with**: a release operation on an atomic **pairs** with an acquire
>   load that reads the stored value (or RMW acq_rel).
> - **happens-before**: transitive closure of sequenced-before + synchronizes-with +
>   thread startup/join edges.

If action A happens-before B, B observes A's side effects (for non-racy non-atomic
accesses governed by the rules).

---

## 3.3.3 Data race = undefined behavior

> **Rule ▸** A **data race** is two unsequenced accesses to the same location, at
> least one a write, neither atomic nor mutex-protected → **UB**. Not "maybe wrong" —
> the entire program has no meaning.

This is the formal restatement of Part 0's one rule. Mutex lock/unlock creates
happens-before (Part 2.2). Atomic ops with proper orders create synchronizes-with.

Thread creation: `thread` start happens-before the new thread's first instruction.
`join()` happens-before code after join in the joining thread.

---

## 3.3.4 Release-store / acquire-load — publishing data

The canonical safe publish pattern:

```
   producer                         consumer
   ────────                         ────────
   write payload (non-atomic OK     ...
   if no concurrent read yet)
   data = 42;
   ready.store(true, release);  ──▶  while (!ready.load(acquire));
                                    use data;   // guaranteed to see 42
```

```
   memory
   ──────
   [ data = 42 ]  ── sequenced-before ──▶ [ ready release-store true ]
                                                    │
                                         synchronizes-with
                                                    │
                                                    ▼
                              [ ready acquire-load true ] ──▶ [ read data ]
```

> **The API ▸** (conceptual)
> ```cpp
> data = payload;
> ready.store(true, std::memory_order_release);
> // other thread:
> while (!ready.load(std::memory_order_acquire)) ;
> consume(data);   // safe
> ```

The **release** store on `ready` ensures prior writes are visible to any thread
that **acquire**-loads `true`. The flag is a synchronization point; `data` need not
be atomic if the happens-before chain is respected.

> **Pitfall ▸** Consumer reads `data` before acquire-loading `ready` — no
> happens-before edge → data race or stale value.

> **Pitfall ▸** Both sides use `relaxed` on the flag — atomicity without ordering;
> consumer may see `ready == true` but stale `data` (Part 3.4).

---

## 3.3.5 Producer/consumer flag example

```cpp
// g++ -std=c++17 -pthread memory_model_demo.cpp -o memory_model_demo
#include <iostream>
#include <thread>
#include <atomic>
#include <string>

std::string message;                    // non-atomic payload
std::atomic<bool> ready{false};

void producer() {
    message = "hello from producer";
    ready.store(true, std::memory_order_release);
}

void consumer() {
    while (!ready.load(std::memory_order_acquire))
        ;   // spin — use CV in real code
    std::cout << message << "\n";
}

int main() {
    std::thread p(producer), c(consumer);
    p.join();
    c.join();
    return 0;
}
```

No data race on `message`: the consumer's read is happens-after the producer's write
via release/acquire on `ready`. This pattern generalizes to lock-free queues,
epoch counters, and RCU (Part 5.5).

**Trade-offs ▸** The memory model gives precise rules but demands discipline. When
in doubt, `mutex` or default `seq_cst` atomics — optimize orders later with proof
(Part 3.4).

---

## Summary

- Compiler and CPU reorder memory; the memory model defines legal outcomes.
- **happens-before** is the visibility ordering; **synchronizes-with** links threads
  through atomics (release/acquire pairs).
- Data race on non-atomic shared mutable state → UB.
- Release store after data write + acquire load before data read = safe publish.
- Thread start/join also establish happens-before edges.
- Memory orders refine synchronizes-with cost (Part 3.4); fences add more (Part 3.5).

Next: [3.4 — Memory orders](04-memory-orders.md)
