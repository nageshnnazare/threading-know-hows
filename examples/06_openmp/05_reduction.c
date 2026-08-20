/*
 * 05_reduction.c -- reduction(op:var) clause.
 *
 *      #pragma omp parallel for reduction(+:sum)
 *      for (...) sum += a[i];
 *
 *  Each thread gets a PRIVATE copy of `sum` initialized to the identity
 *  element of `op` (0 for +, 1 for *, INT_MIN for max, etc.). At the end,
 *  the runtime combines the private copies pairwise using `op` to produce
 *  the final value.
 *
 *  Picture (4 threads summing 1..N):
 *
 *      iters: 1 2 3 4 ... N
 *      T0: sum0 = sum of T0's slice
 *      T1: sum1 = sum of T1's slice
 *      T2: sum2 = sum of T2's slice
 *      T3: sum3 = sum of T3's slice
 *      combine: total = ((sum0+sum1) + (sum2+sum3))
 *
 *  Supported ops include:
 *      +  *  -  &  |  ^  &&  ||  max  min
 *  (User-defined reductions are also possible: declare reduction.)
 */

#ifdef _OPENMP
#include <omp.h>
#include <stdio.h>

int main(void)
{
    long N = 1000000;
    long sum = 0, prod = 1; (void)prod;
    int  vmax = 0;

    #pragma omp parallel for reduction(+:sum) reduction(max:vmax)
    for (long i = 1; i <= N; ++i) {
        sum  += i;
        if ((int)i > vmax) vmax = (int)i;
    }
    printf("sum(1..%ld) = %ld  (expected %ld)\n", N, sum, N*(N+1)/2);
    printf("max          = %d\n", vmax);
}
#else
#include <stdio.h>
int main(void){ puts("Compile with -fopenmp."); return 0; }
#endif
