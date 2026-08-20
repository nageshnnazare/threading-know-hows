/*
 * 03_trylock.c -- Non-blocking attempt to lock.
 *
 *  pthread_mutex_trylock() returns:
 *      0       -- lock acquired
 *      EBUSY   -- somebody else holds it; we did NOT wait
 *
 *  Useful when:
 *      - You can do alternative work if the lock is busy
 *      - You want to detect deadlock potential
 *      - You're doing optimistic concurrency
 *
 *  Picture:
 *
 *      pthread_mutex_lock(m)        pthread_mutex_trylock(m)
 *      ------------------------     -----------------------------
 *      |                            |
 *      v  [lock free?]              v  [lock free?]
 *     yes -> acquire, return 0     yes -> acquire, return 0
 *     no  -> SLEEP until free      no  -> return EBUSY immediately
 */

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static void *holder(void *arg)
{
    (void)arg;
    pthread_mutex_lock(&lock);
    printf("[holder]   acquired the lock; holding for 2 seconds\n");
    sleep(2);
    pthread_mutex_unlock(&lock);
    printf("[holder]   released the lock\n");
    return NULL;
}

static void *poker(void *arg)
{
    (void)arg;
    /* Give holder time to grab the lock first. */
    usleep(100 * 1000);

    for (int i = 0; i < 5; ++i) {
        int rc = pthread_mutex_trylock(&lock);
        if (rc == 0) {
            printf("[poker ]   got it on attempt %d!\n", i);
            pthread_mutex_unlock(&lock);
            break;
        } else if (rc == EBUSY) {
            printf("[poker ]   busy (attempt %d), do other work...\n", i);
            sleep(1);                       /* simulate doing other work */
        } else {
            printf("[poker ]   trylock unexpected error %d\n", rc);
            break;
        }
    }
    return NULL;
}

int main(void)
{
    pthread_t h, p;
    pthread_create(&h, NULL, holder, NULL);
    pthread_create(&p, NULL, poker, NULL);
    pthread_join(h, NULL);
    pthread_join(p, NULL);
    pthread_mutex_destroy(&lock);
    return 0;
}
