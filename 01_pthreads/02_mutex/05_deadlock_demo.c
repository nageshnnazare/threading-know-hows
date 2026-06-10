/*
 * 05_deadlock_demo.c -- Two-mutex deadlock and how to avoid it.
 *
 *  THE DEADLOCK (lock-ordering inversion):
 *
 *      thread A:                thread B:
 *      lock(m1)                 lock(m2)
 *      lock(m2)  <-- waits B   lock(m1)  <-- waits A
 *      ...                      ...
 *      unlock(m2)               unlock(m1)
 *      unlock(m1)               unlock(m2)
 *
 *      circular wait:
 *
 *          A holds m1, wants m2     +---+    wants    +---+
 *                                   | A |  -------->  | m2|
 *                                   +---+             +---+
 *                                     ^                 |
 *                                holds|                 |held by
 *                                     |                 v
 *                                   +---+    wants    +---+
 *                                   | m1|  <--------  | B |
 *                                   +---+             +---+
 *
 *      Coffman's four conditions for deadlock (need ALL four):
 *        1. Mutual exclusion       (lock can be held by only one thread)
 *        2. Hold-and-wait          (hold one resource while waiting for another)
 *        3. No preemption          (can't take a lock from a thread)
 *        4. Circular wait          (cycle in the wait-for graph)
 *
 *      Easiest fix: BREAK CIRCULAR WAIT by always locking in a fixed
 *      global order. Below we lock by the lower address first.
 *
 *  This file:
 *    - First runs the buggy version with a small race window so it
 *      reliably deadlocks. We bound it with a 2-second alarm that aborts.
 *      (Comment out main()'s call to deadlock_run() once you've seen it.)
 *    - Then runs the fixed version that always succeeds.
 */

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

static pthread_mutex_t m1 = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t m2 = PTHREAD_MUTEX_INITIALIZER;

/* ---------- BUGGY VERSION (deadlocks) -------------------------- */

static void *bad_a(void *arg)
{
    (void)arg;
    pthread_mutex_lock(&m1);
    printf("[bad A] got m1, sleeping...\n");
    usleep(100 * 1000);                    /* widen race window */
    printf("[bad A] trying for m2...\n");
    pthread_mutex_lock(&m2);
    printf("[bad A] got both!\n");
    pthread_mutex_unlock(&m2);
    pthread_mutex_unlock(&m1);
    return NULL;
}

static void *bad_b(void *arg)
{
    (void)arg;
    pthread_mutex_lock(&m2);
    printf("[bad B] got m2, sleeping...\n");
    usleep(100 * 1000);
    printf("[bad B] trying for m1...\n");
    pthread_mutex_lock(&m1);
    printf("[bad B] got both!\n");
    pthread_mutex_unlock(&m1);
    pthread_mutex_unlock(&m2);
    return NULL;
}

static void deadlock_run(void)
{
    printf("--- Demonstrating deadlock (will abort after 2 seconds) ---\n");
    alarm(2);                              /* SIGALRM kills the process */
    pthread_t a, b;
    pthread_create(&a, NULL, bad_a, NULL);
    pthread_create(&b, NULL, bad_b, NULL);
    pthread_join(a, NULL);                 /* never returns */
    pthread_join(b, NULL);
    alarm(0);
}

/* ---------- FIXED VERSION (lock ordering by address) ----------- */

static void lock_ordered(pthread_mutex_t *x, pthread_mutex_t *y)
{
    if (x < y) { pthread_mutex_lock(x); pthread_mutex_lock(y); }
    else       { pthread_mutex_lock(y); pthread_mutex_lock(x); }
}

static void unlock_two(pthread_mutex_t *x, pthread_mutex_t *y)
{
    /* Order of unlock doesn't affect correctness for blocking unlocks. */
    pthread_mutex_unlock(x);
    pthread_mutex_unlock(y);
}

static void *good_a(void *arg)
{
    (void)arg;
    lock_ordered(&m1, &m2);
    printf("[good A] got both safely\n");
    unlock_two(&m1, &m2);
    return NULL;
}

static void *good_b(void *arg)
{
    (void)arg;
    lock_ordered(&m2, &m1);                /* same global order */
    printf("[good B] got both safely\n");
    unlock_two(&m1, &m2);
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc > 1 && argv[1][0] == 'd') {
        deadlock_run();                    /* demo deadlock then SIGALRM-exit */
    }
    pthread_t a, b;
    pthread_create(&a, NULL, good_a, NULL);
    pthread_create(&b, NULL, good_b, NULL);
    pthread_join(a, NULL);
    pthread_join(b, NULL);
    printf("--- Fixed version completed without deadlock ---\n");
    return 0;
}
