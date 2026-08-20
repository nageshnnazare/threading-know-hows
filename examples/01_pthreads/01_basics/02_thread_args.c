/*
 * 02_thread_args.c -- Passing arguments to threads.
 *
 *  Compile : gcc -pthread -o 02_thread_args 02_thread_args.c
 *  Run     : ./02_thread_args
 *
 *  pthread_create takes a SINGLE void* arg. If you need more than one piece
 *  of data, bundle them in a struct and pass a pointer to it.
 *
 *  CAREFUL: the memory you pass MUST outlive the worker. The classic bug:
 *
 *      for (int i = 0; i < N; i++)
 *          pthread_create(&t[i], 0, fn, &i);   // BUG! all threads see the
 *                                              // same `i` -- it's mutating!
 *
 *  Memory layout when we pass an array of structs:
 *
 *     workers[]
 *     +---------------+   <-- worker 0 reads from here
 *     | id=0, n=10    |
 *     +---------------+   <-- worker 1 reads from here
 *     | id=1, n=20    |
 *     +---------------+   <-- worker 2 reads from here
 *     | id=2, n=30    |
 *     +---------------+
 *
 *  Each thread receives a UNIQUE pointer, so there's no race.
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define N 4

typedef struct {
    int id;
    int n;
} arg_t;

static void *worker(void *arg)
{
    arg_t *a = (arg_t *)arg;                /* cast back to real type */
    long sum = 0;
    for (int i = 1; i <= a->n; ++i)
        sum += i;
    printf("[t%d] sum(1..%d) = %ld\n", a->id, a->n, sum);
    return NULL;
}

int main(void)
{
    pthread_t tids[N];
    arg_t     args[N];                      /* MUST live until threads exit */

    for (int i = 0; i < N; ++i) {
        args[i].id = i;
        args[i].n  = (i + 1) * 10;          /* 10, 20, 30, 40 */
        pthread_create(&tids[i], NULL, worker, &args[i]);
    }

    for (int i = 0; i < N; ++i)
        pthread_join(tids[i], NULL);

    return 0;
}
