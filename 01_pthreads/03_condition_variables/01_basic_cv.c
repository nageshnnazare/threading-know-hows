/*
 * 01_basic_cv.c -- Condition variables: "wait until something is true".
 *
 *  A mutex protects shared data. A condition variable lets a thread go
 *  to sleep and be woken when shared data CHANGES into a state of interest.
 *
 *  THREE INVIOLABLE RULES of condvars:
 *
 *    1. Always pair a CV with a mutex AND a predicate (boolean expression
 *       on shared data).
 *    2. Always wait in a LOOP rechecking the predicate (spurious wakeups,
 *       and other threads might race in between signal and wake).
 *    3. The mutex MUST be held when calling pthread_cond_wait. wait()
 *       atomically releases the mutex AND sleeps; on wake-up, it
 *       reacquires the mutex.
 *
 *  Canonical pattern:
 *
 *      // ----- consumer -----
 *      pthread_mutex_lock(&m);
 *      while (!predicate)              <-- LOOP, never an `if`
 *          pthread_cond_wait(&cv, &m); <-- atomically: unlock+sleep, on wake re-lock
 *      // predicate is true; do something
 *      pthread_mutex_unlock(&m);
 *
 *      // ----- producer -----
 *      pthread_mutex_lock(&m);
 *      // change state so predicate becomes true
 *      pthread_cond_signal(&cv);       // wake ONE waiter
 *      // (or pthread_cond_broadcast(&cv) to wake ALL)
 *      pthread_mutex_unlock(&m);
 *
 *  ASCII timeline:
 *
 *     consumer                    producer
 *     --------                    --------
 *     lock(m)
 *     while !ready:
 *         cond_wait ----+
 *         (sleeps)      |
 *                       |          lock(m)
 *                       |          ready = 1
 *                       |          cond_signal
 *                       |          unlock(m)
 *         <--- wakes ---+
 *         (re-locks m)
 *     do_work()
 *     unlock(m)
 *
 *  Why MUST we hold the mutex while signaling?
 *      Because otherwise this race exists:
 *
 *        consumer                       producer
 *        --------                       --------
 *        check predicate (false)
 *                                       set predicate true
 *                                       cond_signal     <-- nobody hears!
 *        cond_wait                       (sleeps forever)
 *
 *      With the mutex held during the predicate-update + signal, the
 *      consumer either hasn't yet locked it (so it'll see the new value
 *      after locking) or is already inside cond_wait (so it'll be woken).
 */

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_mutex_t m       = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cv      = PTHREAD_COND_INITIALIZER;
static int             ready   = 0;
static int             payload = 0;

static void *consumer(void *arg)
{
    (void)arg;
    pthread_mutex_lock(&m);
    while (!ready) {
        printf("[consumer] not ready, sleeping on cv...\n");
        pthread_cond_wait(&cv, &m);
        printf("[consumer] woke up, rechecking predicate\n");
    }
    printf("[consumer] got payload = %d\n", payload);
    pthread_mutex_unlock(&m);
    return NULL;
}

static void *producer(void *arg)
{
    (void)arg;
    sleep(1);                              /* let consumer sleep first */

    pthread_mutex_lock(&m);
    payload = 42;
    ready   = 1;
    pthread_cond_signal(&cv);              /* could be broadcast for many */
    pthread_mutex_unlock(&m);
    printf("[producer] signaled\n");
    return NULL;
}

int main(void)
{
    pthread_t c, p;
    pthread_create(&c, NULL, consumer, NULL);
    pthread_create(&p, NULL, producer, NULL);
    pthread_join(c, NULL);
    pthread_join(p, NULL);
    return 0;
}
