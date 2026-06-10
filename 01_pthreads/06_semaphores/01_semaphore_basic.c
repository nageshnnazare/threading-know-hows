/*
 * 01_semaphore_basic.c -- POSIX unnamed semaphore (sem_t).
 *
 *  A semaphore is a non-negative counter with two atomic operations:
 *
 *      sem_wait(s)    aka P()    if s>0: s--, else block
 *      sem_post(s)    aka V()    s++; possibly wakes one waiter
 *
 *  With initial value 1 it's a binary semaphore (mutex-ish, but any thread
 *  can post -- no ownership). With value N it gates N concurrent users.
 *
 *  Picture: 3 connections allowed at once into a "DB pool":
 *
 *     pool sem starts at 3:
 *
 *     T1 sem_wait -> 2
 *     T2 sem_wait -> 1
 *     T3 sem_wait -> 0
 *     T4 sem_wait -> blocks (-1 conceptually)
 *     T1 sem_post -> 0     (wakes T4 -> still 0 from its perspective)
 *     T4 ...
 *
 *  When to choose semaphore vs mutex/cv:
 *      - Counting resources (slots, tokens, buffers) -> semaphore
 *      - Mutual exclusion of a section                -> mutex
 *      - "Wait until predicate"                       -> mutex + cond_var
 *
 *  NOTE: macOS deprecated unnamed semaphores. We use named sem on macOS.
 */

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>           /* for O_CREAT */
#include <stdlib.h>

#define POOL  3
#define USERS 6

#ifdef __APPLE__
static sem_t *gate;
#define SEM_OBJ gate
#define SEM_NAME "/threading_guide_demo_sem"
#else
static sem_t  gate_storage;
static sem_t *gate = &gate_storage;
#define SEM_OBJ gate
#endif

static void *user(void *arg)
{
    long id = (long)arg;
    printf("[U%ld] waiting for a slot...\n", id);
    sem_wait(SEM_OBJ);
    printf("    [U%ld] working in the pool\n", id);
    sleep(1);
    printf("    [U%ld] leaving the pool\n", id);
    sem_post(SEM_OBJ);
    return NULL;
}

int main(void)
{
#ifdef __APPLE__
    sem_unlink(SEM_NAME);
    SEM_OBJ = sem_open(SEM_NAME, O_CREAT, 0600, POOL);
    if (SEM_OBJ == SEM_FAILED) { perror("sem_open"); return 1; }
#else
    sem_init(SEM_OBJ, 0 /*pshared=0:thread-shared*/, POOL);
#endif

    pthread_t t[USERS];
    for (long i = 0; i < USERS; ++i)
        pthread_create(&t[i], NULL, user, (void *)i);
    for (int i = 0; i < USERS; ++i)
        pthread_join(t[i], NULL);

#ifdef __APPLE__
    sem_close(SEM_OBJ);
    sem_unlink(SEM_NAME);
#else
    sem_destroy(SEM_OBJ);
#endif
    return 0;
}
