# 2.6 — C++20 Concurrency

C++20 filled gaps that engineers had been patching with third-party libraries or
raw pthread patterns: a thread that **always joins**, cooperative **cancellation**,
and standard **latch**, **barrier**, and **semaphore** types. This chapter covers
the `<thread>` / `<stop_token>` / `<latch>` / `<barrier>` / `<semaphore>` additions
and `atomic::wait` / `notify`.

Build with **C++20**: `g++ -std=c++20 -pthread`.

---

## 2.6.1 std::jthread — RAII + cancellation

`std::jthread` wraps `std::thread` with two fixes from Part 2.1:

```
   std::thread destructor + joinable  →  std::terminate()
   std::jthread destructor            →  request_stop(); join();
```

> **The API ▸** — `<thread>`
> ```cpp
> class jthread {
> public:
>     template<class F, class... Args>
>     explicit jthread(F&& f, Args&&... args);
>     ~jthread();                    // joins if joinable
>     void join();
>     void detach();                 // discouraged — loses stop coordination
>     bool joinable() const;
>     stop_source get_stop_source();
>     stop_token get_stop_token() const;
> };
> ```
> If the callable accepts `stop_token` as first parameter, the runtime injects
> one tied to the `jthread`'s internal `stop_source`.

Cancellation is **cooperative** — nothing preempts your thread:

```cpp
void work(std::stop_token st) {
    while (!st.stop_requested()) {
        // do chunk; check st periodically
    }
}
```

When the `jthread` is destroyed (or `request_stop()` is called on its source), waiters
see `stop_requested() == true`.

---

## 2.6.2 stop_token, stop_source, stop_callback

> **The API ▸** — `<stop_token>`
> ```cpp
> class stop_token {
>     bool stop_requested() const noexcept;
>     bool stop_possible() const noexcept;
> };
> class stop_source {
>     void request_stop() noexcept;
>     stop_token get_token() const;
> };
> template<class Callback>
> class stop_callback { /* registers; unregisters on destroy */ };
> ```
> One `stop_source` fans out to many `stop_token`s. `stop_callback` runs your
> functor when stop is requested — useful to unblock a blocking call:

```
   jthread dtor / request_stop()
              │
              ▼
   stop_source flag set
              │
      ┌───────┴───────┐
      ▼               ▼
   token in worker   stop_callback → cv.notify_all()
```

> **Rule ▸** Cancellation is not forced. Long-running loops must poll
> `stop_requested()` or register a callback. There is no POSIX-style cancel.

---

## 2.6.3 std::latch — single-use countdown

> **The API ▸** — `<latch>`
> ```cpp
> class latch {
> public:
>     explicit latch(ptrdiff_t expected);
>     void count_down(ptrdiff_t n = 1);
>     bool try_wait() const noexcept;
>     void wait() const;
>     void arrive_and_wait(ptrdiff_t n = 1);
>     static constexpr ptrdiff_t max() noexcept;
> };
> ```
> A **single-phase** barrier: wait until the counter hits zero. `count_down`
> decrements; `wait` blocks until zero. **Not reusable** — once released, done.

Use case: "main waits until N workers finish setup":

```cpp
std::latch start_latch(1);
std::latch ready_latch(N);

// each worker:
ready_latch.count_down();   // signal ready
start_latch.wait();         // wait for go
// work...
```

Compare pthread barrier (Part 1.4) — `latch` is one-shot only.

---

## 2.6.4 std::barrier — reusable synchronization

> **The API ▸** — `<barrier>`
> ```cpp
> template<class CompletionFunction = /* see standard */>
> class barrier {
> public:
>     explicit barrier(ptrdiff_t expected,
>                      CompletionFunction completion = {});
>     void arrive_and_drop();   // C++20; reduce count permanently
>     ptrdiff_t arrive(ptrdiff_t update = 1);
>     void wait();
> };
> ```
> Threads **arrive** at phases; when `expected` arrivals complete a phase, the
> **completion function** runs (serially, by one thread), then the barrier resets
> for the next phase.

```
   phase 1:  [A][B][C] all arrive → completion() → phase 2
   phase 2:  [A][B][C] all arrive → completion() → phase 3
```

Ideal for iterative parallel algorithms (Jacobi, time-step loops) where all threads
must finish step *k* before any starts step *k+1*.

---

## 2.6.5 Semaphores

> **The API ▸** — `<semaphore>`
> ```cpp
> template<ptrdiff_t LeastMaxValue = /* impl-defined */>
> class counting_semaphore {
>     explicit counting_semaphore(ptrdiff_t desired);
>     void acquire();
>     void release(ptrdiff_t update = 1);
>     bool try_acquire() noexcept;
>     // try_acquire_for, try_acquire_until...
> };
> using binary_semaphore = counting_semaphore<1>;
> ```
> A counter of permits: `acquire` decrements (blocks at 0); `release` increments.
> `binary_semaphore` is mutex-like but can **signal without holding** the lock —
> useful for producer–consumer (Part 4.2), unlike a mutex which must be unlocked
> by the owner.

**Trade-offs ▸** Semaphores are flexible but easy to misuse (forgotten `release`,
> wrong initial count). Prefer mutex + CV when ownership semantics matter; prefer
> semaphores for resource counting.

---

## 2.6.6 atomic::wait / notify

C++20 adds blocking wait on atomics (futex-like):

> **The API ▸** — `<atomic>`
> ```cpp
> void atomic<T>::wait(T old, memory_order order = memory_order::seq_cst) const;
> void atomic<T>::notify_one();
> void atomic<T>::notify_all();
> ```
> `wait` blocks until the value changes from `old` (or spuriously wakes — re-check
> in a loop). Pairs with `notify_*` for lightweight event signaling without a mutex.

Useful for lock-free structures and thread parking; semantics tie into Part 3.4
memory orders.

---

## 2.6.7 Example: jthread + stop_token

```cpp
// g++ -std=c++20 -pthread jthread_demo.cpp -o jthread_demo
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

using namespace std::chrono_literals;

void worker(std::stop_token st, int id) {
    int count = 0;
    while (!st.stop_requested()) {
        std::this_thread::sleep_for(10ms);
        if (++count % 50 == 0)
            std::cout << "worker " << id << " tick " << count << "\n";
    }
    std::cout << "worker " << id << " stopped after " << count << " ticks\n";
}

int main() {
    std::vector<std::jthread> pool;
    for (int i = 0; i < 3; ++i)
        pool.emplace_back(worker, i);

    std::this_thread::sleep_for(200ms);
    // jthreads join + request stop on destruction
    return 0;
}
```

Scope exit destroys `pool` → each `jthread` requests stop → workers observe
`stop_requested()` → destructor joins. No `std::terminate`, no leaked threads.

> **Pitfall ▸** Detaching a `jthread` loses clean stop/join semantics. Treat
> `jthread` like a scoped handle — let it join at end of scope.

---

## Summary

- `std::jthread` auto-joins on destruction and integrates `stop_token` for
  cooperative cancellation.
- `stop_source` / `stop_token` / `stop_callback` propagate stop requests; workers
  must poll or register callbacks.
- `latch` — single countdown to zero; `barrier` — reusable phases with optional
  completion function.
- `counting_semaphore` / `binary_semaphore` count resources; signal without
  mutex ownership.
- `atomic::wait` / `notify_*` block until atomic value changes — ties to memory
  model (Part 3).
- Prefer C++20 over manual join/detach when your toolchain supports it.

Next: [3.1 — Atomics basics](../03-atomics-and-memory-model/01-atomics-basics.md)
