# 0.4 — The Shared-State Problem

Part 0.1 established that threads share heap and globals. Part 0.2 and 0.3
showed they run interleaved and preempted on shared cores. This chapter
dissects **what goes wrong** when two threads touch the same mutable byte
without coordination — mechanically, instruction by instruction — and names
the two fixes the rest of the guide builds on.

This is the concrete form of the guide's core rule (README):

> **Rule ▸** Two threads must never access the same memory location
> concurrently when at least one access is a **write**, unless that access is
> **atomic** or protected by **synchronization**.

---

## 0.4.1 A race condition, dissected

Consider two threads incrementing a shared `int count`:

```c
count++;   /* looks like one operation — it is not */
```

The compiler emits roughly three machine instructions:

```
   count++  ≡   LOAD  count → register
                ADD   1
                STORE register → count
```

![Two interleaved increments can lose an update](figures/race-condition.svg)

Now two threads interleave:

```
   count starts at 0

   thread A              thread B              count in memory
   ────────              ────────              ─────────────────
   LOAD  → reg=0                               0
                         LOAD  → reg=0         0
   ADD   → reg=1                               0
                         ADD   → reg=1         0
   STORE reg=1                                 1
                         STORE reg=1           1   ← expected 2, got 1
```

Both threads read 0, both compute 1, both write 1. One increment is **lost**.
This is a **lost update** — the simplest race.

> **Pitfall ▸** The bug is **not** "sometimes you get the wrong value." In C and
> C++ it is **undefined behavior** — the compiler and CPU are allowed to assume
> no concurrent unsynchronized access exists and may optimize accordingly. Your
> program has no meaning, not merely an incorrect counter.

---

## 0.4.2 Critical sections and mutual exclusion

The region of code that reads-modifies-writes shared state is a **critical
section**. Only one thread may execute it at a time — **mutual exclusion**:

```
   thread A                         thread B
   ────────                         ────────
   enter critical section  ✓
     count++                          enter critical section  ✗ blocked
   leave critical section
                                      enter critical section  ✓
                                        count++
                                      leave critical section
```

A **mutex** (Part 1.2) implements this: lock before the critical section,
unlock after. While A holds the lock, B's `pthread_mutex_lock` blocks in the
kernel (via futex, Part 0.3) until A unlocks.

**Trade-offs ▸** Mutexes are simple and correct but serialize access — threads
wait. Keep critical sections **short** (increment, pointer publish, queue
operation) and do I/O **outside** the lock.

---

## 0.4.3 Atomicity

An operation is **atomic** if it appears to happen all at once — no other thread
can observe a half-finished state.

```
   NOT atomic (3 separate ops):     count++
   atomic (single HW instruction):  LOCK XADD on x86, or C11 atomic_fetch_add
```

> **Under the hood ▸** On x86-64, `atomic_fetch_add` typically lowers to a
> `lock`-prefixed read-modify-write that holds the cache line exclusively for
> the duration. Other cores' attempts to modify the same line stall at the
> cache-coherence protocol (MESI). That is hardware-enforced atomicity without
> a mutex — but only for **one** memory location and only for operations the
> ISA supports atomically (Part 3.1).

**Trade-offs ▸** Atomics are faster than mutexes for simple counters and flags
but do not compose into multi-variable invariants without careful ordering
(Part 3.3–3.4).

---

## 0.4.4 Visibility: writes are not instantly seen

Even if thread A's store "completes," thread B is not guaranteed to see it
immediately:

```
   thread A                         thread B
   ────────                         ────────
   count = 42  (store)
                                    if (count == 42)  ← may still read 0!
```

> **Under the hood ▸** Each core has **private caches** and often a **store
> buffer**. A write may sit in A's store buffer or L1 while B reads a stale
> copy from its own cache. The CPU and compiler may also **reorder** memory
> operations for performance (Part 3.5). **Synchronization** — mutex
> lock/unlock, atomic with proper memory order — creates **happens-before**
> edges that force visibility across threads.

Mutex lock/unlock is not just mutual exclusion; on all mainstream platforms it
also acts as a **memory fence**: unlock makes prior writes visible to the next
locker.

---

## 0.4.5 Data race = undefined behavior

The C11/C++11 definitions align:

```
   DATA RACE  =  two threads access the same memory location
                 concurrently (no happens-before between them)
                 AND at least one access is a write
                 AND neither is atomic
                 →  UNDEFINED BEHAVIOR
```

| Scenario | Race? | UB? |
|----------|-------|-----|
| Two threads `count++` unsynchronized | ✓ | ✓ |
| One reads, one writes `flag` unsynchronized | ✓ | ✓ |
| Two threads read `config` (no writes) | ✗ | ✗ |
| Two threads `atomic_fetch_add(&count,1)` | ✗ | ✗ |
| Two threads increment under same mutex | ✗ | ✗ |

Tools like **ThreadSanitizer** (`-fsanitize=thread`) instrument loads and stores
at runtime and report races with stack traces. Use it in CI for any threaded code.

> **Rule ▸** "It works on my machine" is not evidence. A data race is UB even if
> you have never seen a wrong value. The compiler may elide your load entirely
> under the no-race assumption.

---

## 0.4.6 Two fixes — preview

Every synchronization problem in this guide is a variation on shared mutable
state. There are two fundamental tools:

```
   FIX A — MUTUAL EXCLUSION (Part 1.2+)
   ─────────────────────────────────────
   pthread_mutex_lock(&m);
   count++;                    /* critical section */
   pthread_mutex_unlock(&m);

   FIX B — ATOMIC OPERATIONS (Part 3.1+)
   ─────────────────────────────────────
   atomic_fetch_add(&count, 1);
   /* or atomic loads/stores with explicit memory_order */
```

| Approach | Best for | Cost |
|----------|----------|------|
| Mutex | Multi-step invariants, complex structures | syscall on contention |
| Atomic | Counters, flags, lock-free structures | cache-line contention |

Higher-level patterns — condition variables (Part 1.3), read-write locks
(Part 1.4), memory orders (Part 3.4) — combine these primitives. The
**invariant never changes**: no concurrent unsynchronized write.

---

## 0.4.7 Demonstrating the lost update

```c
// gcc -Wall -pthread race_demo.c -o race_demo
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define NTHREADS 4
#define ITERS    1000000

static int counter = 0;   /* deliberately unsynchronized → data race → UB */

static void *worker(void *arg)
{
    (void)arg;
    for (int i = 0; i < ITERS; ++i)
        counter++;       /* LOAD / ADD / STORE — not atomic */
    return NULL;
}

int main(void)
{
    pthread_t t[NTHREADS];
    for (int i = 0; i < NTHREADS; ++i) {
        int rc = pthread_create(&t[i], NULL, worker, NULL);
        if (rc != 0) {
            fprintf(stderr, "pthread_create: %d\n", rc);
            return EXIT_FAILURE;
        }
    }
    for (int i = 0; i < NTHREADS; ++i) {
        int rc = pthread_join(t[i], NULL);
        if (rc != 0) {
            fprintf(stderr, "pthread_join: %d\n", rc);
            return EXIT_FAILURE;
        }
    }
    int expected = NTHREADS * ITERS;
    printf("counter = %d (expected %d) %s\n",
           counter, expected,
           counter == expected ? "OK" : "LOST UPDATES");
    return 0;
}
```

Run under ThreadSanitizer to see the race reported even when the printed value
happens to match. Fix with a mutex (Part 1.2) or `atomic_fetch_add` (Part 3.1).

---

## Summary

- `count++` is three instructions; interleaved loads/stores cause **lost
  updates** — the canonical race.
- **Critical sections** need **mutual exclusion** (mutex) or **atomicity**
  (hardware atomics).
- **Visibility** is separate from atomicity: a write in one thread is not
  automatically visible in another without synchronization fences.
- A **data race** (concurrent access, ≥1 write, no sync, not atomic) is
  **undefined behavior** in C/C++ — not a benign bug.
- The two fixes: **locks** (Part 1) and **atomics** (Part 3); both enforce the
  guide's core rule.

Next: [Part 1.1 — Thread lifecycle](../01-pthreads/01-thread-lifecycle.md)
