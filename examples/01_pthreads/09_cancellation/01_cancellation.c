/*
 * 01_cancellation.c -- Cooperative thread cancellation.
 *
 *  pthread_cancel(t) sets a flag asking thread t to stop. Whether/when t
 *  actually stops depends on its CANCEL STATE and CANCEL TYPE:
 *
 *      CANCEL STATE
 *        ENABLE  (default) : cancellation requests honored
 *        DISABLE           : pending cancels stay pending until enabled
 *
 *      CANCEL TYPE (only matters when state=ENABLE)
 *        DEFERRED (default): cancel acts at the next "cancellation point"
 *                            (read, write, sleep, cond_wait, etc.)
 *        ASYNCHRONOUS      : cancel any time -- DANGEROUS, almost never use
 *
 *  Cleanup handlers:
 *      pthread_cleanup_push(fn, arg)   register
 *      pthread_cleanup_pop(0_or_1)     unregister; arg=1 means "also run"
 *
 *      They are unwound (LIFO) when the thread is canceled or calls
 *      pthread_exit. Use them to release locks, free buffers, close
 *      files in cancel-safe code.
 *
 *  Picture:
 *
 *      main                worker
 *      ----                -------
 *                          ...
 *                          (cancel state ENABLED, DEFERRED)
 *                          ...
 *      pthread_cancel ----> (flag set, but worker still running)
 *                          ...
 *                          calls sleep()  <-- cancellation point!
 *                          (cleanup handlers run, thread exits)
 *      pthread_join() <----
 *
 *  Modern best practice: avoid cancellation. Use a shared atomic flag
 *  ("please stop") and check it at safe places. It's portable, debuggable,
 *  and avoids the surprises of unwinding mid-syscall.
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void cleanup(void *arg)
{
    printf("    [cleanup] releasing resource %p\n", arg);
    free(arg);
}

static void *worker(void *arg)
{
    (void)arg;
    int *resource = malloc(sizeof(int));
    *resource = 0xDEADBEEF;

    pthread_cleanup_push(cleanup, resource);

    /* Loop with periodic cancellation points. */
    while (1) {
        printf("[worker] tick\n");
        sleep(1);                          /* sleep IS a cancellation point */
    }

    pthread_cleanup_pop(1);                /* never reached on cancel */
    return NULL;
}

int main(void)
{
    pthread_t t;
    pthread_create(&t, NULL, worker, NULL);

    sleep(3);
    printf("[main] requesting cancellation\n");
    pthread_cancel(t);

    void *rv;
    pthread_join(t, &rv);
    if (rv == PTHREAD_CANCELED)
        printf("[main] worker was canceled (cleanup ran)\n");
    else
        printf("[main] worker exited normally\n");
    return 0;
}
