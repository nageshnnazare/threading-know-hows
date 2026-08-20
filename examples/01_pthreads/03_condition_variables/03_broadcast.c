/*
 * 03_broadcast.c -- signal() vs broadcast().
 *
 *      pthread_cond_signal     wakes ONE waiter (any one, no order)
 *      pthread_cond_broadcast  wakes ALL waiters
 *
 *  Use BROADCAST when:
 *      - The state change makes more than one waiter potentially eligible.
 *      - Multiple distinct predicates use the SAME CV (anti-pattern, but
 *        still happens) -- broadcast lets each thread re-check its own.
 *      - You want a "barrier-like" "go!" event for many waiters.
 *
 *  Use SIGNAL when:
 *      - Only one waiter can possibly proceed (e.g. one item produced,
 *        only one consumer can take it).
 *
 *  Picture: 3 worker threads waiting on `start`:
 *
 *     workers           cond_var "start"
 *     -------           ----------------
 *     T1 wait ---->     [ T1, T2, T3 ]
 *     T2 wait ---->
 *     T3 wait ---->
 *
 *     main:
 *       broadcast() ---> wakes T1, T2, T3 all at once
 *
 *     vs:
 *       signal()    ---> wakes ONE of {T1,T2,T3}, others still asleep
 */

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#define N 4

static pthread_mutex_t m  = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cv = PTHREAD_COND_INITIALIZER;
static int             go = 0;

static void *worker(void *arg)
{
    long id = (long)arg;
    pthread_mutex_lock(&m);
    while (!go)
        pthread_cond_wait(&cv, &m);
    pthread_mutex_unlock(&m);

    printf("[w%ld] running!\n", id);
    return NULL;
}

int main(void)
{
    pthread_t t[N];
    for (long i = 0; i < N; ++i)
        pthread_create(&t[i], NULL, worker, (void *)i);

    sleep(1);
    printf("[main] firing the starting gun\n");

    pthread_mutex_lock(&m);
    go = 1;
    pthread_cond_broadcast(&cv);           /* wake them ALL at once */
    pthread_mutex_unlock(&m);

    for (int i = 0; i < N; ++i)
        pthread_join(t[i], NULL);
    return 0;
}
