# 5.6 — Lost Wakeups

A **lost wakeup** occurs when a thread signals "data is ready" but the waiting
thread has **not yet entered** its sleep — the notification is a no-op, the waiter
blocks forever, and the system stalls. This is not a data race on the payload; it
is a **synchronization ordering bug** between the predicate, the mutex, and the
condition variable. Part 1.3 and Part 2.3 cover the API; this chapter explains
*why* the predicate loop is mandatory and how semaphores differ.

---

## 5.6.1 The lost-wakeup race

The failure mode in timeline form:

```
   consumer (waiter)                 producer (signaler)
   ─────────────────                 ───────────────────
   check ready == false  ✓
                                     lock(m)
                                     ready = true
                                     notify_one()     ← signal sent NOW
                                     unlock(m)
   lock(m)
   wait(cv)               ← goes to sleep — NO pending signal
   (never wakes)
```

The notify **happened before** the wait — POSIX and C++ condition variables do
**not** queue wakeups for future waiters. A signal with no waiter is discarded.

![Notify fires between predicate check and wait — waiter sleeps forever](figures/lost-wakeup.svg)

> **Rule ▸** Treat `notify_one()` / `pthread_cond_signal()` like a **pulse**, not
> a **message**. If nobody is blocked in `wait()` at that instant, the pulse is
> lost forever.

---

## 5.6.2 Spurious wakeups

Even when you avoid lost wakeups, **`wait()` may return without a notify** —
**spurious wakeup** (implementation artifact, signal delivery, pthreads spec
permits it):

```
   waiter:  wait(cv)  →  wakes up
            but ready is still false   ← spurious
```

Without re-checking the predicate, spurious wakeups cause the same logical bugs as
lost wakeups (proceeding when the condition is false).

> **The API ▸**
> ```cpp
> cv.wait(lock, predicate);   // equivalent to:
> while (!predicate()) cv.wait(lock);
> ```
> ```c
> while (!ready) pthread_cond_wait(&cv, &m);
> ```

Both **lost wakeup** and **spurious wakeup** are defeated by the same pattern.

---

## 5.6.3 The fix: mutex + predicate loop

The invariant that works:

1. **Mutate shared state under the mutex.**
2. **Notify while holding the mutex** (or immediately after unlock — see below).
3. **Wait in a loop** that re-tests the predicate **while holding the mutex**.

```
   producer:                          consumer:
   lock(m)                            lock(m)
   ready = true                       while (!ready)
   notify_one()                         wait(cv, m)  // releases m while sleeping
   unlock(m)                            unlock(m)
```

Why it works for **lost wakeup**:

```
   consumer checks ready under lock → false
   consumer enters wait → atomically releases lock AND sleeps
   OR producer runs first:
   producer sets ready + notifies under lock
   consumer's wait sees ready true → returns immediately, never sleeps
```

The **`wait(lock, predicate)`** overload checks the predicate **before** sleeping.
If the producer finished first, the consumer never blocks — the wakeup cannot be
lost.

> **Under the hood ▸** `pthread_cond_wait` atomically releases the mutex and enters
> the wait queue — but the **predicate check before wait** is your code's
> responsibility. The C++ standard encodes it in the two-argument `wait`.

**Notify under lock or after?** POSIX allows both. Holding the lock guarantees the
waiter sees the updated predicate when it wakes and re-acquires. Notifying after
unlock can reduce waiter latency (waiter doesn't block on lock held by signaler) but
requires the predicate loop — which you need anyway.

---

## 5.6.4 Broken → fixed

```cpp
// g++ -std=c++17 -pthread lost_wakeup.cpp -o lost_wakeup
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

std::mutex              m;
std::condition_variable cv;
bool                    ready = false;

/* BROKEN — check outside lock + wait without predicate */
void broken_consumer() {
    if (!ready) {   // race window opens here
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::unique_lock<std::mutex> g(m);
        cv.wait(g);   // may sleep forever if notify happened during sleep above
    }
}

/* FIXED — predicate loop under the mutex */
void fixed_consumer() {
    std::unique_lock<std::mutex> g(m);
    cv.wait(g, []{ return ready; });
    std::cout << "consumer: ready=" << ready << "\n";
}

void producer() {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    {
        std::lock_guard<std::mutex> g(m);
        ready = true;
        cv.notify_all();
    }
}

int main() {
    ready = false;
    std::thread c(fixed_consumer);
    std::thread p(producer);
    c.join();
    p.join();
    std::cout << "fixed path completed\n";
    // broken_consumer() — do not join in production; shown for instruction only
}
```

Producer-consumer queues (Part 4.2) use the same pattern on `!queue.empty()`.

---

## 5.6.5 Raw condvars vs semaphores

| Mechanism | Remembers signal? | Typical use |
|-----------|-------------------|-------------|
| **Condition variable** | **No** — lost if no waiter | Predicate + mutex (complex conditions) |
| **Counting semaphore** | **Yes** — count of posts minus waits | Producer-consumer **permit** counting |

```
   semaphore (count = 0):
   post post wait     → count: 0→1→2→1   (second post remembered)
   condvar:
   signal signal wait → one waiter wakes; second signal lost
```

A semaphore **remembers** excess `post()` operations in its counter — you cannot
"lose" a wakeup the same way, though you can still misuse it (wait without checking
invariants on a **bounded buffer** — semaphores count slots, not buffer contents
semantics alone). Part 1.5 compares both.

> **Pitfall ▸** Replacing a condvar with a semaphore because of lost wakeups without
> redesigning the protocol often breaks **broadcast** semantics or multi-condition
> predicates. Fix the condvar pattern first.

---

## 5.6.6 POSIX equivalent

```c
// gcc -pthread lost_wakeup.c -o lost_wakeup
#include <pthread.h>
#include <stdbool.h>

pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  cv = PTHREAD_COND_INITIALIZER;
bool ready = false;

void* consumer(void* arg) {
    (void)arg;
    pthread_mutex_lock(&m);
    while (!ready)                    /* NOT if — while */
        pthread_cond_wait(&cv, &m);
    pthread_mutex_unlock(&m);
    return NULL;
}

void* producer(void* arg) {
    (void)arg;
    pthread_mutex_lock(&m);
    ready = true;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&m);
    return NULL;
}
```

Same rule: **`while`, not `if`**, on the predicate.

---

## Summary

- **Lost wakeup**: `notify` between predicate test and `wait()` → waiter sleeps
  forever; condvars do not queue signals.
- **Spurious wakeup**: `wait()` returns without notify — must recheck predicate.
- **Fix**: hold mutex for state change + notify; **`wait(lock, predicate)`** or
  `while (!pred) wait()` (Part 1.3, Part 2.3).
- **Semaphores remember posts**; condvars do not — choose by protocol complexity.

Next: [6.1 — The OpenMP model](../06-openmp/01-openmp-model.md)
