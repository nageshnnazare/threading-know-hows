# OpenMP -- The Pragma-Driven Way

OpenMP is a *compiler-supported* parallelism model. You sprinkle `#pragma
omp ...` directives over otherwise-serial code and the compiler/runtime
take care of thread management.

```
   #pragma omp parallel for
   for (int i = 0; i < n; ++i)
       a[i] = b[i] * c[i];

         |
         v
   +-------------- compiler expansion ----------------+
   |  spawn N threads                                 |
   |  for thread t in 0..N-1:                         |
   |      for i in t * n/N .. (t+1) * n/N:            |
   |          a[i] = b[i] * c[i]                      |
   |  barrier                                         |
   +--------------------------------------------------+
```

## When to choose OpenMP vs std::thread

| Situation                              | Pick |
|----------------------------------------|------|
| Tight numerical loops on arrays        | OpenMP |
| Heterogeneous tasks, services          | std::thread / thread pool |
| Need fine-grained synchronization      | std::thread + atomics |
| Want vectorization with multi-threading | OpenMP (4.0+ has SIMD pragmas) |
| Portable to non-OpenMP compilers       | std::thread |

## Examples

| #  | File                | Topic |
|----|---------------------|-------|
| 01 | `01_parallel.c`     | `#pragma omp parallel`, `omp_get_thread_num` |
| 02 | `02_for_loop.c`     | `parallel for`, scheduling |
| 03 | `03_sections.c`     | Different code paths in parallel |
| 04 | `04_critical.c`     | `critical`, `atomic`, named critical sections |
| 05 | `05_reduction.c`    | Reductions (sum/max/min/...) |
| 06 | `06_tasks.c`        | OpenMP 3.0 task model for irregular work |

Compile: `cc -fopenmp ...` (gcc) or `clang -fopenmp ...`. macOS Apple
Clang doesn't ship libomp by default; install via `brew install libomp`.

Build: `make`. Falls back to a friendly message if OpenMP isn't available.
