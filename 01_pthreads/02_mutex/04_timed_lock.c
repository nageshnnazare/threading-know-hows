/*
 * 04_timed_lock.c -- Lock with a deadline.
 *
 *  pthread_mutex_timedlock(&m, &abs_time):
 *      - blocks until either it gets the lock OR `abs_time` passes
 *      - returns 0 on success, ETIMEDOUT on timeout
 *
 *  Note that `abs_time` is an ABSOLUTE point in time (CLOCK_REALTIME by
 *  default), NOT a duration. So you typically:
 *
 *      struct timespec ts;
 *      clock_gettime(CLOCK_REALTIME, &ts);
 *      ts.tv_sec += 1;                     // wait up to 1 second
 *      pthread_mutex_timedlock(&m, &ts);
 *
 *  ASCII timeline (lock currently held by holder for 3 sec, we wait 1 sec):
 *
 *      time --->
 *      holder:   |==========================|
 *      us:       (try)----wait----timeout
 *
 *  NOTE: Not available on macOS without extensions. This file uses an
 *  #ifdef so it still compiles there (the call gracefully degrades).
 */

#define _XOPEN_SOURCE 700
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static void *holder(void *arg)
{
    (void)arg;
    pthread_mutex_lock(&lock);
    printf("[holder] locked, sleeping 3 seconds\n");
    sleep(3);
    pthread_mutex_unlock(&lock);
    return NULL;
}

static void *waiter(void *arg)
{
    (void)arg;
    /* let holder lock first */
    usleep(100 * 1000);

#ifdef __APPLE__
    /* pthread_mutex_timedlock is not in macOS libc by default; emulate. */
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += 1;

    while (1) {
        int rc = pthread_mutex_trylock(&lock);
        if (rc == 0) {
            printf("[waiter] got the lock!\n");
            pthread_mutex_unlock(&lock);
            return NULL;
        }
        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        if (now.tv_sec  >  deadline.tv_sec ||
           (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec)) {
            printf("[waiter] timeout (poll-emulated)\n");
            return NULL;
        }
        struct timespec wait = { 0, 10 * 1000 * 1000 }; /* 10ms */
        nanosleep(&wait, NULL);
    }
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 1;                                   /* 1-second deadline */

    int rc = pthread_mutex_timedlock(&lock, &ts);
    if (rc == 0) {
        printf("[waiter] got the lock\n");
        pthread_mutex_unlock(&lock);
    } else if (rc == ETIMEDOUT) {
        printf("[waiter] timed out after 1 second (lock still held)\n");
    } else {
        printf("[waiter] error %d\n", rc);
    }
#endif
    return NULL;
}

int main(void)
{
    pthread_t h, w;
    pthread_create(&h, NULL, holder, NULL);
    pthread_create(&w, NULL, waiter, NULL);
    pthread_join(h, NULL);
    pthread_join(w, NULL);
    pthread_mutex_destroy(&lock);
    return 0;
}
