# 2.1 — std::thread & jthread

C++11 gave us a portable, type-safe thread API in `<thread>`. Under the hood it
wraps the same OS primitives as pthreads (Part 1.1), but the surface is RAII-friendly
callables instead of `void* (*)(void*)`. This chapter is the lifecycle: spawn,
join, detach, move — and the rules that keep you out of `std::terminate`.

---

## 2.1.1 Constructing a thread

A `std::thread` runs a **callable** (function, functor, lambda) on a new thread.
Arguments after the callable are **decayed and copied/moved** into thread-local
storage, then passed to the callable:

```
   main thread                          new std::thread
   ───────────                          ───────────────
   std::thread t(f, arg1, arg2)  ──▶    copies/moves args into thread storage
                                        invokes f(decayed_args...)
```

> **The API ▸**
> ```cpp
> #include <thread>
> template<class F, class... Args>
> explicit thread(F&& f, Args&&... args);
> ```
> Creates a joinable thread that executes `INVOKE(decay_copy(f), decay_copy(args)...)`.
> The thread begins running **before** the constructor returns (as-if concurrent
> with the caller). Requires linking with `-pthread`.

The copy-by-default behavior is the #1 surprise for C++ newcomers:

> **Pitfall ▸** Passing a reference without `std::ref`:
> ```cpp
> void inc(int& n) { ++n; }
> int x = 0;
> std::thread t(inc, x);           // ✗ compiles on some compilers, copies x
> std::thread t(inc, std::ref(x)); // ✓ thread sees the real x
> ```
> `std::thread` always stores arguments by value in its internal tuple. A
> `T&` parameter receives a reference to a **copy inside the thread** — which
> dies when the thread ends. Use `std::ref` / `std::cref`, or capture by
> reference in a lambda: `std::thread t([&x]{ ++x; });`.

Member functions work with `std::bind` or a lambda:

```cpp
struct Worker {
    void run(int id) { /* ... */ }
};
Worker w;
std::thread t1(&Worker::run, &w, 42);           // pointer + args
std::thread t2([&w]{ w.run(42); });             // often clearer
```

---

## 2.1.2 join() vs detach()

Every joinable thread must end its life in exactly one of two ways:

```
   joinable thread at destruction
              │
      ┌───────┴───────┐
      ▼               ▼
   join()          detach()
   wait until      orphan thread;
   thread ends     no handle left
      │               │
      └───────┬───────┘
              ▼
        not joinable → safe to destroy
```

> **The API ▸**
> ```cpp
> void join();    // blocks until thread finishes; releases OS resources
> void detach();  // thread runs independently; handle becomes non-joinable
> bool joinable() const noexcept;
> ```
> After `join()` or `detach()`, `joinable()` is `false`. Calling either on a
> non-joinable thread throws `std::system_error`.

> **Rule ▸** Never let a **joinable** `std::thread` be destroyed. Its destructor
> calls `std::terminate()` — a hard abort, not an exception. This is deliberate:
> the standard refuses to guess whether you meant to join or detach.

> **Under the hood ▸** `join()` maps to `pthread_join` on POSIX: the kernel
> reaps the thread's task struct and returns its exit status. `detach()` is
> `pthread_detach` — the thread keeps running but nobody waits for it. A detached
> thread that touches shared state after main exits is UB if the process tears
> down while it still runs.

**Trade-offs ▸** Prefer `join()` unless you have a fire-and-forget daemon with
a clear lifetime (logging, telemetry). Detached threads are easy to leak or
race with shutdown.

---

## 2.1.3 Thread identity and hardware concurrency

> **The API ▸**
> ```cpp
> std::thread::id get_id() const noexcept;
> unsigned int hardware_concurrency() noexcept;  // static
> ```
> `get_id()` returns an opaque ID; default-constructed threads and threads after
> join/detach share a distinct "not-a-thread" ID. `hardware_concurrency()` hints
> at logical cores (may return 0 if unknown).

Useful for logging and associating per-thread data (Part 2.5):

```cpp
std::cout << "worker " << std::this_thread::get_id() << "\n";
int n = std::thread::hardware_concurrency();  // e.g. 8 — not a promise of 8 threads
```

Do **not** treat `hardware_concurrency()` as a hard cap — I/O-bound workloads
often benefit from more threads than cores; compute-bound pools usually match
core count (Part 4.1).

---

## 2.1.4 Moving threads

`std::thread` is **move-only** (not copyable). Ownership of the OS thread handle
transfers on move:

```cpp
std::thread t1(worker);
std::thread t2 = std::move(t1);   // t1 is now non-joinable (empty)
// t2 owns the thread — must join or detach t2
t2.join();
```

This enables containers and thread pools:

```cpp
std::vector<std::thread> pool;
pool.emplace_back(worker, 0);
pool.emplace_back(worker, 1);
for (auto& t : pool) t.join();
```

A moved-from thread is not joinable — destroying it is safe.

---

## 2.1.5 Exceptions do not cross the thread boundary

If the callable throws and the exception escapes, **`std::terminate` is called**
(same as an uncaught exception in a C callback). There is no stack unwinding
into the spawning thread.

```
   thread A spawns thread B
   thread B throws std::runtime_error
              │
              ▼
   terminate() — thread A never sees the exception
```

To propagate errors, use `std::promise` / `std::future` (Part 2.4) or catch inside
the thread and signal failure through shared state protected by a mutex.

> **Pitfall ▸** Letting exceptions fly out of a thread callable. Always catch
> inside the thread or use futures.

---

## 2.1.6 A complete example

```cpp
// g++ -std=c++17 -pthread std_thread_demo.cpp -o std_thread_demo
#include <iostream>
#include <thread>
#include <functional>
#include <vector>
#include <chrono>

void worker(int id, int& shared) {
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(50ms * id);
    ++shared;   // ⚠ data race — fix with mutex (Part 2.2) or atomic (Part 3.1)
    std::cout << "thread " << id << " id=" << std::this_thread::get_id() << "\n";
}

int main() {
    int total = 0;
    std::vector<std::thread> threads;

    for (int i = 0; i < 4; ++i)
        threads.emplace_back(worker, i, std::ref(total));

    for (auto& t : threads)
        if (t.joinable()) t.join();

    std::cout << "done (total may be < 4 — unsynchronized)\n";
    return 0;
}
```

The unsynchronized `++shared` is intentional — it connects back to Part 0.4's
shared-state problem. Real code wraps that increment in a mutex or atomic.

---

## 2.1.7 Teaser: std::jthread (Part 2.6)

C++20 adds `std::jthread` — a **joining thread**: its destructor automatically
`request_stop()` + `join()`, so you cannot forget. It also wires cooperative
cancellation through `std::stop_token`. If you've ever hit `std::terminate` because
a joinable `std::thread` went out of scope, `jthread` is the fix:

```cpp
// C++20 — full treatment in Part 2.6
std::jthread t([](std::stop_token st) {
    while (!st.stop_requested()) { /* work */ }
});  // joins here — no terminate
```

For now, treat every `std::thread` as **must join or detach before destruction**.

---

## Summary

- `std::thread(callable, args...)` **copies/decays** arguments — use `std::ref`
  for references; lambdas with `[&]` are often clearer.
- A joinable thread **must** be `join()`ed or `detach()`ed before destruction;
  otherwise `std::terminate()`.
- `join()` waits and reaps; `detach()` orphans the thread — prefer join unless
  lifetime is truly independent.
- `std::thread` is move-only; moved-from threads are empty and non-joinable.
- Exceptions in the thread callable call `std::terminate` — use futures or catch
  internally.
- `hardware_concurrency()` is a hint, not a law; `get_id()` identifies threads
  opaquely.
- C++20 `std::jthread` auto-joins and supports cancellation (Part 2.6).

Next: [2.2 — Mutexes & lock wrappers](02-mutexes-and-locks.md)
