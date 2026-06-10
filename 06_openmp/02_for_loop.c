/*
 * 02_for_loop.c -- Parallel for loop with scheduling.
 *
 *      #pragma omp parallel for schedule(KIND[, CHUNK])
 *
 *  KIND options:
 *      static   -- divide iterations into equal chunks at compile/launch time
 *                  (best when each iteration costs the same)
 *      dynamic  -- threads pull chunks from a shared queue (best when
 *                  iterations vary in cost)
 *      guided   -- like dynamic but chunks shrink (good for "tail" load)
 *      auto     -- let the implementation decide
 *      runtime  -- read OMP_SCHEDULE env var
 *
 *  Picture: 12 iterations across 4 threads, schedule(static):
 *
 *      T0: 0,1,2
 *      T1: 3,4,5
 *      T2: 6,7,8
 *      T3: 9,10,11
 *
 *  schedule(static, 1) (round-robin one-at-a-time):
 *
 *      T0: 0,4,8
 *      T1: 1,5,9
 *      T2: 2,6,10
 *      T3: 3,7,11
 */

#ifdef _OPENMP
#include <omp.h>
#include <stdio.h>
#include <unistd.h>

int main(void)
{
    int N = 12;
    int who[12] = {0};

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < N; ++i) {
        who[i] = omp_get_thread_num();
        usleep(50 * 1000);
    }
    printf("static schedule: ");
    for (int i = 0; i < N; ++i) printf("[%d->T%d] ", i, who[i]);
    printf("\n");

    #pragma omp parallel for schedule(dynamic, 2)
    for (int i = 0; i < N; ++i) {
        who[i] = omp_get_thread_num();
        usleep((i % 4) * 30 * 1000);     /* uneven cost */
    }
    printf("dynamic,2:        ");
    for (int i = 0; i < N; ++i) printf("[%d->T%d] ", i, who[i]);
    printf("\n");
}
#else
#include <stdio.h>
int main(void){ puts("Compile with -fopenmp."); return 0; }
#endif
