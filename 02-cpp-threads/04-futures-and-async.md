# 2.4 — Futures, Promises & async

Threads give you parallel **execution**. **Futures** give you parallel **results**:
a typed channel to retrieve a value (or exception) computed elsewhere, without
manual mutex/condition-variable wiring. The trio — `std::async`, `std::promise`,
`std::packaged_task` — covers most "run this, give me that" patterns.

---

## 2.4.1 The future/promise model

![Future and async execution paths](figures/future-async.svg)

```
   async world                         your thread
   ───────────                         ───────────
   std::promise ──shared state──▶ std::future
        │                              │
   set_value(v)                    get() blocks
   set_exception(e)                rethrows e
        │                              │
        └──── one-shot channel ────────┘
              (get() consumes it)
```

> **The API ▸** — `<future>`
> ```cpp
> template<class T> class future {
>     T get();                          // blocks; move-only result; ONE call
>     void wait();
>     bool valid() const;
>     // wait_for, wait_until...
> };
> template<class T> class promise {
>     void set_value(/* T or void */);
>     void set_exception(std::exception_ptr p);
>     future<T> get_future();
> };
> ```
> `promise` is the **sender**; `future` is the **receiver**. They share an internal
> reference-counted state. `get_future()` may be called once per promise.

> **Rule ▸** `future::get()` may be called **once**. It blocks until the result
> is ready, then **moves** the value out (or rethrows the stored exception).

---

## 2.4.2 std::async — fire and forget (almost)

> **The API ▸**
> ```cpp
> template<class F, class... Args>
> future<invoke_result_t<F, Args...>>
> async(launch policy, F&& f, Args&&... args);

> enum class launch { async, deferred };
> ```
> `async(launch::async, f, args...)` — runs `f` on a **new thread** (implementation
> may pool). Returns a `future` for the result.
> `async(launch::deferred, f, args...)` — **lazy**: `f` runs on the thread that
> first calls `wait()` or `get()` on the future.

The default policy is `launch::async | launch::deferred` — the implementation
**chooses**:

> **Pitfall ▸** Default `std::async(f)` may run **deferred** — no parallelism,
> surprise latency on `get()`, and the callable runs on the **calling** thread.
> For real background work, pass `launch::async` explicitly:
> ```cpp
> auto fut = std::async(std::launch::async, heavy_work, arg);
> ```

> **Pitfall ▸** Destroying a `std::future` returned from `std::async` with
> default policy **blocks** until the async work completes — like an implicit
> join. This can deadlock if the async task tries to touch objects being
> destroyed. Keep the `future` alive until you're done, or use `launch::deferred`
> knowingly.

---

## 2.4.3 Exceptions cross the boundary via futures

Unlike raw `std::thread` (Part 2.1), exceptions in an `async` task or
`packaged_task` are stored and **rethrown** from `future::get()`:

```cpp
auto fut = std::async(std::launch::async, []{
    throw std::runtime_error("boom");
});
try {
    fut.get();
} catch (const std::exception& e) {
    // "boom" — propagated
}
```

This is the idiomatic way to surface errors from background work.

---

## 2.4.4 std::packaged_task

> **The API ▸**
> ```cpp
> template<class Sig> class packaged_task;  // Sig = R(Args...)
> packaged_task(F&& f);
> future<R> get_future();
> void operator()(Args... args);   // run the task; result → future
> ```
> Wraps any callable in a `future`-backed task. Useful with thread pools: the pool
> runs `task()`, you keep the `future`:

```cpp
std::packaged_task<int()> task([]{ return 42; });
std::future<int> fut = task.get_future();
std::thread t(std::move(task));   // run task in thread
std::cout << fut.get() << "\n";
t.join();
```

---

## 2.4.5 std::shared_future — many getters

`future` is move-only; only one owner may `get()`. `shared_future` copies the
shared state — multiple threads can wait and read the same result:

> **The API ▸**
> ```cpp
> template<class T> class shared_future {
>     const T& get() const;   // reference to stored value — not moved out
> };
> shared_future<T> fut2 = fut.share();  // from future<T>
> ```

Use when several threads need the same async outcome (e.g. broadcast config load).

---

## 2.4.6 Example: parallel map with async

```cpp
// g++ -std=c++17 -pthread futures_demo.cpp -o futures_demo
#include <iostream>
#include <future>
#include <vector>
#include <numeric>

long long heavy(int i) {
    long long s = 0;
    for (int j = 0; j < 1'000'000; ++j) s += (i + j);
    return s;
}

int main() {
    std::vector<std::future<long long>> futs;
    for (int i = 0; i < 8; ++i)
        futs.push_back(std::async(std::launch::async, heavy, i));

    long long total = 0;
    for (auto& f : futs)
        total += f.get();   // blocks per future; could use wait_all pattern

    std::cout << "total = " << total << "\n";
    return 0;
}
```

For many small tasks, a thread pool (Part 4.1) amortizes thread creation cost;
`std::async` spawns (or reuses) per call — fine for coarse-grained work.

**Trade-offs ▸** Futures simplify result plumbing but hide scheduling policy.
Promises allow manual fulfillment (callbacks, thread pools) but require discipline:
don't forget to set value/exception, don't set twice, and keep the `promise` alive
until the `future` side consumes it.

---

## Summary

- `promise` sets value/exception; `future::get()` blocks once and retrieves or
  rethrows.
- `std::async(launch::async, ...)` for real parallelism; default policy may defer.
- Destroying an async `future` can block — keep it alive or choose policy explicitly.
- `packaged_task` decouples "work object" from "thread that runs it."
- `shared_future` allows multiple getters; `future` is single-consumer.
- Futures are how exceptions legally cross thread boundaries in C++.

Next: [2.5 — call_once & thread_local](05-call-once-and-thread-local.md)
