/*
 * 01_spinlock.c -- pthread_spinlock_t: busy-wait lock.
 *
 *  A spinlock loops in a tight CPU-burning loop trying to acquire the lock,
 *  rather than putting the thread to sleep. That eliminates the
 *  context-switch overhead of mutexes (~microseconds) IF the critical
 *  section is very short (a few ns to maybe 1 us).
 *
 *  Trade-off:
 *
 *     mutex (sleeps)         spinlock (burns CPU)
 *     -----------------      -----------------------
 *     +  no CPU while wait   +  no syscall
 *     +  fair-ish            +  minimum latency once free
 *     -  ~1us context-switch -  wasteful if section is long
 *     -  syscall path        -  worse on uniprocessor
 *
 *  Rule of thumb: prefer std::mutex / pthread_mutex unless profiling shows
 *  contended ultra-short sections (think: enqueue an int into a single
 *  field). Modern Linux glibc mutexes are already adaptive (spin briefly
 *  then sleep), often making explicit spinlocks unnecessary.
 *
 *  Picture:
 *
 *      mutex                  spinlock
 *      -----                  --------
 *      lock?  --no--> sleep   lock? --no--> goto retry  (HOT loop)
 *                              (sets cache line on fire across cores)
 *
 *  macOS does not provide pthread_spinlock; we fall back to a mutex there.
 */

#define _XOPEN_SOURCE 700
#include <pthread.h>
#include <stdio.h>

#define ITERS  100000
#define NTH    4

#ifdef __APPLE__
typedef pthread_mutex_t my_lock_t;
#define LOCK_INIT(l)    pthread_mutex_init((l), NULL)
#define LOCK_DESTROY(l) pthread_mutex_destroy(l)
#define LOCK(l)         pthread_mutex_lock(l)
#define UNLOCK(l)       pthread_mutex_unlock(l)
#else
typedef pthread_spinlock_t my_lock_t;
#define LOCK_INIT(l)    pthread_spin_init((l), PTHREAD_PROCESS_PRIVATE)
#define LOCK_DESTROY(l) pthread_spin_destroy(l)
#define LOCK(l)         pthread_spin_lock(l)
#define UNLOCK(l)       pthread_spin_unlock(l)
#endif

static my_lock_t lock;
static long       n = 0;

static void *worker(void *arg)
{
    (void)arg;
    for (int i = 0; i < ITERS; ++i) {
        LOCK(&lock);
        ++n;
        UNLOCK(&lock);
    }
    return NULL;
}

int main(void)
{
    LOCK_INIT(&lock);
    pthread_t t[NTH];
    for (int i = 0; i < NTH; ++i) pthread_create(&t[i], NULL, worker, NULL);
    for (int i = 0; i < NTH; ++i) pthread_join(t[i], NULL);
    LOCK_DESTROY(&lock);
    printf("n = %ld (expected %d)\n", n, NTH * ITERS);
    return 0;
}
