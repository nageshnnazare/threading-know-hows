/*
 * 01_basic_mutex.c -- The fundamental tool: pthread_mutex_t.
 *
 *  Without a mutex (SHOWING THE RACE):
 *
 *     thread A: tmp = counter;     (reads 5)
 *     thread B: tmp = counter;     (reads 5)
 *     thread A: tmp = tmp + 1;
 *     thread B: tmp = tmp + 1;
 *     thread A: counter = tmp;     (writes 6)
 *     thread B: counter = tmp;     (writes 6)
 *                                  expected 7, got 6  <-- LOST UPDATE
 *
 *  With a mutex:
 *
 *      thread A                 thread B
 *      --------                 --------
 *      lock()  -- got it
 *        counter++              lock() ... blocks
 *      unlock()                 lock() -- got it
 *                                 counter++
 *                               unlock()
 *
 *  Mental model: a mutex is a TOKEN. Whoever holds the token may touch the
 *  shared data; everyone else waits in a queue.
 *
 *      +--------+      +-------------+
 *      | mutex  | <--- | thread A    |
 *      |  held  |      | (in CS)     |
 *      +--------+      +-------------+
 *           ^
 *           |  blocked threads waiting:
 *           +-- thread B
 *           +-- thread C
 *
 *  Initialization options:
 *      pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;     // static, default
 *      pthread_mutex_init(&m, NULL);                      // dynamic, default
 *      pthread_mutex_init(&m, &attr);                     // dynamic, custom
 */

#include <pthread.h>
#include <stdio.h>

#define NTHREADS  8
#define ITERS     100000

static long             counter = 0;
static pthread_mutex_t  lock    = PTHREAD_MUTEX_INITIALIZER;

static void *worker(void *arg)
{
    (void)arg;
    for (int i = 0; i < ITERS; ++i) {
        pthread_mutex_lock(&lock);
        ++counter;                         /* CRITICAL SECTION */
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

int main(void)
{
    pthread_t t[NTHREADS];

    for (int i = 0; i < NTHREADS; ++i)
        pthread_create(&t[i], NULL, worker, NULL);
    for (int i = 0; i < NTHREADS; ++i)
        pthread_join(t[i], NULL);

    long expected = (long)NTHREADS * ITERS;
    printf("counter = %ld (expected %ld) %s\n",
           counter, expected,
           counter == expected ? "OK" : "WRONG!");

    /* Always destroy mutexes you initialized with pthread_mutex_init.
     * It's harmless to destroy a statically initialized mutex too. */
    pthread_mutex_destroy(&lock);
    return 0;
}
