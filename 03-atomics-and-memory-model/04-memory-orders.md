# 3.4 — Memory Orders

Every `load`, `store`, and RMW on `std::atomic` takes a **`memory_order`**. The
order controls **which** other memory operations may be reordered across the atomic
and **what** synchronizes-with edges you get. Default is `memory_order_seq_cst` —
safest, often slowest. Expert concurrency means picking the **weakest order that
still proves correct**.

---

## 3.4.1 The six orders (and one deprecated)

![Memory order strength ladder](figures/memory-orders.svg)

```
   STRONGEST ─────────────────────────────────────────▶ WEAKEST
   seq_cst    acq_rel (RMW only)    acquire / release    relaxed
      │              │                    │                  │
   total global   RMW both dirs      one-way sync      atomicity only
   order          read+write edge    publish OR         no cross-thread
                                       consume            ordering
```

> **The API ▸** — `<atomic>`
> ```cpp
> enum class memory_order {
>     relaxed, consume, acquire, release, acq_rel, seq_cst
> };
> ```

| Order | Typical use |
|-------|-------------|
| `seq_cst` | Default; reasoning is easiest |
| `acquire` | Loads that **receive** published data |
| `release` | Stores that **publish** data |
| `acq_rel` | RMW that both publishes and observes (CAS, `fetch_add` with ordering) |
| `relaxed` | Counters, stats — no ordering of other memory |
| `consume` | **Discouraged** — rarely implemented; use `acquire` |

---

## 3.4.2 memory_order_seq_cst

Single **total order** on all `seq_cst` operations — all threads agree on one global
timeline:

```
   thread A: seq_cst store X=1; seq_cst store Y=1
   thread B: seq_cst load Y; seq_cst load X
   // impossible: see Y==1 and X==0 (with no other writers)
```

> **Trade-offs ▸** Easiest mental model — "just works" for small programs. On weak
> ISAs, `seq_cst` may emit fences on every operation → measurable cost on hot paths.

Use `seq_cst` until profiling and proof justify weakening.

---

## 3.4.3 acquire and release — one-directional sync

**Release** (store/RMW success side): prior **ordinary** writes in this thread cannot
be reordered **after** the release.

**Acquire** (load/RMW success side): subsequent **ordinary** reads in this thread
cannot be reordered **before** the acquire.

```
   producer:  data = x;  flag.store(1, release);
   consumer:  if (flag.load(acquire)) use(data);
```

Pair forms **synchronizes-with** (Part 3.3). No global total order required between
unrelated atomics — cheaper than `seq_cst`.

> **Rule ▸** Release on the publisher, acquire on the consumer, for every handoff
> of non-atomic data.

---

## 3.4.4 acq_rel — for read-modify-write

CAS and `fetch_add` both read and write:

```cpp
head.compare_exchange_weak(old, new,
    std::memory_order_acq_rel,   // success
    std::memory_order_acquire);  // failure (still need to see current head)
```

Success: release prior writes + acquire subsequent reads. Failure load side often
uses `acquire` or `relaxed` depending on loop logic (Treiber stack, Part 3.2).

---

## 3.4.5 relaxed — atomicity only

```cpp
stats.fetch_add(1, std::memory_order_relaxed);
```

Guarantees no torn RMW on `stats`. **Does not** order other memory. Safe for
monotonic counters where no other data is published through the counter.

> **Pitfall ▸** Using `relaxed` on a flag that guards visibility of non-atomic
> payload — classic bug. Relaxed is for "this word alone matters."

---

## 3.4.6 consume — why it's discouraged

`memory_order_consume` was meant to allow **dependency-ordered** reads (pointer
chains) without full acquire cost. Implementations largely treat it as `acquire`;
the standard marked it deprecated. **Use `acquire`** on pointer loads that start
a dependent read chain.

---

## 3.4.7 Decision guide

```
   Q: Does another thread read non-atomic data you wrote?
      YES → release (you) + acquire (them) on a flag/pointer
      NO  → continue

   Q: Is it a pure statistic / counter with no linked data?
      YES → relaxed on the counter
      NO  → continue

   Q: Do you need global ordering across several atomics?
      YES → seq_cst (or careful proof with acq/rel)
      NO  → acquire/release on handoff atomics only

   Q: Unsure?
      → seq_cst or mutex
```

**Cost ladder** (typical weak ISA):

```
   relaxed  <  acquire/release  <  seq_cst  ≈  mutex (for single ops; mutex worse under contention)
```

On x86, many orders collapse in hardware (Part 3.5) — still respect the C++ rules;
ARM/POWER do not forgive sloppy orders.

---

## 3.4.8 Example: three counters, three orders

```cpp
// g++ -std=c++17 -pthread memory_orders.cpp -o memory_orders
#include <atomic>
#include <thread>
#include <iostream>

std::atomic<int> relaxed_cnt{0};
std::atomic<int> seq_cnt{0};
std::atomic<bool> done{false};

void worker() {
    for (int i = 0; i < 100'000; ++i) {
        relaxed_cnt.fetch_add(1, std::memory_order_relaxed);
        seq_cnt.fetch_add(1, std::memory_order_seq_cst);
    }
    done.store(true, std::memory_order_release);
}

int main() {
    std::thread a(worker), b(worker);
    a.join(); b.join();
    while (!done.load(std::memory_order_acquire)) { }
    std::cout << "relaxed=" << relaxed_cnt.load(std::memory_order_relaxed)
              << " seq_cst=" << seq_cnt.load(std::memory_order_relaxed)
              << " (both expected 200000)\n";
    return 0;
}
```

Both counters are exact — only atomicity needed. `done` uses release/acquire so
main doesn't print early (pedantic here since `join` already synchronizes).

---

## Summary

- Six orders; default `seq_cst` is strongest and simplest.
- `release`/`acquire` pair for publishing non-atomic data safely.
- `acq_rel` for RMW; `relaxed` for standalone counters/stats.
- Avoid `consume` — use `acquire`.
- Weaken orders only with a happens-before proof; when unsure, stay strong.
- Hardware cost varies by ISA — Part 3.5 covers fences and TSO vs weak models.

Next: [3.5 — Fences & hardware reordering](05-fences-and-reordering.md)
