/*
 * 01_barrier_basic.c -- pthread_barrier_t: rendezvous of N threads.
 *
 *  All threads call pthread_barrier_wait(). The first N-1 callers block.
 *  When the Nth thread arrives, the barrier "releases" and EVERYONE
 *  proceeds at the same time. Useful for phased computations:
 *
 *     phase 1     barrier      phase 2     barrier      phase 3
 *     --------    -------      --------    -------      --------
 *     T1 work --> wait()  -->  T1 work --> wait()  -->  T1 work
 *     T2 work --> wait()  -->  T2 work --> wait()  -->  T2 work
 *     T3 work --> wait()  -->  T3 work --> wait()  -->  T3 work
 *               (all aligned)            (all aligned)
 *
 *  Exactly ONE thread (the "serial thread") gets the special return value
 *  PTHREAD_BARRIER_SERIAL_THREAD; the others get 0. That lets one designated
 *  thread do post-phase serial work (printing summary, swapping buffers).
 *
 *  Note: macOS lacks pthread_barrier; this example is Linux-tested.
 */

#define _XOPEN_SOURCE 700
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#define N_THREADS 4

#ifdef __APPLE__
int main(void) { puts("pthread_barrier not available on macOS"); return 0; }
#else
static pthread_barrier_t bar;

static void *worker(void *arg)
{
    long id = (long)arg;
    for (int phase = 1; phase <= 3; ++phase) {
        usleep((id + 1) * 100 * 1000);     /* unequal work to show waiting */
        printf("[T%ld] reached barrier phase %d\n", id, phase);

        int rc = pthread_barrier_wait(&bar);
        if (rc == PTHREAD_BARRIER_SERIAL_THREAD)
            printf("=== phase %d complete (announced by T%ld) ===\n", phase, id);
    }
    return NULL;
}

int main(void)
{
    pthread_barrier_init(&bar, NULL, N_THREADS);

    pthread_t t[N_THREADS];
    for (long i = 0; i < N_THREADS; ++i)
        pthread_create(&t[i], NULL, worker, (void *)i);
    for (int i = 0; i < N_THREADS; ++i)
        pthread_join(t[i], NULL);

    pthread_barrier_destroy(&bar);
    return 0;
}
#endif
