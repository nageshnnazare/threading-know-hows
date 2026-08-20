# 2.3 — Condition Variables

A mutex answers "who may touch shared state right now?" A **condition variable**
answers "wake me when shared state **becomes** what I need." It is the standard
way to block a thread until a predicate on guarded data is true — producer/consumer
queues, thread pools waiting for work, barriers between phases.

Unlike a busy-wait spin loop, a CV puts the thread to sleep in the kernel until
another thread `notify`s.

---

## 2.3.1 The wait/notify contract

```
   waiter                              notifier
   ──────                              ────────
   lock(m)                             lock(m)
   while (!predicate)                  mutate shared state
       wait(cv, m)  ── unlock+sleep ──▶  predicate now true
       ▲                                 notify_one/all(cv)
       └── wake, re-lock m
   use shared state
   unlock(m)
```

> **The API ▸** — `<condition_variable>`
> ```cpp
> class condition_variable {
>     void wait(unique_lock<mutex>& lock);
>     template<class Pred> void wait(unique_lock<mutex>& lock, Pred pred);
>     void notify_one() noexcept;
>     void notify_all() noexcept;
>     // wait_for, wait_until with chrono overloads...
> };
> ```
> `wait` **requires** `unique_lock<std::mutex>` — it atomically unlocks the
> mutex and blocks; on wakeup it re-acquires the lock before returning.
> `notify_*` does not require holding the mutex (but see pitfalls below).

The two-argument form is what you should always use:

```cpp
cv.wait(lock, []{ return ready; });
// equivalent to:
while (!ready) cv.wait(lock);
```

It loops for you — critical for spurious wakeups and lost-wakeup avoidance.

---

## 2.3.2 Why the predicate loop is mandatory

Two separate mechanisms force the `while`:

**Spurious wakeup** — POSIX and the C++ standard permit `wait` to return even
when nobody called `notify`. Without re-checking the predicate you may proceed
when the condition is false.

**Lost wakeup** — if the waiter checks the predicate **before** locking, or if
`notify` fires **before** `wait` begins sleeping, the signal is missed:

```
   WRONG (no lock around check):
   thread B: if (queue.empty()) wait...   // sees empty
   thread A: push item; notify            // fires before B sleeps → LOST

   RIGHT:
   thread B: lock; while (empty) wait;    // check under lock
   thread A: lock; push; notify; unlock
```

> **Rule ▸** Always wait on a **predicate** tied to shared state, checked under
> the same mutex the CV uses. Pattern: lock → while (!pred) wait → act → unlock.

This is the same invariant as Part 1.3 and Part 5.6 (lost wakeups).

---

## 2.3.3 notify_one vs notify_all

```
   notify_one()              notify_all()
   ────────────              ────────────
   wake ONE waiter           wake EVERY waiter
   cheaper if one suffices   safe when many must re-check
   risk: wrong one wakes     thundering herd — all re-contend for lock
```

Use `notify_one` when exactly one thread can make progress (one slot in a bounded
buffer). Use `notify_all` when the predicate change affects many waiters (shutdown
flag, phase change) or when you're unsure.

> **Pitfall ▸** Calling `notify` before mutating shared state, or without the
> waiter holding the lock during the predicate check. The predicate must be
> updated **before** notify, under the mutex.

---

## 2.3.4 Timed waits

> **The API ▸**
> ```cpp
> template<class Rep, class Period>
> cv_status wait_for(unique_lock<mutex>& lock,
>                    const chrono::duration<Rep, Period>& rel_time);
> template<class Pred, class Rep, class Period>
> bool wait_for(unique_lock<mutex>& lock,
>               const chrono::duration<Rep, Period>& rel_time, Pred pred);

> cv_status wait_until(unique_lock<mutex>& lock,
>                      const chrono::time_point<...>& abs_time);
> ```
> Returns `cv_status::timeout` if the deadline passes with predicate still false.
> The predicate overload returns `bool` — `true` if pred became true.

Timed waits still follow the same predicate discipline.

---

## 2.3.5 condition_variable_any

> **The API ▸** — `<condition_variable>`
> ```cpp
> class condition_variable_any {
>     template<class Lock> void wait(Lock& lock);
>     template<class Lock, class Pred> void wait(Lock& lock, Pred pred);
>     // ...
> };
> ```
> Works with **any** BasicLockable, not just `unique_lock<mutex>`. Use when you
> need a `shared_lock` or custom lock type. Slightly heavier internally.

For 99% of code, `condition_variable` + `unique_lock<mutex>` is the right choice.

---

## 2.3.6 Producer–consumer sketch

```cpp
// g++ -std=c++17 -pthread cv_producer_consumer.cpp -o cv_producer_consumer
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

std::mutex m;
std::condition_variable cv;
std::queue<int> q;
bool done = false;

void producer() {
    for (int i = 0; i < 10; ++i) {
        {
            std::lock_guard<std::mutex> lk(m);
            q.push(i);
        }
        cv.notify_one();
    }
    {
        std::lock_guard<std::mutex> lk(m);
        done = true;
    }
    cv.notify_one();
}

void consumer() {
    while (true) {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, []{ return !q.empty() || done; });
        if (q.empty() && done) break;
        int v = q.front();
        q.pop();
        lk.unlock();
        std::cout << "got " << v << "\n";
    }
}

int main() {
    std::thread p(producer), c(consumer);
    p.join();
    c.join();
    return 0;
}
```

The consumer waits until `!q.empty() || done` — the predicate covers both "data
available" and "clean shutdown." Full producer–consumer design is Part 4.2.

> **Under the hood ▸** `wait` compiles to: unlock mutex → `futex_wait` (sleep) →
> on wakeup, compete for mutex again. `notify` marks waiters runnable; they don't
> run until they re-acquire the lock.

**Trade-offs ▸** Condition variables are efficient for "wait until event" but
require careful pairing with mutex + predicate. Alternatives: semaphores (Part 2.6),
polling atomics (Part 3), or blocking queues in libraries.

---

## Summary

- A condition variable blocks until shared state satisfies a predicate, in
  cooperation with a mutex.
- Always use `wait(lock, predicate)` — handles spurious wakeups and forces
  re-check after every wake.
- Update shared state under the lock **before** `notify_one` / `notify_all`.
- `unique_lock<mutex>` is required; `condition_variable_any` generalizes locks.
- `wait_for` / `wait_until` add timeouts with the same predicate rules.
- Producer–consumer is the canonical pattern; lost wakeups happen when you skip
  the lock/predicate discipline (Part 5.6).

Next: [2.4 — Futures, promises & async](04-futures-and-async.md)
