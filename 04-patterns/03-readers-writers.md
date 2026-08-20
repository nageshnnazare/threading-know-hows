# 4.3 — Readers-Writers

Many threads need to read a shared data structure; a few need to write it.
A plain mutex lets only **one** thread in at a time — readers block each other
even though reads do not conflict. The **readers-writers** problem asks: can
many readers proceed **simultaneously** while writers still get **exclusive**
access?

```
   plain mutex:     R ── R ── R ── W ── R     (one at a time, always)
   reader-writer:   R   R   R   W   R   R     (reads overlap; write alone)
                    R R R       R R R
```

Part 1.4 covers `pthread_rwlock`; Part 2.2 covers `std::shared_mutex`. This
chapter explains the policy variants, when the extra mechanism pays off, and
when a simpler primitive wins.

---

## 4.3.1 The problem and its variants

Given shared data D:

```
   reader:  lock(shared) → read D → unlock
   writer:  lock(exclusive) → modify D → unlock
```

Three policy families matter in practice:

```
   READER-PREFERENCE          WRITER-PREFERENCE         FAIR / FIFO
   ─────────────────          ─────────────────         ───────────
   readers pile in            writers jump ahead        alternating queue
   writers may starve         readers may starve        no starvation*
   good for read-heavy        good for write-heavy      higher overhead
   config caches              journal / metrics         databases (sometimes)

   * true FIFO fairness is expensive; most rwlocks approximate
```

> **Under the hood ▸** `pthread_rwlock` and `std::shared_mutex` implement
> **unspecified** preference — typically reader-biased on Linux glibc, but do
> not rely on starvation behavior without reading your platform's docs. Part
> 5.3 covers starvation mechanics.

---

## 4.3.2 Implementation with std::shared_mutex

C++17's `std::shared_mutex` maps directly:

```
   std::shared_lock<std::shared_mutex>  g(sm);   // shared / reader
   std::unique_lock<std::shared_mutex>  g(sm);   // exclusive / writer
```

State machine (conceptual):

```
                    ┌─────────────────┐
         readers ──▶│  shared mode    │◀── many shared_lock holders
                    │  (readers ≥ 1)  │
                    └────────┬────────┘
                             │ writer waits for readers to drain
                             ▼
                    ┌─────────────────┐
         writer  ──▶│ exclusive mode  │◀── one unique_lock holder
                    │  (writers = 1)  │
                    └─────────────────┘
```

> **The API ▸** `#include <shared_mutex>`
> `std::shared_mutex sm;`
> `std::shared_lock lk(sm);` — multiple concurrent holders OK.
> `std::unique_lock lk(sm);` — blocks until no shared holders; blocks new
> shared locks.

Readers-writer locks **do not** upgrade from shared to exclusive in-place
(portable code acquires exclusive from the start if a read may become a write).

---

## 4.3.3 When shared_mutex beats a plain mutex

**Use `std::shared_mutex` when ALL of these hold:**

```
   ✓ read/write ratio is heavily skewed toward reads (10:1 or more)
   ✓ the critical section is non-trivial (not a single atomic load)
   ✓ the protected data is a struct, container, or computed snapshot
   ✓ you have enough concurrent readers to overlap (≥2 threads reading)
```

Example: an in-memory configuration map loaded rarely, queried thousands of
times per second by worker threads.

**Trade-offs ▸** `shared_mutex` carries higher constant overhead than
`std::mutex` — extra bookkeeping for reader counts, more syscalls on some
paths. On a **2-core** machine with **one** reader and **one** occasional
writer, a plain mutex often wins in benchmarks.

---

## 4.3.4 When it does NOT beat a plain mutex (or atomics)

```
   ✗ single-word counter           → std::atomic<int> (Part 3.1)
   ✗ read-mostly but tiny value    → atomic load/store, no lock
   ✗ write-heavy workload          → shared_mutex adds overhead, no win
   ✗ low core count, low contention → mutex is simpler and faster
   ✗ need writer priority          → platform rwlock may not provide it
```

Decision tree:

```
   fits in one atomic word, trivial read?
       YES → atomic
       NO  → read-heavy + non-trivial critical section?
                 YES → shared_mutex
                 NO  → mutex
```

> **Pitfall ▸** Protecting a `int` with `shared_mutex` because "we read more
> than we write" is slower than `std::atomic<int>` — the lock serializes
> cache lines unnecessarily (Part 5.4 false sharing is a separate concern).

---

## 4.3.5 Example: read-heavy cache

```cpp
// g++ -std=c++17 -pthread 03_readers_writers.cpp -o 03_readers_writers
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>

struct Config {
    std::shared_mutex mu;
    std::string       host = "localhost";
    int               port = 8080;

    std::string endpoint() const {
        std::shared_lock g(mu);                    // many readers OK
        return host + ":" + std::to_string(port);
    }

    void rebind(std::string h, int p) {
        std::unique_lock g(mu);                    // exclusive
        host = std::move(h);
        port = p;
    }
};

int main() {
    Config cfg;
    std::vector<std::thread> readers;
    for (int i = 0; i < 8; ++i)
        readers.emplace_back([&] {
            for (int k = 0; k < 1000; ++k)
                (void)cfg.endpoint();
        });

    std::thread writer([&] {
        for (int k = 0; k < 10; ++k)
            cfg.rebind("host" + std::to_string(k), 9000 + k);
    });

    for (auto& t : readers) t.join();
    writer.join();
    std::cout << "final " << cfg.endpoint() << "\n";
}
```

Eight readers hammer `endpoint()` in parallel; the writer occasionally
exclusively updates — readers see a consistent `{host, port}` pair, never a
torn string mid-assignment.

---

## 4.3.6 Alternatives worth knowing

```
   atomic<shared_ptr<const T>> snapshot:
      writer builds new T, atomic_store the pointer
      readers atomic_load and use immutable copy — zero read-side locking
      (RCU-like; reclamation is the hard part)

   copy-on-write under mutex:
      readers get shared_ptr to const data; writer clones, modifies, swaps
      similar trade-offs, simpler lifetime
```

These beat `shared_mutex` when reads dominate **and** the structure is
large enough that even shared locking contends on cache lines.

---

## Summary

- Readers-writers allows **concurrent reads** and **exclusive writes**; policy
  variants differ in **reader vs writer preference** and **starvation** risk.
- **`std::shared_mutex`** + `shared_lock` / `unique_lock` is the C++17
  implementation; pthread has `pthread_rwlock_t` (Part 1.4).
- Wins on **read-heavy, non-trivial** critical sections with real concurrent
  readers; **loses** to atomics for word-sized data and to plain mutexes under
  low contention.
- Measure before adopting — overhead is real on small core counts.

Next: [4.4 — Dining philosophers](04-dining-philosophers.md)
