/*
 * 04_critical.c -- Mutual exclusion in OpenMP.
 *
 *      #pragma omp critical [(name)]    -- region locked by a global mutex,
 *                                          named or anonymous.
 *      #pragma omp atomic               -- use a hardware atomic op for
 *                                          a SIMPLE update (++, +=, ...).
 *      #pragma omp single               -- only ONE thread runs the block;
 *                                          others wait at implicit barrier.
 *      #pragma omp master               -- like single but only thread 0,
 *                                          and NO implicit barrier.
 *      #pragma omp barrier              -- explicit synchronization point.
 *
 *  Performance hint: prefer atomic over critical when applicable -- it's
 *  much cheaper.
 *
 *  Picture (critical):
 *
 *      thread A: ----+--+----  (in CS)
 *      thread B: ----+ wait  ---+---+---  (in CS)
 *      thread C: ----+ wait  ---+ wait  ---+---+--  (in CS)
 *                    \____ critical region serializes ____/
 */

#ifdef _OPENMP
#include <omp.h>
#include <stdio.h>

int main(void)
{
    long n_critical = 0;
    long n_atomic   = 0;

    #pragma omp parallel for
    for (int i = 0; i < 100000; ++i) {
        #pragma omp critical
        { ++n_critical; }
    }

    #pragma omp parallel for
    for (int i = 0; i < 100000; ++i) {
        #pragma omp atomic
        ++n_atomic;
    }

    printf("critical = %ld  atomic = %ld\n", n_critical, n_atomic);
}
#else
#include <stdio.h>
int main(void){ puts("Compile with -fopenmp."); return 0; }
#endif
