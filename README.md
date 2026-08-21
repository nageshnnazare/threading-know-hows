# The C/C++ Multithreading Mastery Guide

> A single, deep, diagram-driven reference for concurrent and parallel
> programming in C and C++: from what a thread *is* at the OS level, through the
> pthreads and C++ `std::thread` APIs, into atomics and the memory model, the
> classic concurrency patterns and pitfalls, and OpenMP — with the hardware and
> kernel mechanics that decide whether your parallel code is correct *and* fast.
>
> Written for **C and C++ engineers who want expert-level mechanical detail** —
> what the CPU, cache, and scheduler actually do — not a cookbook of API calls.
> Every construct is grounded in diagrams, compilable code, exact synchronization
> semantics, and the failure modes (races, deadlocks, reordering) that only make
> sense once you see the mechanism.

---

## Who this is for

You can already write C/C++. You may have spawned a `std::thread`, locked a
`std::mutex`, or seen a data race corrupt a counter. But you want to *truly*
understand:

- What a thread shares with its siblings (heap, globals, fds) and what it owns
  (stack, registers) — and why that makes a race possible.
- Why `count++` from two threads can lose an update, mechanically, step by step.
- The difference between **concurrency** (structure) and **parallelism**
  (simultaneous execution), and why Amdahl's law caps your speedup.
- Why a condition-variable `wait()` *must* sit in a `while` loop on a predicate.
- What `std::memory_order_acquire`/`release` actually order, and why
  `relaxed` is fast but dangerous.
- Why two correct-looking stores and loads can produce a result "impossible" on
  paper (StoreLoad reordering).
- How a lock-free stack works with `compare_exchange`, and how the ABA problem
  breaks it.
- Why **false sharing** can make a 16-core program slower than a 1-core one.
- How a thread pool, pipeline, and work-stealing scheduler are built.

If you finish this guide, you will be able to read `perf`/ThreadSanitizer
output, reason about a memory-ordering bug, and design concurrency that is both
correct and scalable.

---

## The 30,000-foot map

```
   ┌───────────────────────────────────────────────────────────────────┐
   │ APIs you write with                                               │
   │   pthreads (C)      std::thread / mutex / future (C++)   OpenMP   │
   └───────────────┬───────────────────┬───────────────────────┬───────┘
                   │                   │                       │
                   ▼                   ▼                       ▼
   ┌───────────────────────────────────────────────────────────────────┐
   │ SHARED-STATE problem: races, critical sections, atomicity         │
   │   solved by → mutexes · condition variables · atomics · fences    │
   └───────────────────────────────┬───────────────────────────────────┘
                                   ▼
   ┌───────────────────────────────────────────────────────────────────┐
   │ THE MEMORY MODEL: happens-before, memory_order, hardware reorder  │
   └───────────────────────────────┬───────────────────────────────────┘
                                   ▼
   ┌───────────────────────────────────────────────────────────────────┐
   │ OS + hardware: kernel schedules threads (tasks) onto cores;       │
   │ caches + store buffers reorder memory unless you say otherwise    │
   └───────────────────────────────────────────────────────────────────┘

   built into → patterns (pool, producer/consumer, pipeline)
   broken by  → pitfalls (deadlock, livelock, false sharing, ABA)
```

Each box is a part this guide dissects. Each arrow is a mechanism with a cost, a
correctness rule, and a way to get it wrong.

---

## How to read this guide

The parts are ordered as a **learning path** from "what is a thread" up to the
memory model and production patterns. If you already know the basics, jump to
Part 3 (atomics & memory model), Part 4 (patterns), or Part 5 (pitfalls) — the
mechanically demanding heart of the guide.

Every chapter has:

- **Concept** sections with hand-drawn diagrams.
- **The API ▸** call-outs: exact signature, header, and semantics.
- **Under the hood ▸** boxes: what the kernel / cache / hardware does.
- **Example ▸** blocks: compilable, correct C or C++ (build line included).
- **Trade-offs ▸** and **Pitfall ▸**: real bugs explained by the mechanics.
- **Rule ▸**: the invariant you must not violate.

---

## Table of contents

### Part 0 — Foundations (`00-foundations/`)
1. [What is a thread?](00-foundations/01-what-is-a-thread.md)
2. [Concurrency vs parallelism](00-foundations/02-concurrency-vs-parallelism.md)
3. [Threads and the OS](00-foundations/03-threads-and-the-os.md)
4. [The shared-state problem](00-foundations/04-the-shared-state-problem.md)

### Part 1 — pthreads: the POSIX API (`01-pthreads/`)
1. [Thread lifecycle](01-pthreads/01-thread-lifecycle.md)
2. [Mutexes](01-pthreads/02-mutexes.md)
3. [Condition variables](01-pthreads/03-condition-variables.md)
4. [Read-write locks & barriers](01-pthreads/04-rwlocks-and-barriers.md)
5. [Semaphores & spinlocks](01-pthreads/05-semaphores-and-spinlocks.md)
6. [Thread-local storage & cancellation](01-pthreads/06-tls-and-cancellation.md)

### Part 2 — Modern C++ threading (`02-cpp-threads/`)
1. [std::thread & jthread](02-cpp-threads/01-std-thread.md)
2. [Mutexes & lock wrappers](02-cpp-threads/02-mutexes-and-locks.md)
3. [Condition variables](02-cpp-threads/03-condition-variables.md)
4. [Futures, promises & async](02-cpp-threads/04-futures-and-async.md)
5. [call_once & thread_local](02-cpp-threads/05-call-once-and-thread-local.md)
6. [C++20 concurrency: jthread, latch, barrier, semaphore](02-cpp-threads/06-cpp20-concurrency.md)

### Part 3 — Atomics & the memory model (`03-atomics-and-memory-model/`)
1. [Atomics basics](03-atomics-and-memory-model/01-atomics-basics.md)
2. [Lock-free programming & CAS](03-atomics-and-memory-model/02-lock-free-and-cas.md)
3. [The C++ memory model](03-atomics-and-memory-model/03-the-memory-model.md)
4. [Memory orders](03-atomics-and-memory-model/04-memory-orders.md)
5. [Fences & hardware reordering](03-atomics-and-memory-model/05-fences-and-reordering.md)

### Part 4 — Concurrency patterns (`04-patterns/`)
1. [Thread pool](04-patterns/01-thread-pool.md)
2. [Producer-consumer](04-patterns/02-producer-consumer.md)
3. [Readers-writers](04-patterns/03-readers-writers.md)
4. [Dining philosophers](04-patterns/04-dining-philosophers.md)
5. [Pipeline](04-patterns/05-pipeline.md)
6. [Work-stealing](04-patterns/06-work-stealing.md)
7. [Double-checked locking](04-patterns/07-double-checked-locking.md)

### Part 5 — Pitfalls (`05-pitfalls/`)
1. [Race conditions](05-pitfalls/01-race-conditions.md)
2. [Deadlock](05-pitfalls/02-deadlock.md)
3. [Livelock & starvation](05-pitfalls/03-livelock-and-starvation.md)
4. [False sharing](05-pitfalls/04-false-sharing.md)
5. [The ABA problem](05-pitfalls/05-aba-problem.md)
6. [Lost wakeups](05-pitfalls/06-lost-wakeups.md)

### Part 6 — OpenMP (`06-openmp/`)
1. [The OpenMP model](06-openmp/01-openmp-model.md)
2. [Worksharing: for & sections](06-openmp/02-worksharing.md)
3. [Synchronization & reductions](06-openmp/03-synchronization-and-reductions.md)
4. [Tasks](06-openmp/04-tasks.md)

### Reference (`99-reference/`)
- [API cheat sheet (pthreads · C++ · OpenMP)](99-reference/api-cheatsheet.md)
- [Glossary](99-reference/glossary.md)

### Runnable examples (`examples/`)
70+ compilable programs organized by topic. Build everything with:

```bash
make            # delegates to examples/ (needs gcc/g++; OpenMP for section 6)
```

---

## Conventions used in this guide

| Notation / call-out | Meaning                                                     |
|---------------------|-------------------------------------------------------------|
| **The API ▸**       | Exact signature, header, and semantics                      |
| **Under the hood ▸**| What the kernel / cache / hardware does                     |
| **Example ▸**       | Compilable, correct C/C++ (build line shown)               |
| **Trade-offs ▸**    | Advantages vs disadvantages of a construct                  |
| **Pitfall ▸**       | A common mistake explained mechanically                     |
| **Rule ▸**          | An invariant you must not break                             |
| critical section    | Code that must run with exclusive access to shared state    |
| data race           | Two unsynchronized accesses, ≥1 a write → undefined behavior |
| happens-before      | The ordering that makes one thread's writes visible to another |

Build note: C examples use `-pthread`; C++ uses `-std=c++17 -pthread` (C++20 for
`jthread`/`latch`); OpenMP uses `-fopenmp`.

---

## The one rule that never changes (read this first)

Concurrency bugs almost all reduce to a single violation:

> **Rule ▸** Two threads must never access the same memory location
> concurrently when at least one access is a **write**, unless that access is
> **atomic** or protected by **synchronization** (a mutex, or a proper
> `memory_order` edge). Break this and the C/C++ standard says your whole
> program has **undefined behavior** — not "a wrong value," but *no guarantees at
> all*.

```c
// BROKEN: two threads run counter++ with no synchronization → data race → UB
counter++;

// FIXED, option A — a lock:
pthread_mutex_lock(&m);  counter++;  pthread_mutex_unlock(&m);

// FIXED, option B — an atomic read-modify-write:
atomic_fetch_add(&counter, 1);
```

If you internalize "**shared + mutable + concurrent ⇒ you must synchronize**,"
you have the core of every chapter that follows. We derive the *why* from the
hardware on purpose.

Let's begin. → [Part 0.1: What is a thread?](00-foundations/01-what-is-a-thread.md)
