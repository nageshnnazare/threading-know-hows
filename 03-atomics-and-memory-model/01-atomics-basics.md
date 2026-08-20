# 3.1 — Atomics Basics

Mutexes serialize **whole critical sections**. Sometimes you only need one
indivisible operation on a single word — increment a counter, publish a flag,
swap a pointer. **`std::atomic<T>`** promises that concurrent read-modify-write
(RMW) operations on one location behave as if they executed in some serial order,
with **no torn reads or writes**. That is the foundation of lock-free code and
the memory model (Part 3.3–3.4).

---

## 3.1.1 What "atomic" guarantees

Without atomics, `count++` on a shared `int` is three steps (load, add, store) —
another thread can interleave anywhere (Part 0.4):

```
   thread A: LOAD count (=5)
   thread B: LOAD count (=5)   ADD 1   STORE 6
   thread A:            ADD 1   STORE 6   ← lost update (expected 7)
```

An atomic RMW is **one** indivisible operation from every other thread's view:

```
   atomic fetch_add:  read old, write old+1 — no interleaving inside
```

> **Rule ▸** Atomicity ≠ full synchronization of arbitrary data structures. It
> applies to **one atomic object at a time**. Publishing a struct still needs
> release/acquire or a mutex (Part 3.3).

---

## 3.1.2 std::atomic<T>

> **The API ▸** — `<atomic>`
> ```cpp
> template<class T> struct atomic;  // T must be trivially copyable, ≤ lock-free size
> atomic() noexcept = default;
> explicit atomic(T desired);
> T load(memory_order order = memory_order_seq_cst) const noexcept;
> void store(T desired, memory_order order = memory_order_seq_cst) noexcept;
> T exchange(T desired, memory_order order = memory_order_seq_cst) noexcept;
> bool compare_exchange_weak/strong(T& expected, T desired, ...);
> T fetch_add(T arg, memory_order order = memory_order_seq_cst) noexcept;  // integrals
> T fetch_sub(...); fetch_and/or/xor; ++/-- operators on integrals
> ```
> Atomics are not copyable or movable. Default initialization leaves **indeterminate**
> value until `store` or constructor with value — unlike plain `int`.

Specializations exist for pointers (`fetch_add` as byte offset) and `bool`.

---

## 3.1.3 Compare-and-swap — the lock-free primitive

![Compare-exchange retry loop](figures/atomic-cas.svg)

```
   CAS(addr, expected, desired):
      if *addr == expected:
          *addr = desired;  return true
      else:
          expected = *addr; return false   (strong/weak differ on spurious fail)
```

> **The API ▸**
> ```cpp
> bool compare_exchange_weak(T& expected, T desired,
>                            memory_order success, memory_order failure);
> bool compare_exchange_strong(T& expected, T desired, ...);
> ```
> **Strong**: fails only if value ≠ expected. **Weak**: may fail spuriously even
> when equal — use in retry loops (Part 3.2). On success, reads `expected`, writes
> `desired` atomically.

CAS is how lock-free stacks, queues, and counters are built.

---

## 3.1.4 atomic_flag — the minimal atomic

> **The API ▸**
> ```cpp
> struct atomic_flag {
>     bool test_and_set(memory_order order = memory_order_seq_cst) noexcept;
>     void clear(memory_order order = memory_order_seq_cst) noexcept;
> };
> ```
> The **only** type guaranteed **always lock-free** on every platform. No load/store
> — only test-and-set (set to true, return previous) and clear (to false). Classic
> spin-lock building block:

```cpp
std::atomic_flag lock = ATOMIC_FLAG_INIT;

void spin_lock() {
    while (lock.test_and_set(std::memory_order_acquire)) { /* spin */ }
}
void spin_unlock() {
    lock.clear(std::memory_order_release);
}
```

Prefer `std::mutex` unless profiling shows spin locks win (very short critical
sections, low contention).

---

## 3.1.5 Lock-free vs locked atomics

> **The API ▸**
> ```cpp
> bool is_lock_free() const noexcept;
> bool is_always_lock_free<T>();  // C++17 compile-time trait
> ```
> If `is_lock_free()` is false, the implementation may guard the atomic with an
> internal mutex — still correct, but not wait-free and may surprise you on large
> `atomic<MyStruct>`.

Check hot-path types at compile time:

```cpp
static_assert(std::atomic<int>::is_always_lock_free);
```

> **Under the hood ▸** Lock-free atomics map to CPU instructions: `LOCK XADD` on
> x86, `ldxr`/`stxr` pairs on ARM. Large or misaligned types fall back to locks.

---

## 3.1.6 Atomics vs mutex for tiny scalars

```
   mutex + int counter          atomic int counter
   ───────────────────          ──────────────────
   blocks on contention         RMW in userspace (often)
   protects multi-step work     single-word only
   easier to compose            needs memory orders for publish
```

For a simple global counter with no associated data, `fetch_add` with
`memory_order_relaxed` (Part 3.4) is the right tool. For updating a counter **and**
a related buffer, use a mutex — atomics don't lock both together.

**Trade-offs ▸** Atomics scale better for hot counters and flags but do not replace
design for invariants spanning multiple fields.

---

## 3.1.7 Example: atomic counter

```cpp
// g++ -std=c++17 -pthread atomic_counter.cpp -o atomic_counter
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>

std::atomic<long> counter{0};

void worker(int n) {
    for (int i = 0; i < n; ++i)
        counter.fetch_add(1, std::memory_order_relaxed);
}

int main() {
    constexpr int per = 100'000;
    std::vector<std::thread> ts;
    for (int i = 0; i < 8; ++i)
        ts.emplace_back(worker, per);
    for (auto& t : ts) t.join();
    std::cout << "counter = " << counter.load() << " (expected "
              << 8 * per << ")\n";
    return 0;
}
```

`relaxed` is sufficient here — we only need atomicity, not ordering of other
memory (Part 3.4). The final `load()` before print is fine for a demo; production
code joining threads already synchronizes.

> **Pitfall ▸** Using `operator++` on `atomic` with default `seq_cst` everywhere
> — correct but slower than needed for pure statistics. Match order to intent.

---

## Summary

- Atomics make single-location RMW operations indivisible — no torn reads/writes.
- `load`, `store`, `exchange`, `fetch_*`, and CAS are the core operations.
- `atomic_flag` is always lock-free; use for spin locks sparingly.
- `is_lock_free()` / `is_always_lock_free` tell you if hardware atomics back your type.
- Atomics suit single-word hot paths; mutexes still win for multi-field invariants.
- CAS retry loops underpin lock-free structures (Part 3.2).

Next: [3.2 — Lock-free programming & CAS](02-lock-free-and-cas.md)
