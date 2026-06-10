/*
 * 05_thread_attr.c -- Customize a thread via pthread_attr_t.
 *
 *  By default pthread_create uses defaults that are usually fine. When you
 *  need to tweak something (stack size, scheduling, detach state), set up a
 *  pthread_attr_t and pass it.
 *
 *      +---------------- pthread_attr_t ----------------+
 *      | detachstate    : JOINABLE | DETACHED           |
 *      | stack          : address & size                |
 *      | guardsize      : pages of "no man's land" past |
 *      |                  the stack to catch overflows  |
 *      | schedpolicy    : SCHED_OTHER | SCHED_FIFO ...  |
 *      | schedparam     : priority (RT only)            |
 *      | inheritsched   : EXPLICIT | INHERIT            |
 *      | scope          : SYSTEM | PROCESS              |
 *      +------------------------------------------------+
 *
 *  This example:
 *    - Sets a custom 1 MiB stack.
 *    - Marks the thread as detached at creation time.
 *
 *  Tip: always call pthread_attr_destroy() when you're done with the attr.
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void *worker(void *arg)
{
    (void)arg;
    /* Allocate a small array on the stack to "use" some of it. */
    char buf[256 * 1024];                       /* 256 KiB */
    buf[0] = 'X';
    printf("[worker] stack-allocated 256 KiB at %p\n", (void *)buf);
    sleep(1);
    printf("[worker] done\n");
    return NULL;
}

int main(void)
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);

    /* 1 MiB stack (default is often 8 MiB on Linux glibc). */
    size_t stacksize = 1 * 1024 * 1024;
    pthread_attr_setstacksize(&attr, stacksize);

    /* Create as detached so we don't have to join. */
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_t tid;
    pthread_create(&tid, &attr, worker, NULL);
    pthread_attr_destroy(&attr);

    /* We can't join a detached thread, so wait by other means. */
    sleep(2);
    printf("[main] exiting\n");
    return 0;
}
