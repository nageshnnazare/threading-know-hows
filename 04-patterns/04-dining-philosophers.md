# 4.4 — Dining Philosophers

Five philosophers sit at a round table. Between each pair is one fork. To eat,
a philosopher needs **both** adjacent forks. The puzzle, posed by Dijkstra, is
a compact model of **resource allocation with circular dependency** — the same
shape that produces **deadlock** in real systems (lock ordering on two tables,
two mutexes, two files).

![Five philosophers around a table, each needing two adjacent forks](figures/dining-philosophers.svg)

```
          (P0)
        F4    F0
      (P4)     (P1)
        F3    F1
          (P3)----F2----(P2)

   philosopher i needs fork[i] AND fork[(i+1) % 5]
```

Part 5.2 generalizes the four deadlock conditions; here we watch them form in
five lines of naive code and break the cycle with three standard fixes.

---

## 4.4.1 The naive approach and why it deadlocks

Every philosopher grabs **left**, then **right**:

```cpp
// BROKEN — do not use
void philosopher(int i) {
    std::lock_guard g1(forks[i]);
    std::lock_guard g2(forks[(i + 1) % N]);   // circular wait
    eat();
}
```

Timeline when all five reach for their left fork simultaneously:

```
   P0 holds F0, wants F1
   P1 holds F1, wants F2
   P2 holds F2, wants F3
   P3 holds F3, wants F4
   P4 holds F4, wants F0   ← cycle: nobody can get a second fork
```

All four Coffman conditions are satisfied (Part 5.2):

```
   mutual exclusion   ✓  one thread per fork
   hold and wait      ✓  holding left, waiting for right
   no preemption      ✓  locks aren't forcibly taken
   circular wait      ✓  P0→F1→P1→F2→…→P4→F0→P0
```

> **Rule ▸** Any time threads acquire **multiple locks** in an order that can
> form a cycle, deadlock is possible. The fix is to break at least one
> condition — usually circular wait.

---

## 4.4.2 Solution 1: resource ordering (number the forks)

Assign each fork a global index `0..N-1`. Each philosopher locks the **lower-
numbered fork first**, then the higher:

```
   philosopher i:
       left  = i
       right = (i + 1) % N
       first  = min(left, right)
       second = max(left, right)
       lock(first); lock(second); eat(); unlock both
```

For philosopher 4, `left=F4, right=F0` → lock **F0 first, then F4**. No thread
can hold a high fork while waiting for a lower one — the wait-for graph is a
DAG, not a cycle.

```
   before (cycle):     P0─▶F1─▶P1─▶F2─▶ … ─▶P4─▶F0─▶P0

   after (ordering):   all edges point low fork → high fork  (acyclic)
```

**Trade-offs ▸** Simple, zero extra threads, works for any N. Slightly
asymmetric — one philosopher (who inverts natural left/right) may wait longer
(Part 5.3 starvation is possible but not deadlock).

---

## 4.4.3 Solution 2: arbitrator / limiter (at most N−1 eat)

A counting **semaphore** (Part 1.5, Part 2.6) permits at most **N−1**
philosophers to "sit" (attempt to pick up forks). With only four at the table,
at least one philosopher holds **both** forks or is not competing — someone
can always finish and release.

```
   sem = N - 1   (initial count)

   philosopher:
       sem.acquire()          // may block here
       lock left; lock right
       eat()
       unlock both
       sem.release()
```

**Trade-offs ▸** Guarantees progress without per-fork ordering rules. Adds a
central throttle — fine for five philosophers, less elegant when resources
aren't symmetric.

---

## 4.4.4 Solution 3: try-lock and backoff

Grab one fork with `try_lock`; if the second fails, **release the first** and
retry (often after `yield` or random backoff):

```
   loop:
       lock(left)
       if try_lock(right):
           eat(); unlock both; break
       unlock(left)
       yield / random sleep
```

Breaks **hold-and-wait** — you never wait on the second fork while holding
the first indefinitely. Can livelock under bad luck (Part 5.3); random backoff
mitigates.

> **Pitfall ▸** Fixed-order retry without backoff can spin forever under
> contention — that's livelock, not progress.

---

## 4.4.5 Modern C++: std::scoped_lock

C++17 `std::scoped_lock(a, b, ...)` calls `std::lock` internally — it tries
all mutexes in a deadlock-free order without you numbering resources:

```cpp
std::scoped_lock g(forks[i], forks[(i + 1) % N]);   // OK
```

This is the pragmatic choice when you have a **small fixed set** of locks per
operation. Resource ordering remains the teaching model because it scales to
**arbitrary lock sets** (databases, file hierarchies) where the standard
library cannot pick an order for you.

---

## 4.4.6 Example: ordering fix

```cpp
// g++ -std=c++17 -pthread 04_dining_philosophers.cpp -o 04_dining_philosophers
#include <algorithm>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

constexpr int N = 5;

int main() {
    std::mutex forks[N];
    std::mutex io;

    auto philosopher = [&](int id) {
        for (int meal = 0; meal < 3; ++meal) {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));

            int left  = id;
            int right = (id + 1) % N;
            int first  = std::min(left, right);
            int second = std::max(left, right);

            std::lock_guard g1(forks[first]);
            std::lock_guard g2(forks[second]);

            {
                std::lock_guard g(io);
                std::cout << "P" << id << " eating meal " << meal << "\n";
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    };

    std::vector<std::thread> ps;
    for (int i = 0; i < N; ++i) ps.emplace_back(philosopher, i);
    for (auto& t : ps) t.join();
    std::cout << "all done — no deadlock\n";
}
```

Philosopher 4 locks F0 before F4; the circular wait is broken.

---

## Summary

- Dining philosophers models **multiple resources per task** and **circular
  wait** — the classic deadlock setup (Part 5.2).
- Naive "left then right" deadlocks when all five grab their first fork at
  once.
- Fixes: **global resource ordering** (low-index first), **semaphore limit
  N−1**, **try-lock + backoff**, or **`std::scoped_lock`** for fixed pairs.
- In production, prefer consistent lock ordering or `scoped_lock` whenever a
  thread holds more than one mutex.

Next: [4.5 — Pipeline](05-pipeline.md)
