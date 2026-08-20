# 5.1 — Race Conditions

A **race condition** is any outcome that depends on the **unsynchronized timing**
of concurrent threads. The C/C++ standard names the worst case a **data race**:
two threads access the same memory location, at least one is a write, and neither
access is atomic or ordered by synchronization → **undefined behavior** (not "a
wrong counter," but *no guarantees at all*). Part 0.4 introduced the lost-update
mechanic; this chapter dissects the anatomy, the common shapes, detection, and
every standard fix.

---

## 5.1.1 The definition, mechanically

```
   thread A                          thread B
   ────────                          ────────
   load  counter → 5
                                     load  counter → 5
   add   reg, 1  → 6
                                     add   reg, 1  → 6
   store counter ← 6
                                     store counter ← 6

   final counter = 6   (expected 7 — one update lost)
```

`counter++` is **not** one operation. It is at least three machine instructions
(load, modify, store). The scheduler may switch threads **between any two of
them** (Part 0.1). When two threads interleave like above, both read the same
value, both write back an increment of that value, and one update vanishes.

> **Rule ▸** Two threads must never access the same memory location concurrently
> when at least one access is a **write**, unless that access is **atomic** or
> protected by **synchronization** (mutex, condition variable edge, or a proper
> `memory_order` — Part 3). Break this and the program has **undefined behavior**.

The standard's term **data race** is narrower than "race condition" in everyday
speech. A race condition is any bug whose correctness depends on timing; a data
race is the specific UB trigger above. Many race conditions (TOCTOU below) involve
a data race on the check or the act.

---

## 5.1.2 Read-modify-write races

Any compound update — increment, append-to-list, bitwise OR, `balance -= amount`
— is a **read-modify-write (RMW)** sequence unless you make it indivisible:

```
   logical:   shared = f(shared)     // one statement in source

   machine:   t = load(shared)
              t = f(t)
              store(shared, t)        // three steps → three preemption points
```

Fixes force atomicity at the right level:

| Fix | Mechanism | When |
|-----|-----------|------|
| Mutex | Only one thread in the critical section | General shared state |
| AtomicInteger atomics | Hardware RMW (`fetch_add`, `compare_exchange`) | Counters, flags, lock-free structures |
| Immutability | No writes after publish | Config, message payloads |
| Confinement | Only one thread ever touches the data | Per-thread accumulators (Part 5.4) |

> **The API ▸**
> ```cpp
> #include <atomic>
> std::atomic<long> counter{0};
> counter.fetch_add(1, std::memory_order_relaxed);  // single indivisible RMW
> // or: ++counter;  (atomic overload — seq_cst by default)
> ```
> ```c
> #include <stdatomic.h>
> atomic_fetch_add(&counter, 1);   // C11 equivalent
> ```

---

## 5.1.3 Check-then-act (TOCTOU)

A subtler race: a thread **checks** a condition, then **acts** on it, but another
thread changes the condition in between:

```
   thread A (consumer)               thread B (producer)
   ─────────────────                 ─────────────────
   if (queue.empty())  ✓ true
                                     queue.push(item)
                                     queue.empty() is now false
   wait / return "empty"             // A acted on stale knowledge
```

Classic shapes:

- **Double-checked locking** without proper fences (Part 4.7).
- **Lazy initialization**: `if (!ptr) ptr = new T;` from two threads.
- **File/resource checks**: `if (access(path)) open(path)` — the file may change
  between check and open (the kernel names this TOCTOU too).

The fix is always the same pattern: **hold synchronization across check and act**:

```cpp
// BROKEN: check-then-act race
if (!initialized) {
    instance = new Widget();   // two threads may both "win"
    initialized = true;
}

// FIXED: check and act under one lock (or std::call_once — Part 2.5)
std::lock_guard g(m);
if (!initialized) {
    instance = new Widget();
    initialized = true;
}
```

> **Pitfall ▸** Making the *flag* atomic but not the pointer publish is still
> wrong without acquire/release ordering (Part 3.3). An atomic `initialized =
> true` does not magically make the write to `instance` visible unless you
> establish a happens-before edge.

---

## 5.1.4 Broken counter → fixed

```cpp
// g++ -std=c++17 -pthread -fsanitize=thread 01_race.cpp -o 01_race
#include <atomic>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

constexpr int N_THREADS = 8, ITERS = 200'000;

int main() {
    /* BROKEN — data race → UB (TSan will scream) */
    {
        long counter = 0;
        auto worker = [&]{ for (int i = 0; i < ITERS; ++i) ++counter; };
        std::vector<std::thread> ts;
        for (int i = 0; i < N_THREADS; ++i) ts.emplace_back(worker);
        for (auto& t : ts) t.join();
        std::cout << "broken:  " << counter
                  << " (expected " << long(N_THREADS) * ITERS << ")\n";
    }

    /* FIXED A — mutex: correct, serializes the critical section */
    {
        long counter = 0;
        std::mutex m;
        auto worker = [&]{
            for (int i = 0; i < ITERS; ++i) {
                std::lock_guard g(m);
                ++counter;
            }
        };
        std::vector<std::thread> ts;
        for (int i = 0; i < N_THREADS; ++i) ts.emplace_back(worker);
        for (auto& t : ts) t.join();
        std::cout << "mutex:   " << counter << "\n";
    }

    /* FIXED B — atomic: correct, hardware RMW under contention */
    {
        std::atomic<long> counter{0};
        auto worker = [&]{
            for (int i = 0; i < ITERS; ++i)
                counter.fetch_add(1, std::memory_order_relaxed);
        };
        std::vector<std::thread> ts;
        for (int i = 0; i < N_THREADS; ++i) ts.emplace_back(worker);
        for (auto& t : ts) t.join();
        std::cout << "atomic:  " << counter.load() << "\n";
    }
}
```

**Trade-offs ▸** Mutexes are simpler for multi-field invariants ("transfer money
from A to B"). Atomics win for hot counters and lock-free structures but demand
memory-order literacy (Part 3). Immutability and confinement avoid the problem
entirely when the data model allows it.

---

## 5.1.5 Detection with ThreadSanitizer

Races are **non-deterministic** — the bug may hide for billions of iterations.
**ThreadSanitizer (TSan)** instruments every memory access at compile time and
reports unsynchronized conflicting accesses at runtime:

```bash
# C++
g++ -std=c++17 -pthread -fsanitize=thread -g race.cpp -o race && ./race

# C
gcc -pthread -fsanitize=thread -g race.c -o race && ./race
```

Typical TSan output pinpoints both stacks:

```
WARNING: ThreadSanitizer: data race
  Write of size 8 at 0x... by thread T2
  Previous read of size 8 at 0x... by thread T1
```

> **Under the hood ▸** TSan tracks a *happens-before graph* at runtime (shadow
> memory per location). It is slow (~5–15×) and needs rebuild — use it in CI
> and debug builds, not production. It catches data races, not all logical race
> conditions (TOCTOU under a lock is fine; TOCTOU without one may still be a
> logic bug TSan won't flag if each individual access is synchronized).

Other tools: **Helgrind** (Valgrind, slower), **Intel Inspector**, `-race` in
some commercial static analyzers (less complete than TSan for C++).

---

## 5.1.6 The fix decision tree

```
   shared mutable state touched by ≥2 threads?
              │
              ├─ can one thread own it entirely?  → confinement / thread_local
              │
              ├─ can it be immutable after setup? → publish once, then read-only
              │
              ├─ single scalar hot counter/flag?  → std::atomic / _Atomic
              │
              └─ multi-field invariant?             → mutex (or lock-free + CAS
                                                         with memory orders)
```

Every path in Part 4 (pools, producer-consumer, readers-writers) is an instance
of this tree applied to a concrete architecture.

---

## Summary

- A **data race** = concurrent access to the same location, ≥1 write, no
  synchronization → **undefined behavior** (Part 0.4).
- RMW operations (`++`, compound updates) are multi-instruction; interleaving
  loses updates unless you use a mutex or atomic RMW.
- **Check-then-act (TOCTOU)** races stale reads against concurrent writers;
  check and act must be one atomic transaction under a lock or proper atomic
  protocol.
- Fixes: **mutex** (general), **atomics** (scalars, lock-free — Part 3),
  **immutability**, **confinement** (Part 5.4).
- Detect with **ThreadSanitizer** (`-fsanitize=thread`); run under TSan in CI.

Next: [5.2 — Deadlock](02-deadlock.md)
