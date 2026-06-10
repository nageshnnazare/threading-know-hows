/*
 * 01_parallel.c -- "#pragma omp parallel" basics.
 *
 *  The block below the pragma is executed by a TEAM of threads. The
 *  number of threads defaults to the number of cores (or whatever
 *  OMP_NUM_THREADS / omp_set_num_threads chooses).
 *
 *  Picture (4 threads):
 *
 *      main thread
 *           |
 *           |  #pragma omp parallel  -- fork
 *           +-----+-----+-----+
 *           |     |     |     |
 *           T0    T1    T2    T3
 *           |     |     |     |
 *          (each runs the same block)
 *           |     |     |     |
 *           +-----+-----+-----+  -- implicit join (barrier)
 *           |
 *           v
 *      main resumes
 */

#ifdef _OPENMP
#include <omp.h>
#include <stdio.h>

int main(void)
{
    printf("max threads = %d\n", omp_get_max_threads());

    #pragma omp parallel
    {
        int id  = omp_get_thread_num();
        int tot = omp_get_num_threads();
        printf("hello from thread %d / %d\n", id, tot);
    }
    return 0;
}
#else
#include <stdio.h>
int main(void){ puts("Compile with -fopenmp."); return 0; }
#endif
