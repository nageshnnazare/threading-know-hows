# 0.3 — Threads and the OS

Your `pthread_create` call crosses the user/kernel boundary and creates a
**schedulable entity** the OS treats almost identically to a process. Understanding
that mapping — and what a context switch actually costs — explains why locks
block, why affinity matters, and why "just add threads" has a ceiling.

Part 0.1 said threads share an address space; Part 0.2 said the scheduler
time-slices them. This chapter opens the kernel side of that story.

---

## 0.3.1 User threads → kernel tasks (1:1 on Linux)

![Each pthread becomes a kernel schedulable task](figures/thread-os-mapping.svg)

On Linux, **pthreads use a 1:1 model**: every `pthread_t` is backed by one
kernel **task struct** (historically called a "lightweight process"):

```
   your process
   ┌─────────────────────────────────────────────────────────┐
   │  address space (shared)                                  │
   │    code · heap · globals · mapped files                  │
   │                                                          │
   │  thread 0 (main)  ──▶  task struct  ──▶  run queue     │
   │  thread 1         ──▶  task struct  ──▶  run queue     │
   │  thread 2         ──▶  task struct  ──▶  run queue     │
   └─────────────────────────────────────────────────────────┘
        same PID, same mm_struct, different kernel stacks
```

> **Under the hood ▸** `pthread_create` ultimately calls `clone()` with flags
> like `CLONE_VM | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD`. `CLONE_VM`
> shares the page tables; `CLONE_THREAD` puts the new task in the same thread
> group (same PID from userspace's view). The kernel does **not** maintain a
> separate "user-thread" layer — glibc's pthread is thin wrapper over `clone()`.
> Other models exist (N:1 green threads in old runtimes; M:N in Go's scheduler)
> but POSIX on Linux is 1:1.

**Trade-offs ▸** 1:1 is simple and truly parallel on multi-core hardware, but
each thread consumes kernel memory (~8–16 KB for the task struct plus an 8 MB
default stack mapping) and participates in global scheduling.

---

## 0.3.2 The scheduler, time slices, and preemption

The **scheduler** picks which runnable task runs on which CPU. Linux's
**Completely Fair Scheduler (CFS)** on normal workloads (`SCHED_OTHER`) assigns
each task a **vruntime** and runs the one that has been treated most "unfairly"
— approximating equal CPU share.

```
   run queue (per-CPU or global, simplified):

   runnable:  [ T_main ] [ T_worker1 ] [ T_worker2 ] [ T_io ]
                  ▲
                  └── currently on core 2

   blocked on mutex:  T_worker3  (not in run queue until woken)
   blocked on I/O:    T_io       (not in run queue until interrupt)
```

A thread runs until it **voluntarily** yields (blocks on I/O, mutex, condvar,
`sched_yield`) or is **preempted** when its time slice expires or a higher-
priority task becomes runnable.

> **Under the hood ▸** Preemption on Linux uses the scheduler tick (or
> `CONFIG_NO_HZ_FULL` tickless behavior on dedicated cores). When the tick
> fires, the kernel checks whether the current task should be replaced. There
> is no guarantee your thread runs to completion of a critical section unless
> you hold a lock — the scheduler can switch away **between any two
> instructions** (Part 0.4).

---

## 0.3.3 Context switch cost

Switching from thread A to thread B is not free:

```
   context switch on thread A → thread B:

   1. save A's registers, PC, stack pointer  → A's task struct
   2. pick B from run queue
   3. restore B's registers, PC, stack pointer
   4. (possibly) switch address space — NOT for threads in same process
   5. resume B on the CPU
```

| Cost component | Typical magnitude | Notes |
|----------------|-------------------|-------|
| Register save/restore | ~hundreds of ns | mandatory |
| Kernel scheduler logic | ~1–3 µs | depends on load |
| TLB effects | variable | same process → TLB mostly kept |
| Cache cold start | **dominant at scale** | B's data may evict A's lines |

> **Under the hood ▸** Threads in the **same process share page tables**, so
> a thread switch avoids the full TLB flush that a **process** switch triggers.
> But B's stack and working set may live in cache lines A had hot — after the
> switch, both threads suffer **cache pollution**. This is one reason
> oversubscribing cores (many more threads than CPUs) hurts: constant switching
> keeps caches cold.

**Trade-offs ▸** Blocking mutexes pay one context switch to sleep and one to
wake — ~2–10 µs total, cheap compared to milliseconds of I/O, expensive
compared to a nanosecond increment. Spinlocks (Part 1.5) avoid the switch but
burn CPU while waiting.

---

## 0.3.4 futex(): the primitive under pthread mutexes

When `pthread_mutex_lock` finds the mutex **contended** (another thread holds
it), glibc does not spin forever by default — it asks the kernel to **block**
the caller until the owner unlocks.

> **Under the hood ▸** Linux implements this with **futex** ("fast userspace
> mutex"):
>
> ```
>   unlock path (fast, no syscall):
>     atomic store 0 to futex word in userspace
>     if waiters exist → futex(FUTEX_WAKE)
>
>   lock path (contended):
>     atomic CAS failed → futex(FUTEX_WAIT)  ← thread sleeps in kernel
>     woken by FUTEX_WAKE when owner unlocks
> ```
>
> Uncontended lock/unlock stays entirely in userspace (one atomic instruction).
> Contention crosses into the kernel — that's the context-switch cost above.
> Condition variables (`pthread_cond_wait`) also bottom out in futex waits on
> the internal mutex word.

This is why keeping critical sections **short** matters: you stay on the fast
userspace path more often.

---

## 0.3.5 CPU affinity (brief)

By default the scheduler may move a thread between cores from one time slice to
the next. **CPU affinity** pins a thread to a subset of CPUs:

> **The API ▸** `#include <pthread.h>`
> ```c
> int pthread_setaffinity_np(pthread_t thread, size_t cpusetsize,
>                            const cpu_set_t *cpuset);
> int pthread_getaffinity_np(pthread_t thread, size_t cpusetsize,
>                            cpu_set_t *cpuset);
> ```
> Returns 0 on success, or an **errno value** on failure (pthread functions
> return errno directly — they do not set the global `errno` variable).

```
   without affinity:  T_worker runs on core 0, then 3, then 1, ...
   with affinity:     T_worker pinned to core 2 only
```

**Trade-offs ▸** Affinity can improve cache locality (a thread's stack and hot
data stay on one core's caches) and is essential for **NUMA** systems where
remote memory access is 2–3× slower. Over-pinning can cause load imbalance if
one core is saturated while others idle. Use when profiling shows migration or
NUMA penalties — not by default.

---

## 0.3.6 What the OS guarantees — and what it does not

The kernel guarantees:

- Each thread has its own kernel stack and register context.
- A blocked thread does not consume CPU (unlike a spinlock).
- Preemption can occur at almost any point in user code.

The kernel does **not** guarantee:

- Which order threads run in.
- That a write in thread A is visible to thread B without synchronization.
- Fair progress if your code deadlocks (Part 5.2).

That last gap is exactly the **shared-state problem** (Part 0.4) and the
guide's core rule: concurrent unsynchronized writes are undefined behavior in
C/C++, regardless of what the kernel does.

---

## Summary

- Linux pthreads are **1:1** with kernel tasks via `clone()` — same PID, shared
  address space, separate kernel stacks and scheduling state.
- The **scheduler** (CFS for normal threads) time-slices runnable tasks;
  **preemption** can occur between any two user instructions.
- A **context switch** saves/restores registers; same-process threads avoid TLB
  flush but still pay cache-cold costs — oversubscription hurts.
- Contended mutexes block via **futex** — fast userspace atomics when
  uncontended, kernel sleep/wake when contended.
- **CPU affinity** (`pthread_setaffinity_np`) trades flexibility for cache/NUMA
  locality; use when profiling justifies it.

Next: [0.4 — The shared-state problem](04-the-shared-state-problem.md)
