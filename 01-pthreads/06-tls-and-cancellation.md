# 1.6 — Thread-Local Storage & Cancellation

Not all state should be shared. **Thread-local storage (TLS)** gives each
thread its own copy of a variable — per-thread buffers, errno-like scratch
space, request context. **Cancellation** lets one thread forcibly stop another —
powerful, fragile, and largely superseded by cooperative shutdown flags.

---

## 1.6.1 Why thread-local storage?

Part 0.1 listed TLS in the "private to each thread" column. Without it, a
function using a static buffer would be a race:

```c
static char buf[4096];   /* BUG if two threads call format() concurrently */
```

TLS moves `buf` into each thread's private storage:

```
   process address space
   ┌──────────────────────────────────────────────┐
   │  global key k  ──▶  per-thread slot table    │
   │                     │         │         │    │
   │                 thread A  thread B  thread C |
   │                    buf_A     buf_B     buf_C |
   └──────────────────────────────────────────────┘
```

No mutex needed for thread-private data — different memory locations, no shared
mutable access (Part 0.4).

---

## 1.6.2 POSIX TLS: pthread_key_t

> **The API ▸** `#include <pthread.h>`
> ```c
> int pthread_key_create(pthread_key_t *key, void (*destructor)(void *));
> int pthread_key_delete(pthread_key_t key);
> int pthread_setspecific(pthread_key_t key, const void *value);
> void *pthread_getspecific(pthread_key_t key);
> int pthread_once(pthread_once_t *once, void (*init)(void));
> ```
> pthread functions return **0 or errno value** — not global `errno`.

- `pthread_key_create` — once per process; optional **destructor** runs at
  thread exit when the thread had a non-NULL value.
- `pthread_setspecific` / `pthread_getspecific` — per-thread get/set.
- `pthread_once` — initialize the key exactly once (thread-safe singleton pattern).

> **Under the hood ▸** glibc maintains a per-thread array indexed by key
> number. `pthread_key_create` allocates a key index under a global lock.
> Destructors run during thread teardown, after `start_routine` returns or
> `pthread_exit`, in unspecified order among keys.

---

## 1.6.3 Compiler TLS: `__thread` / `_Thread_local`

> **The API ▸** (GCC/Clang, C11)
> ```c
> __thread int tls_counter;           /* GCC extension */
> _Thread_local int tls_counter;      /* C11 standard */
> ```
> The compiler emits access via a **thread-local storage block** (TLS segment /
> TP-relative addressing on x86-64). No runtime key lookup — faster than
> `pthread_getspecific`.

**Trade-offs ▸**

| | `pthread_key_t` | `__thread` / `_Thread_local` |
|---|-----------------|------------------------------|
| Destructor at thread exit | ✓ | ✗ (C11 TLS has no destructor) |
| Dynamic per-key allocation | ✓ | static/link-time only |
| Access cost | indirect lookup | direct, faster |
| C++ `thread_local` | — | ✓ with constructors/destructors (Part 2.5) |

Use `pthread_key_t` when you need **destructors** (free per-thread heap).
Use `__thread`/`thread_local` for simple per-thread counters and pointers.

---

## 1.6.4 TLS example with destructor

```c
// gcc -Wall -pthread tls_demo.c -o tls_demo
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static pthread_key_t  key;
static pthread_once_t key_once = PTHREAD_ONCE_INIT;

static void buf_destroy(void *p)
{
    printf("  [dtor] free %p\n", p);
    free(p);
}

static void make_key(void)
{
    if (pthread_key_create(&key, buf_destroy) != 0) abort();
}

static char *thread_buf(void)
{
    if (pthread_once(&key_once, make_key) != 0) abort();
    char *p = pthread_getspecific(key);
    if (!p) {
        p = malloc(64);
        if (!p) return NULL;
        snprintf(p, 64, "tid=%lu", (unsigned long)pthread_self());
        if (pthread_setspecific(key, p) != 0) abort();
    }
    return p;
}

static void *worker(void *arg)
{
    long id = (long)arg;
    char *b = thread_buf();
    printf("[T%ld] %s\n", id, b);
    return NULL;   /* destructor runs here for this thread's buffer */
}

int main(void)
{
    pthread_t t[3];
    for (long i = 0; i < 3; ++i) {
        if (pthread_create(&t[i], NULL, worker, (void *)i) != 0) abort();
    }
    for (int i = 0; i < 3; ++i) pthread_join(t[i], NULL);
    pthread_key_delete(key);
    return EXIT_SUCCESS;
}
```

---

## 1.6.5 Thread cancellation

> **The API ▸** `#include <pthread.h>`
> ```c
> int pthread_cancel(pthread_t thread);
> int pthread_setcancelstate(int state, int *oldstate);
>    /* PTHREAD_CANCEL_ENABLE | PTHREAD_CANCEL_DISABLE */
> int pthread_setcanceltype(int type, int *oldtype);
>    /* PTHREAD_CANCEL_DEFERRED | PTHREAD_CANCEL_ASYNCHRONOUS */
> void pthread_testcancel(void);
> void pthread_cleanup_push(void (*routine)(void *), void *arg);
> void pthread_cleanup_pop(int execute);
> ```
> Returns 0 or errno. `pthread_cancel` is **async with respect to the caller**
> — it only *requests* cancellation.

```
   cancellation flow (DEFERRED, default):

   main                          worker
   ────                          ──────
                                 (cancel ENABLED, DEFERRED)
   pthread_cancel(worker) ──▶    flag set; worker still running
                                 ...
                                 hits cancellation point (sleep, cond_wait, ...)
                                 cleanup handlers run LIFO
                                 thread exits (PTHREAD_CANCELED)
   pthread_join ◀──────────────
```

**Cancellation points** include `pthread_cond_wait`, `read`, `write`, `sleep`,
`pthread_join`, and others listed in POSIX. Between those, cancellation is
deferred.

> **Pitfall ▸** `PTHREAD_CANCEL_ASYNCHRONOUS` can stop the thread **mid-
> instruction**, mid-mutex — leaving locks held, invariants broken. Almost
> never use it.

> **Pitfall ▸** `pthread_cleanup_push`/`pop` must appear as **matched pairs in
> the same function** at the same lexical scope (they expand to a brace block
> on some platforms). This is a notorious footgun.

---

## 1.6.6 Cleanup handlers

```c
void cleanup(void *arg) { free(arg); }

pthread_cleanup_push(cleanup, resource);
/* ... cancellation points ... */
pthread_cleanup_pop(1);   /* 1 = run handler; 0 = discard */
```

Handlers run when the thread is canceled **or** calls `pthread_exit`, unwound
LIFO. They are how cancel-safe code releases locks and frees buffers — but the
control-flow macros are fragile.

---

## 1.6.7 Cooperative shutdown is usually better

**Trade-offs ▸** Modern practice favors an **`atomic` or `volatile sig_atomic_t`
stop flag** checked at well-defined points:

```c
// gcc -Wall -pthread stop_flag.c -o stop_flag
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static atomic_bool stop = ATOMIC_VAR_INIT(false);

static void *worker(void *arg)
{
    (void)arg;
    while (!atomic_load(&stop)) {
        printf("[worker] working\n");
        sleep(1);   /* safe check point between iterations */
    }
    printf("[worker] clean exit\n");
    return NULL;
}

int main(void)
{
    pthread_t t;
    if (pthread_create(&t, NULL, worker, NULL) != 0) abort();
    sleep(3);
    atomic_store(&stop, true);
    if (pthread_join(t, NULL) != 0) abort();
    printf("[main] done\n");
    return EXIT_SUCCESS;
}
```

| | Cancellation | Atomic stop flag |
|---|--------------|------------------|
| Portable mental model | complex | simple |
| Invariants at stop | fragile | you choose the point |
| Mutex state on stop | may be held | you release before checking |
| Debuggability | poor | good |

> **Rule ▸** Prefer **cooperative shutdown** via a flag the worker polls at
> known-safe points. Reserve `pthread_cancel` for legacy code or bounded
> blocking I/O you cannot refactor. Part 4 patterns (thread pool shutdown)
> always use flags or poison pills.

If the stop flag is written by one thread and read by another, use `atomic_bool`
(or mutex) — a plain `bool` race is still UB (Part 0.4).

---

## Summary

- **TLS** gives each thread private storage — no lock for thread-private data.
- `pthread_key_t` supports **destructors**; `__thread`/`thread_local` is faster
  but C11 TLS lacks destructors.
- Initialize keys with `pthread_once` + `pthread_key_create`.
- **Cancellation** is deferred by default at **cancellation points**; async
  cancellation is dangerous.
- `pthread_cleanup_push`/`pop` release resources on cancel — awkward macros.
- **Cooperative stop flags** (`atomic_bool`) are clearer, safer, and preferred
  for new code.

Next: [Part 2.1 — std::thread & jthread](../02-cpp-threads/01-std-thread.md)
