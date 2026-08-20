# 3.5 — Fences & Hardware Reordering

Memory orders on atomics tell the compiler and CPU how an **atomic operation** relates
to surrounding memory. Sometimes you need a **standalone fence** — ordering without
an associated load/store on a particular object — or you need to understand **why**
StoreLoad reordering breaks naive synchronization. This chapter maps the four
hardware reorderings, `atomic_thread_fence`, and x86 vs ARM differences.

---

## 3.5.1 The four reorderings

For two ordinary operations A then B in program order, the CPU/compiler may reorder:

```
   LoadLoad    (LL):  r1; r2;     — often reordered on weak ISAs
   LoadStore   (LS):  r1; w2;    — often reordered
   StoreStore  (SS):  w1; w2;    — often reordered on weak ISAs
   StoreLoad   (SL):  w1; r2;    — the surprising one
```

![StoreLoad reordering via store buffer](figures/store-load-reorder.svg)

**StoreLoad** is special: even **x86** (TSO) allows it via the **store buffer**:

```
   thread A                    thread B
   ────────                    ────────
   x = 1;                      y = 1;
   r1 = y;   (may see 0)        r2 = x;   (may see 0)
   // both r1==0 and r2==0 possible without proper sync!
```

Each core buffers stores before they drain to cache/coherence. A load can execute
before an earlier store to a **different** address becomes globally visible.

> **Under the hood ▸** x86 gives strong **Total Store Order** for same-thread
> visibility rules, but StoreLoad still bites without fences or locked instructions.
> ARM and POWER are **weak** — all four reorderings are fair game unless constrained.

---

## 3.5.2 Dekker-style intuition

Classic mutual exclusion without atomics fails because of StoreLoad:

```cpp
// Illustrative broken Dekker — DO NOT USE
// g++ -std=c++17 -pthread dekker_broken.cpp -o dekker_broken
#include <atomic>
#include <thread>
#include <iostream>

std::atomic<bool> flag_a{false}, flag_b{false};
int turn = 0;   // intentionally non-atomic for illustration of races

void thread_a() {
    flag_a.store(true, std::memory_order_relaxed);
    while (flag_b.load(std::memory_order_relaxed)) {
        if (turn != 0) {
            flag_a.store(false, std::memory_order_relaxed);
            while (flag_b.load(std::memory_order_relaxed)) { }
            flag_a.store(true, std::memory_order_relaxed);
        }
    }
    // critical section
    turn = 1;
    flag_a.store(false, std::memory_order_relaxed);
}

void thread_b() {
    flag_b.store(true, std::memory_order_relaxed);
    while (flag_a.load(std::memory_order_relaxed)) {
        if (turn != 1) {
            flag_b.store(false, std::memory_order_relaxed);
            while (flag_a.load(std::memory_order_relaxed)) { }
            flag_b.store(true, std::memory_order_relaxed);
        }
    }
    turn = 0;
    flag_b.store(false, std::memory_order_relaxed);
}
```

Real Dekker needs carefully paired orders (or atomics with seq_cst). The lesson:
**relaxed** flag stores + plain loads can interleave so both threads think they
entered the critical section. Production code uses mutexes (Part 2.2), not Dekker.

---

## 3.5.3 std::atomic_thread_fence

> **The API ▸**
> ```cpp
> void atomic_thread_fence(memory_order order);
> ```
> A **fence** is a barrier in the memory order graph without touching a particular
> atomic variable:

```
   release fence:  all prior writes  ──▶  ordered before  ──▶  subsequent atomics
   acquire fence:  subsequent reads  ◀──  ordered after   ◀──  prior atomics
   seq_cst fence:  both directions + participates in seq_cst total order
```

Typical pattern — release payload writes, then fence, then relaxed flag store:

```cpp
data = 42;
std::atomic_thread_fence(std::memory_order_release);
ready.store(true, std::memory_order_relaxed);
```

Consumer:

```cpp
while (!ready.load(std::memory_order_relaxed)) ;
std::atomic_thread_fence(std::memory_order_acquire);
use(data);
```

Equivalent to `ready.store/release` + `ready.load/acquire` when the flag is the
only synchronizer (Part 3.3).

> **Rule ▸** Prefer memory orders **on the atomic** that carries the synchronizes-with
> edge. Standalone fences are for exotic layouts (hand-written lock-free, porting
> C code) or when one fence orders many prior stores.

---

## 3.5.4 When fences vs atomic orders

```
   atomic with acquire/release/seq_cst
   ───────────────────────────────────
   ✓ ties ordering to a visible sync variable
   ✓ what you want 99% of the time

   atomic_thread_fence
   ───────────────────
   ✓ ordering non-atomic writes before a relaxed atomic publish
   ✓ legacy interop, specialized lock-free algorithms
   ✗ harder to audit — easy to get half a sync edge
```

C++ atomics with explicit orders usually compile to single instructions + implied
fences on weak ISAs. A standalone fence emits `dmb`/`mfence`-class ops.

---

## 3.5.5 x86 (TSO) vs ARM/POWER (weak)

```
   x86 TSO                         ARM / POWER
   ───────                         ───────────
   StoreStore rarely reordered     all four reorderings common
   LoadLoad mostly kept            need explicit barriers / ordered atomics
   StoreLoad still possible        consume/acquire/release map to barriers
   locked ops are full barriers    LL/SC for CAS — weak CAS spurious fails
```

**Portable rule:** write to the C++ memory model, not to x86 folklore. Code that
"works on Intel" with plain `volatile` or ad-hoc fences often breaks on ARM.

> **Pitfall ▸** Assuming `volatile` provides thread synchronization. It does not —
> only atomics and mutexes do (Part 3.3).

> **Under the hood ▸** `memory_order_seq_cst` load/store on x86 often uses `mov`
> + `lock`ed RMW for read-modify-write. On ARM, `seq_cst` may emit `dmb ish`.
> Profile on your target ISA.

---

## 3.5.6 StoreLoad example with fences

```cpp
// g++ -std=c++17 -pthread store_load_fence.cpp -o store_load_fence
#include <atomic>
#include <thread>
#include <iostream>

std::atomic<int> x{0}, y{0};
std::atomic<int> r1{0}, r2{0};

void thread1() {
    x.store(1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_seq_cst);  // StoreLoad barrier
    r1.store(y.load(std::memory_order_relaxed), std::memory_order_relaxed);
}

void thread2() {
    y.store(1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    r2.store(x.load(std::memory_order_relaxed), std::memory_order_relaxed);
}

int main() {
    for (int i = 0; i < 100'000; ++i) {
        x = y = r1 = r2 = 0;
        std::thread a(thread1), b(thread2);
        a.join(); b.join();
        if (r1.load() == 0 && r2.load() == 0) {
            std::cout << "both saw zero (can happen without sync)\n";
            break;
        }
    }
    return 0;
}
```

Without the `seq_cst` fences (or acq/rel on x/y), observing both `r1==0` and
`r2==0` is allowed — the "impossible" outcome on paper.

---

## Summary

- Four reordering classes: LL, LS, SS, SL — StoreLoad bites via store buffers.
- x86 is TSO-strong but not sequentially consistent for racy plain accesses.
- ARM/POWER need explicit ordering — portable code uses C++ memory orders.
- `atomic_thread_fence` orders surrounding memory relative to atomics; use sparingly.
- Prefer acquire/release on the synchronizing atomic over manual fences.
- Dekker and similar protocols expose why naive flag spinning fails — use mutexes
  or proven lock-free algorithms.

Next: [4.1 — Thread pool](../04-patterns/01-thread-pool.md)
