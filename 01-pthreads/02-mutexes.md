# 1.2 — Mutexes

Part 0.4 showed why `count++` from two threads loses updates. A **mutex**
(mutual exclusion lock) is the most direct fix: one thread holds the lock while
in the critical section; everyone else blocks until it is released.

![A mutex serializes access to shared state](figures/mutex.svg)

This chapter covers initialization, the lock operations, mutex types, and the
deadlock that appears the moment you need two locks.

---

## 1.2.1 What a mutex guarantees

```
   thread A                    mutex                  thread B
   ────────                    ─────                  ────────
   lock()  ────────────────▶  HELD by A
   counter++                                           lock() ──▶ blocks
   unlock() ───────────────▶  FREE
                                                       lock() ──▶ got it
                                                       counter++
                                                       unlock()
```

A mutex provides:

1. **Mutual exclusion** — at most one holder.
2. **Memory synchronization** — unlock in A happens-before lock in B, making
   A's prior writes visible to B (Part 0.4).

> **Under the hood ▸** Uncontended `pthread_mutex_lock` is a userspace atomic
> on glibc's futex word. Contended lock → `futex(FUTEX_WAIT)` → kernel sleep
> (Part 0.3). Unlock → atomic store + possible `FUTEX_WAKE`.

---

## 1.2.2 Initialization and destruction

> **The API ▸** `#include <pthread.h>`
> ```c
> pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;   /* static only */
> int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *attr);
> int pthread_mutex_destroy(pthread_mutex_t *m);
> ```

| Method | When to use |
|--------|-------------|
| `PTHREAD_MUTEX_INITIALIZER` | Static/global storage; default attributes |
| `pthread_mutex_init(&m, NULL)` | Stack/heap mutex; optional custom attributes |
| `pthread_mutex_init(&m, &attr)` | Custom type (recursive, error-check, etc.) |

> **Rule ▸** Destroy a mutex only when **no thread** holds it and none will
> ever lock it again. Double-destroy or destroy-while-held is undefined.

---

## 1.2.3 Lock, unlock, trylock, timedlock

> **The API ▸**
> ```c
> int pthread_mutex_lock(pthread_mutex_t *m);       /* block until acquired */
> int pthread_mutex_trylock(pthread_mutex_t *m);    /* 0=got it, EBUSY=not */
> int pthread_mutex_timedlock(pthread_mutex_t *m,
>                             const struct timespec *abstime);
> int pthread_mutex_unlock(pthread_mutex_t *m);
> ```
> All return **0 on success** or an **errno value** (e.g. `EDEADLK`,
> `EINVAL`, `ETIMEDOUT`). They do not set global `errno`.

```
   lock path:
     trylock → EBUSY?  → do other work, retry
                       → or timedlock with deadline
                       → or plain lock (block)
     critical section
     unlock
```

> **Pitfall ▸** Unlocking a mutex **from a thread that does not hold it** is
> **undefined behavior** on POSIX — even if no other thread currently holds it.
> Mutexes have **ownership** semantics (unlike semaphores, Part 1.5).

> **Rule ▸** Keep critical sections **short**. Do not call blocking I/O,
> `malloc` of large buffers, or user callbacks while holding a lock you do not
> control — you serialize the world and invite deadlock.

---

## 1.2.4 Mutex types

> **The API ▸**
> ```c
> int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type);
> ```
> Types: `PTHREAD_MUTEX_NORMAL` (default), `PTHREAD_MUTEX_ERRORCHECK`,
> `PTHREAD_MUTEX_RECURSIVE`.

| Type | Behavior |
|------|----------|
| **normal** | Double-lock by same thread → **deadlock**. Unlock from non-owner → UB. |
| **errorcheck** | Double-lock → returns `EDEADLK`. Unlock from non-owner → `EPERM`. |
| **recursive** | Same thread may lock N times; must unlock N times. |

**Trade-offs ▸** Use **errorcheck** in debug builds to catch lock-order bugs.
Use **recursive** sparingly — it often masks bad design (a function that
needs re-entry because it calls back into code that re-locks). Prefer splitting
into locked/unlocked helpers.

---

## 1.2.5 Deadlock demo

Deadlock needs four conditions simultaneously (Coffman): mutual exclusion,
hold-and-wait, no preemption, **circular wait**.

```
   thread A:  lock(m1) ──▶ lock(m2) ──▶ ...
   thread B:  lock(m2) ──▶ lock(m1) ──▶ ...

   circular wait:
        A holds m1, wants m2
        B holds m2, wants m1
        → both block forever
```

```c
// gcc -Wall -pthread deadlock.c -o deadlock
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static pthread_mutex_t m1 = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t m2 = PTHREAD_MUTEX_INITIALIZER;

static void *thread_a(void *arg)
{
    (void)arg;
    pthread_mutex_lock(&m1);
    usleep(100000);
    pthread_mutex_lock(&m2);   /* waits forever if B holds m2 */
    pthread_mutex_unlock(&m2);
    pthread_mutex_unlock(&m1);
    return NULL;
}

static void *thread_b(void *arg)
{
    (void)arg;
    pthread_mutex_lock(&m2);
    usleep(100000);
    pthread_mutex_lock(&m1);   /* waits forever if A holds m1 */
    pthread_mutex_unlock(&m1);
    pthread_mutex_unlock(&m2);
    return NULL;
}
```

**Fix:** impose a **global lock order** — always lock the lower address first:

```c
static void lock_two(pthread_mutex_t *a, pthread_mutex_t *b)
{
    if (a < b) { pthread_mutex_lock(a); pthread_mutex_lock(b); }
    else       { pthread_mutex_lock(b); pthread_mutex_lock(a); }
}
```

Part 5.2 covers deadlock detection and avoidance in depth.

---

## 1.2.6 Example: fixing the lost update

```c
// gcc -Wall -pthread mutex_counter.c -o mutex_counter
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define NTHREADS 8
#define ITERS    100000

static long             counter = 0;
static pthread_mutex_t  lock    = PTHREAD_MUTEX_INITIALIZER;

static void *worker(void *arg)
{
    (void)arg;
    for (int i = 0; i < ITERS; ++i) {
        int rc = pthread_mutex_lock(&lock);
        if (rc != 0) {
            fprintf(stderr, "lock: %d\n", rc);
            return NULL;
        }
        ++counter;
        rc = pthread_mutex_unlock(&lock);
        if (rc != 0) {
            fprintf(stderr, "unlock: %d\n", rc);
            return NULL;
        }
    }
    return NULL;
}

int main(void)
{
    pthread_t t[NTHREADS];
    for (int i = 0; i < NTHREADS; ++i) {
        int rc = pthread_create(&t[i], NULL, worker, NULL);
        if (rc != 0) { fprintf(stderr, "create: %d\n", rc); return EXIT_FAILURE; }
    }
    for (int i = 0; i < NTHREADS; ++i) {
        int rc = pthread_join(t[i], NULL);
        if (rc != 0) { fprintf(stderr, "join: %d\n", rc); return EXIT_FAILURE; }
    }
    long expected = (long)NTHREADS * ITERS;
    printf("counter = %ld (expected %ld) %s\n",
           counter, expected, counter == expected ? "OK" : "WRONG");
    return EXIT_SUCCESS;
}
```

This satisfies the guide's core rule: the write to `counter` is protected by
synchronization. Alternative: `atomic_fetch_add` (Part 3.1) for a simple counter.

---

## Summary

- A mutex enforces **mutual exclusion** and provides **memory synchronization**
  across unlock/lock pairs.
- Initialize statically (`PTHREAD_MUTEX_INITIALIZER`) or dynamically
  (`pthread_mutex_init`); destroy when permanently done.
- `lock` / `trylock` / `timedlock` / `unlock` — all return 0 or errno; unlock
> from non-owner is UB.
- Types: **normal** (default), **errorcheck** (debug), **recursive** (use
> sparingly).
- **Deadlock** arises from circular wait — fix with global lock ordering.
- Keep critical sections short; uncontended locks stay in userspace via futex.

Next: [1.3 — Condition variables](03-condition-variables.md)
