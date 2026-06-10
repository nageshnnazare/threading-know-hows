/*
 * 03_sections.c -- "#pragma omp sections": run different code blocks
 *                  on different threads.
 *
 *  Useful when you have N independent steps to do simultaneously rather
 *  than N iterations of one task.
 *
 *      sections
 *      +----------------+----------------+----------------+
 *      | section A      | section B      | section C      |
 *      | (thread X)     | (thread Y)     | (thread Z)     |
 *      +----------------+----------------+----------------+
 *           |                |                |
 *           +------ implicit barrier ---------+
 */

#ifdef _OPENMP
#include <omp.h>
#include <stdio.h>
#include <unistd.h>

int main(void)
{
    #pragma omp parallel sections
    {
        #pragma omp section
        { printf("A start (T%d)\n", omp_get_thread_num()); sleep(1);
          printf("A done\n"); }

        #pragma omp section
        { printf("B start (T%d)\n", omp_get_thread_num()); sleep(2);
          printf("B done\n"); }

        #pragma omp section
        { printf("C start (T%d)\n", omp_get_thread_num()); sleep(1);
          printf("C done\n"); }
    }
    printf("(all sections joined)\n");
}
#else
#include <stdio.h>
int main(void){ puts("Compile with -fopenmp."); return 0; }
#endif
