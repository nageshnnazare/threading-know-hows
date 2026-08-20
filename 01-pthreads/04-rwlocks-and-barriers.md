# 1.4 — Read-Write Locks & Barriers

Mutexes serialize all access. Sometimes reads dominate and can safely overlap —
**read-write locks** exploit that. When threads must finish a phase together
before any proceeds, **barriers** provide a rendezvous. Both build on the same
shared-state discipline from Part 0.4.

---

## 1.4.1 Read-write locks

![Many readers OR one writer — never both](figures/rwlock.svg)

> **The API ▸** `#include <pthread.h>`
> ```c
> pthread_rwlock_t rw = PTHREAD_RWLOCK_INITIALIZER;
> int pthread_rwlock_init(pthread_rwlock_t *rw, const pthread_rwlockattr_t *attr);
> int pthread_rwlock_destroy(pthread_rwlock_t *rw);
> int pthread_rwlock_rdlock(pthread_rwlock_t *rw);
> int pthread_rwlock_wrlock(pthread_rwlock_t *rw);
> int pthread_rwlock_tryrdlock(pthread_rwlock_t *rw);
> int pthread_rwlock_trywrlock(pthread_rwlock_t *rw);
> int pthread_rwlock_unlock(pthread_rwlock_t *rw);
> ```
> All return **0 or an errno value** — pthread convention, not global `errno`.

```
   state machine (simplified):

              free ──rdlock──▶  N readers ──rdlock──▶  N+1 readers
               │                                              │
               │ wrlock                                       │ wrlock
               ▼                                              ▼
            1 writer  ◀────────────────────────────────  (writers block
            (exclusive)                                     until readers go)
```

| Caller holds | rdlock | wrlock |
|--------------|--------|--------|
| nothing | ✓ | ✓ |
| readers | ✓ (more readers) | blocks |
| writer | blocks | blocks |

---

## 1.4.2 When RW locks help — and when they do not

**Trade-offs ▸** RW locks pay off when:

- Reads are **much more frequent** than writes.
- Read critical sections are **non-trivial** (enough work to amortize lock
  overhead).

They often **hurt** when:

- Writes are frequent — every writer blocks all readers; you serialize anyway.
- Critical sections are tiny — a plain mutex may be faster (fewer book-keeping
  atomic ops inside the RW lock).
- You need **writer priority** but the implementation prefers readers (or vice
  versa) — check platform behavior if starvation matters.

> **Pitfall ▸** **Writer starvation**: a steady stream of incoming readers can
> prevent a waiting writer from ever acquiring the lock on some implementations.
> glibc's RW lock tends toward **writer preference** after a writer blocks, but
> do not rely on this portably for real-time guarantees.

> **Rule ▸** Profile before replacing mutexes with RW locks. "Many readers" in
> design docs is not the same as "many readers" in `perf`.

---

## 1.4.3 RW lock example

```c
// gcc -Wall -pthread rwlock_demo.c -o rwlock_demo
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static pthread_rwlock_t rw = PTHREAD_RWLOCK_INITIALIZER;
static int              shared_data = 0;

static void *reader(void *arg)
{
    long id = (long)arg;
    for (int i = 0; i < 3; ++i) {
        int rc = pthread_rwlock_rdlock(&rw);
        if (rc != 0) { fprintf(stderr, "rdlock: %d\n", rc); return NULL; }
        printf("[R%ld] read %d\n", id, shared_data);
        usleep(50000);
        pthread_rwlock_unlock(&rw);
    }
    return NULL;
}

static void *writer(void *arg)
{
    long id = (long)arg;
    for (int i = 0; i < 2; ++i) {
        int rc = pthread_rwlock_wrlock(&rw);
        if (rc != 0) { fprintf(stderr, "wrlock: %d\n", rc); return NULL; }
        ++shared_data;
        printf("[W%ld] wrote %d\n", id, shared_data);
        usleep(100000);
        pthread_rwlock_unlock(&rw);
    }
    return NULL;
}

int main(void)
{
    pthread_t r[3], w[1];
    for (long i = 0; i < 3; ++i)
        if (pthread_create(&r[i], NULL, reader, (void *)i) != 0) abort();
    if (pthread_create(&w[0], NULL, writer, (void *)0) != 0) abort();
    for (int i = 0; i < 3; ++i) pthread_join(r[i], NULL);
    pthread_join(w[0], NULL);
    pthread_rwlock_destroy(&rw);
    return EXIT_SUCCESS;
}
```

Writers still require exclusive access — all mutations to `shared_data` happen
under `wrlock`, readers under `rdlock`. No unsynchronized access (Part 0.4).

---

## 1.4.4 Barriers: phased computation

![All N threads must arrive before any proceeds](figures/barrier.svg)

A **barrier** is a reusable rendezvous: N threads each call `wait`; the first
N−1 block; when the Nth arrives, **all** are released simultaneously.

```
   phase 1        barrier         phase 2        barrier
   ───────        ───────         ───────        ───────
   T0 work ──┐                    T0 work ──┐
   T1 work ──┼── all wait ──▶     T1 work ──┼── all wait ──▶ ...
   T2 work ──┘    (sync)          T2 work ──┘    (sync)
```

> **The API ▸** `#include <pthread.h>` (requires `_XOPEN_SOURCE 700` or
> `_GNU_SOURCE` on glibc; **not available on macOS**)
> ```c
> int pthread_barrier_init(pthread_barrier_t *barrier,
>                          const pthread_barrierattr_t *attr, unsigned count);
> int pthread_barrier_destroy(pthread_barrier_t *barrier);
> int pthread_barrier_wait(pthread_barrier_t *barrier);
> ```
> `count` is the number of threads that must arrive. `pthread_barrier_wait`
> returns `0` for all but one thread; exactly **one** thread receives
> `PTHREAD_BARRIER_SERIAL_THREAD` — useful for one thread to run post-phase
> serial work (merge results, swap buffers).

> **Under the hood ▸** Implementation is typically a mutex + condition variable
> (or futex) with an arrival counter inside the barrier object. Each `wait`
> increments the counter under lock; the last arrival broadcasts to the rest.

---

## 1.4.5 Barrier example: three phases

```c
// gcc -Wall -pthread -D_GNU_SOURCE barrier_demo.c -o barrier_demo
// Linux only — pthread_barrier not on macOS
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define N 4

static pthread_barrier_t bar;

static void *worker(void *arg)
{
    long id = (long)arg;
    for (int phase = 1; phase <= 3; ++phase) {
        usleep((unsigned)(id + 1) * 50000);   /* unequal work */
        printf("[T%ld] finished phase %d work\n", id, phase);

        int rc = pthread_barrier_wait(&bar);
        if (rc == PTHREAD_BARRIER_SERIAL_THREAD)
            printf("=== phase %d complete (announced by T%ld) ===\n", phase, id);
        else if (rc != 0) {
            fprintf(stderr, "barrier_wait: %d\n", rc);
            return NULL;
        }
    }
    return NULL;
}

int main(void)
{
    if (pthread_barrier_init(&bar, NULL, N) != 0) {
        fprintf(stderr, "barrier_init failed\n");
        return EXIT_FAILURE;
    }

    pthread_t t[N];
    for (long i = 0; i < N; ++i) {
        if (pthread_create(&t[i], NULL, worker, (void *)i) != 0) abort();
    }
    for (int i = 0; i < N; ++i) pthread_join(t[i], NULL);

    pthread_barrier_destroy(&bar);
    return EXIT_SUCCESS;
}
```

**Trade-offs ▸** Barriers coordinate **all** participants — if one thread is
slow or stuck, everyone waits. Condition variables (Part 1.3) offer finer-grained
"wait until predicate" control. C++20's `std::barrier` (Part 2.6) is the modern
equivalent.

---

## 1.4.6 Barriers vs condition variables

| Need | Use |
|------|-----|
| "Wait until buffer non-empty" | mutex + condvar + predicate |
| "All 8 workers finish step 2 before step 3" | `pthread_barrier` |
| "One thread merges after everyone done" | barrier + `PTHREAD_BARRIER_SERIAL_THREAD` |

Both require that shared state mutated in a phase is visible to the next — the
barrier's internal synchronization provides a happens-before between `wait`
calls across phases for data written before the barrier.

---

## Summary

- **RW locks**: many concurrent readers **or** one writer; good for read-heavy,
  write-rare workloads with non-trivial read sections.
- Watch for **writer starvation** and don't assume RW locks beat mutexes without
  profiling.
- **Barriers**: N-way rendezvous; all threads wait, all released together;
  one thread gets `PTHREAD_BARRIER_SERIAL_THREAD`.
- Barriers suit **phased parallel algorithms**; condvars suit **predicate-based**
  waiting.
- Linux/glibc only for `pthread_barrier`; macOS lacks it.

Next: [1.5 — Semaphores & spinlocks](05-semaphores-and-spinlocks.md)
