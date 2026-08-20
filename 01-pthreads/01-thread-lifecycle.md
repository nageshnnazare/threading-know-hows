# 1.1 — Thread Lifecycle

Part 0 established what a thread is and why shared state is dangerous. Part 1
turns to the **POSIX threads API** — the C-level interface every Linux server,
embedded runtime, and many C++ standard-library implementations sit on top of.
This chapter covers creating a thread, passing arguments safely, collecting its
return value, and the joinable vs detached distinction that governs resource
lifetime.

---

## 1.1.1 Creating a thread

![Thread lifecycle: create → run → join or detach → resources reclaimed](figures/pthread-lifecycle.svg)

> **The API ▸** `#include <pthread.h>`
> ```c
> int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
>                    void *(*start_routine)(void *), void *arg);
> ```
> On success returns **0**. On failure returns an **errno value directly** —
> pthread functions do **not** set the global `errno` variable and do **not**
> return -1. Always check `rc != 0`.

```
   main thread                          new thread
   ───────────                          ──────────
   pthread_create(&tid, ...)  ────────▶  start_routine(arg)
   continues immediately                    runs independently
   pthread_join(tid, ...)     ◀────────  return retval (or pthread_exit)
```

- `thread` — output handle (`pthread_t`, opaque; compare with `pthread_equal`,
  never `==`).
- `attr` — `NULL` for defaults (joinable, platform default stack ~8 MB on
  glibc Linux).
- `start_routine` — must have signature `void *fn(void *arg)`; return value
  becomes joinable via `pthread_join`.
- `arg` — single `void *`; cast inside the worker. Must point to memory that
  **outlives** the thread's read of it.

---

## 1.1.2 Passing arguments safely

Because `arg` is one pointer, bundle multiple values in a struct:

```c
typedef struct { int id; int n; } arg_t;
arg_t args[N];          /* array — each element has stable address */
for (int i = 0; i < N; ++i) {
    args[i].id = i;
    pthread_create(&t[i], NULL, worker, &args[i]);
}
```

> **Pitfall ▸** The **loop-variable-address bug** — one of the most common
> pthread mistakes:
> ```c
> for (int i = 0; i < N; ++i)
>     pthread_create(&t[i], NULL, worker, &i);   /* BUG */
> ```
> All threads receive the **same address**. By the time they read `*arg`, the
> loop may have advanced — every thread sees `N-1`, or garbage. Fix: pass
> `&args[i]` where `args` is an array, or heap-allocate per thread, or cast
> `(void *)(long)i` for a single integer.

---

## 1.1.3 Joining and return values

> **The API ▸**
> ```c
> int pthread_join(pthread_t thread, void **retval);
> ```
> Blocks until `thread` terminates. If `retval != NULL`, writes the pointer
> returned from `start_routine` (or passed to `pthread_exit`). Returns 0 or an
> errno value.

```c
void *worker(void *arg) {
    int *result = malloc(sizeof(int));
    *result = 42;
    return result;              /* ownership transferred to joiner */
}

void *rv;
pthread_join(tid, &rv);
int *p = rv;
printf("result = %d\n", *p);
free(p);
```

> **Rule ▸** Every joinable thread must be **joined exactly once** (or
> detached). A joinable thread that is neither joined nor detached leaks kernel
> resources — a "zombie thread" until process exit.

---

## 1.1.4 Detach vs join

> **The API ▸**
> ```c
> int pthread_detach(pthread_t thread);
> int pthread_exit(void *retval);
> ```
> `pthread_detach` — mark thread as **detached**; resources are reclaimed
> automatically at thread exit; `pthread_join` on a detached thread fails
> (`EINVAL`). `pthread_exit` — terminate **calling** thread with `retval`
> (visible to joiner); equivalent to `return retval` from `start_routine`.

```
   JOINABLE (default)              DETACHED
   ─────────────────               ────────
   must pthread_join once          no join needed
   or pthread_detach before exit   resources auto-freed at exit
   retval available via join        retval lost
```

**Trade-offs ▸** Detached threads suit fire-and-forget background work. Joinable
threads suit tasks where the main thread needs the result or must know
completion before proceeding. You cannot join a thread you already detached.

---

## 1.1.5 Thread identity

> **The API ▸**
> ```c
> pthread_t pthread_self(void);
> int pthread_equal(pthread_t t1, pthread_t t2);
> ```
> `pthread_self()` returns the caller's ID. `pthread_equal` compares two IDs
> (never compare `pthread_t` with `==` — it may be a struct on some platforms).

Useful for per-thread logging, asserting "I hold this lock" in debug builds, or
detecting recursive lock attempts (Part 1.2).

---

## 1.1.6 Attributes: stack size and detach state

> **The API ▸**
> ```c
> int pthread_attr_init(pthread_attr_t *attr);
> int pthread_attr_destroy(pthread_attr_t *attr);
> int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate);
> int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize);
> ```
> `detachstate`: `PTHREAD_CREATE_JOINABLE` (default) or
> `PTHREAD_CREATE_DETACHED`. All return 0 or errno value.

Each thread gets its own stack (default often 8 MB **virtual** on Linux — only
committed pages cost RAM). Shrink with `setstacksize` for thousands of threads;
grow if deep recursion or large stack frames are needed.

> **Under the hood ▸** Stack overflow into another thread's stack or the heap
> corrupts the process silently — there is no guard on the shared side. Use
> `pthread_attr_setguardsize` for a protected guard page below the stack on
> platforms that support it.

---

## 1.1.7 Full example: create, join, return value

```c
// gcc -Wall -pthread lifecycle.c -o lifecycle
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int id;
    int n;
} arg_t;

static void *worker(void *arg)
{
    arg_t *a = (arg_t *)arg;
    long sum = 0;
    for (int i = 1; i <= a->n; ++i)
        sum += i;

    long *result = malloc(sizeof(long));
    if (!result)
        pthread_exit(NULL);
    *result = sum;
    return result;
}

int main(void)
{
    enum { N = 4 };
    pthread_t tids[N];
    arg_t     args[N];

    for (int i = 0; i < N; ++i) {
        args[i].id = i;
        args[i].n  = (i + 1) * 100;
        int rc = pthread_create(&tids[i], NULL, worker, &args[i]);
        if (rc != 0) {
            fprintf(stderr, "pthread_create(%d): %d\n", i, rc);
            return EXIT_FAILURE;
        }
    }

    long grand = 0;
    for (int i = 0; i < N; ++i) {
        void *rv = NULL;
        int rc = pthread_join(tids[i], &rv);
        if (rc != 0) {
            fprintf(stderr, "pthread_join(%d): %d\n", i, rc);
            return EXIT_FAILURE;
        }
        if (rv) {
            grand += *(long *)rv;
            free(rv);
        }
    }
    printf("grand total = %ld\n", grand);
    return EXIT_SUCCESS;
}
```

Remember the shared-state rule (README, Part 0.4): if these workers had
incremented a shared counter without a mutex, that would be a data race. Here
each thread writes only to its own `result` and the joiner reads after
synchronization via `pthread_join` (which establishes a happens-before edge).

---

## Summary

- `pthread_create` spawns a joinable thread; returns **0 or errno**, not -1.
- Pass args via stable addresses — **never** `&i` from a mutating loop variable.
- `pthread_join` waits and collects `retval`; each joinable thread must be
  joined or detached exactly once.
- `pthread_detach` for fire-and-forget; `pthread_exit` / `return` to exit with
  a value.
- `pthread_self` / `pthread_equal` for identity; attributes control detach
  state and stack size.
- `pthread_join` synchronizes with thread exit — safe handoff of return values.

Next: [1.2 — Mutexes](02-mutexes.md)
