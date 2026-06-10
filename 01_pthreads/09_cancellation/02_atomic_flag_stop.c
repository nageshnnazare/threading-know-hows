/*
 * 02_atomic_flag_stop.c -- Modern alternative to pthread_cancel:
 *                          a cooperative atomic stop flag.
 *
 *  Problem with pthread_cancel:
 *      - Unwinds in surprising places.
 *      - Requires cleanup handlers everywhere there's a lock.
 *      - Disabled by some libraries.
 *      - Hard to test deterministically.
 *
 *  Idiom (works in C and C++):
 *
 *      worker:                 supervisor:
 *      while (!stop)           stop = 1;
 *          do_chunk();
 *
 *  As long as `stop` is atomic and the worker checks it at known
 *  safe points, the worker shuts down cleanly with no surprise unwinding.
 *
 *  ASCII:
 *
 *      stop=0
 *       |   chunk1   chunk2   chunk3
 *       |   ======   ======   ======
 *       |                ^
 *       |                | supervisor sets stop=1
 *       |                v
 *       |   chunk1   chunk2  (peek-stop) -> exit gracefully
 */

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdatomic.h>

static atomic_int stop = 0;

static void *worker(void *arg)
{
    (void)arg;
    int chunks = 0;
    while (!atomic_load_explicit(&stop, memory_order_acquire)) {
        ++chunks;
        printf("[worker] chunk %d\n", chunks);
        usleep(300 * 1000);
    }
    printf("[worker] saw stop, did %d chunks, exiting cleanly\n", chunks);
    return NULL;
}

int main(void)
{
    pthread_t t;
    pthread_create(&t, NULL, worker, NULL);
    sleep(2);
    printf("[main] asking worker to stop\n");
    atomic_store_explicit(&stop, 1, memory_order_release);
    pthread_join(t, NULL);
    return 0;
}
