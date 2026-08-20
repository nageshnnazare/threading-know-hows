/*
 * 01_tls_pthread.c -- Thread-local storage with pthread_key_t.
 *
 *  Sometimes each thread needs its OWN copy of a variable -- e.g.
 *  a per-thread errno, scratch buffer, or RNG state. Two ways:
 *
 *    1. Compiler-provided  __thread  (GCC) / thread_local (C11)  -- easy.
 *    2. POSIX runtime API  pthread_key_t  -- more flexible
 *       (you can attach a destructor that runs at thread exit).
 *
 *  Picture:
 *
 *     +-------- shared key (pthread_key_t k) --------+
 *     |  k -> per-thread slot                        |
 *     +----------------------------------------------+
 *           |                |                |
 *     thread A:        thread B:        thread C:
 *      slot[k] = X      slot[k] = Y      slot[k] = Z
 *
 *  Lifetime:
 *      pthread_key_create(&k, dtor)   -- ONCE per process
 *      pthread_setspecific(k, ptr)    -- per thread
 *      pthread_getspecific(k)         -- per thread
 *      (when thread exits, dtor(ptr) is called -- great for cleanup)
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static pthread_key_t   key;
static pthread_once_t  key_once = PTHREAD_ONCE_INIT;

/* Destructor: runs in the exiting thread once, after start_routine returns
 * but before the thread is fully torn down. */
static void buf_dtor(void *p)
{
    printf("    [dtor] freeing per-thread buffer %p\n", p);
    free(p);
}

static void make_key(void) { pthread_key_create(&key, buf_dtor); }

static char *scratch(void)
{
    pthread_once(&key_once, make_key);
    char *p = pthread_getspecific(key);
    if (!p) {
        p = malloc(64);
        pthread_setspecific(key, p);
        snprintf(p, 64, "thread %lu's buffer", (unsigned long)pthread_self());
    }
    return p;
}

static void *worker(void *arg)
{
    long id = (long)arg;
    printf("[T%ld] my scratch -> \"%s\"\n", id, scratch());
    /* call again -- same buffer, no re-allocation */
    char *p = scratch();
    strcat(p, " (touched)");
    printf("[T%ld] now    -> \"%s\"\n", id, p);
    return NULL;
}

int main(void)
{
    pthread_t t[3];
    for (long i = 0; i < 3; ++i) pthread_create(&t[i], NULL, worker, (void *)i);
    for (int i = 0; i < 3; ++i) pthread_join(t[i], NULL);
    pthread_key_delete(key);
    return 0;
}
