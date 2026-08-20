/*
 * 02_producer_consumer.c -- Bounded buffer with cond vars (the canonical
 *                           multi-producer / multi-consumer queue).
 *
 *  Buffer (capacity = N):
 *
 *      head                                tail
 *       |                                   |
 *       v                                   v
 *      +---+---+---+---+---+---+---+---+---+
 *      | a | b | c | d | _ | _ | _ | _ | _ |  <-- circular array
 *      +---+---+---+---+---+---+---+---+---+
 *       <--- count items --->
 *
 *  Two predicates, two CVs:
 *      not_full   : count < N    (producers wait when full)
 *      not_empty  : count > 0    (consumers wait when empty)
 *
 *  Why TWO CVs (not just one)?
 *      With a single CV and broadcast, you get "thundering herd" --
 *      every wake-up some threads check, find their predicate false,
 *      and go back to sleep. Wasted work. Two CVs let signal() target
 *      exactly the kind of thread that should now be able to make
 *      progress.
 *
 *  The flow:
 *
 *      Producer                                 Consumer
 *      --------                                 --------
 *      lock(m)                                  lock(m)
 *      while count == N:                        while count == 0:
 *          wait(not_full, m)                        wait(not_empty, m)
 *      put(item)                                item = take()
 *      count++                                  count--
 *      signal(not_empty)                        signal(not_full)
 *      unlock(m)                                unlock(m)
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define CAP 4
#define N_PRODUCERS 2
#define N_CONSUMERS 3
#define ITEMS_PER_PRODUCER 8

typedef struct {
    int             buf[CAP];
    int             head;          /* next write index */
    int             tail;          /* next read index */
    int             count;
    pthread_mutex_t m;
    pthread_cond_t  not_full;
    pthread_cond_t  not_empty;
} queue_t;

static void q_init(queue_t *q)
{
    q->head = q->tail = q->count = 0;
    pthread_mutex_init(&q->m, NULL);
    pthread_cond_init(&q->not_full, NULL);
    pthread_cond_init(&q->not_empty, NULL);
}

static void q_destroy(queue_t *q)
{
    pthread_mutex_destroy(&q->m);
    pthread_cond_destroy(&q->not_full);
    pthread_cond_destroy(&q->not_empty);
}

static void q_put(queue_t *q, int v)
{
    pthread_mutex_lock(&q->m);
    while (q->count == CAP)
        pthread_cond_wait(&q->not_full, &q->m);

    q->buf[q->head] = v;
    q->head = (q->head + 1) % CAP;
    q->count++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->m);
}

static int q_get(queue_t *q)
{
    pthread_mutex_lock(&q->m);
    while (q->count == 0)
        pthread_cond_wait(&q->not_empty, &q->m);

    int v = q->buf[q->tail];
    q->tail = (q->tail + 1) % CAP;
    q->count--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->m);
    return v;
}

/* ---- workers ------------------------------------------------- */

static queue_t Q;

static void *producer(void *arg)
{
    int id = (int)(long)arg;
    for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
        int item = id * 1000 + i;
        q_put(&Q, item);
        printf("[P%d] put %d\n", id, item);
        usleep(50 * 1000);
    }
    return NULL;
}

static void *consumer(void *arg)
{
    int id = (int)(long)arg;
    /* terminate when we've consumed our share */
    int total = (N_PRODUCERS * ITEMS_PER_PRODUCER) / N_CONSUMERS + 1;
    for (int i = 0; i < total; ++i) {
        int item = q_get(&Q);
        printf("    [C%d] got %d\n", id, item);
        usleep(120 * 1000);
    }
    return NULL;
}

int main(void)
{
    q_init(&Q);

    pthread_t p[N_PRODUCERS], c[N_CONSUMERS];
    for (long i = 0; i < N_PRODUCERS; ++i)
        pthread_create(&p[i], NULL, producer, (void *)i);
    for (long i = 0; i < N_CONSUMERS; ++i)
        pthread_create(&c[i], NULL, consumer, (void *)i);

    /* Producers will finish naturally; consumers will keep going if no
       items arrive, so we sleep then cancel them. In real code, send a
       sentinel value through the queue. */
    for (int i = 0; i < N_PRODUCERS; ++i) pthread_join(p[i], NULL);
    sleep(2);
    for (int i = 0; i < N_CONSUMERS; ++i) pthread_cancel(c[i]);
    for (int i = 0; i < N_CONSUMERS; ++i) pthread_join(c[i], NULL);

    q_destroy(&Q);
    return 0;
}
