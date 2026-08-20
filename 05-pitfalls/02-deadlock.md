# 5.2 — Deadlock

**Deadlock** is a state where two or more threads are **blocked forever**, each
waiting for a resource held by another in the group. No thread makes progress;
CPU time may drop to zero while everyone sleeps on a lock. Unlike a race (wrong
answer), deadlock is **total stall** — and it often appears only under production
load when lock orderings diverge for the first time.

---

## 5.2.1 The four Coffman conditions

Deadlock requires **all four** conditions simultaneously (Coffman, 1971). Break
**any one** and deadlock cannot occur:

```
   ┌─────────────────────────┬────────────────────────────────────────────┐
   │ 1. Mutual exclusion     │ Resource usable by only one thread at a time │
   │ 2. Hold and wait        │ Thread holds ≥1 resource while waiting for more│
   │ 3. No preemption        │ Resources released only voluntarily by holder  │
   │ 4. Circular wait        │ T0 waits for T1 waits for ... waits for T0     │
   └─────────────────────────┴────────────────────────────────────────────┘
```

Mutexes provide (1) by definition. Normal lock APIs provide (3) — the OS will not
forcibly take a mutex from a thread. Deadlock prevention strategies target (2)
and (4) most often.

![Circular wait: each thread holds one lock and waits for another](figures/deadlock.svg)

---

## 5.2.2 The classic two-lock circular wait

The textbook example: two threads, two mutexes, **opposite acquisition order**:

```
   thread A                          thread B
   ────────                          ────────
   lock(m1)  ✓
                                     lock(m2)  ✓
   lock(m2)  … waits for B
                                     lock(m1)  … waits for A

   ═══════════════ DEADLOCK ═══════════════
   A holds m1, wants m2
   B holds m2, wants m1  → circular wait (condition 4)
```

```cpp
// BROKEN — opposite lock order → deadlock (may hang forever)
std::mutex m1, m2;

void thread_a() {
    std::lock_guard g1(m1);
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // widen window
    std::lock_guard g2(m2);   // blocks if B holds m2
}

void thread_b() {
    std::lock_guard g2(m2);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::lock_guard g1(m1);   // blocks if A holds m1
}
```

> **Under the hood ▸** Blocked mutex waiters sit on a futex queue in the kernel.
> Neither thread runs the critical section; neither releases its held lock.
> `top` shows threads in `D` (uninterruptible sleep) or low CPU — classic symptom.

This is the same structural bug as the **dining philosophers** (Part 4.4) when
every philosopher picks up the left fork first and the table is full.

---

## 5.2.3 Prevention: global lock ordering

Assign every lock in the system a **unique rank**. Always acquire in **increasing
rank** (or decreasing — but be consistent):

```cpp
// FIXED — global order: always lock lower address first
void take_both(std::mutex& a, std::mutex& b) {
    if (&a < &b) {
        std::lock_guard g1(a);
        std::lock_guard g2(b);
    } else {
        std::lock_guard g1(b);
        std::lock_guard g2(a);
    }
    // ... critical section ...
}
// locks released at scope end
```

Address order is a cheap total order when locks are distinct objects. For
production code, use an explicit **lock hierarchy** (level numbers assigned at
design time):

```
   hierarchy level 0:  coarse "database" lock
   hierarchy level 1:  per-table locks
   hierarchy level 2:  per-row locks

   Rule: never acquire level N while holding level M if N <= M
```

> **Rule ▸** Document the hierarchy. Code review checks that every multi-lock
> path respects it. One violation reintroduces circular wait.

---

## 5.2.4 Prevention: std::scoped_lock / std::lock

C++17's `std::scoped_lock` (and C++11's `std::lock`) acquire **multiple mutexes
atomically** using internal try-and-backoff — they cannot deadlock *each other*:

> **The API ▸**
> ```cpp
> #include <mutex>
> std::scoped_lock lock(m1, m2, m3);   // C++17 — all or none, deadlock-free
> // equivalent:
> std::lock(m1, m2);
> std::lock_guard g1(m1, std::adopt_lock);
> std::lock_guard g2(m2, std::adopt_lock);
> ```

```cpp
// FIXED — scoped_lock breaks circular wait via try-and-backoff
void worker() {
    std::scoped_lock g(m1, m2);   // ✓ safe regardless of call-site order
    // critical section using both locks
}
```

> **Under the hood ▸** `std::lock` tries `try_lock` on all mutexes in a
> deadlock-avoidance algorithm (try in varying order, release and retry on
> failure). Worst case is **livelock** under extreme contention (Part 5.3) —
> rare with OS mutexes, more relevant to spinlocks.

---

## 5.2.5 Prevention: try-lock + backoff

When you cannot impose a global order (e.g. lock handoff protocols), use
**try_lock** and **release everything** on failure, then retry with backoff:

```cpp
// FIXED — try both; release and backoff if either fails
bool try_take_both(std::mutex& a, std::mutex& b) {
    for (;;) {
        std::unique_lock la(a, std::try_to_lock);
        if (!la) { std::this_thread::yield(); continue; }
        std::unique_lock lb(b, std::try_to_lock);
        if (!lb) {
            la.unlock();
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            continue;
        }
        // both held — la and lb release at return/destruction
        return true;
    }
}
```

**Trade-offs ▸** Backoff turns potential deadlock into **bounded retry** and
breaks lock-step livelock (Part 5.3). Cost: latency jitter under contention.

---

## 5.2.6 Detection

Prevention is ideal; **detection** helps when third-party code holds locks you
cannot reorder:

| Approach | How |
|----------|-----|
| **Watchdog / alarm** | `alarm(2)` or a timer thread — if work doesn't finish, dump stacks |
| **Lock order graphs** | Static analyzers build "must acquire A before B" graphs |
| **TSan deadlock detector** | `-fsanitize=thread` reports lock-order inversion at runtime |
| **JVM-style** | N/A in raw C++; some debug allocators track lock sets |

```bash
g++ -std=c++17 -pthread -fsanitize=thread -g deadlock.cpp -o deadlock
./deadlock   # TSan: "deadlock detected" with both thread stacks
```

For pthread programs, **`pthread_mutexattr_settype(..., PTHREAD_MUTEX_ERRORCHECK)`**
returns `EDEADLK` when a thread tries to re-lock its own error-check mutex —
catches one class of self-deadlock, not circular wait between threads.

---

## 5.2.7 Broken → fixed: complete example

```cpp
// g++ -std=c++17 -pthread deadlock_demo.cpp -o deadlock_demo
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

std::mutex m1, m2;

/* BROKEN — uncomment to hang:
void broken() {
    std::thread a([]{
        std::lock_guard g1(m1);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::lock_guard g2(m2);
    });
    std::thread b([]{
        std::lock_guard g2(m2);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::lock_guard g1(m1);
    });
    a.join(); b.join();
}
*/

void fixed_scoped() {
    auto work = []{ std::scoped_lock g(m1, m2); };
    std::thread a(work), b(work);
    a.join(); b.join();
    std::cout << "scoped_lock: ok\n";
}

void fixed_order() {
    auto work = [](std::mutex& x, std::mutex& y){
        std::mutex& first  = (&x < &y) ? x : y;
        std::mutex& second = (&x < &y) ? y : x;
        std::lock_guard g1(first);
        std::lock_guard g2(second);
    };
    std::thread a([&]{ work(m1, m2); });
    std::thread b([&]{ work(m2, m1); });  // same order internally
    a.join(); b.join();
    std::cout << "global order: ok\n";
}

int main() {
    fixed_scoped();
    fixed_order();
}
```

---

## Summary

- Deadlock needs **mutual exclusion, hold-and-wait, no preemption, circular
  wait** — break any one to prevent.
- The two-lock **opposite-order** pattern is the canonical circular-wait example.
- **Prevention**: global lock ordering / hierarchy, `std::scoped_lock`, try-lock
  + backoff.
- **Detection**: TSan, watchdogs, lock-order static analysis.
- Related stalls: **livelock** (threads run but don't progress — Part 5.3) and
  **priority inversion** (Part 5.3).

Next: [5.3 — Livelock & starvation](03-livelock-and-starvation.md)
