# 1.5 — Semaphores & Spinlocks

Mutexes (Part 1.2) enforce mutual exclusion with **ownership** — only the
locker may unlock. **Semaphores** count resources without ownership. **Spinlocks**
busy-wait instead of sleeping. Both have narrow, well-defined niches.

![Semaphore: a counter gating N concurrent users](figures/semaphore.svg)

---

## 1.5.1 POSIX semaphores

> **The API ▸** `#include <semaphore.h>`
> ```c
> int sem_init(sem_t *sem, int pshared, unsigned value);  /* unnamed, thread-shared */
> int sem_destroy(sem_t *sem);
> int sem_wait(sem_t *sem);      /* P(): if 0, block; else decrement */
> int sem_trywait(sem_t *sem);   /* fail with EAGAIN if 0 */
> int sem_post(sem_t *sem);      /* V(): increment; wake one waiter */
>
> sem_t *sem_open(const char *name, int oflag, mode_t mode, unsigned value);
> int sem_close(sem_t *sem);
> int sem_unlink(const char *name);
> ```
> `sem_init`/`sem_destroy`/`sem_wait`/`sem_post` return **0 or -1** with
> **`errno` set** — unlike pthread functions. `sem_open` returns `SEM_FAILED`
> on error.

```
   counting semaphore, initial value = 3 (pool of 3 slots):

   sem_wait  →  3→2→1→0  →  next wait blocks
   sem_post  →  0→1       →  wakes one blocked waiter
```

| Kind | Initial value | Use |
|------|---------------|-----|
| **Counting** | N > 1 | N identical resources (DB connections, buffer slots) |
| **Binary** | 1 | mutex-like gate (any thread may post — no owner) |

**Trade-offs ▸** Choose semaphores when you are counting **resources** or
**permits**. Choose mutex + condvar when you need a **predicate** ("queue
non-empty"). Choose a mutex when you need **ownership** (only holder unlocks).

---

## 1.5.2 Unnamed vs named semaphores

```
   unnamed (sem_init)              named (sem_open)
   ──────────────────              ────────────────
   sem_t in process memory         kernel-persistent name "/mysem"
   pshared=0: threads in process   cross-process (pshared=1 on some systems)
   destroyed at sem_destroy        sem_unlink to remove name
```

> **Pitfall ▸** macOS **deprecated unnamed semaphores** (`sem_init`). Use
> `sem_open`/`sem_close`/`sem_unlink` on macOS, or prefer mutex/cv/portable
> abstractions. Linux glibc supports both.

---

## 1.5.3 Semaphore example: connection pool

```c
// gcc -Wall -pthread semaphore_pool.c -o semaphore_pool
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>

#define POOL  3
#define USERS 6

static sem_t gate;

static void *user(void *arg)
{
    long id = (long)arg;
    if (sem_wait(&gate) != 0) { perror("sem_wait"); return NULL; }
    printf("[U%ld] in pool\n", id);
    sleep(1);
    printf("[U%ld] leaving\n", id);
    if (sem_post(&gate) != 0) perror("sem_post");
    return NULL;
}

int main(void)
{
    if (sem_init(&gate, 0, POOL) != 0) {
        perror("sem_init");
        return EXIT_FAILURE;
    }

    pthread_t t[USERS];
    for (long i = 0; i < USERS; ++i) {
        if (pthread_create(&t[i], NULL, user, (void *)i) != 0) abort();
    }
    for (int i = 0; i < USERS; ++i) pthread_join(t[i], NULL);

    sem_destroy(&gate);
    return EXIT_SUCCESS;
}
```

At most three threads execute `sleep(1)` concurrently — the semaphore counts
slots, not protecting a specific variable. If these threads also mutated shared
state, you would still need mutexes around that state (Part 0.4).

---

## 1.5.4 Spinlocks: busy-wait mutual exclusion

> **The API ▸** `#include <pthread.h>` (`_XOPEN_SOURCE 700`; **not on macOS**)
> ```c
> int pthread_spin_init(pthread_spinlock_t *lock, int pshared);
> int pthread_spin_destroy(pthread_spinlock_t *lock);
> int pthread_spin_lock(pthread_spinlock_t *lock);
> int pthread_spin_trylock(pthread_spinlock_t *lock);
> int pthread_spin_unlock(pthread_spinlock_t *lock);
> ```
> Return **0 or errno value** (pthread convention). `pshared` must be
> `PTHREAD_PROCESS_PRIVATE` for thread spinlocks.

```
   mutex (contended):              spinlock (contended):
   ─────────────────               ────────────────────
   lock fails → kernel sleep       lock fails → retry in tight loop
   (~µs, no CPU burn)               (burns CPU on ALL cores' caches)
```

> **Under the hood ▸** `pthread_spin_lock` loops on an atomic test-and-set (or
> equivalent) in userspace. No futex sleep. On success, latency is minimal — no
> context switch. While waiting, the core executes the spin loop at full speed,
> hammering the **cache line** holding the lock — **false sharing** territory
> (Part 5.4) if unrelated data shares the line.

---

## 1.5.5 When to spin — and when not to

**Trade-offs ▸**

| | Mutex | Spinlock |
|---|-------|----------|
| Wait behavior | sleep in kernel | busy-loop |
| Best for | general use, long CS | **very short** CS on **multicore** |
| Uniprocessor | OK | **disastrous** (wastes the only core) |
| May block in CS | OK (others sleep) | **never** — you hold a core while waiting |
| glibc adaptive mutex | spins briefly, then sleeps | N/A |

> **Rule ▸** Default to **mutex**. Use spinlocks only when profiling shows
> contended ultra-short sections (e.g. push to a lock-free queue's spin-protected
> head) on multi-core hardware. **Never** spin if the critical section might
> sleep, allocate, or call code that blocks — the holder blocks while waiters
> burn CPU forever.

> **Pitfall ▸** Modern glibc mutexes are **adaptive** — they spin briefly before
> futex sleep. Explicit spinlocks are often redundant unless you have measured
> a need.

---

## 1.5.6 Spinlock example

```c
// gcc -Wall -pthread -D_GNU_SOURCE spinlock_demo.c -o spinlock_demo
// Linux only — macOS lacks pthread_spinlock
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define NTH   4
#define ITERS 1000000

static pthread_spinlock_t spin;
static long               counter;

static void *worker(void *arg)
{
    (void)arg;
    for (int i = 0; i < ITERS; ++i) {
        if (pthread_spin_lock(&spin) != 0) abort();
        ++counter;
        if (pthread_spin_unlock(&spin) != 0) abort();
    }
    return NULL;
}

int main(void)
{
    if (pthread_spin_init(&spin, PTHREAD_PROCESS_PRIVATE) != 0) abort();

    pthread_t t[NTH];
    for (int i = 0; i < NTH; ++i)
        if (pthread_create(&t[i], NULL, worker, NULL) != 0) abort();
    for (int i = 0; i < NTH; ++i) pthread_join(t[i], NULL);

    pthread_spin_destroy(&spin);
    printf("counter = %ld (expected %d)\n", counter, NTH * ITERS);
    return EXIT_SUCCESS;
}
```

The increment is protected — no data race. Whether spinlock beats mutex here
depends on contention and core count; measure, don't assume.

---

## Summary

- **Semaphores** count permits; `sem_wait`/`sem_post` use **errno**, unlike
  pthread calls.
- **Counting** semaphores gate N resources; **binary** (initial 1) acts like a
  lock without ownership.
- **Named** semaphores (`sem_open`) work cross-process; macOS favors named over
  `sem_init`.
- **Spinlocks** busy-wait — low latency for tiny critical sections on
  **multicore**, wasteful otherwise.
- Never spin if you might block; prefer mutex unless profiling proves otherwise.
- glibc adaptive mutexes often make explicit spinlocks unnecessary.

Next: [1.6 — Thread-local storage & cancellation](06-tls-and-cancellation.md)
