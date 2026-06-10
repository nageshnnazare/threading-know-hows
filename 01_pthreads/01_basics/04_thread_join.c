/*
 * 04_thread_join.c -- Joinable vs detached threads.
 *
 *  A thread can be in one of two "join states":
 *
 *      JOINABLE (default)
 *      ------------------
 *      Resources stay around until somebody calls pthread_join().
 *      pthread_join() lets you observe the return value AND reclaims the
 *      thread's stack/control block.
 *
 *      DETACHED
 *      --------
 *      "Fire and forget". The runtime cleans up automatically when the
 *      thread exits -- but you can NEVER join it (or query its result).
 *
 *      JOINABLE              DETACHED
 *      --------              --------
 *      ./worker./             ./worker./
 *           |                       |
 *      pthread_join() blocks    nothing blocks
 *      until worker exits;      anywhere; resources
 *      then resources freed.    auto-freed on exit.
 *
 *  Pick:
 *      - Joinable    if you need the result OR need to know it's done.
 *      - Detached    for "background" tasks whose result you discard
 *                    (logging flush, periodic ping, etc.).
 *
 *  Pitfall: forgetting to join a joinable thread -> resource leak (the
 *  thread control block survives until process exit).
 */

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>          /* sleep() */

static void *joinable_fn(void *arg)
{
    (void)arg;
    printf("    [joinable] doing work...\n");
    sleep(1);
    printf("    [joinable] done\n");
    return (void *)0xCAFE;
}

static void *detached_fn(void *arg)
{
    (void)arg;
    for (int i = 0; i < 3; ++i) {
        printf("    [detached] tick %d\n", i);
        sleep(1);
    }
    printf("    [detached] exiting (cleans itself up)\n");
    return NULL;
}

int main(void)
{
    /* JOINABLE example ------------------------------------------- */
    pthread_t j;
    pthread_create(&j, NULL, joinable_fn, NULL);

    void *rv;
    pthread_join(j, &rv);
    printf("[main] joinable returned 0x%lx\n", (unsigned long)rv);

    /* DETACHED example ------------------------------------------- */
    pthread_t d;
    pthread_create(&d, NULL, detached_fn, NULL);
    pthread_detach(d);                     /* mark as detached */

    /* If we exited main here, the detached thread would be killed by
     * process termination. Sleep so we can see it run. */
    printf("[main] sleeping while detached thread runs...\n");
    sleep(4);
    printf("[main] exiting\n");
    return 0;
}
