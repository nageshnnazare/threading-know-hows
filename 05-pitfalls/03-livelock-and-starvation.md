# 5.3 — Livelock & Starvation

**Deadlock** (Part 5.2) freezes threads in a circular wait — nobody runs. **Livelock**
is worse in a different way: threads **keep running** but make **no application
progress**, usually because they politely yield to each other in a repeating
pattern. **Starvation** is when one thread **never** acquires a resource it needs,
while others proceed. Both are scheduling and locking pathologies; both require
mechanical understanding to fix.

---

## 5.3.1 Livelock: the corridor metaphor

Two people meet in a narrow corridor. Each steps aside to let the other pass —
in the **same direction**. They shuffle back and forth forever; neither passes:

```
   time ──▶

   person A:  ← step   → step   ← step   → step   ...
   person B:  → step   ← step   → step   ← step   ...

   both active, zero progress
```

In code, the same pattern appears when two threads **acquire one lock, fail on
the second, release both, and immediately retry** — in perfect lock-step:

```
   thread A:  lock m1  try m2 ✗  unlock m1  retry ...
   thread B:  lock m2  try m1 ✗  unlock m2  retry ...

   neither holds both; neither completes the transaction
```

This is not deadlock — no thread is blocked waiting on a futex. CPUs burn cycles;
the system looks "busy" but throughput is zero.

---

## 5.3.2 Naive try-lock + retry without backoff

The try-lock cure for deadlock (Part 5.2) **becomes livelock** if both threads
retry instantly:

```cpp
// LIVLOCK-PRONE — no backoff between retries
bool transfer(std::mutex& from, std::mutex& to) {
    for (;;) {
        from.lock();
        if (to.try_lock()) {
            // ... move funds ...
            to.unlock();
            from.unlock();
            return true;
        }
        from.unlock();
        // immediate retry → synchronized with the other thread's retry
    }
}
```

> **Pitfall ▸** `std::lock`'s internal try-and-backoff can livelock under extreme
> spinlock contention (busy-wait mutexes). OS mutexes usually sleep on failure,
> which injects enough jitter to break lock-step — but **do not rely on the
> scheduler to save you**.

**Fix: randomized exponential backoff** — after failure, sleep a random duration
before retry; double the cap on repeated failures (Ethernet CSMA/CD uses the same
idea):

```cpp
// FIXED — randomized backoff breaks lock-step
#include <random>

bool transfer_fixed(std::mutex& from, std::mutex& to, std::mt19937& rng) {
    int backoff_us = 50;
    for (;;) {
        from.lock();
        if (to.try_lock()) {
            // ... critical section ...
            to.unlock();
            from.unlock();
            return true;
        }
        from.unlock();
        std::uniform_int_distribution<int> dist(0, backoff_us);
        std::this_thread::sleep_for(std::chrono::microseconds(dist(rng)));
        backoff_us = std::min(backoff_us * 2, 10'000);
    }
}
```

**Trade-offs ▸** Backoff adds latency under contention but guarantees eventual
progress with probability 1 (assuming fair scheduler). Too-aggressive backoff
hurts throughput; too-little recreates livelock.

---

## 5.3.3 Starvation: one thread never wins

**Starvation** means a thread is **ready** to run (or ready to acquire a lock) but
**never gets the resource** while others repeatedly do:

```
   lock queue (unfair):  T1 T1 T1 T1 T1 T1 ...   T2 never reaches the front
                         ↑ high-priority or lucky thread keeps re-acquiring
```

Common causes:

| Cause | Mechanism |
|-------|-----------|
| **Unfair locks** | `pthread_mutex` default may favor last owner (implementation-defined) |
| **Priority scheduling** | High-priority thread always preempts low-priority waiter |
| **Writer preference RW-lock** | Continuous readers block a waiting writer — or vice versa (Part 4.3) |
| **Busy spinning** | One core monopolizes cache line; others spin without acquiring |

Starvation is **not** UB — the program may be "correct" but **unfair** and
violates latency SLOs.

---

## 5.3.4 Priority inversion

A classic starvation variant on **fixed-priority** RTOS / real-time Linux:

```
   low-priority L holds lock M
   high-priority H waits on M          ← H blocked by L
   medium-priority M preempts L        ← M runs instead of H!

   H is inverted: indirectly blocked by M, which is not even waiting for M
```

Timeline:

```
   L: ── holds M ── preempted ──────────────── ...
   M:              ── runs (medium) ── ...
   H: ── wants M ── BLOCKED ───────────────── ...
```

> **Under the hood ▸** **Priority inheritance**: when H blocks on M held by L, the
> kernel **temporarily boosts L's priority** to H's level until L releases M.
> Linux `SCHED_FIFO` + PI mutexes (`PTHREAD_PRIO_INHERIT`) implement this.
> Without PI, a low-priority housekeeping thread can delay a critical control loop
> indefinitely.

```c
// pthread priority-inheritance mutex (POSIX)
pthread_mutexattr_t attr;
pthread_mutexattr_init(&attr);
pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
pthread_mutex_init(&m, &attr);
```

---

## 5.3.5 Fairness: queuing locks and OS policy

**Fair locks** (FIFO wait queue) guarantee **bounded** wait: a thread that
requests the lock eventually acquires it if every holder releases:

```
   unfair (default-ish):     winner may jump the queue
   fair (queued):            T1 → T2 → T3 → T1 → ... strict turn-taking
```

C++20 has no standard fair mutex. Options:

- **`std::mutex`** — generally fair enough for app code; not RT-guaranteed.
- **Ticket spinlock** — `fetch_add` on `next_ticket`, wait until `now_serving ==
  my_ticket` (Part 3.1).
- **Semaphores with FIFO ordering** — counting semaphore remembers permits
  (Part 1.5); unlike raw condvars, a post is not lost.

For **CPU scheduling starvation**, use explicit QoS: cgroups, `nice`, real-time
priorities — outside the language but part of production tuning.

---

## 5.3.6 Example: livelock vs backoff

```cpp
// g++ -std=c++17 -pthread livelock.cpp -o livelock
#include <chrono>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>

std::mutex m1, m2;

void try_both_no_backoff(int id, int& attempts) {
    std::mutex& first  = (id == 0) ? m1 : m2;
    std::mutex& second = (id == 0) ? m2 : m1;
    for (int n = 0; n < 500; ++n) {
        first.lock();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (second.try_lock()) {
            second.unlock();
            first.unlock();
            attempts = n;
            return;
        }
        first.unlock();
        // no backoff — livelock-prone under adversarial scheduling
    }
    attempts = -1;
}

void try_both_with_backoff(int id, int& attempts) {
    std::mutex& first  = (id == 0) ? m1 : m2;
    std::mutex& second = (id == 0) ? m2 : m1;
    std::mt19937 rng(id ^ 0xdeadbeef);
    for (int n = 0; n < 500; ++n) {
        first.lock();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (second.try_lock()) {
            second.unlock();
            first.unlock();
            attempts = n;
            return;
        }
        first.unlock();
        std::uniform_int_distribution<int> d(50, 500);
        std::this_thread::sleep_for(std::chrono::microseconds(d(rng)));
    }
    attempts = -1;
}

int main() {
    int a = 0, b = 0;
    std::thread t1(try_both_no_backoff, 0, std::ref(a));
    std::thread t2(try_both_no_backoff, 1, std::ref(b));
    t1.join(); t2.join();
    std::cout << "no backoff:    attempts A=" << a << " B=" << b << "\n";

    a = b = 0;
    std::thread t3(try_both_with_backoff, 0, std::ref(a));
    std::thread t4(try_both_with_backoff, 1, std::ref(b));
    t3.join(); t4.join();
    std::cout << "with backoff:  attempts A=" << a << " B=" << b << "\n";
}
```

On many systems the no-backoff version succeeds eventually (scheduler jitter) but
may take hundreds of attempts; backoff typically finishes in single digits.

---

## Summary

- **Livelock**: threads run but make no progress — polite retry loops in
  lock-step; fix with **randomized exponential backoff** or `std::scoped_lock`.
- **Starvation**: a thread never acquires a lock/CPU while others do — unfair
  locks, priority scheduling, RW-lock policy.
- **Priority inversion**: high-priority thread blocked behind low-priority lock
  holder while medium-priority work runs — fix with **priority inheritance**.
- **Fairness**: queued locks, ticket algorithms, semaphores; distinguish from
  **deadlock** (Part 5.2) where nobody runs at all.

Next: [5.4 — False sharing](04-false-sharing.md)
