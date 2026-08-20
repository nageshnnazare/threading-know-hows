# 4.2 — Producer-Consumer

Two roles, one buffer: **producers** generate items; **consumers** process
them. They run at different rates, so you decouple them with a **thread-safe
queue**. When the queue has a **capacity limit**, producers block when full and
consumers block when empty — automatic **back-pressure** that prevents unbounded
memory growth.

```
   producer(s)                         consumer(s)
       │                                    ▲
       │ push when not full                 │ pop when not empty
       ▼                                    │
   ┌──────────────────────────────────────────────┐
   │  bounded queue  [ · · · | cap ]              │
   │  mutex + not_full_cv + not_empty_cv          │
   └──────────────────────────────────────────────┘
```

This pattern appears everywhere: log pipelines, job dispatch (Part 4.1's queue
is the consumer side of "submit"), GPU command buffers, and any "generate then
process" pipeline (Part 4.5). Part 1.3 and Part 2.3 introduced condition
variables; here we build the reusable component they were preparing you for.

---

## 4.2.1 Why bounded, not unbounded

```
   unbounded queue:
      fast producer + slow consumer → queue grows without limit → OOM

   bounded queue (capacity = C):
      queue full  → producer blocks on not_full_cv
      queue empty → consumer blocks on not_empty_cv
      in-flight items ≤ C at all times
```

**Trade-offs ▸** Bounded queues add latency under burst load (producers wait)
but give predictable memory and apply pressure upstream — a slow downstream
stage cannot be overrun. An unbounded queue is simpler but hides overload until
the process dies.

> **Rule ▸** Every `wait()` on a condition variable must use the **predicate
> form** (Part 1.3, Part 5.6). Both `not_full` and `not_empty` predicates
> must also account for **shutdown** so threads can exit cleanly.

---

## 4.2.2 Two condition variables: not-full and not-empty

One mutex, two condvars — the canonical design:

```
   push(v):
       lock
       wait(not_full_cv,  q.size() < cap || closed)
       if closed: return false
       q.push(v)
       notify_one(not_empty_cv)     // wake a waiting consumer
       unlock

   pop():
       lock
       wait(not_empty_cv, !q.empty() || closed)
       if q.empty(): return nullopt  // closed and drained
       v = q.pop()
       notify_one(not_full_cv)      // wake a waiting producer
       unlock; return v
```

Why two condvars instead of one?

```
   single cv on bounded queue:
      producer wakes consumer ✓
      consumer wakes producer ✓
      but broadcast wakes EVERYONE → thundering herd on large pools

   two cv's:
      push  → notify not_empty only  (consumers)
      pop   → notify not_full only   (producers)
      less spurious contention
```

> **The API ▸** `std::condition_variable::wait(lock, predicate)` is
> equivalent to `while (!pred) wait(lock)` — use it always. Headers:
> `<condition_variable>`, `<mutex>`, `<queue>`.

---

## 4.2.3 Back-pressure in the pipeline

Back-pressure propagates stall signals backward through a chain:

```
   stage A ──q1(cap=4)──▶ stage B ──q2(cap=4)──▶ stage C

   C slows down → q2 fills → B blocks on push to q2
                → q1 fills → A blocks on push to q1
```

No central coordinator needed — each bounded link enforces its own limit.
Part 4.5 uses this property for staged parallelism.

> **Under the hood ▸** A blocked producer is descheduled in the kernel (Part
> 0.3). It consumes no CPU while waiting; the scheduler runs other threads.
> This is why blocking queues beat busy-wait spin loops for I/O-heavy or
> mismatched rates.

---

## 4.2.4 Multiple producers and multiple consumers

The two-condvar design is **MPMC-safe** as long as:

```
   ✓ all queue mutations happen under one mutex
   ✓ wait predicates re-check state after wakeup
   ✓ notify after releasing is OK (std::notify is safe if lock was held)
   ✗ never pop outside the lock or read q_.empty() without it
```

Any number of producer threads may call `push`; any number of consumers may
call `pop`. The mutex serializes queue structure changes; parallelism comes
from consumers executing **outside** the lock after they pop an item.

**Trade-offs ▸** A single mutex becomes hot under extreme contention. Lock-free
MPMC rings (Disruptor, bounded SPSC queues per pair) exist for microsecond
latency — but mutex + two condvars is correct, readable, and fast enough for
most C++ services.

---

## 4.2.5 Shutdown: poison pill and close

Two common shutdown styles:

```
   (A) poison pill:  producer pushes sentinel value (e.g. -1)
                     each consumer exits on seeing sentinel

   (B) close flag:   queue.close() sets closed_=true, notify_all both cv's
                     pop() returns empty optional once drained
```

Style (B) scales better with multiple producers — no need to inject N
sentinels. The last producer calls `close()` after finishing; consumers drain
remaining items then observe `empty && closed` and exit.

> **Pitfall ▸** Forgetting to `notify_all` on `close()` leaves consumers
> blocked on an empty closed queue forever (Part 5.6).

---

## 4.2.6 A compilable bounded queue

```cpp
// g++ -std=c++17 -pthread 02_producer_consumer.cpp -o 02_producer_consumer
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <vector>

template <class T>
class BoundedQueue {
    std::mutex              mu_;
    std::condition_variable not_full_, not_empty_;
    std::queue<T>           q_;
    size_t                  cap_;
    bool                    closed_ = false;

public:
    explicit BoundedQueue(size_t cap) : cap_(cap) {}

    bool push(T v) {
        std::unique_lock g(mu_);
        not_full_.wait(g, [&] { return closed_ || q_.size() < cap_; });
        if (closed_) return false;
        q_.push(std::move(v));
        not_empty_.notify_one();
        return true;
    }

    std::optional<T> pop() {
        std::unique_lock g(mu_);
        not_empty_.wait(g, [&] { return closed_ || !q_.empty(); });
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
    constexpr int NP = 2, NC = 3, ITEMS = 8;
    BoundedQueue<int> q(4);
    std::atomic<int> producers_done{0};

    std::vector<std::thread> producers, consumers;
    for (int p = 0; p < NP; ++p)
        producers.emplace_back([&, p] {
            for (int i = 0; i < ITEMS; ++i) {
                q.push(p * 100 + i);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (producers_done.fetch_add(1) + 1 == NP) q.close();
        });

    for (int c = 0; c < NC; ++c)
        consumers.emplace_back([&, c] {
            while (auto v = q.pop())
                std::cout << "C" << c << " got " << *v << "\n";
        });

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();
}
```

Run with three consumers and two producers — watch consumers interleave while
the queue never holds more than four items.

---

## Summary

- Producer-consumer decouples **generation** from **processing** via a shared
  queue; **bounded** capacity applies **back-pressure** when consumers lag.
- The standard implementation uses **one mutex + two condition variables**
  (`not_full`, `not_empty`) with predicate waits on both sides.
- **Multiple producers and consumers** are safe when all queue access is
  serialized and work runs outside the lock.
- **Shutdown** via a `close()` flag + `notify_all`, or poison-pill sentinels;
  always wake every waiter.

Next: [4.3 — Readers-writers](03-readers-writers.md)
