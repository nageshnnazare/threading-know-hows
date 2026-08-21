# 0.1 — What Is a Thread?

A thread is, mechanically, **an independent stream of execution that shares its
process's memory with other threads**:

```
   one process = one address space
        ├── thread 0 : own stack + registers, runs its own instruction stream
        ├── thread 1 : own stack + registers, runs its own instruction stream
        └── ... all sharing the SAME heap, globals, code, and open files
```

That single sentence — *separate execution, shared memory* — is the source of
both the power (cheap communication, real parallelism) and the peril (races,
deadlocks) that the rest of this guide is about.

---

## 0.1.1 What a thread owns vs shares

![Threads share the address space; processes do not](figures/process-vs-thread.svg)

Compare two threads in one process against two separate processes:

```
   ┌───────────────────────────┬───────────────────────────────────────┐
   │ SHARED between threads    │ PRIVATE to each thread                │
   ├───────────────────────────┼───────────────────────────────────────┤
   │ heap (malloc/new)         │ stack (locals, call frames)           │
   │ global / static variables │ CPU registers, program counter        │
   │ code (text segment)       │ thread-local storage (thread_local)   │
   │ open file descriptors     │ errno (it's thread-local!)            │
   │ the process's PID         │ its own kernel scheduling entity      │
   └───────────────────────────┴───────────────────────────────────────┘
```

This is the crucial contrast with **processes**, which share *nothing* by
default — each has its own address space, so a wild pointer in one cannot touch
another. Threads trade that isolation for speed:

> **The trade ▸** Threads communicate by simply reading and writing shared
> memory — no copying, no kernel round-trip. That is enormously faster than
> inter-process communication. The cost is that *every* shared, mutable byte is
> now a place two threads can collide (Part 0.4).

---

## 0.1.2 Why use threads at all?

Two distinct reasons, often conflated:

```
   1. PARALLELISM — do more work per second by using multiple cores.
      (a compute-bound job split across 8 cores can run ~8x faster)

   2. CONCURRENCY / RESPONSIVENESS — keep making progress on task B while
      task A is blocked (e.g. a UI thread stays live while I/O runs).
```

Part 0.2 makes this distinction precise. For now: you can want threads for raw
throughput, for responsiveness, or both — and the right synchronization depends
on which.

---

## 0.1.3 A thread is cheaper than a process, not free

```
   fork() a process   : new page tables, copy-on-write of the whole address
                        space, new PID          → ~hundreds of microseconds
   create a thread     : new stack + kernel task, shares existing address space
                                                 → ~tens of microseconds
   a function call     : push a frame            → ~nanoseconds
```

Threads are ~10x cheaper to create than processes but still **thousands of times
more expensive than a function call**, and each carries a stack (often 1–8 MB of
address space). This is why real systems use a **thread pool** (Part 4.1) rather
than spawning a thread per task.

> **Under the hood ▸** On Linux there is no separate "thread" object in the
> kernel — `pthread_create` calls `clone()` with flags that say "share the
> address space, file descriptors, and signal handlers." The kernel schedules
> the result as an ordinary task, exactly like a process. "Threads" and
> "processes" are the same kind of schedulable entity that differ only in *what
> they share* (Part 0.3).

---

## 0.1.4 The mental model you must hold

Because threads share memory and run at unpredictable relative speeds, you must
stop imagining a single, linear timeline. The correct model:

```
   thread A:  ──▶ instr ──▶ instr ──▶ instr ──▶ ...
                       ╳ interleaved in ANY order, at ANY point ╳
   thread B:  ──▶ instr ──▶ instr ──▶ instr ──▶ ...
```

The scheduler may switch threads **between any two machine instructions** — even
in the middle of what looks like one statement in your source (`count++` is
three instructions; Part 0.4). Worse, the CPU and compiler may **reorder**
memory operations (Part 3.5). Any interleaving the rules permit *will* eventually
happen on some machine, under some load. Correct concurrent code is code that is
correct for **every** legal interleaving.

> **Rule ▸** Never reason about "the" order of events across threads. Reason
> about what orderings you have *forced* with synchronization, and assume the
> scheduler is adversarial about everything you left unforced.

---

## 0.1.5 A first example: shared memory, visibly

```c
// build: gcc -Wall -pthread hello_shared.c -o hello_shared
#include <pthread.h>
#include <stdio.h>

int shared = 0;   // in the heap/globals → visible to every thread

void *worker(void *arg) {
    int id = *(int *)arg;
    shared += 1;                 // ⚠ UNSYNCHRONIZED write to shared state
    printf("thread %d sees shared = %d\n", id, shared);
    return NULL;
}

int main(void) {
    pthread_t t[4];
    int ids[4] = {0, 1, 2, 3};
    for (int i = 0; i < 4; i++)
        pthread_create(&t[i], NULL, worker, &ids[i]);
    for (int i = 0; i < 4; i++)
        pthread_join(t[i], NULL);   // wait for all four to finish
    printf("final shared = %d\n", shared);   // hoped for 4 — not guaranteed!
    return 0;
}
```

All four threads touch the *same* `shared`. That is the feature. It is also a
**data race** — the `shared += 1` is unsynchronized — so the final value may not
be 4, and the program technically has undefined behavior. We fix it properly
with mutexes (Part 1.2) and atomics (Part 3.1); Part 0.4 explains exactly how the
count gets lost.

---

## Summary

- A thread is an independent execution stream that **shares its process's
  address space** (heap, globals, code, fds) while owning its **stack and
  registers**.
- Threads exist for **parallelism** (more cores) and/or **concurrency**
  (responsiveness while blocked) — distinguished in Part 0.2.
- They are ~10x cheaper than processes but far costlier than a call, and each
  has a large stack → real systems pool them.
- On Linux a thread is just a `clone()`d task that shares memory; the kernel
  schedules threads and processes identically.
- Shared memory + unpredictable interleaving (and reordering) means you must
  reason about *all* legal orderings, not one timeline — the foundation for
  every rule that follows.

Next: [0.2 — Concurrency vs parallelism](02-concurrency-vs-parallelism.md)
