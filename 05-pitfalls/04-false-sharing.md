# 5.4 — False Sharing

**False sharing** is a cache-coherence pathology: threads mutate **logically
independent** variables that happen to live on the **same cache line**, causing
the line to **ping-pong** between cores. No data race exists — each thread writes
its own field — yet parallel performance can be **worse than serial**. Part 5.1's
confinement fix assumes truly private cache lines; this chapter shows when
"private" variables are not private at the hardware level.

---

## 5.4.1 Cache lines and coherence

CPUs do not track individual bytes for coherence. They track **cache lines**
(typically **64 bytes** on x86-64 and AArch64):

```
   core 0 cache          core 1 cache
   ┌────────────────┐    ┌────────────────┐
   │ line @ 0x1000  │    │ line @ 0x1000  │   same physical line
   │  bytes 0..63   │    │  bytes 0..63   │
   └────────────────┘    └────────────────┘
            │                      │
            └────── bus / snoop ────┘
                   (MESI protocol)
```

**MESI** (Modified / Exclusive / Shared / Invalid) states govern each line:

```
   core 0 writes byte 0  →  line becomes Modified on core 0
                          →  Invalid on all other cores (RFO: read-for-ownership)
   core 1 writes byte 40 →  steals line to core 1
                          →  core 0's copy invalidated again
```

Every write to **any byte** in the line is a coherence transaction for the
**whole line**.

![Independent variables on one cache line ping-pong between cores](figures/false-sharing.svg)

> **Under the hood ▸** A **read-modify-write** on a line another core owns costs
> ~100–300 cycles (cross-socket higher). Millions of increments on false-shared
> counters turn a embarrassingly parallel loop into a coherence traffic storm.

---

## 5.4.2 The symptom: parallel slower than serial

Layout that looks thread-safe:

```cpp
struct counters {
    std::atomic<long> c0;   // thread 0 only
    std::atomic<long> c1;   // thread 1 only
    std::atomic<long> c2;   // thread 2 only
    std::atomic<long> c3;   // thread 3 only
};   // all four atomics likely fit in ONE 64-byte line
```

Each thread increments **its own** `c[i]` — no logical sharing. But:

```
   memory layout (one cache line):
   ┌──────┬──────┬──────┬──────┬ ... padding ... ┐
   │  c0  │  c1  │  c2  │  c3  │                 │
   └──────┴──────┴──────┴──────┴─────────────────┘
     ↑ T0 writes here invalidates the ENTIRE line on T1,T2,T3
```

Benchmark signature:

```
   1 thread:  1.0 s
   4 threads: 3.5 s   ← scaling inverted — classic false sharing
```

This is **not** a race (atomics synchronize the RMW). It is a **performance bug**
only — but in HPC it is fatal.

---

## 5.4.3 Detection

| Tool | What it shows |
|------|---------------|
| **`perf c2c`** (Linux) | Cache-line contention — HITM (hit modified) counts per line |
| **`perf stat -e`** | `cache-misses`, `L1-dcache-load-misses`, `mem_load_uops_retired.hit_lfb` |
| **Intel VTune** | "False sharing" analysis, HITM hotspots |
| **Scaling curve** | Throughput flat or negative as threads increase |

```bash
# record HITM events (may need: echo 1 | sudo tee /proc/sys/kernel/perf_event_paranoid)
perf c2c record -a -- ./bench
perf c2c report
# look for lines with high HITM count shared by multiple threads
```

TSan will **not** flag false sharing — there is no data race.

---

## 5.4.4 Fixes: alignment and padding

Put each hot per-thread variable on its **own cache line**:

> **The API ▸**
> ```cpp
> #include <new>   // C++17
> alignas(64) std::atomic<long> counter;   // manual line size
> // portable when available:
> alignas(std::hardware_destructive_interference_size) std::atomic<long> counter;
> ```
> `std::hardware_destructive_interference_size` (C++17, `<new>`) is the minimum
> offset to avoid false sharing — typically 64. `hardware_constructive_interference_size`
> is the max recommended group size for true sharing.

```cpp
#ifdef __cpp_lib_hardware_interference_size
constexpr size_t CL = std::hardware_destructive_interference_size;
#else
constexpr size_t CL = 64;
#endif

struct alignas(CL) PaddedCounter {
    std::atomic<long> value{0};
    char pad[CL - sizeof(std::atomic<long>)];  // pad to full line
};
PaddedCounter slots[N];   // each slot on its own line(s)
```

Alternative: **`thread_local`** accumulators + one merge at the end (Part 6.3
reduction pattern) — zero cross-core writes during the hot loop.

**Trade-offs ▸** Padding increases memory footprint (64 B per counter vs 8 B).
For millions of counters, use **striped aggregation** (N buffers, hash thread id
to buffer) instead of N full lines.

---

## 5.4.5 Before/after benchmark

```cpp
// g++ -std=c++17 -pthread -O2 false_sharing.cpp -o false_sharing
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

constexpr int N = 4;
constexpr int ITERS = 50'000'000;

#ifdef __cpp_lib_hardware_interference_size
constexpr size_t CL = std::hardware_destructive_interference_size;
#else
constexpr size_t CL = 64;
#endif

struct Bad {
    std::atomic<long> c[N];   // packed — false sharing
};

struct alignas(CL) Padded {
    std::atomic<long> counter{0};
    char pad[CL - sizeof(std::atomic<long>)];
};

template<class F>
double bench(const char* name, F f) {
    auto t0 = std::chrono::steady_clock::now();
    std::vector<std::thread> ts;
    for (int i = 0; i < N; ++i) ts.emplace_back(f, i);
    for (auto& t : ts) t.join();
    double sec = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t0).count();
    std::cout << name << ": " << sec << " s\n";
    return sec;
}

int main() {
    Bad bad{};
    double t_bad = bench("false-shared (bad)", [&](int id){
        for (int k = 0; k < ITERS; ++k)
            bad.c[id].fetch_add(1, std::memory_order_relaxed);
    });

    Padded good[N]{};
    double t_good = bench("padded (good)  ", [&](int id){
        for (int k = 0; k < ITERS; ++k)
            good[id].counter.fetch_add(1, std::memory_order_relaxed);
    });

    std::cout << "speedup: " << (t_bad / t_good) << "x\n";
}
```

Typical output on a quad-core machine: **5–20×** speedup after padding. The
"bad" version is often slower with 4 threads than with 1.

> **Rule ▸** Any **write-hot** per-thread field in a parallel loop must be
> line-aligned or accumulated locally. Read-mostly sharing of one line is fine
> (constructive interference).

---

## Summary

- CPUs coherence-track **64-byte cache lines**; one write invalidates the whole
  line on other cores (MESI).
- **False sharing** = independent variables on the same line → coherence
  ping-pong → parallel can lose to serial.
- Detect with **`perf c2c`**, VTune, or negative scaling curves — not TSan.
- Fix with **`alignas(64)`** / `hardware_destructive_interference_size`, padding,
  or per-thread local accumulators + merge (OpenMP `reduction` — Part 6.3).

Next: [5.5 — The ABA problem](05-aba-problem.md)
