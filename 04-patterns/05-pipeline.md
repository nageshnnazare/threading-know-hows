# 4.5 — Pipeline

Some workloads decompose into **stages**: parse → transform → serialize →
write. Each stage is independent work on **one item at a time**, but different
items can be in different stages simultaneously. A **pipeline** connects stages
with **bounded queues** (Part 4.2); each stage runs in its own thread, pulling
from an input queue and pushing to an output queue.

![Pipeline stages connected by bounded queues](figures/pipeline.svg)

```
   [source] ──q1──▶ [stage 1] ──q2──▶ [stage 2] ──q3──▶ [sink]
      │                 │                 │                 │
   produce           transform           transform          consume
```

Throughput is limited by the **slowest stage** (the bottleneck), not the sum
of stage speeds. Part 0.2's parallelism lesson applies item-by-item: you get
speedup only where stages overlap in wall-clock time.

---

## 4.5.1 Staged parallelism vs data parallelism

```
   DATA PARALLEL (Part 6.2):   same operation on every element in parallel
                               #pragma omp parallel for

   PIPELINE:                   different operations on different elements
                               at the same time, connected by queues
```

Pipeline wins when:

```
   ✓ stages have different costs (I/O vs CPU vs format conversion)
   ✓ items are independent (order may or may not matter)
   ✓ you can bound in-flight memory with queue capacity
```

It loses when setup overhead dominates (tiny batches) or when one stage is
so much slower that adding pipeline depth cannot help — you must **balance**
the slow stage (Part 0.2, Amdahl).

---

## 4.5.2 Throughput = slowest stage

```
   stage A: 100 items/sec
   stage B:  40 items/sec   ← bottleneck
   stage C: 200 items/sec

   steady-state throughput ≈ 40 items/sec (limited by B)
```

Adding threads to A or C does nothing once B is saturated. Fix the bottleneck
(faster algorithm, more B workers, split B into sub-stages) or accept the cap.

```
   wall-clock for N items (steady state, k stages, bottleneck rate R):

   time ≈ N / R  +  pipeline fill/drain overhead
```

> **Under the hood ▸** Queues decouple burstiness: if A is fast and B is slow,
> q1 fills until `push` blocks — back-pressure stalls A without unbounded
> buffering. The pipeline self-regulates to B's rate.

---

## 4.5.3 Balancing stages

Strategies when stages are uneven:

```
   1. replicate slow stage:  two threads both pop q1, push q2 (careful: ordering)
   2. widen slow stage:      internal parallelism per item
   3. merge fast stages:     fewer queue hops → less mutex overhead
   4. tune queue capacity:     larger cap smooths bursts; smaller cap saves memory
```

**Trade-offs ▸** Multiple workers on one stage preserve throughput but may
**reorder** items unless you tag sequence numbers and reorder at the sink.

---

## 4.5.4 Ordering

```
   ordering required:   single consumer per stage, or sequence tags + reorder buffer
   ordering irrelevant:  multiple consumers on a stage (MPMC queue, Part 4.2)
```

If item 7 must exit after item 6, either keep one thread per ordered stage or
attach `uint64_t seq` and buffer at the sink until contiguous sequences arrive.

> **Pitfall ▸** Unbounded parallelism in stage 2 with a ordered sink can
> cause unbounded reorder-buffer memory if early items finish late.

---

## 4.5.5 Back-pressure end-to-end

Each inter-stage link is a Part 4.2 bounded queue:

```
   source.push ──▶ q1(cap) ──▶ stage1 ──▶ q2(cap) ──▶ stage2 ──▶ …

   any queue full → upstream stage blocks on push
   any queue empty → downstream stage blocks on pop
```

Shutdown propagates forward: source **closes** q1 after last item; stage 1
drains q1, **closes** q2, exits; sink drains the final queue and exits.

---

## 4.5.6 Example: three-stage pipeline

```cpp
// g++ -std=c++17 -pthread 05_pipeline.cpp -o 05_pipeline
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>

template <class T>
class BQ {
    std::queue<T> q_;
    size_t cap_;
    bool closed_ = false;
    std::mutex mu_;
    std::condition_variable not_full_, not_empty_;

public:
    explicit BQ(size_t cap) : cap_(cap) {}

    void push(T v) {
        std::unique_lock g(mu_);
        not_full_.wait(g, [&] { return q_.size() < cap_ || closed_; });
        if (closed_) return;
        q_.push(std::move(v));
        not_empty_.notify_one();
    }

    std::optional<T> pop() {
        std::unique_lock g(mu_);
        not_empty_.wait(g, [&] { return !q_.empty() || closed_; });
        if (q_.empty()) return std::nullopt;
        T v = std::move(q_.front());
        q_.pop();
        not_full_.notify_one();
        return v;
    }

    void close() {
        { std::lock_guard g(mu_); closed_ = true; }
        not_empty_.notify_all();
        not_full_.notify_all();
    }
};

int main() {
    BQ<int> q1(4), q2(4);
    BQ<std::string> q3(4);

    std::thread source([&] {
        for (int i = 1; i <= 10; ++i) {
            q1.push(i);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        q1.close();
    });

    std::thread square([&] {
        while (auto v = q1.pop()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            q2.push((*v) * (*v));
        }
        q2.close();
    });

    std::thread stringify([&] {
        while (auto v = q2.pop())
            q3.push("val=" + std::to_string(*v));
        q3.close();
    });

    std::thread sink([&] {
        while (auto s = q3.pop())
            std::cout << "  " << *s << "\n";
    });

    source.join();
    square.join();
    stringify.join();
    sink.join();
}
```

Stage 2 (`square`) is deliberately slower — watch q1 back-pressure stall the
source when four items pile up.

---

## Summary

- A **pipeline** runs each processing stage in its own thread, linked by
  **bounded queues** that provide **back-pressure**.
- **Throughput** equals the **slowest stage** in steady state; optimize the
  bottleneck first.
- **Balance** stages by replication, merging, or internal parallelism; mind
  **ordering** if the sink requires sequential output.
- Shutdown: **close** queues upstream-to-downstream after drain.

Next: [4.6 — Work-stealing](06-work-stealing.md)
