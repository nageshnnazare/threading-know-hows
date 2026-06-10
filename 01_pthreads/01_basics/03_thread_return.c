/*
 * 03_thread_return.c -- Returning a value from a thread.
 *
 *  pthread_join's second argument is a `void **` that receives whatever the
 *  thread returned (or whatever you passed to pthread_exit()).
 *
 *  Two common ways to return a value:
 *
 *  (A) Return a heap-allocated object the caller will free:
 *
 *     thread:                      main:
 *       result = malloc(...)         pthread_join(t, &p);
 *       *result = 42;                printf("%d", *(int*)p);
 *       return result;               free(p);
 *
 *  (B) Return a pointer to caller-owned storage (filled in by the thread):
 *
 *     main:                        thread:
 *       int out;                     a->out = 42;
 *       arg.out_ptr = &out;          return NULL;
 *       pthread_create(..., &arg);
 *
 *  Method (B) is preferred -- no heap, no ownership ambiguity.
 *
 *  This file demonstrates BOTH.
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

/* ------------ method A: heap-allocated return ------------------ */
static void *thread_a(void *arg)
{
    int n = *(int *)arg;
    int *result = malloc(sizeof(int));      /* caller will free */
    *result = n * n;
    return result;                          /* visible via pthread_join */
}

/* ------------ method B: caller-owned out-param ----------------- */
typedef struct { int n; int out; } pkg_t;
static void *thread_b(void *arg)
{
    pkg_t *p = (pkg_t *)arg;
    p->out = p->n * p->n * p->n;            /* cube */
    return NULL;
}

int main(void)
{
    /* (A) */
    pthread_t ta;
    int input = 7;
    pthread_create(&ta, NULL, thread_a, &input);

    void *retval;
    pthread_join(ta, &retval);              /* retval points to the malloc'd int */
    printf("(A) %d^2 = %d\n", input, *(int *)retval);
    free(retval);

    /* (B) */
    pthread_t tb;
    pkg_t pkg = { .n = 7, .out = 0 };
    pthread_create(&tb, NULL, thread_b, &pkg);
    pthread_join(tb, NULL);
    printf("(B) %d^3 = %d\n", pkg.n, pkg.out);

    return 0;
}
