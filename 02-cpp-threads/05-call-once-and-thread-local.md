# 2.5 — call_once & thread_local

Not every shared-state problem is a counter or a queue. Two recurring needs are:
**(1)** run expensive initialization exactly once, safely, from many threads;
**(2)** give each thread its **own** copy of data without locking. C++11 provides
`std::call_once` and `thread_local` for exactly that.

---

## 2.5.1 std::call_once — one-time init

> **The API ▸** — `<mutex>`
> ```cpp
> struct once_flag {};
> template<class Callable, class... Args>
> void call_once(once_flag& flag, Callable&& f, Args&&... args);
> ```
> Calls `f(args...)` **exactly once**, no matter how many threads invoke
> `call_once` concurrently. Subsequent calls are no-ops. Synchronizes with the
> first completion — threads returning from `call_once` see fully initialized
> side effects.

```
   thread A ── call_once(flag, init) ──┐
   thread B ── call_once(flag, init) ──┼──▶ exactly ONE runs init()
   thread C ── call_once(flag, init) ──┘    others block until done
```

Typical singleton:

```cpp
std::once_flag init_flag;
std::unique_ptr<Database> db;

void ensure_db() {
    std::call_once(init_flag, []{
        db = std::make_unique<Database>("connection string");
    });
}
```

All threads calling `ensure_db()` get a fully constructed `db` with no data race
on the init path.

> **Under the hood ▸** Implementations use a fast path (atomic check) plus a
> slow path (mutex or futex) for the first caller. You get correct publication
> without hand-rolled double-checked locking.

---

## 2.5.2 Why not hand-roll double-checked locking?

The broken pattern (Part 4.7):

```cpp
// ✗ DO NOT — data race + reordering bugs
if (!ptr) {
    lock(m);
    if (!ptr) ptr = new Thing;
    unlock(m);
}
```

Even with two checks, unsynchronized reads of `ptr` race with the writer, and
reordering can publish a pointer before the object is visible. `call_once` and
magic statics fix this at the language/library level.

> **Rule ▸** Use `std::call_once` or function-local statics for lazy one-time
> init. Do not invent your own double-checked locking unless you are implementing
> low-level atomics with explicit `memory_order` (Part 3.3–3.4).

**Trade-offs ▸** `call_once` has small overhead on every call (atomic check).
For init that truly runs once at startup, a static initializer or `main()` before
threads spawn is simpler.

---

## 2.5.3 Magic statics (function-local statics)

Since C++11, initialization of a **function-local static** is thread-safe:

```cpp
Database& get_db() {
    static Database instance("connection string");  // thread-safe once
    return instance;
}
```

The compiler emits guard variables (often `call_once` under the hood). First caller
initializes; concurrent callers block until complete. Destruction happens at
program exit (reverse order of construction) — watch static deinitialization order
if background threads outlive `main`.

> **Pitfall ▸** Thread-safe **initialization** does not make the object itself
> thread-safe. If `Database` methods mutate shared state, you still need mutexes
> or immutability after init.

---

## 2.5.4 thread_local — per-thread storage

> **The API ▸** — keyword `thread_local` (storage class)
> ```cpp
> thread_local int tls_counter = 0;
> ```
> Each thread gets a **distinct** instance with static storage duration (lifetime
> = entire thread). No sharing → no data race on that variable.

Recall Part 0.1: threads share heap/globals but have private stacks. `thread_local`
places data in a thread-specific area the runtime manages (like POSIX `pthread_key_t`,
Part 1.6):

```
   thread A                    thread B
   tls_counter = 3             tls_counter = 7
   (same name, different       (no mutex needed —
    memory addresses)           no interaction)
```

Common uses:
- Per-thread caches, RNGs, arena allocators
- Errno-style "last error" without locking
- Thread IDs or request context in server worker threads

```cpp
thread_local std::string log_prefix;

void worker(int id) {
    log_prefix = "worker-" + std::to_string(id);
    // all logging in this thread sees its prefix
}
```

---

## 2.5.5 Per-thread singleton pattern

Combine `thread_local` with lazy init for "one X per thread":

```cpp
struct PerThreadBuffer {
    std::vector<char> buf;
    PerThreadBuffer() { buf.reserve(4096); }
};

PerThreadBuffer& thread_buffer() {
    thread_local PerThreadBuffer instance;
    return instance;
}
```

Each thread's first call constructs its buffer; subsequent calls are free. This
eliminates false sharing on hot counters (Part 5.4) when you aggregate at the end.

> **Pitfall ▸** `thread_local` destructors run at **thread exit**. If thread A's
> destructor touches state already torn down by thread B or by static destruction
> order, you get subtle crashes. Keep TLS destructors simple.

---

## 2.5.6 Example: call_once + thread_local

```cpp
// g++ -std=c++17 -pthread once_tls_demo.cpp -o once_tls_demo
#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <atomic>

std::once_flag config_once;
int shared_config = 0;

void init_config() {
    std::call_once(config_once, []{
        shared_config = 42;   // expensive read simulated
        std::cout << "config initialized once\n";
    });
}

thread_local int hits = 0;

void worker() {
    init_config();
    ++hits;
    std::cout << "thread " << std::this_thread::get_id()
              << " hits=" << hits << " config=" << shared_config << "\n";
}

int main() {
    std::vector<std::thread> ts;
    for (int i = 0; i < 4; ++i)
        ts.emplace_back(worker);
    for (auto& t : ts) t.join();
    return 0;
}
```

`shared_config` is written once under `call_once` — safe. `hits` is per-thread —
no synchronization needed.

---

## Summary

- `std::call_once` + `once_flag` runs initialization exactly once, with proper
  synchronization — prefer over hand-rolled double-checked locking.
- Function-local `static` objects have thread-safe one-time init since C++11.
- `thread_local` gives each thread its own instance — no races on that variable.
- Magic statics init safely; they don't make the object thread-safe for mutation.
- TLS destructors run at thread exit — mind lifetime with shutdown.

Next: [2.6 — C++20 concurrency](06-cpp20-concurrency.md)
