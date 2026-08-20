# 1.3 — Condition Variables

A mutex answers "only one at a time." A **condition variable** answers "wait
until something becomes true" — without burning CPU polling. Mutexes and
condition variables are **always used together**: the mutex protects the
predicate; the condvar puts threads to sleep until the predicate might have
changed.

![Condition variable: wait on predicate under mutex protection](figures/condition-variable.svg)

---

## 1.3.1 The problem with polling

```c
/* BAD — burns CPU, wrong granularity */
pthread_mutex_lock(&m);
while (!ready) {
    pthread_mutex_unlock(&m);
    /* busy spin or usleep — race between check and sleep */
    pthread_mutex_lock(&m);
}
```

You need to **atomically** release the mutex and go to sleep, then re-acquire
when woken. That is exactly what `pthread_cond_wait` does.

---

## 1.3.2 The API

> **The API ▸** `#include <pthread.h>`
> ```c
> int pthread_cond_init(pthread_cond_t *cv, const pthread_condattr_t *attr);
> int pthread_cond_destroy(pthread_cond_t *cv);
> int pthread_cond_wait(pthread_cond_t *cv, pthread_mutex_t *m);
> int pthread_cond_timedwait(pthread_cond_t *cv, pthread_mutex_t *m,
>                            const struct timespec *abstime);
> int pthread_cond_signal(pthread_cond_t *cv);    /* wake one waiter */
> int pthread_cond_broadcast(pthread_cond_t *cv); /* wake all waiters */
> ```
> All return **0 or an errno value** — not -1, do not rely on global `errno`.

> **Under the hood ▸** `pthread_cond_wait` (while holding `m`):
> 1. Atomically enqueue this thread on `cv`'s wait queue and unlock `m`.
> 2. Sleep in the kernel (futex wait).
> 3. On wake, re-acquire `m` before returning.
>
> `signal`/`broadcast` mark waiters runnable and issue `FUTEX_WAKE`.

---

## 1.3.3 The mutex + predicate pattern

A **predicate** is the condition you care about — expressed as a test on shared
state protected by the mutex:

```c
pthread_mutex_lock(&m);
while (!predicate())           /* NOT if — MUST be while */
    pthread_cond_wait(&cv, &m);
/* predicate is true; m is held */
... work ...
pthread_mutex_unlock(&m);
```

On the signaling side:

```c
pthread_mutex_lock(&m);
... change shared state so predicate may become true ...
pthread_cond_signal(&cv);      /* or broadcast */
pthread_mutex_unlock(&m);
```

> **Rule ▸** Always test the predicate in a **`while` loop**, never `if`, around
> `pthread_cond_wait`. See §1.3.4.

---

## 1.3.4 Why you MUST loop: spurious and lost wakeups

Three separate reasons mandate the `while`:

**1. Spurious wakeups** — POSIX permits `pthread_cond_wait` to return even
though nobody signaled. The standard says: re-check the predicate.

**2. Lost wakeup (early signal)** — a signal before `wait` is not queued on
most implementations. That is why you hold the mutex: the signaler cannot set
state and signal between your predicate check and your `wait` if both sides
lock correctly.

**3. Multiple waiters, one signal** — `signal` wakes **one** thread. If the
predicate allows only one to proceed (e.g. one slot free), the others must
re-wait. With `broadcast`, many wake but only one may succeed — the rest loop.

```
   WITHOUT while (broken):

   consumer:  lock; if (empty) wait;  ← spurious wake, empty buffer, crash
   consumer:  lock; if (empty) wait;  ← two consumers woken, one item
```

> **Pitfall ▸** Using `if (predicate) pthread_cond_wait(...)` is one of the
> most common concurrency bugs. It looks correct; it is not.

---

## 1.3.5 Bounded producer-consumer

The canonical pattern uses **two condition variables** — one for "not full,"
one for "not empty" — so `signal` targets the right waiters:

![Producer-consumer with two condition variables](figures/producer-consumer.svg)

```
   Producer                          Consumer
   ────────                          ────────
   lock(m)                           lock(m)
   while (count == CAP):             while (count == 0):
       wait(not_full, m)                 wait(not_empty, m)
   buf[head++] = item                item = buf[tail++]
   count++                           count--
   signal(not_empty)                 signal(not_full)
   unlock(m)                         unlock(m)
```

**Trade-offs ▸** Two condvars avoid **thundering herd** (broadcast waking
everyone who mostly go back to sleep). One condvar + broadcast works for small
programs but scales poorly.

---

## 1.3.6 Full example

```c
// gcc -Wall -pthread producer_consumer.c -o producer_consumer
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define CAP 4
#define N_PROD 2
#define N_CONS 2
#define ITEMS  8

typedef struct {
    int             buf[CAP];
    int             head, tail, count;
    pthread_mutex_t m;
    pthread_cond_t  not_full;
    pthread_cond_t  not_empty;
} queue_t;

static void q_init(queue_t *q)
{
    q->head = q->tail = q->count = 0;
    if (pthread_mutex_init(&q->m, NULL) != 0) abort();
    if (pthread_cond_init(&q->not_full, NULL) != 0) abort();
    if (pthread_cond_init(&q->not_empty, NULL) != 0) abort();
}

static void q_put(queue_t *q, int v)
{
    if (pthread_mutex_lock(&q->m) != 0) abort();
    while (q->count == CAP)
        if (pthread_cond_wait(&q->not_full, &q->m) != 0) abort();
    q->buf[q->head] = v;
    q->head = (q->head + 1) % CAP;
    q->count++;
    if (pthread_cond_signal(&q->not_empty) != 0) abort();
    if (pthread_mutex_unlock(&q->m) != 0) abort();
}

static int q_get(queue_t *q)
{
    if (pthread_mutex_lock(&q->m) != 0) abort();
    while (q->count == 0)
        if (pthread_cond_wait(&q->not_empty, &q->m) != 0) abort();
    int v = q->buf[q->tail];
    q->tail = (q->tail + 1) % CAP;
    q->count--;
    if (pthread_cond_signal(&q->not_full) != 0) abort();
    if (pthread_mutex_unlock(&q->m) != 0) abort();
    return v;
}

static queue_t Q;

static void *producer(void *arg)
{
    long id = (long)arg;
    for (int i = 0; i < ITEMS; ++i) {
        int item = (int)(id * 100 + i);
        q_put(&Q, item);
        printf("[P%ld] put %d\n", id, item);
    }
    return NULL;
}

static void *consumer(void *arg)
{
    long id = (long)arg;
    int total = (N_PROD * ITEMS) / N_CONS;
    for (int i = 0; i < total; ++i) {
        int item = q_get(&Q);
        printf("[C%ld] got %d\n", id, item);
        usleep(50000);
    }
    return NULL;
}

int main(void)
{
    q_init(&Q);
    pthread_t p[N_PROD], c[N_CONS];

    for (long i = 0; i < N_PROD; ++i)
        if (pthread_create(&p[i], NULL, producer, (void *)i) != 0) abort();
    for (long i = 0; i < N_CONS; ++i)
        if (pthread_create(&c[i], NULL, consumer, (void *)i) != 0) abort();

    for (int i = 0; i < N_PROD; ++i) pthread_join(p[i], NULL);
    for (int i = 0; i < N_CONS; ++i) pthread_join(c[i], NULL);

    pthread_mutex_destroy(&Q.m);
    pthread_cond_destroy(&Q.not_full);
    pthread_cond_destroy(&Q.not_empty);
    return EXIT_SUCCESS;
}
```

All shared-state changes to `buf`, `head`, `tail`, `count` happen under `m` —
satisfying the guide's core rule (README, Part 0.4).

---

## Summary

- Condition variables block until signaled, **atomically** releasing the paired
  mutex.
- Always use the **mutex + predicate + while + wait** pattern.
- Loop because of **spurious wakeups**, **early signals**, and **multiple
  waiters** per signal.
- Producer-consumer: two condvars (`not_full`, `not_empty`) target wakeups
  efficiently.
- `signal` wakes one; `broadcast` wakes all — choose based on how many threads
  can make progress.
- `pthread_cond_wait` is a cancellation point (Part 1.6).

Next: [1.4 — Read-write locks & barriers](04-rwlocks-and-barriers.md)
