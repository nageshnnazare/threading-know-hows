/*
 * 04_timed_wait.c -- pthread_cond_timedwait(): wait, but no longer than X.
 *
 *  Same as pthread_cond_wait, but if the deadline passes before a signal
 *  arrives, returns ETIMEDOUT.
 *
 *  A correct timed-wait loop must compute an ABSOLUTE deadline ONCE, then
 *  re-check it each iteration; otherwise spurious wakeups extend your wait.
 *
 *      compute deadline = now + dt
 *      lock
 *      while (!predicate):
 *          rc = timedwait(cv, m, deadline)
 *          if rc == ETIMEDOUT and !predicate:
 *              break  // give up
 *      unlock
 *
 *  ASCII:
 *
 *     time --->
 *     |---now-------------------dt-------------------|deadline
 *           ^ thread sleeping on cv
 *                                               ^ if no signal by here,
 *                                                 timedwait returns
 *                                                 ETIMEDOUT
 */

#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

static pthread_mutex_t m       = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cv      = PTHREAD_COND_INITIALIZER;
static int             ready   = 0;       /* will never become true */

int main(void)
{
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += 1;                  /* 1 second budget */

    pthread_mutex_lock(&m);
    int rc = 0;
    while (!ready) {
        rc = pthread_cond_timedwait(&cv, &m, &deadline);
        if (rc == ETIMEDOUT) {
            printf("[main] gave up waiting (1s deadline reached)\n");
            break;
        }
    }
    pthread_mutex_unlock(&m);
    return 0;
}
