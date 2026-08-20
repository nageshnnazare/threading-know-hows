# 4.7 — Double-Checked Locking

**Lazy initialization**: create an expensive singleton on first use, not at
startup. **Double-checked locking (DCL)** tries to optimize the common path —
read a pointer without locking; lock only if the pointer is null — by checking
twice. The pattern was **famously broken** for decades before C++11 atomics and
the memory model made a correct form possible. Understanding *why* it broke is
a compact lesson in **reordering** (Part 3.3–3.5).

```
   fast path (every call after init):
      if (ptr != null) return ptr;     ← no lock

   slow path (first call):
      lock
      if (ptr == null) ptr = new T();  ← construct + store pointer
      unlock
      return ptr
```

Part 2.5 covers `std::call_once` and magic statics — the fixes you should use
in modern C++.

---

## 4.7.1 The singleton init problem

```cpp
Singleton* get() {
    static Singleton* p = nullptr;   // process-wide, lazy
    if (!p) {
        std::lock_guard g(mu);
        if (!p)
            p = new Singleton();     // expensive
    }
    return p;
}
```

Without the outer `if (!p)`, every call takes the mutex — correct but slow
after initialization. DCL adds the outer check to skip the lock on the hot path.

The bug: the outer check reads `p` **without synchronization**. Another thread
may observe a **non-null but not fully constructed** object.

---

## 4.7.2 Why classic DCL was broken (pre-C++11)

Consider thread A in the slow path and thread B on the fast path:

```
   Thread A (under lock, constructing):
      1. allocate memory for Singleton
      2. construct Singleton in that memory
      3. store pointer p = address

   Compiler/CPU may REORDER to:
      1. allocate
      3. store p = address        ← B can see non-null HERE
      2. construct                ← B reads half-built object
```

Thread B:

```
   if (p != nullptr)   // true — pointer published early
       return p;        // uses Singleton before ctor finished → UB / crash
```

> **Under the hood ▸** Without a **happens-before** edge between construction
> and the pointer publish, the C++ memory model permits the store of `p` to
> become visible before the writes that initialize `Singleton`'s members
> (Part 3.5 StoreLoad and release/acquire). This is not hypothetical — broken
> DCL shipped in production code for years.

> **Pitfall ▸** `volatile` does **not** fix DCL. `volatile` is not a
> synchronization primitive in C++; it does not prevent compiler reordering
> relative to other threads.

---

## 4.7.3 Broken vs fixed (side by side)

**BROKEN** — raw pointer, no atomics:

```cpp
// ✗ UB under concurrent first call
Singleton* p = nullptr;
std::mutex mu;

Singleton* get_broken() {
    if (!p) {                              // unsynchronized read
        std::lock_guard g(mu);
        if (!p)
            p = new Singleton();           // publish may precede construction
    }
    return p;
}
```

**FIXED (modern)** — three correct options below.

---

## 4.7.4 Fix 1: function-local static ("magic statics") — preferred

```cpp
Singleton& get() {
    static Singleton instance;   // C++11: thread-safe one-time init
    return instance;
}
```

The compiler/runtime ensures exactly one initialization, with correct
synchronization, on first entry. No manual locking, no leak on `new`.

> **The API ▸** C++11 §[stmt.dcl]/4: block-scope static initialization is
> thread-safe. Destruction runs at process exit in reverse order of
> construction. Part 2.5.

**Trade-offs ▸** Destruction order vs other statics can bite in complex apps
(destruction order fiasco). For most services, this is the default answer.

---

## 4.7.5 Fix 2: std::call_once

```cpp
std::once_flag once;
Singleton* p = nullptr;

Singleton* get() {
    std::call_once(once, [] { p = new Singleton(); });
    return p;
}
```

`call_once` guarantees the callable runs exactly once, with full synchronization
visible to subsequent `call_once` callers. Part 2.5.

**Trade-offs ▸** Slight indirection; you manage `new`/`delete` lifetime unless
you never destroy the singleton.

---

## 4.7.6 Fix 3: atomic pointer with acquire/release (correct DCL)

When you truly need pointer semantics and the fast unsynchronized read:

```cpp
std::atomic<Singleton*> p{nullptr};
std::mutex mu;

Singleton* get() {
    Singleton* s = p.load(std::memory_order_acquire);   // (1)
    if (!s) {
        std::lock_guard g(mu);
        s = p.load(std::memory_order_relaxed);
        if (!s) {
            s = new Singleton();
            p.store(s, std::memory_order_release);    // (2)
        }
    }
    return s;
}
```

```
   (2) release store of p  ──synchronizes-with──▶  (1) acquire load of p

   if acquire sees non-null → guaranteed to see fully constructed Singleton
```

> **Rule ▸** The **release** on publish must pair with an **acquire** on every
> unsynchronized read of the pointer. A relaxed load on the fast path is **not**
> enough. Part 3.4.

---

## 4.7.7 Compilable: broken pattern vs three fixes

```cpp
// g++ -std=c++17 -pthread 07_double_checked_locking.cpp -o 07_dcl
#include <atomic>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

struct Singleton {
    int value = 42;
    Singleton() { /* imagine non-trivial init */ }
};

// (1) Meyers singleton — use this
Singleton& get_meyers() {
    static Singleton s;
    return s;
}

// (2) call_once
std::once_flag g_once;
Singleton* g_ptr = nullptr;
Singleton* get_once() {
    std::call_once(g_once, [] { g_ptr = new Singleton(); });
    return g_ptr;
}

// (3) atomic DCL
std::atomic<Singleton*> g_atomic{nullptr};
std::mutex g_mu;
Singleton* get_atomic() {
    Singleton* s = g_atomic.load(std::memory_order_acquire);
    if (!s) {
        std::lock_guard g(g_mu);
        s = g_atomic.load(std::memory_order_relaxed);
        if (!s) {
            s = new Singleton();
            g_atomic.store(s, std::memory_order_release);
        }
    }
    return s;
}

int main() {
    std::vector<std::thread> ts;
    for (int i = 0; i < 8; ++i)
        ts.emplace_back([] {
            (void)get_meyers().value;
            (void)get_once()->value;
            (void)get_atomic()->value;
        });
    for (auto& t : ts) t.join();
    std::cout << "all inits OK\n";
}
```

Do **not** ship the broken raw-pointer version — ThreadSanitizer may miss the
subtle reordering on some builds; the standard still allows UB.

---

## Summary

- **DCL** optimizes lazy init with an unsynchronized first check; classic
  implementations were **broken** because the pointer store could become
  visible **before** construction finished.
- **`volatile` does not fix it** — you need happens-before via atomics or
  library guarantees.
- **Correct modern forms**: function-local **`static`** (preferred),
  **`std::call_once`**, or **`atomic<T*>`** with **acquire/release** pairing.
- Prefer Meyers singleton unless you have a specific reason not to.

Next: [Part 5.1 — Race conditions](../05-pitfalls/01-race-conditions.md)
