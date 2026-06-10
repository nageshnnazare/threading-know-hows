/*
 * 01_rwlock_basic.c -- Read/write lock (pthread_rwlock_t).
 *
 *  Idea: many threads can READ in parallel, but writes need exclusive access.
 *
 *      +----------------------------------------------+
 *      | rwlock state matrix                          |
 *      +-------+--------+--------+--------------------+
 *      |       | reader | writer | resulting state    |
 *      +-------+--------+--------+--------------------+
 *      | free  |  ok    |  ok    | (1 reader / 1 wr)  |
 *      | rd-h  |  ok    | block  | multi readers      |
 *      | wr-h  | block  | block  | exclusive          |
 *      +-------+--------+--------+--------------------+
 *
 *  When to use:
 *    - Reads dominate writes (e.g. config that updates rarely).
 *    - Read critical sections are non-trivial (otherwise locking overhead
 *      eats the savings -- a plain mutex may be faster).
 *
 *  Pitfall: writer starvation. If many readers keep arriving, a pending
 *  writer may wait forever. Most implementations use writer preference,
 *  but verify your platform if it matters.
 *
 *  ASCII for "many readers, one writer":
 *
 *     time --->
 *     R1 ===========
 *     R2  ============
 *     R3   ===========
 *     W                       =====
 *     R4                                ==========
 *
 *     During the W's stretch no reader is allowed; once W releases,
 *     reads run in parallel again.
 */

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_rwlock_t rw    = PTHREAD_RWLOCK_INITIALIZER;
static int              shared = 0;

static void *reader(void *arg)
{
    long id = (long)arg;
    for (int i = 0; i < 3; ++i) {
        pthread_rwlock_rdlock(&rw);
        printf("[R%ld] read shared=%d\n", id, shared);
        usleep(100 * 1000);                /* hold the read lock briefly */
        pthread_rwlock_unlock(&rw);
        usleep(50 * 1000);
    }
    return NULL;
}

static void *writer(void *arg)
{
    long id = (long)arg;
    for (int i = 0; i < 2; ++i) {
        pthread_rwlock_wrlock(&rw);
        ++shared;
        printf("[W%ld] wrote shared=%d (exclusive)\n", id, shared);
        usleep(150 * 1000);
        pthread_rwlock_unlock(&rw);
        usleep(150 * 1000);
    }
    return NULL;
}

int main(void)
{
    pthread_t r[4], w[1];
    for (long i = 0; i < 4; ++i) pthread_create(&r[i], NULL, reader, (void *)i);
    for (long i = 0; i < 1; ++i) pthread_create(&w[i], NULL, writer, (void *)i);
    for (int i = 0; i < 4; ++i) pthread_join(r[i], NULL);
    for (int i = 0; i < 1; ++i) pthread_join(w[i], NULL);
    pthread_rwlock_destroy(&rw);
    return 0;
}
