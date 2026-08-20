# 4.6 — Work-Stealing

A central task queue (Part 4.1) works — until one worker's queue is empty while
another's is deep. Idle threads spin or sleep; hot threads pile work locally.
**Work-stealing** gives each worker its own **double-ended queue (deque)**:
the owner **pushes and pops** from one end (LIFO, cache-hot); **thieves** take
from the **opposite** end (FIFO, older tasks). Load balances with minimal
contention on the common queue mutex.

![Per-worker deques; thieves steal from the opposite end](figures/work-stealing.svg)

```
   Worker 0 deque:  [t0 | t1 | t2 | t3]  ← owner pops/pushes here (back, LIFO)
                     ↑
                     thieves steal from front (FIFO)

   Worker 2 idle → steals t0 from Worker 0's front → now executes t0
```

Used by Intel TBB, Cilk, Go's scheduler, Rust's rayon, and JVM
ForkJoinPool — the standard scheduler for **recursive divide-and-conquer**
and **fork-join** workloads.

---

## 4.6.1 Why not one shared queue?

```
   shared queue pool (Part 4.1):
      every submit/pop → same mutex → hot under many workers
      uneven task trees → some workers starve while queue is non-empty elsewhere

   work-stealing:
      owner rarely contends (local deque)
      steal is rare (only when local deque empty)
      recursive tasks push children locally → parent cache stays warm
```

**Trade-offs ▸** Work-stealing shines on **dynamic, nested** parallelism (quicksort
partitions, tree walks, ray tracing tiles). It adds complexity — per-worker
deques, steal protocols, and (in production) **lock-free Chase-Lev deques**. For
flat, uniform tasks, a simple pool is often enough.

---

## 4.6.2 Owner end: LIFO (cache-hot)

When a task forks into subtasks, the parent pushes them onto **its** deque and
continues popping the **most recent** child (back of deque):

```
   task A runs on worker W
   A spawns B, C, D → W.push_back(B,C,D)
   W pops D (LIFO) → executes deepest subtask first

   → depth-first traversal of the task tree
   → subtasks reuse W's cache lines and stack locality
```

> **Under the hood ▸** LIFO on the owner end mimics sequential recursive
> execution — the worker stays on the "hot" subtree it just created. Thieves
> pulling from the **opposite** end take **older, coarser** tasks that are
> less cache-sensitive anyway.

---

## 4.6.3 Thief end: FIFO (reduce contention)

```
   owner:   pop_back()   — only this worker touches the back (usually)
   thief:   steal_front() — different end → less lock overlap with owner

   if both ends were LIFO for everyone:
      thieves fight owner on same end → high contention
```

Steal attempts happen **only when** the local deque is empty — the common case
is zero cross-worker traffic.

> **Rule ▸** Steal from **random** victims (or round-robin) to avoid all idle
> workers hammering the same deque.

---

## 4.6.4 The deque operations (sketch)

Production deques use **lock-free** Chase-Lev arrays with atomic top/bottom
indices. A mutex-protected teaching version:

```
   struct Deque {
       deque<Task> dq;
       mutex       m;
   };

   // Owner (worker id = who):
   Task pop_local(who):
       lock dq[who].m
       if dq empty: return {}
       t = move(dq.back()); pop_back(); return t

   // Thief:
   Task steal_from(victim):
       lock dq[victim].m
       if dq empty: return {}
       t = move(dq.front()); pop_front(); return t   // opposite end

   worker_loop(id):
       loop:
           t = pop_local(id)
           if !t:
               for each victim != id (random order):
                   t = steal_from(victim)
                   if t: break
           if t: run t
           else if no work globally: yield or wait
```

Real Chase-Lev allows the owner to push/pop without locking when no thief is
active; thieves CAS the top index. Part 3.2 covers the atomic machinery.

---

## 4.6.5 Fork-join shape

```
   parallel_for(lo, hi):
       if small enough:
           compute serially
       else:
           mid = (lo + hi) / 2
           spawn parallel_for(lo, mid)    → push to local deque
           spawn parallel_for(mid, hi)    → push to local deque
           wait for children (pop local until done)
```

The runtime schedules spawned tasks on the same worker's deque; siblings steal
when idle. This is why work-stealing pairs naturally with **recursive**
algorithms (Part 6.4 OpenMP tasks use a related model).

---

## 4.6.6 Simplified work-stealing pool (sketch)

```cpp
// g++ -std=c++17 -pthread 06_work_stealing.cpp -o 06_work_stealing
#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

class WorkStealingPool {
    using Task = std::function<void()>;

    struct Worker {
        std::deque<Task> dq;
        std::mutex       mu;
    };

    std::vector<Worker>          workers_;
    std::vector<std::thread>     threads_;
    std::atomic<bool>            stop_{false};
    std::atomic<int>             pending_{0};

    Task pop_local(int id) {
        std::lock_guard g(workers_[id].mu);
        if (workers_[id].dq.empty()) return {};
        Task t = std::move(workers_[id].dq.back());
        workers_[id].dq.pop_back();
        return t;
    }

    Task steal(int thief, int victim) {
        if (thief == victim) return {};
        std::lock_guard g(workers_[victim].mu);
        if (workers_[victim].dq.empty()) return {};
        Task t = std::move(workers_[victim].dq.front());
        workers_[victim].dq.pop_front();
        return t;
    }

    void loop(int id) {
        std::mt19937 rng(id + 1);
        while (!stop_.load()) {
            Task t = pop_local(id);
            if (!t) {
                for (size_t k = 0; k < workers_.size(); ++k) {
                    int v = int(rng() % workers_.size());
                    t = steal(id, v);
                    if (t) break;
                }
            }
            if (t) { t(); pending_.fetch_sub(1); }
            else std::this_thread::yield();
        }
    }

public:
    explicit WorkStealingPool(size_t n) : workers_(n) {
        for (size_t i = 0; i < n; ++i)
            threads_.emplace_back([this, i] { loop(int(i)); });
    }

    void submit(Task t) {
        static std::atomic<size_t> rr{0};
        size_t w = rr.fetch_add(1) % workers_.size();
        {
            std::lock_guard g(workers_[w].mu);
            workers_[w].dq.push_back(std::move(t));
        }
        pending_.fetch_add(1);
    }

    void wait() {
        while (pending_.load() > 0) std::this_thread::yield();
        stop_.store(true);
        for (auto& th : threads_) th.join();
    }
};

int main() {
    WorkStealingPool pool(4);
    std::atomic<long> sum{0};
    for (int i = 1; i <= 100; ++i)
        pool.submit([&, i] { sum.fetch_add(i, std::memory_order_relaxed); });
    pool.wait();
    std::cout << "sum=" << sum << " expected=" << (100 * 101 / 2) << "\n";
}
```

This is mutex-backed for clarity — TBB/rayon use lock-free deques for the
owner hot path.

---

## Summary

- **Work-stealing** uses **per-worker deques**: owner **LIFO** at one end,
  thieves **FIFO** from the other — balances load with low contention.
- Ideal for **recursive fork-join** and uneven task trees; flat workloads may
  not need the complexity.
- Production runtimes (TBB, Cilk, Go, rayon) use **Chase-Lev lock-free deques**
  (Part 3.2 atomics).
- Steal only when local deque is empty; pick victims randomly.

Next: [4.7 — Double-checked locking](07-double-checked-locking.md)
