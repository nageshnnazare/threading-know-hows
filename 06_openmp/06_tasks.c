/*
 * 06_tasks.c -- OpenMP 3.0+ tasks: irregular parallelism.
 *
 *      #pragma omp task
 *      do_work();
 *
 *  Tasks are good for tree/recursive algorithms (parallel quicksort,
 *  fibonacci, graph traversal). The runtime queues them and any idle
 *  thread can run them.
 *
 *  Below we compute Fibonacci with tasks. (Educational; in practice
 *  spawn-everywhere fib(n) is slower than the serial version due to
 *  overhead -- still a great teaching example.)
 *
 *  Picture (fib(4)):
 *
 *      fib(4)
 *      +-- task fib(3)
 *      |    +-- task fib(2) ...
 *      |    +-- task fib(1)
 *      +-- task fib(2) ...
 *      taskwait
 *      sum results
 */

#ifdef _OPENMP
#include <omp.h>
#include <stdio.h>

long fib(int n)
{
    if (n < 2) return n;
    long a, b;
    #pragma omp task shared(a)
    a = fib(n - 1);
    #pragma omp task shared(b)
    b = fib(n - 2);
    #pragma omp taskwait
    return a + b;
}

int main(void)
{
    long r = 0;
    #pragma omp parallel
    {
        #pragma omp single
        r = fib(20);
    }
    printf("fib(20) = %ld\n", r);
}
#else
#include <stdio.h>
int main(void){ puts("Compile with -fopenmp."); return 0; }
#endif
