/*
 * 02_recursive_mutex.c -- A mutex that the SAME thread can lock multiple
 *                         times without deadlocking itself.
 *
 *  Why does this exist? Consider a function f() that calls g(), and BOTH
 *  want to lock the same mutex. With a regular mutex, the second lock by
 *  the same thread deadlocks the program:
 *
 *     f() {
 *         lock(m);      // holds m
 *         g();          // -.
 *         unlock(m);    //  |
 *     }                 //  v
 *     g() {             // (now inside g, same thread)
 *         lock(m);      // <-- regular mutex: DEADLOCK on self
 *         ...
 *         unlock(m);
 *     }
 *
 *  Recursive mutex semantics:
 *
 *     +---------+   lock        +---------+   lock        +---------+
 *     | count=0 | --thread A--> | count=1 | --thread A--> | count=2 |
 *     +---------+               +---------+               +---------+
 *           ^   unlock <----------- |          unlock         |
 *           |       (count==0:      v        (count==1)       |
 *           +-------- released)                               |
 *
 *  The mutex remembers BOTH which thread holds it AND a depth counter.
 *  Only when the count reaches 0 does another thread get a chance.
 *
 *  Cost: slightly slower; recursive mutexes are usually a "code smell"
 *  meaning your locking design is unclear. Use sparingly.
 */

#include <pthread.h>
#include <stdio.h>

static pthread_mutex_t rlock;

static void inner(void)
{
    pthread_mutex_lock(&rlock);            /* second lock by same thread */
    printf("    inner() locked (depth 2)\n");
    pthread_mutex_unlock(&rlock);
    printf("    inner() unlocked (depth 1)\n");
}

static void *worker(void *arg)
{
    (void)arg;
    pthread_mutex_lock(&rlock);            /* first lock */
    printf("worker() locked (depth 1)\n");
    inner();                               /* would deadlock with normal mutex */
    pthread_mutex_unlock(&rlock);          /* depth 0, fully released */
    printf("worker() fully unlocked\n");
    return NULL;
}

int main(void)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&rlock, &attr);
    pthread_mutexattr_destroy(&attr);

    pthread_t t;
    pthread_create(&t, NULL, worker, NULL);
    pthread_join(t, NULL);

    pthread_mutex_destroy(&rlock);
    return 0;
}
