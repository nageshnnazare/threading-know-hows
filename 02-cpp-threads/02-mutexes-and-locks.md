# 2.2 — Mutexes & Lock Wrappers

A mutex (**mut**ual **ex**clusion) is the simplest fix for the shared-state
problem (Part 0.4): only one thread at a time may execute a **critical section**
that touches shared mutable data. C++ wraps the same pthread mutex idea (Part 1.2)
with RAII lock types so you cannot forget to unlock on every exit path.

---

## 2.2.1 The mutex family

![Mutex types and lock wrappers](figures/lock-types.svg)

```
   thread wants critical section
              │
              ▼
         lock(mutex)  ──▶  if free: enter
              │           if held: block (kernel wait queue)
              ▼
         ... critical section — ONE thread only ...
              │
              ▼
         unlock(mutex)  ──▶  wake a waiter
```

> **The API ▸** — `<mutex>`
> ```cpp
> class mutex {
>     void lock();
>     bool try_lock();              // non-blocking
>     void unlock();
> };
> class recursive_mutex { /* same + same thread may re-lock */ };
> class timed_mutex {
>     bool try_lock_for(const chrono::duration& d);
>     bool try_lock_until(const chrono::time_point& t);
> };
> class shared_mutex {              // C++17
>     void lock();                  // exclusive (writer)
>     void lock_shared();           // shared (reader)
>     void unlock(); void unlock_shared();
> };
> ```
> All are **non-copyable, non-movable**. A thread that `lock()`s must `unlock()`.
> `lock()` blocks; `try_lock()` returns immediately.

| Type | When to use |
|------|-------------|
| `mutex` | Default — one owner, no re-entry |
| `recursive_mutex` | Callbacks / legacy code that re-locks same mutex (prefer redesign) |
| `timed_mutex` | Bounded wait — timeout instead of block forever |
| `shared_mutex` | Many readers OR one writer (Part 4.3) |

> **Under the hood ▸** Uncontended `lock()` is often a single atomic compare-and-swap
> in userspace. On contention the runtime falls through to a **futex** (Linux) or
> similar  — the thread sleeps in the kernel until `unlock()` wakes it. That
> syscall path is why mutexes are cheap when uncontended and expensive under fight.

---

## 2.2.2 lock_guard — minimal RAII

> **The API ▸**
> ```cpp
> template<class Mutex> class lock_guard {
> public:
>     explicit lock_guard(Mutex& m);
>     lock_guard(Mutex& m, adopt_lock_t);  // mutex already locked
>     ~lock_guard();   // unlocks m
> };
> ```
> Construct → lock. Destroy → unlock. **Not movable.** Use when the lock scope
> equals the guard's lifetime and you never need to defer or hand off.

```cpp
std::mutex m;
int counter = 0;

void increment() {
    std::lock_guard<std::mutex> lk(m);
    ++counter;   // critical section
}   // unlock here, even if exception thrown
```

---

## 2.2.3 unique_lock — the flexible one

`unique_lock` is what condition variables require (Part 2.3):

> **The API ▸**
> ```cpp
> template<class Mutex> class unique_lock {
> public:
>     explicit unique_lock(Mutex& m);
>     unique_lock(Mutex& m, defer_lock_t);    // don't lock yet
>     unique_lock(Mutex& m, try_to_lock_t);
>     unique_lock(Mutex& m, adopt_lock_t);
>     void lock(); void unlock(); bool owns_lock() const;
>     // movable; not copyable
> };
> ```

```
   lock_guard          unique_lock
   ──────────          ───────────
   lock on ctor        defer_lock → lock later
   unlock on dtor      try_lock / timed try
   fixed scope         movable — transfer ownership
   no condvar          required by condition_variable::wait
```

**Trade-offs ▸** `lock_guard` is lighter when you only need scoped lock/unlock.
`unique_lock` pays for flexibility: deferred locking, timed waits, and releasing
the mutex inside `wait()`.

---

## 2.2.4 scoped_lock — multiple mutexes, no deadlock

Locking two mutexes in inconsistent order is a classic deadlock (Part 5.2).
C++17's `scoped_lock` locks **all** mutexes atomically via `std::lock`:

> **The API ▸**
> ```cpp
> template<class... Mutexes>
> class scoped_lock {
> public:
>     explicit scoped_lock(Mutexes&... m);  // locks all via std::lock
>     ~scoped_lock();                        // unlocks all
> };
> ```

```cpp
std::mutex m1, m2;

void transfer() {
    std::scoped_lock lk(m1, m2);   // deadlock-safe — order doesn't matter
    // touch data guarded by both
}
```

> **The API ▸** — free function
> ```cpp
> template<class L1, class L2, class... L3>
> void lock(L1& m1, L2& m2, L3&... ms);   // deadlock-avoiding multi-lock
> void adopt_lock(adopt_lock_t, Mutex&...);  // mark already-locked mutexes
> ```
> `std::lock` uses a deadlock-avoidance algorithm (try, back off, retry). After
> manual multi-locking, pass `std::adopt_lock` to the guard so it unlocks on exit
> without re-locking.

---

## 2.2.5 shared_lock — concurrent readers

With `shared_mutex`, many `shared_lock` holders can read simultaneously; a
`unique_lock`/`lock_guard` writer excludes everyone:

```cpp
std::shared_mutex rw;
std::map<std::string, int> cache;

int lookup(const std::string& key) {
    std::shared_lock lk(rw);          // shared — readers OK together
    return cache.at(key);
}

void insert(std::string key, int val) {
    std::lock_guard<std::shared_mutex> lk(rw);  // exclusive writer
    cache[std::move(key)] = val;
}
```

Readers-writer patterns are developed fully in Part 4.3.

---

## 2.2.6 Example: protecting shared state

```cpp
// g++ -std=c++17 -pthread mutex_demo.cpp -o mutex_demo
#include <iostream>
#include <thread>
#include <mutex>
#include <vector>

std::mutex m;
long counter = 0;

void worker(int n) {
    for (int i = 0; i < n; ++i) {
        std::lock_guard<std::mutex> lk(m);
        ++counter;
    }
}

int main() {
    constexpr int per_thread = 250'000;
    std::vector<std::thread> ts;
    for (int i = 0; i < 4; ++i)
        ts.emplace_back(worker, per_thread);
    for (auto& t : ts) t.join();
    std::cout << "counter = " << counter << " (expected "
              << 4 * per_thread << ")\n";
    return 0;
}
```

Every increment is inside the critical section → no data race → result is exact.
The cost: threads serialize on `m` — fine for tiny sections, a bottleneck if the
work inside the lock grows (Part 5.4 false sharing, Part 3 atomics for counters).

> **Pitfall ▸** Holding a lock while doing I/O or slow work — you turn parallelism
> into serial execution. Keep critical sections **short**.

> **Rule ▸** If two threads can touch the same mutable location and at least one
> writes, that access must be inside a critical section (or atomic). The mutex
> only protects what happens **between** `lock` and `unlock`.

---

## Summary

- `std::mutex` gives exclusive access; variants add re-entry, timeouts, or
  shared/exclusive modes (`shared_mutex`).
- `lock_guard` — simple RAII; `unique_lock` — defer/try/timed, movable, required
  by condition variables.
- `scoped_lock` + `std::lock` lock multiple mutexes without deadlock ordering bugs.
- `shared_lock` pairs with `shared_mutex` for reader concurrency.
- Mutex contention may syscall to the kernel; keep critical sections short.
- Always prefer RAII guards over raw `lock()`/`unlock()`.

Next: [2.3 — Condition variables](03-condition-variables.md)
