/*
 * 01_hello_thread.c -- The simplest possible pthread program.
 *
 *  Compile : gcc -pthread -o 01_hello_thread 01_hello_thread.c
 *  Run     : ./01_hello_thread
 *
 *  Concept: pthread_create() launches a new OS-level thread that runs
 *  start_routine(arg). pthread_join() blocks until that thread finishes.
 *
 *  ASCII timeline:
 *
 *   main thread        worker thread
 *   -----------        -------------
 *      |
 *      |  pthread_create() ----> spawned
 *      |                              |
 *      |                              |  printf("hello")
 *      |                              |
 *      |  pthread_join()  <----- exits|
 *      |
 *      v
 *    main returns
 *
 *  Key data structure:
 *
 *   pthread_t  -- an OPAQUE handle, not the thread itself.
 *                 Think of it as "a ticket I hand back to the library
 *                 to refer to that thread later".
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Every pthread function has the signature:  void *fn(void *arg);
 * The void* in/out is the universal "anything" type in C.
 */
static void *worker(void *arg)
{
    (void)arg;                                    /* silence unused warning */
    printf("[worker]  hello from the worker thread!\n");
    return NULL;                                  /* return value visible via pthread_join */
}

int main(void)
{
    pthread_t tid;                                /* handle for the new thread */

    printf("[main  ]  about to spawn a worker...\n");

    /*
     * pthread_create(out_tid, attr, start_fn, arg)
     *  - attr=NULL  -> default attributes (joinable, default stack)
     *  - returns 0 on success, errno-like value on failure (NOT -1!)
     */
    int rc = pthread_create(&tid, NULL, worker, NULL);
    if (rc != 0) {
        fprintf(stderr, "pthread_create failed: %d\n", rc);
        return EXIT_FAILURE;
    }

    /*
     * pthread_join: wait for the thread to finish AND release its resources.
     * Without join (or detach) you have a "zombie" thread leak.
     */
    pthread_join(tid, NULL);

    printf("[main  ]  worker has finished, exiting.\n");
    return EXIT_SUCCESS;
}
