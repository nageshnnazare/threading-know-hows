# 4.1 — Thread Pool

Spawning a `std::thread` per task is simple — and wrong for most production
workloads. Part 0.1 showed that thread creation costs tens of microseconds and
allocates a large stack. A **thread pool** keeps a fixed set of **worker
threads** alive, feeding them work from a **shared task queue**. Submitting a
task becomes a cheap enqueue + notify; you amortize creation cost and **bound
concurrency** to a known number of cores.

![Thread pool: submitters enqueue tasks; workers dequeue and run](figures/thread-pool.svg)

```
   submitters (any thread)                worker threads (fixed N)
         │                                       │
         │  enqueue std::function<void()>      │
         ▼                                       ▼
   ┌─────────────────────────────────────────────────────┐
   │  task queue  ◀── mutex + condition_variable ──▶     │
   └─────────────────────────────────────────────────────┘
         │         pop when non-empty / not stopping
         └──────▶ worker runs job() outside the lock
```

This is the default architecture behind HTTP servers, game engines, and most
`std::async`-style runtimes. Part 4.6 extends it with per-worker queues and
work-stealing; Part 4.2 supplies the bounded queue variant when you need
back-pressure on submit.

---

## 4.1.1 Why pool instead of per-task threads

```
   per-task thread model:
      task arrives → pthread_create / std::thread → run → join
      1000 tasks/sec → 1000 creates + 1000 joins → scheduler thrash

   pool model:
      startup: create N workers once
      each task: lock → push to queue → notify_one → return
      workers: wait → pop → unlock → execute → repeat
```

**Trade-offs ▸** A pool wins when tasks are **short**, **numerous**, and
**similar in cost**. It loses when tasks are long-running and heterogeneous —
you may need dynamic sizing, priority queues, or cancellation (C++20
`stop_token`, Part 2.6). It also does not help CPU-bound work that exceeds
`hardware_concurrency()` unless you accept oversubscription and context-switch
overhead.

> **Under the hood ▸** Each worker is an ordinary kernel task (Part 0.3). The
> pool does not pin threads to cores unless you call `pthread_setaffinity_np`
> or platform equivalents. On a 4-core machine with an 8-thread pool, the OS
> time-slices eight runnable workers — you get parallelism up to four cores and
> contention for the queue mutex on top.

---

## 4.1.2 The core design

Four pieces, always the same shape:

```
   1. std::vector<std::thread> workers_
   2. std::queue<std::function<void()>> tasks_
   3. std::mutex mu_  +  std::condition_variable cv_
   4. bool stop_  (or std::atomic<bool>)
```

Worker loop (the heart of every pool):

```
   loop forever:
       unique_lock mu_
       cv_.wait(lock, predicate: stop_ || !tasks_.empty())
       if stop_ && tasks_.empty(): return          // graceful exit
       job = move(tasks_.front()); pop
       unlock mu_
       job()                                       // NEVER run under the lock
```

> **Rule ▸** Never execute a task while holding the queue mutex. A slow or
> blocking task under the lock serializes every other worker and can deadlock
> if the task itself tries to submit work to the same pool.

> **The API ▸** Submit side: wrap the callable in a
> `std::packaged_task<R()>` (Part 2.4), push a lambda that invokes it onto the
> queue, return `pkg->get_future()`. Callers block on `future::get()` without
> holding any pool lock.

---

## 4.1.3 Submitting tasks and returning futures

`std::function<void()>` erases type but cannot propagate return values. The
standard pattern pairs it with `std::packaged_task`:

```
   submit(f, args...):
       pkg = make_shared<packaged_task<R()>>(
           bind f with args into nullary callable)
       fut = pkg->get_future()
       lock mu_
       tasks_.push([pkg]{ (*pkg)(); })
       unlock; cv_.notify_one()
       return fut
```

`notify_one()` wakes **one** sleeping worker — enough when each task is
independent. Use `notify_all()` only when every worker must re-check state
(e.g. shutdown).

> **Pitfall ▸** Capturing a `packaged_task` by reference in the queue lambda
> is a use-after-free once `submit()` returns. Capture a `shared_ptr` to the
> task, as in the example below.

---

## 4.1.4 Graceful shutdown

Destruction order matters:

```
   1. lock mu_;  stop_ = true;  unlock
   2. cv_.notify_all()          // wake every worker blocked on empty queue
   3. join every worker thread
```

Workers that wake with `stop_ == true` and an empty queue **exit**. Workers
mid-task finish the current job before checking again — pending work drains
unless you explicitly choose to drop it on shutdown.

> **Pitfall ▸** Setting `stop_` without `notify_all()` leaves workers sleeping
> forever → `join()` blocks indefinitely (Part 5.6 lost-wakeup family).

**Trade-offs ▸** Draining pending tasks on shutdown is polite but can delay
process exit. Production pools often expose `shutdown(now=true)` that clears
the queue and rejects new submits.

---

## 4.1.5 Sizing the pool

```
   CPU-bound tasks:     N ≈ std::thread::hardware_concurrency()
   I/O-bound tasks:     N can exceed core count (threads block in kernel)
   mixed workload:      start at hardware_concurrency(), measure, tune
```

`std::thread::hardware_concurrency()` returns logical processors (cores × SMT
if enabled). Zero means "unknown" — fall back to 2 or 4.

> **Under the hood ▸** Oversubscribing CPU-bound work creates runnable threads
> that fight for cores. Each extra thread adds cache pressure and context
> switches without increasing useful throughput (Part 0.2, Amdahl's law in the
> glossary). Undersubscribing leaves cores idle when the queue is deep.

---

## 4.1.6 A minimal, compilable thread pool

```cpp
// g++ -std=c++17 -pthread 01_thread_pool.cpp -o 01_thread_pool
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(size_t n = std::thread::hardware_concurrency()) {
        if (n == 0) n = 2;
        for (size_t i = 0; i < n; ++i)
            workers_.emplace_back([this] { worker_loop(); });
    }

    ~ThreadPool() {
        { std::lock_guard g(mu_); stop_ = true; }
        cv_.notify_all();
        for (auto& w : workers_) w.join();
    }

    template <class F, class... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using R = std::invoke_result_t<F, Args...>;
        auto pkg = std::make_shared<std::packaged_task<R()>>(
            [fn = std::forward<F>(f),
             tup = std::make_tuple(std::forward<Args>(args)...)]() mutable {
                return std::apply(fn, std::move(tup));
            });
        std::future<R> fut = pkg->get_future();
        {
            std::lock_guard g(mu_);
            if (stop_) throw std::runtime_error("pool shut down");
            tasks_.emplace([pkg] { (*pkg)(); });
        }
        cv_.notify_one();
        return fut;
    }

private:
    void worker_loop() {
        for (;;) {
            std::function<void()> job;
            {
                std::unique_lock g(mu_);
                cv_.wait(g, [&] { return stop_ || !tasks_.empty(); });
                if (stop_ && tasks_.empty()) return;
                job = std::move(tasks_.front());
                tasks_.pop();
            }
            job();
        }
    }

    std::vector<std::thread>          workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex                        mu_;
    std::condition_variable           cv_;
    bool                              stop_ = false;
};

int main() {
    ThreadPool pool(4);
    std::vector<std::future<int>> futs;
    for (int i = 0; i < 12; ++i)
        futs.push_back(pool.submit([i] { return i * i; }));
    for (int i = 0; i < 12; ++i)
        std::cout << "f(" << i << ") = " << futs[i].get() << "\n";
}
```

Extensions worth knowing (not shown): bounded queue + blocking `submit` for
back-pressure (Part 4.2), work-stealing deques (Part 4.6), and
`std::jthread` workers with `stop_token` (Part 2.6).

---

## Summary

- A thread pool **amortizes thread creation** and **caps concurrency** with N
  long-lived workers pulling from a shared queue.
- The queue is guarded by **mutex + condition_variable**; workers wait on a
  predicate (`stop_ || !empty`), pop under the lock, **run outside** the lock.
- **Submit** via `std::packaged_task` + `std::function<void()>` to return a
  `std::future` to the caller.
- **Shutdown**: set `stop_`, `notify_all()`, `join()` — wake every sleeper.
- Size near **`hardware_concurrency()`** for CPU-bound work; measure for I/O.

Next: [4.2 — Producer-consumer](02-producer-consumer.md)
